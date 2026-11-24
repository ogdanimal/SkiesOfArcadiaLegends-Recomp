// 60 fps: the Dolphin build's frame interpolation (Source/Core/VideoCommon/FrameInterpolator.cpp)
// on this host.
//
// While a frame is processed, its GX command stream is recorded (display lists expanded, indexed
// matrix loads turned into immediate ones, the vertex-array bytes each indexed draw reads copied),
// together with where each XF matrix load and each draw's inline float positions sit in it. At the
// frame's XFB copy the stream is blended with the previous frame's: matrices and positions with a
// counterpart (by XF address and the next draw's key; draws by vertex format, count and texture,
// the lists of alike ones aligned by their first vertices) are averaged, values that jump (a camera
// cut, the menu's fade quad) snap, and a frame with too few counterparts (a scene change) isn't
// interpolated. Skies transforms vertices on the CPU (Ninja), so positions matter as much as
// matrices. The blend, Align, Blendable and VertexJumps are the Dolphin build's. [Skies] Each draw also
// records which game object (task) drew it (DrawTask), and two objects' draws are never paired; an object whose draws
// are alike one for one in both frames pairs them by their order (Prim::ordinal).
//
// The game's thread records (InterpCommand, from hw_gpu.cpp); at the frame's XFB copy the recording
// goes to the video thread (video.cpp) with the frame end, which keeps the real picture there
// (InterpFrameEndVideo) and blends and replays once the next frame's tokens are passed
// (InterpReplayVideo), beside the game, as the Dolphin build's GPU thread does. The replay runs in
// Aurora's replay context (fork 0019: its own GX state on a spare EFB): the registers the frame
// started with, then the blended stream with each indexed draw's arrays handed over as host pointers
// (Aurora's command 0x50, sub 0x0010 + array), Aurora keeping the in-between picture just before the
// replay's own XFB copy. The real picture is kept too, and video.cpp presents the in-between one at
// the first retrace after the copy, the real one at the next (the Dolphin build's order).
//
// [Skies] FrameMode (-1 the default and 0: 30 fps, 1: 60 fps, 2: the display's refresh rate; SKIES_INTERP=0/1/2
// overrides it), held off while the game runs fast (menu.cpp qol::Write).
//
// At the display's refresh rate (FrameMode 2, from Dusklight's game_clock.cpp: the game at its own rate, the
// pictures between its last two frames at the time each is shown) the recordings stay here a while
// (s_display) and video.cpp's timeline asks for a blend at any position between two of them
// (InterpDisplayReplay) for each refresh.
// SKIES_INTERP_T: the blend's position between the frames (0.5). SKIES_INTERP_STATS=1: counts and
// times every 600 frames. SKIES_INTERP_TRACE=1 (video.cpp): each frame end and retrace present. To compare
// (issues #18 and #24), =0 turns off: SKIES_INTERP_CENTROID (pairing by mean vertex), SKIES_INTERP_GAP_FLOOR (the
// least cost of leaving a draw or a position matrix unpaired), SKIES_INTERP_ORDINAL (an object's draws paired by number),
// SKIES_INTERP_LOCAL_SNAP (a jump snaps its object, or nearby object-less draws, not every draw with its texture),
// SKIES_INTERP_CAMERA_PAIR (an object's alike pieces paired again by its motion or the camera's),
// SKIES_INTERP_COLOURS (=0 off, or a mask, default 28: 4 direct vertex colours, 8 indexed colours and texture coordinates,
// 16 direct texture coordinates, of paired draws blended like the positions, a coordinate that jumps (a flipbook cell) taking
// the previous frame's for the first half of the way: the pyramid's torch glow quads scroll their coordinates through an
// indexed array; 1 lights, 2 the chans' ambient and material colour registers: blended they made other scenes worse),
// SKIES_INTERP_RIGID (an object that moved as one rigid body, the map with the camera: its draws without a partner of
// their shape, or that jumped, follow that motion, and a draw that doesn't follow it finds its partner by it) and
// SKIES_INTERP_NEW_MATRICES (a position or normal matrix with no partner gets its previous value from the camera's
// motion). SKIES_INTERP_STRICT=1 (a test, with SKIES_INTERP_T=0): what had no partner isn't drawn, so the in-between
// should equal the previous frame (development notes, "60 fps after v0.93").
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <unordered_map>
#include <mutex>
#include <tuple>
#include <vector>

#ifdef SKIES_STANDALONE_AURORA
extern "C" {
void aurora_backend_write_raw(const void* data, u32 size);
bool aurora_backend_drain_if_over(u32 bytes);
void aurora_backend_replay_context(int enter);
int aurora_backend_keep_picture(int slot);
int aurora_backend_in_replay_context(void);
unsigned aurora_backend_texture_frame(void);
void aurora_backend_replay_texture_frame(unsigned frame);
}
#endif

namespace host
{
namespace
{
// XF addresses (VideoCommon/XFMemory.h)
constexpr u32 XFMEM_POSMATRICES_END = 0x100;
constexpr u32 XFMEM_NORMALMATRICES = 0x400;
constexpr u32 XFMEM_NORMALMATRICES_END = 0x460;
constexpr u32 XFMEM_POSTMATRICES = 0x500;
constexpr u32 XFMEM_POSTMATRICES_END = 0x600;
constexpr u32 XFMEM_SETPROJECTION = 0x1020;
// [Skies] Lights (0x600 + 16 * i: three unused words, the colour, then the cosine and distance attenuation, the position
// and the direction, floats) and the chans' colour registers (0x100A-0x100D: ambient 0 and 1, material 0 and 1)
constexpr u32 XFMEM_LIGHTS = 0x600;
constexpr u32 XFMEM_LIGHTS_END = 0x680;
constexpr u32 XFMEM_AMBIENT0 = 0x100A;
constexpr u32 XFMEM_MATERIAL1 = 0x100D;
// ComponentFormat
constexpr u8 FORMAT_UBYTE = 0, FORMAT_BYTE = 1, FORMAT_USHORT = 2, FORMAT_SHORT = 3, FORMAT_FLOAT = 4;

// An XF load that sets matrices or the projection, and where its data sits in the stream.
struct Load
{
  u32 data_offset;  // first data word in the recorded stream
  u16 address;
  u8 count;         // words
  u64 key;          // the next draw's vertex format, count and texture, to tell objects apart
  u32 occurrence;   // how many earlier loads in the frame had the same address, count and key
  u32 task = 0;     // [Skies] the game object (task) of the draw that uses it, 0 if not known (as Prim::task)
  u32 func = 0;     // the task's function
  u32 texture = 0;  // the next draw's texture (in key): its shape without it is key ^ (texture << 20)
  u32 prim = ~0u;   // the next draw, when it has direct positions: its index in Frame::prims
};

// A draw with its vertex positions in the stream (direct positions only).
struct Prim
{
  u32 data_offset;  // first vertex in the recorded stream
  u16 num_vertices;
  u16 vertex_size;
  u8 pos_offset;    // within a vertex
  u8 pos_format;    // ComponentFormat
  u8 pos_count;     // 2 or 3
  u64 key;
  u32 texture;      // texture unit 0's image address: draws of one object share it
  u32 task = 0;     // the game object (task) that drew it, 0 if not known (DrawTask)
  u32 func = 0;     // the task's function (its class), against a task freed and another made at its address
  u32 ordinal = 0;  // the object's draws numbered in the order it made them (recorded blocks by recording order)
  // [Skies] The colours the draw sends direct (CP colour 0 and 1: offset within a vertex, 0xFF if not direct, and
  // their ComponentFormat), blended with the positions (ApplyPlan)
  u8 col_off[2] = {0xFF, 0xFF};
  u8 col_fmt[2] = {0, 0};
  // [Skies] Texture coordinates of units 0-3 sent direct (offset within a vertex, 0xFF if not; components 1 or 2, their
  // ComponentFormat and fraction bits): scrolling or flipbook coordinates blended like the positions
  struct TexAttr
  {
    u8 off = 0xFF, elems = 0, format = 0, frac = 0;
  };
  TexAttr tex[4];
};

// Vertex array data a draw reads by index, copied when it was drawn.
struct ArrayCopy
{
  u8 array;         // CP array: 0 position, 1 normal, 2-3 colours, 4-11 texture coordinates
  u32 data_offset;  // in Frame::array_data
  u32 lo;           // bytes from the array base to the first byte copied
  u32 size;
  u32 guest_base;   // the array's base register when it was drawn, put back after the replay's draw
  // [Skies] What blends (ApplyPlan): 0 nothing, 1 a colour array (colour_format), 2 texture coordinates (elems of
  // tex_format, frac fraction bits);
  // where the draw's index sits in a vertex, its size, and the array's stride
  u8 kind = 0, idx_off = 0, idx_size = 0, elems = 0, colour_format = 0, tex_format = 0, frac = 0;
  u32 stride = 0;
};

// Every draw command: where it is, and its array copies (array_copies[arrays_begin..arrays_end])
struct Draw
{
  u32 offset, length;
  u32 arrays_begin, arrays_end;
};

struct Frame
{
  std::vector<u8> stream;
  std::vector<Load> loads;
  std::vector<Prim> prims;
  std::vector<Draw> draws;
  std::vector<u8> array_data;
  std::vector<ArrayCopy> array_copies;
  // Loads by address, count, key and occurrence, and draws by key (then draw order), as indices:
  // sorted once, the current frame's order serves again when it becomes the previous frame.
  std::vector<u32> load_order, prim_order;
  bool sorted = false;
  u64 serial = 0;      // set when the recording is handed over (BuildBlendedStream's plan key)
  GpRegisters start;   // the registers when the frame began (the previous frame's XFB copy done)
  bool has_start = false;
  // Aurora's texture-hash frame of the frame's draws (InterpFrameEndVideo): the replay draws with the
  // textures they had. The game rewrites some in place every frame (the glyph ring the menus' text goes
  // through), and by the replay RAM holds the next frame's: other letters (issue #37).
  u32 texture_frame = 0;

  void Clear()
  {
    stream.clear();
    loads.clear();
    prims.clear();
    draws.clear();
    array_data.clear();
    array_copies.clear();
    load_order.clear();
    prim_order.clear();
    sorted = false;
    serial = 0;
    has_start = false;
    texture_frame = 0;
  }
};

// The game's thread records into s_current; the video thread keeps the last complete recording in
// s_previous to blend the next with. Recordings come from a pool, so their buffers are used again.
Frame* s_current = nullptr;   // the game's thread
Frame* s_previous = nullptr;  // the video thread
std::mutex s_pool_mutex;
std::vector<Frame*> s_pool;

Frame* TakeFrame()
{
  {
    std::lock_guard lock(s_pool_mutex);
    if (!s_pool.empty())
    {
      Frame* frame = s_pool.back();
      s_pool.pop_back();
      return frame;
    }
  }
  return new Frame;
}

void GiveBack(Frame* frame)
{
  if (!frame)
    return;
  frame->Clear();
  std::lock_guard lock(s_pool_mutex);
  s_pool.push_back(frame);
}
// Statistics (SKIES_INTERP_STATS) every 600 frames: loads and draws, blended, and changed
// Time on the video thread per frame end, ms: keeping the real picture, the blend, the replay's
// context switch in, its feed, keeping the in-between picture and the switch back out
double s_ms[6] = {};
using Clock = std::chrono::steady_clock;
double Ms(Clock::time_point since)
{
  return std::chrono::duration<double, std::milli>(Clock::now() - since).count();
}
u64 s_stat_loads = 0, s_stat_prims = 0, s_stat_loads_blended = 0, s_stat_prims_blended = 0,
    s_stat_words_changed = 0, s_stat_draws = 0, s_stat_prims_changed = 0;
u64 s_stat_repaired = 0;  // draws paired across a texture change (issue #18)
u64 s_stat_loads_apart = 0;  // matrix pairings refused as two objects' (issue #24)
u64 s_stat_loads_by_object = 0;  // [Skies] groups of loads paired by object alone, their kinds unmatched (issue #24)
u64 s_stat_synth_matrices = 0;  // [Skies] new matrices moved back with the camera (issue #24)
u64 s_stat_jumps_explained = 0;  // [Skies] paired draws past the jump test that their object's motion explains (issue #24)
u64 s_stat_collapsed = 0, s_stat_matrix_snaps = 0;  // draws and matrices snapped as turning round (issue #35)
u64 s_stat_camera_paired = 0;  // [Skies] draws re-paired by the camera's motion (issue #24)
u64 s_stat_tasked = 0, s_stat_kept_apart = 0, s_stat_by_number = 0, s_stat_moved = 0, s_stat_rigid_repaired = 0;  // draws with a known object; pairings refused as two objects'
// Which game object drew each draw (the game's thread). Skies runs every object as a task (decomp
// docs/draw.md: TaskRunAll calls each task's function, which updates and draws it), and the running task is
// at 0x80311A7C (USA), so a draw made inside TaskRunAll's call belongs to that task. Most draws go straight into
// the FIFO: hw_gpu.cpp takes the object at the FIFO burst that brought a command's first bytes, which is
// while the game writes it (the GPU runs synchronously). The draw queue's display lists (battles, cutscene
// models) are recorded inside the task's call and replayed later, sorted by depth: the object is taken
// when GXBeginDisplayList points the CPU FIFO at the block (the PI FIFO base) and looked up when the block
// is called. Draws made by the deferred tasks (TaskRunDeferred doesn't update the running task) take the task from
// its loop (DeferredTask); the 2D layer and the frame's own drawing get 0. The pairing below never pairs two different
// objects' draws: alike sprites of two objects that depth sorting swaps (#18, #24) stay apart. Each object's draws
// are numbered in the order it made them (blocks by recording order, not the depth-sorted replay's).
std::unordered_map<u32, std::pair<u32, u32>> s_dl_tasks;  // block address (physical) -> task, its block number, this frame
u32 s_dl_task = 0;                        // the top-level display list being replayed
u32 s_dl_block = 0, s_dl_draw = 0;        // its number among its object's blocks; draws in it so far
std::unordered_map<u32, u32> s_task_blocks, s_task_draws;  // task -> blocks recorded, direct draws made, this frame
bool s_in_dl = false;
u32 s_top_task = 0;                       // the top-level command being applied
// SKIES_INTERP_TASKS=0: don't use the objects (to compare). Read on first use, not at static init:
// Android sets env.txt's variables after the library's static initializers have run.
bool TasksOn()
{
  static const bool on = [] {
    const char* env = std::getenv("SKIES_INTERP_TASKS");
    return !env || env[0] != '0';
  }();
  return on;
}

// The game's addresses per region (USA; Europe and Japan from tools/dolmatch.py's map, exact matches):
// the running task, TaskRunAll (0x244 bytes) and TaskRunDeferred (0x164 bytes)
struct TaskAddresses
{
  u32 running, run_all, run_deferred;
};
const TaskAddresses& Tasks()
{
  static constexpr TaskAddresses USA{0x80311A7C, 0x8022642C, 0x80226984};
  static constexpr TaskAddresses EUR{0x803150FC, 0x802290DC, 0x80229634};
  static constexpr TaskAddresses JAP{0x803114FC, 0x802261A4, 0x802266FC};
  const u8 code = DiscHeader()[3];  // the game id's region letter
  return code == 'P' ? EUR : code == 'J' ? JAP : USA;
}

// [Skies] The deferred task being run (decomp docs/draw.md, TaskRunDeferred). The deferred pass doesn't update the
// running task, but its loop keeps r30 = &gTaskDeferredList[i] while it calls list entry i's function (r29 = the task).
// A callee that needs r30 saves it in its own frame, so the value is in r30 now or in a saved word between the stack
// pointer and TaskRunDeferred's frame. The list's address and count are read from TaskRunDeferred's own code (the same
// instructions on every disc). The pyramid's flames and glows are deferred tasks (issue #18): before this they had no
// object, and depth sorting swapped them.
struct DeferredList
{
  u32 list = 0, count = 0;  // 0: not found
};
const DeferredList& Deferred()
{
  static const DeferredList found = [] {
    DeferredList d;
    const u32 at = Tasks().run_deferred;
    const u32 lis = Read32(at + 0x48), addi = Read32(at + 0x50), lwz = Read32(at + 0x114);
    // lis r3,hi / addi r30,r3,lo / lwz r0,count(r13)
    if ((lis & 0xFFFF0000u) != 0x3C600000u || (addi & 0xFFFF0000u) != 0x3BC30000u || (lwz & 0xFFFF0000u) != 0x800D0000u)
    {
      Log("[interp] TaskRunDeferred not as expected (%08x %08x %08x): deferred tasks get no object\n", lis, addi, lwz);
      return d;
    }
    d.list = (lis << 16) + static_cast<u32>(static_cast<s32>(static_cast<s16>(addi & 0xFFFFu)));
    d.count = g_cpu.gpr[13] + static_cast<u32>(static_cast<s32>(static_cast<s16>(lwz & 0xFFFFu)));
    return d;
  }();
  return found;
}
// Whether the code the task function was in when it called down (its callee's saved return address, at its frame + 4)
// is the task's own function (+0x00, the class): a stale copy of another entry is another class's, or same-class
// entries save r30 in the same slot, where the current one has overwritten it.
bool InTaskFunction(u32 task, u32 task_frame)
{
  if (task < 0x80000000u || task >= 0x81800000u)
    return false;
  if (task_frame == g_cpu.gpr[1])  // the task function is writing itself: nothing saved below it
    return true;
  const u32 function = Read32(task), at = Read32(task_frame + 4);
  return at >= function && at - function < 0x4000u;
}
u32 DeferredTask(u32 sp, u32 task_frame, u32 deferred_frame)
{
  const DeferredList& d = Deferred();
  if (!d.list)
    return 0;
  const s32 count = static_cast<s32>(Read32(d.count));
  if (count <= 0 || count > 0x400)
    return 0;
  const u32 end = d.list + static_cast<u32>(count) * 4u;
  const auto in_list = [&](u32 v) { return v >= d.list && v < end && (v & 3u) == 0; };
  if (in_list(g_cpu.gpr[30]))
    return Read32(g_cpu.gpr[30]);
  // The slot found last time, while it still holds the entry (called once per FIFO burst: ~3,300 times a frame in the
  // pyramid, a full scan each time cost the game thread a millisecond or two)
  static u32 cached_frame = 0, cached_slot = 0, cached_entry = 0;
  if (task_frame == cached_frame && cached_slot >= sp && cached_slot < deferred_frame &&
      Read32(cached_slot) == cached_entry && in_list(cached_entry) && InTaskFunction(Read32(cached_entry), task_frame))
    return Read32(cached_entry);
  // Saved words, outermost first (the task function saves r30 at the top of its frame); the first whose task is the
  // class the code is in, else the outermost
  u32 outermost = 0;
  for (u32 a = deferred_frame - 4; a >= sp && a + 0x4000u >= deferred_frame; a -= 4)
  {
    const u32 v = Read32(a);
    if (!in_list(v))
      continue;
    if (InTaskFunction(Read32(v), task_frame))
    {
      cached_frame = task_frame;
      cached_slot = a;
      cached_entry = v;
      return Read32(v);
    }
    if (!outermost)
      outermost = v;
  }
  return outermost ? Read32(outermost) : 0;
}

// The running task, if the guest is inside TaskRunAll's call of it now: a saved return address on the
// guest stack (each frame's back chain word, the caller's return address at the frame above +4) inside
// TaskRunAll and none inside TaskRunDeferred first; inside TaskRunDeferred's call, the deferred task (DeferredTask).
u32 DrawTask(bool* deferred = nullptr)
{
  if (deferred)
    *deferred = false;
  const TaskAddresses& t = Tasks();
  u32 sp = g_cpu.gpr[1];
  for (int i = 0; i < 32; ++i)
  {
    if (sp < 0x80000000u || sp >= 0x81800000u)
      return 0;
    const u32 next = Read32(sp);
    if (next < 0x80000000u || next >= 0x81800000u || next <= sp)
      return 0;
    const u32 ret = Read32(next + 4);
    if (ret >= t.run_all && ret < t.run_all + 0x244)
      return Read32(t.running);
    // Its call of a task (bctrl at +0x108); its own calls of DrawSubmit replay recorded blocks, which carry their objects
    if (ret == t.run_deferred + 0x10C)
    {
      if (deferred)
        *deferred = true;
      return DeferredTask(g_cpu.gpr[1], sp, next);
    }
    if (ret >= t.run_deferred && ret < t.run_deferred + 0x164)
      return 0;
    sp = next;
  }
  return 0;
}
// The game's thread
std::vector<size_t> s_unkeyed;  // loads in s_current still waiting for the draw that uses them
bool s_on_game = false;         // recording
bool s_recording = false;       // this frame's stream is complete so far (not over the cap)
// The video thread
bool s_on = false;  // replaying
u64 s_frames = 0, s_interpolated = 0;
// Whether the next frames are worth recording (the budget's, on the video thread, for the game's)
std::atomic<bool> s_record_wanted{true};

// A frame's recording beyond this (no XFB copy ending it: a long load) is dropped.
constexpr size_t MAX_RECORDED_BYTES = 64u << 20;

auto LoadTuple(const Load& load)
{
  return std::make_tuple(load.address, load.count, load.key, load.occurrence);
}

void Sort(Frame& frame)
{
  if (frame.sorted)
    return;
  frame.load_order.resize(frame.loads.size());
  for (u32 i = 0; i < frame.load_order.size(); ++i)
    frame.load_order[i] = i;
  std::sort(frame.load_order.begin(), frame.load_order.end(), [&](u32 a, u32 b) {
    return LoadTuple(frame.loads[a]) < LoadTuple(frame.loads[b]);
  });
  frame.prim_order.resize(frame.prims.size());
  for (u32 i = 0; i < frame.prim_order.size(); ++i)
    frame.prim_order[i] = i;
  std::sort(frame.prim_order.begin(), frame.prim_order.end(), [&](u32 a, u32 b) {
    const u64 ka = frame.prims[a].key, kb = frame.prims[b].key;
    return ka != kb ? ka < kb : a < b;
  });
  frame.sorted = true;
}

bool IsMatrixOrProjection(u32 address)
{
  return address < XFMEM_POSMATRICES_END ||
         (address >= XFMEM_NORMALMATRICES && address < XFMEM_NORMALMATRICES_END) ||
         (address >= XFMEM_POSTMATRICES && address < XFMEM_POSTMATRICES_END) ||
         (address >= XFMEM_SETPROJECTION && address < XFMEM_SETPROJECTION + 6);
}

// SKIES_INTERP_COLOURS: 0 off, otherwise a mask (default 28): 1 lights, 2 the chans' colour registers (both off: blended they made the Red Alarm scene and the first-person reef worse), 4 direct vertex
// colours, 8 indexed colours and texture coordinates, 16 direct texture coordinates
int ColourMask()
{
  static const int mask = [] {
    const char* env = std::getenv("SKIES_INTERP_COLOURS");
    return env ? std::atoi(env) : 28;
  }();
  return mask;
}

bool ColoursOn()
{
  return ColourMask() != 0;
}

// How a word of an XF load is blended: floats (matrices, a light's attenuation, position and direction), a packed RGBA
// colour (a light's, the chans' ambient and material), or not at all (the lights' unused words, the chans' other
// settings: the current frame's stay)
enum class Word : u8 { Float, Colour, Keep };

Word WordKind(u32 address)
{
  if (address >= XFMEM_LIGHTS && address < XFMEM_LIGHTS_END)
  {
    const u32 w = address & 15;
    return w < 3 ? Word::Keep : w == 3 ? Word::Colour : Word::Float;
  }
  if (address >= XFMEM_AMBIENT0 && address <= XFMEM_MATERIAL1)
    return Word::Colour;
  if (address >= 0x1000 && address < 0x1020)
    return Word::Keep;
  return Word::Float;
}

// A load of lights or colours (not counted with the matrices when frames are told apart)
bool IsColourLoad(u32 address, u32 count)
{
  return (address >= XFMEM_LIGHTS && address < XFMEM_LIGHTS_END) ||
         (address >= 0x1000 && address <= XFMEM_MATERIAL1 && address + count > XFMEM_AMBIENT0);
}

float ColourDistance(const u8* c, const u8* p)
{
  float sum = 0.0f;
  for (int k = 0; k < 4; ++k)
    sum += std::fabs(static_cast<float>(c[k]) - static_cast<float>(p[k]));
  return sum * (1.0f / 255.0f);
}

// The distance between two loads' word (the alignment's cost)
float WordDistance(u32 address, const u8* c, const u8* p)
{
  switch (WordKind(address))
  {
  case Word::Colour:
    return ColourDistance(c, p);
  case Word::Keep:
    return 0.0f;
  default:
    return std::fabs(std::bit_cast<float>(BE32(c)) - std::bit_cast<float>(BE32(p)));
  }
}

float ReadFloat(const u8* p)
{
  return std::bit_cast<float>(BE32(p));
}

void WriteFloat(u8* p, float value)
{
  const u32 word = std::bit_cast<u32>(value);
  p[0] = static_cast<u8>(word >> 24);
  p[1] = static_cast<u8>(word >> 16);
  p[2] = static_cast<u8>(word >> 8);
  p[3] = static_cast<u8>(word);
}

// Whether two loads of the same matrix are close enough to blend: a large jump means a camera cut
// or a different object, which should snap rather than smear.
bool Blendable(const Load& load, const u8* cur, const u8* prev)
{
  for (u32 i = 0; i < load.count; ++i)
  {
    const u32 address = load.address + i;
    if (WordKind(address) != Word::Float)
      continue;
    const float c = ReadFloat(cur + i * 4), p = ReadFloat(prev + i * 4);
    if (!std::isfinite(c) || !std::isfinite(p))
      return false;
    const bool translation =
        (address < XFMEM_POSMATRICES_END ||
         (address >= XFMEM_POSTMATRICES && address < XFMEM_POSTMATRICES_END)) &&
        (address & 3) == 3;
    const float slack = translation ? 200.0f : 0.5f;
    if (std::fabs(c - p) > 0.5f * std::max(std::fabs(c), std::fabs(p)) + slack)
      return false;
  }
  return true;
}

// A position matrix blended element by element shrinks while it turns: halfway through a 90 degree turn
// its axes are 0.71 as long, through a 180 degree one they vanish, so a character turning round in one
// frame collapsed to a sliver in the in-between picture (issue #35's "warping"; Vyse turning back on
// Tenkou Island's stairs). Each whole 3x4 matrix in the load: an axis (the 3x3 part's columns) shorter
// than 0.7 of the frames' mean length (about a 90 degree turn or more in one frame) snaps the matrix to
// the current frame's, as a cut; otherwise the axes get the mean length back. The translation column is
// left as blended. `out` holds the blended words.
void KeepMatrixScale(const Load& load, const u8* cur, const u8* prev, float t, u8* out)
{
  // Position matrices: 3 rows of 4 words from a multiple of 4. Normal matrices (lighting): 3 rows of 3 words
  // from 0x400 plus a multiple of 3, treated the same so a snapped model isn't lit by a shrunken normal matrix.
  u32 stride, size, end;
  if (load.address < XFMEM_POSMATRICES_END && (load.address & 3) == 0)
    stride = 4, size = 12, end = XFMEM_POSMATRICES_END;
  else if (load.address >= XFMEM_NORMALMATRICES && load.address < XFMEM_NORMALMATRICES_END &&
           (load.address - XFMEM_NORMALMATRICES) % 3 == 0)
    stride = 3, size = 9, end = XFMEM_NORMALMATRICES_END;
  else
    return;
  for (u32 m = 0; m + size <= load.count && load.address + m + size <= end; m += size)
  {
    float scale[3];
    bool snap = false;
    for (u32 col = 0; col < 3; ++col)
    {
      float lc = 0, lp = 0, lb = 0;
      for (u32 row = 0; row < 3; ++row)
      {
        const u32 w = (m + row * stride + col) * 4;
        const float c = ReadFloat(cur + w), p = ReadFloat(prev + w), b = ReadFloat(out + w);
        lc += c * c;
        lp += p * p;
        lb += b * b;
      }
      const float target = std::sqrt(lp) + (std::sqrt(lc) - std::sqrt(lp)) * t;
      const float blended = std::sqrt(lb);
      if (target <= 0.0f)
      {
        scale[col] = 1.0f;
        continue;
      }
      if (blended < 0.7f * target)
      {
        snap = true;
        ++s_stat_matrix_snaps;
        break;
      }
      scale[col] = target / blended;
    }
    for (u32 row = 0; row < 3; ++row)
    {
      for (u32 col = 0; col < 3; ++col)
      {
        const u32 w = (m + row * stride + col) * 4;
        WriteFloat(out + w, snap ? ReadFloat(cur + w) : ReadFloat(out + w) * scale[col]);
      }
      if (snap && stride == 4)
      {
        const u32 w = (m + row * 4 + 3) * 4;
        WriteFloat(out + w, ReadFloat(cur + w));
      }
    }
  }
}

// Whether a float vertex position jumps between frames: a coordinate moves more than half its value
// and more than 50 units, or a tenth of the vertex's distance from the camera if that is more (the
// positions are in view space: turning the camera moves a far-off vertex's coordinate a lot).
bool VertexJumps(const u8* cur, const u8* prev, u32 count)
{
  float c[3] = {}, p[3] = {};
  for (u32 k = 0; k < count; ++k)
  {
    c[k] = ReadFloat(cur + k * 4);
    p[k] = ReadFloat(prev + k * 4);
    if (!std::isfinite(c[k]) || !std::isfinite(p[k]))
      return true;
  }
  const float length = std::sqrt(std::max(c[0] * c[0] + c[1] * c[1] + c[2] * c[2],
                                          p[0] * p[0] + p[1] * p[1] + p[2] * p[2]));
  const float slack = std::max(50.0f, 0.1f * length);
  for (u32 k = 0; k < count; ++k)
  {
    if (std::fabs(c[k] - p[k]) > std::max(0.5f * std::max(std::fabs(c[k]), std::fabs(p[k])), slack))
      return true;
  }
  return false;
}

// Whether a draw's float positions blended halfway shrink: the RMS distance of its vertices from their
// mean, blended, under 0.7 of the frames' (blended) value. Characters the game skins on the CPU send
// positions, not matrices, so a model turning round in one frame was blended vertex by vertex into a
// sliver (issue #35's "warping"; Vyse turning back on Tenkou Island's stairs); such a draw snaps, as a
// jump. Draws too small to judge (a point, a line) don't count.
bool Collapses(const u8* cur_data, const u8* prev_data, const Prim& prim, float t)
{
  if (prim.num_vertices < 3)
    return false;
  double mc[3] = {}, mp[3] = {};
  for (u32 v = 0; v < prim.num_vertices; ++v)
  {
    const u32 at = v * prim.vertex_size + prim.pos_offset;
    for (u32 k = 0; k < prim.pos_count; ++k)
    {
      mc[k] += ReadFloat(cur_data + at + k * 4);
      mp[k] += ReadFloat(prev_data + at + k * 4);
    }
  }
  for (u32 k = 0; k < 3; ++k)
    mc[k] /= prim.num_vertices, mp[k] /= prim.num_vertices;
  double rc = 0, rp = 0, rb = 0;
  for (u32 v = 0; v < prim.num_vertices; ++v)
  {
    const u32 at = v * prim.vertex_size + prim.pos_offset;
    for (u32 k = 0; k < prim.pos_count; ++k)
    {
      const double c = ReadFloat(cur_data + at + k * 4) - mc[k];
      const double p = ReadFloat(prev_data + at + k * 4) - mp[k];
      const double b = p + (c - p) * t;
      rc += c * c, rp += p * p, rb += b * b;
    }
  }
  rc = std::sqrt(rc), rp = std::sqrt(rp), rb = std::sqrt(rb);
  const double target = rp + (rc - rp) * t;
  if (!(target > 1e-6) || !std::isfinite(rb))
    return false;
  return rb < 0.7 * target;
}

// Pairs two lists of alike items, n_cur in this frame and n_prev in the previous one, allowing for
// items that appear or go (one of many cloud strips culled): the lists are aligned by dynamic
// programming in a band around the diagonal by distance(i, j); pair(i, j) is called for each pair.
// Returns how many items were left without a partner. The Dolphin build's Align. [Skies] gap_floor(is_cur, index), if
// given, is the least a gap costs for that item (issue #24, below).
struct NoGapFloor
{
  float operator()(bool, int) const { return 0.0f; }
};
template <typename Distance, typename PairFn, typename GapFloor = NoGapFloor>
int Align(int n_cur, int n_prev, const Distance& distance, const PairFn& pair, const GapFloor& gap_floor = GapFloor{})
{
  if (n_cur == 1 && n_prev == 1)
  {
    pair(0, 0);
    return 0;
  }
  const int band = std::abs(n_cur - n_prev) + 3;
  const int width = 2 * band + 1;
  // A kind whose count changes by hundreds (particles) would make the table huge: pair those in
  // order.
  if (static_cast<s64>(n_cur + 1) * width > 200000)
  {
    for (int i = 0; i < std::min(n_cur, n_prev); ++i)
      pair(i, i);
    return std::abs(n_cur - n_prev);
  }
  static std::vector<float> cost_table, typical;
  static std::vector<u8> move_table;
  // A gap costs a few times the typical motion of an item, so unpaired items only win over a
  // pairing that is clearly wrong.
  typical.clear();
  for (int i = 0; i < n_cur; ++i)
  {
    float nearest = 1e30f;
    for (int j = std::max(0, i - 1); j <= std::min(n_prev - 1, i + 1); ++j)
      nearest = std::min(nearest, distance(i, j));
    typical.push_back(nearest);
  }
  std::nth_element(typical.begin(), typical.begin() + typical.size() / 2, typical.end());
  const float gap = std::max(4.0f * typical[typical.size() / 2], 1.0f);
  // cost_table[i][d]: best cost with i current and j = i + d - band previous items used.
  cost_table.assign(static_cast<size_t>(n_cur + 1) * width, 1e30f);
  move_table.assign(static_cast<size_t>(n_cur + 1) * width, 0);
  const auto at = [&](int i, int j) -> int {
    const int d = j - i + band;
    return (d < 0 || d >= width || j < 0 || j > n_prev) ? -1 : i * width + d;
  };
  cost_table[at(0, 0)] = 0.0f;
  for (int i = 0; i <= n_cur; ++i)
  {
    for (int j = std::max(0, i - band); j <= std::min(n_prev, i + band); ++j)
    {
      const int here = at(i, j);
      if (here < 0 || (i == 0 && j == 0))
        continue;
      float best = 1e30f;
      u8 move = 0;
      if (i > 0 && j > 0 && at(i - 1, j - 1) >= 0)
      {
        const float c = cost_table[at(i - 1, j - 1)] + distance(i - 1, j - 1);
        if (c < best)
          best = c, move = 1;  // pair
      }
      if (i > 0 && at(i - 1, j) >= 0)
      {
        const float c = cost_table[at(i - 1, j)] + std::max(gap, gap_floor(true, i - 1));
        if (c < best)
          best = c, move = 2;  // a new item
      }
      if (j > 0 && at(i, j - 1) >= 0)
      {
        const float c = cost_table[at(i, j - 1)] + std::max(gap, gap_floor(false, j - 1));
        if (c < best)
          best = c, move = 3;  // an item that went
      }
      cost_table[here] = best;
      move_table[here] = move;
    }
  }
  int i = n_cur, j = n_prev;
  int unpaired = 0;
  if (at(i, j) < 0)
    return n_cur;  // outside the band: leave this kind unblended
  // As many items as before: in order, unless the alignment is far better (motion faster than the
  // items' spacing made a neighbour's old position look closer: seams in spinning strips).
  if (n_cur == n_prev)
  {
    float in_order = 0.0f;
    for (int k = 0; k < n_cur; ++k)
      in_order += distance(k, k);
    if (!(cost_table[at(i, j)] < 0.5f * in_order))
    {
      for (int k = 0; k < n_cur; ++k)
        pair(k, k);
      return 0;
    }
  }
  while (i > 0 || j > 0)
  {
    const u8 move = move_table[at(i, j)];
    if (move == 1)
    {
      pair(i - 1, j - 1);
      --i, --j;
    }
    else if (move == 2)
    {
      --i, ++unpaired;
    }
    else if (move == 3)
    {
      --j, ++unpaired;
    }
    else
    {
      break;
    }
  }
  return unpaired;
}

// The halfway matrices and positions written into a copy of the current stream. False when too few
// loads and draws have a partner in the previous frame to be worth it (a scene change). The Dolphin
// build's BuildBlendedStream, less its trace lines.
float DefaultT()
{
  static const float t = [] {
    const char* env = std::getenv("SKIES_INTERP_T");
    return env ? static_cast<float>(std::atof(env)) : 0.5f;
  }();
  return t;
}

// The pairing of two frames, which doesn't depend on the blend's position: FrameMode 2 blends a pair at
// several positions (one per refresh) and pairs them once. BuildPlan is the Dolphin build's
// BuildBlendedStream up to the blend; ApplyPlan the blend at t.
struct BlendPlan
{
  u64 cur_serial = 0, prev_serial = 0;  // the frames (pooled recordings are used again: by serial)
  std::vector<std::pair<const Load*, const Load*>> loads;  // Blendable pairs
  struct PrimPair
  {
    u32 index;          // in cur_frame.prims
    const Prim* old;    // its partner
    bool jumps;         // VertexJumps (float positions only)
    bool check_collapse;  // float positions, same layout, no jump: Collapses decides at t
  };
  std::vector<PrimPair> prims;          // partners with the same layout
  std::vector<u64> jumping_textures;    // textures with a paired draw that jumps (sorted)
  // [Skies] Each draw that jumped or went unpaired after a release: texture, object and first vertex (local snapping)
  struct Trigger
  {
    u64 texture;
    u32 task;
    std::array<float, 3> at;
  };
  std::vector<Trigger> triggers;
  // [Skies] Draws of an object with no usable partner (a map piece the game clipped to another vertex count: issue
  // #24's pier), moved by the rigid motion fitted to that object's paired draws: prev = r (cur - from) + to
  struct Moved
  {
    u32 index;
    std::array<float, 9> r;
    std::array<float, 3> from, to;
    float limit = 0.0f;  // the miss a draw that follows the motion stays within
  };
  std::vector<Moved> moved;
  // SKIES_INTERP_STRICT (a test): the loads and draws that had no partner (new in the current frame), hidden
  std::vector<u32> new_loads, new_prims;
  // [Skies] New position and normal matrices given a previous frame's value by the camera's motion: the load (index in
  // cur_frame.loads) and its words as they'd have been
  struct Synth
  {
    u32 index;
    std::array<float, 12> words;
  };
  std::vector<Synth> synth;
  size_t total = 0;
};

BlendPlan::Trigger MakeTrigger(const Frame& frame, const Prim& prim)
{
  BlendPlan::Trigger trigger{prim.texture, prim.task, {0.0f, 0.0f, 0.0f}};
  if (prim.pos_format == FORMAT_FLOAT)
  {
    const u8* p = frame.stream.data() + prim.data_offset + prim.pos_offset;
    for (u32 k = 0; k < prim.pos_count; ++k)
      trigger.at[k] = ReadFloat(p + k * 4);
  }
  return trigger;
}

// [Skies] The largest eigenvector of a symmetric 4x4 matrix (Jacobi rotations)
std::array<double, 4> LargestEigenvector(std::array<std::array<double, 4>, 4> a)
{
  std::array<std::array<double, 4>, 4> v{};
  for (int i = 0; i < 4; ++i)
    v[i][i] = 1.0;
  for (int sweep = 0; sweep < 32; ++sweep)
  {
    double off = 0.0;
    for (int i = 0; i < 4; ++i)
      for (int j = i + 1; j < 4; ++j)
        off += a[i][j] * a[i][j];
    if (off < 1e-24)
      break;
    for (int p = 0; p < 4; ++p)
    {
      for (int q = p + 1; q < 4; ++q)
      {
        if (std::fabs(a[p][q]) < 1e-30)
          continue;
        const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
        const double tt = (theta >= 0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
        const double c = 1.0 / std::sqrt(tt * tt + 1.0), sn = tt * c;
        for (int k = 0; k < 4; ++k)
        {
          const double akp = a[k][p], akq = a[k][q];
          a[k][p] = c * akp - sn * akq;
          a[k][q] = sn * akp + c * akq;
        }
        for (int k = 0; k < 4; ++k)
        {
          const double apk = a[p][k], aqk = a[q][k];
          a[p][k] = c * apk - sn * aqk;
          a[q][k] = sn * apk + c * aqk;
        }
        for (int k = 0; k < 4; ++k)
        {
          const double vkp = v[k][p], vkq = v[k][q];
          v[k][p] = c * vkp - sn * vkq;
          v[k][q] = sn * vkp + c * vkq;
        }
      }
    }
  }
  int best = 0;
  for (int i = 1; i < 4; ++i)
    if (a[i][i] > a[best][best])
      best = i;
  return {v[0][best], v[1][best], v[2][best], v[3][best]};
}

// [Skies] The rotation and translation that best take the points cur to prev (Horn's quaternion method, over the
// points with keep set), as BlendPlan::Moved's r / from / to; the mean distance left over in *residual
bool FitRigid(const std::vector<std::array<float, 3>>& cur, const std::vector<std::array<float, 3>>& prev,
              const std::vector<u8>& keep, BlendPlan::Moved* fit)
{
  double cc[3] = {}, cp[3] = {};
  size_t n = 0;
  for (size_t i = 0; i < cur.size(); ++i)
  {
    if (!keep[i])
      continue;
    for (int k = 0; k < 3; ++k)
      cc[k] += cur[i][k], cp[k] += prev[i][k];
    ++n;
  }
  if (n < 8)
    return false;
  for (int k = 0; k < 3; ++k)
    cc[k] /= n, cp[k] /= n;
  double m[3][3] = {};
  for (size_t i = 0; i < cur.size(); ++i)
  {
    if (!keep[i])
      continue;
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        m[r][c] += (cur[i][r] - cc[r]) * (prev[i][c] - cp[c]);
  }
  const double sxx = m[0][0], sxy = m[0][1], sxz = m[0][2], syx = m[1][0], syy = m[1][1], syz = m[1][2],
               szx = m[2][0], szy = m[2][1], szz = m[2][2];
  const std::array<std::array<double, 4>, 4> nm = {{{sxx + syy + szz, syz - szy, szx - sxz, sxy - syx},
                                                     {syz - szy, sxx - syy - szz, sxy + syx, szx + sxz},
                                                     {szx - sxz, sxy + syx, -sxx + syy - szz, syz + szy},
                                                     {sxy - syx, szx + sxz, syz + szy, -sxx - syy + szz}}};
  const std::array<double, 4> q = LargestEigenvector(nm);
  const double w = q[0], x = q[1], y = q[2], z = q[3];
  const double r[9] = {w * w + x * x - y * y - z * z, 2 * (x * y - w * z),         2 * (x * z + w * y),
                       2 * (x * y + w * z),         w * w - x * x + y * y - z * z, 2 * (y * z - w * x),
                       2 * (x * z - w * y),         2 * (y * z + w * x),         w * w - x * x - y * y + z * z};
  for (int k = 0; k < 9; ++k)
    fit->r[k] = static_cast<float>(r[k]);
  for (int k = 0; k < 3; ++k)
    fit->from[k] = static_cast<float>(cc[k]), fit->to[k] = static_cast<float>(cp[k]);
  return true;
}

std::array<float, 3> MoveBack(const BlendPlan::Moved& fit, const float* c)
{
  const float d[3] = {c[0] - fit.from[0], c[1] - fit.from[1], c[2] - fit.from[2]};
  return {fit.r[0] * d[0] + fit.r[1] * d[1] + fit.r[2] * d[2] + fit.to[0],
          fit.r[3] * d[0] + fit.r[4] * d[1] + fit.r[5] * d[2] + fit.to[1],
          fit.r[6] * d[0] + fit.r[7] * d[1] + fit.r[8] * d[2] + fit.to[2]};
}

// SKIES_INTERP_STRICT=1 (a test, with SKIES_INTERP_T=0): what had no partner in the previous frame (new) isn't drawn,
// so the in-between should equal the previous frame; every pixel that differs is a pairing or position to look at
// (or a change that isn't a position: colours, texture animation)
// [Skies] A position matrix (12 words) or normal matrix (9) that is the identity: draws already in view space (Maramba's
// rocks, the map) load it before their own positions, and the draws after them use it too
bool IdentityMatrix(const u8* words, u32 count)
{
  const u32 columns = count == 12 ? 4 : 3;
  for (u32 w = 0; w < count; ++w)
  {
    const float v = ReadFloat(words + w * 4);
    const float want = (w % columns) == (w / columns) ? 1.0f : 0.0f;
    if (std::fabs(v - want) > 1e-5f)
      return false;
  }
  return true;
}

bool StrictTest()
{
  static const bool on = std::getenv("SKIES_INTERP_STRICT") != nullptr;
  return on;
}

void BuildPlan(const Frame& cur_frame, const Frame& prev_frame, BlendPlan* plan)
{
  static std::vector<u8> load_had_partner;
  if (StrictTest())
    load_had_partner.assign(cur_frame.loads.size(), 0);
  plan->loads.clear();
  plan->prims.clear();
  plan->jumping_textures.clear();
  plan->triggers.clear();
  plan->moved.clear();
  // [Skies] Two objects' matrices are never blended (issue #24): an alike object leaving the picture as another came in
  // could pair the newcomer's matrix with the other's (a single item of a kind always was), sweeping it across the
  // screen for an in-between
  const auto loads_apart = [](const Load& a, const Load& b) {
    return a.task != 0 && b.task != 0 && (a.task != b.task || a.func != b.func);
  };
  // Which loads were aligned with one (whether blended or not), for the pass by object below
  static std::vector<u8> cur_load_aligned, prev_load_aligned;
  cur_load_aligned.assign(cur_frame.loads.size(), 0);
  prev_load_aligned.assign(prev_frame.loads.size(), 0);
  bool by_object_pass = false;  // the second alignment, by object alone
  const auto blend_load = [&](const Load& load, const Load& match) {
    cur_load_aligned[static_cast<size_t>(&load - cur_frame.loads.data())] = 1;
    prev_load_aligned[static_cast<size_t>(&match - prev_frame.loads.data())] = 1;
    if (StrictTest())
      load_had_partner[static_cast<size_t>(&load - cur_frame.loads.data())] = 1;
    if (loads_apart(load, match))
    {
      ++s_stat_loads_apart;
      return;
    }
    const u8* cur = cur_frame.stream.data() + load.data_offset;
    const u8* prev = prev_frame.stream.data() + match.data_offset;
    if (!Blendable(load, cur, prev))
      return;
    // Colour and light loads are paired through their draws, below
    if (IsColourLoad(load.address, load.count))
      return;
    ++s_stat_loads_blended;
    s_stat_words_changed += std::memcmp(cur, prev, load.count * 4u) != 0 ? 1 : 0;
    plan->loads.emplace_back(&load, &match);
  };
  // Loads of a kind (address, count and the next draw's key) are aligned like draws: when the game
  // culls one of many alike objects, pairing them by occurrence put every later object's matrix
  // halfway to its neighbour's.
  {
    static std::vector<const Load*> cur_loads, prev_loads;
    const auto kind = [](const Load& l) { return std::make_tuple(l.address, l.count, l.key); };
    const std::vector<u32>& cur_lo = cur_frame.load_order;
    const std::vector<u32>& prev_lo = prev_frame.load_order;
    size_t la = 0, lb = 0;
    while (la < cur_lo.size() && lb < prev_lo.size())
    {
      const auto k = kind(cur_frame.loads[cur_lo[la]]);
      const auto pk = kind(prev_frame.loads[prev_lo[lb]]);
      if (k != pk)
      {
        if (k < pk)
          ++la;
        else
          ++lb;
        continue;
      }
      cur_loads.clear();
      prev_loads.clear();
      for (; la < cur_lo.size() && kind(cur_frame.loads[cur_lo[la]]) == k; ++la)
        cur_loads.push_back(&cur_frame.loads[cur_lo[la]]);
      for (; lb < prev_lo.size() && kind(prev_frame.loads[prev_lo[lb]]) == k; ++lb)
        prev_loads.push_back(&prev_frame.loads[prev_lo[lb]]);
      // The projection type word isn't a float: compare the six parameters only.
      const u32 address = std::get<0>(k);
      u32 words = cur_loads[0]->count;
      if (address <= XFMEM_SETPROJECTION + 6 && address + words > XFMEM_SETPROJECTION + 6)
        words = XFMEM_SETPROJECTION + 6 - address;
      const auto distance = [&](int i, int j) {
        if (loads_apart(*cur_loads[i], *prev_loads[j]))
          return 1e30f;
        const u8* c = cur_frame.stream.data() + cur_loads[i]->data_offset;
        const u8* p = prev_frame.stream.data() + prev_loads[j]->data_offset;
        float sum = 0.0f;
        for (u32 w = 0; w < words; ++w)
          sum += WordDistance(address + w, c + w * 4, p + w * 4);
        return std::isfinite(sum) ? sum : 1e30f;
      };
      // [Skies] As for draws below (issue #24): leaving a position matrix unpaired costs at least a tenth of its
      // distance from the camera. A kind whose items are mostly alike identity matrices (the map, already in view
      // space) had a typical motion of 0, so one real object's matrix among them, moving with the camera, was left
      // unpaired and drawn a frame ahead in some frames and not in others (Maramba: 75 of 90 frames, now 0).
      static const bool load_gap_floor_on = [] {
        const char* env = std::getenv("SKIES_INTERP_GAP_FLOOR");
        return !env || env[0] != '0';
      }();
      const bool has_translation = load_gap_floor_on && address < 0x78 && address % 4 == 0 && words >= 12;
      const auto load_gap_floor = [&](bool is_cur, int index) {
        if (!has_translation)
          return 0.0f;
        const u8* d = (is_cur ? cur_frame.stream.data() + cur_loads[index]->data_offset
                              : prev_frame.stream.data() + prev_loads[index]->data_offset);
        const float x = ReadFloat(d + 12), y = ReadFloat(d + 28), z = ReadFloat(d + 44);
        const float length = std::sqrt(x * x + y * y + z * z);
        return std::isfinite(length) ? 0.1f * length : 0.0f;
      };
      // [Skies] Each object's loads are aligned on their own (their order in a frame is the object's, but objects of a
      // kind can come in another order), then the rest (no object known, or an object in one frame only) together.
      // A kind whose loads come from the same objects in the same order as before (most, most frames) is aligned
      // whole, as one: grouping every kind cost the Pocket 5 8% speed at the save point.
      bool same_objects = cur_loads.size() == prev_loads.size();
      for (size_t k = 0; same_objects && k < cur_loads.size(); ++k)
        same_objects = cur_loads[k]->task == prev_loads[k]->task && cur_loads[k]->func == prev_loads[k]->func;
      if (same_objects)
      {
        Align(static_cast<int>(cur_loads.size()), static_cast<int>(prev_loads.size()), distance,
              [&](int i, int j) { blend_load(*cur_loads[i], *prev_loads[j]); }, load_gap_floor);
        continue;
      }
      static std::vector<const Load*> all_cur, all_prev;
      static std::vector<std::pair<u64, u32>> cur_ids, prev_ids;
      all_cur = cur_loads;
      all_prev = prev_loads;
      cur_ids.clear();
      prev_ids.clear();
      const auto id_of = [](const Load* l) { return (u64{l->task} << 32) | l->func; };
      for (u32 i = 0; i < all_cur.size(); ++i)
        if (all_cur[i]->task)
          cur_ids.emplace_back(id_of(all_cur[i]), i);
      for (u32 j = 0; j < all_prev.size(); ++j)
        if (all_prev[j]->task)
          prev_ids.emplace_back(id_of(all_prev[j]), j);
      std::stable_sort(cur_ids.begin(), cur_ids.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
      std::stable_sort(prev_ids.begin(), prev_ids.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
      static std::vector<u8> cur_done, prev_done;
      cur_done.assign(all_cur.size(), 0);
      prev_done.assign(all_prev.size(), 0);
      const bool grouped = !cur_ids.empty() && !prev_ids.empty();
      for (size_t a = 0, b = 0; grouped && a < cur_ids.size() && b < prev_ids.size();)
      {
        if (cur_ids[a].first != prev_ids[b].first)
        {
          (cur_ids[a].first < prev_ids[b].first ? a : b)++;
          continue;
        }
        const u64 id = cur_ids[a].first;
        cur_loads.clear();
        prev_loads.clear();
        for (; a < cur_ids.size() && cur_ids[a].first == id; ++a)
          cur_loads.push_back(all_cur[cur_ids[a].second]), cur_done[cur_ids[a].second] = 1;
        for (; b < prev_ids.size() && prev_ids[b].first == id; ++b)
          prev_loads.push_back(all_prev[prev_ids[b].second]), prev_done[prev_ids[b].second] = 1;
        Align(static_cast<int>(cur_loads.size()), static_cast<int>(prev_loads.size()), distance,
              [&](int i, int j) { blend_load(*cur_loads[i], *prev_loads[j]); }, load_gap_floor);
      }
      cur_loads.clear();
      prev_loads.clear();
      for (u32 i = 0; i < all_cur.size(); ++i)
        if (!cur_done[i])
          cur_loads.push_back(all_cur[i]);
      for (u32 j = 0; j < all_prev.size(); ++j)
        if (!prev_done[j])
          prev_loads.push_back(all_prev[j]);
      if (!cur_loads.empty() && !prev_loads.empty())
        Align(static_cast<int>(cur_loads.size()), static_cast<int>(prev_loads.size()), distance,
              [&](int i, int j) { blend_load(*cur_loads[i], *prev_loads[j]); }, load_gap_floor);
    }
  }
  // [Skies] A load's kind takes in the next draw (its vertex format, count and texture) to tell objects apart, so an
  // object whose draw after a matrix changed between frames (Maramba's sky backdrop, issue #24) had no partner of its
  // kind and its matrices took the next frame's place: a frame ahead, every pixel of the sky. Loads of a known object
  // left without a partner are aligned again by object, matrix address and size alone; loads of no known object by
  // the next draw's shape without its texture, matrix address and size.
  {
    struct Left
    {
      u32 task, func;
      u64 shape;  // no object known: the next draw's key without its texture (a buffer the game draws into, used
                  // from alternating addresses: Maramba's shadows, every matrix before them unpaired when it swapped)
      u16 address;
      u8 count;
      u32 index;
      bool operator<(const Left& o) const
      {
        return std::tie(task, func, shape, address, count, index) <
               std::tie(o.task, o.func, o.shape, o.address, o.count, o.index);
      }
    };
    by_object_pass = true;
    static std::vector<Left> cur_left, prev_left;
    cur_left.clear();
    prev_left.clear();
    for (u32 i = 0; i < cur_frame.loads.size(); ++i)
    {
      const Load& l = cur_frame.loads[i];
      if (!cur_load_aligned[i])
        cur_left.push_back({l.task, l.func, l.task ? 0 : l.key ^ (u64{l.texture} << 20), l.address, l.count, i});
    }
    for (u32 j = 0; j < prev_frame.loads.size(); ++j)
    {
      const Load& l = prev_frame.loads[j];
      if (!prev_load_aligned[j])
        prev_left.push_back({l.task, l.func, l.task ? 0 : l.key ^ (u64{l.texture} << 20), l.address, l.count, j});
    }
    if (!cur_left.empty() && !prev_left.empty())
    {
      std::sort(cur_left.begin(), cur_left.end());
      std::sort(prev_left.begin(), prev_left.end());
      static std::vector<const Load*> cl, pl;
      const auto same = [](const Left& a, const Left& b) {
        return a.task == b.task && a.func == b.func && a.shape == b.shape && a.address == b.address && a.count == b.count;
      };
      for (size_t a = 0, b = 0; a < cur_left.size() && b < prev_left.size();)
      {
        if (!same(cur_left[a], prev_left[b]))
        {
          (cur_left[a] < prev_left[b] ? a : b)++;
          continue;
        }
        cl.clear();
        pl.clear();
        const Left head = cur_left[a];
        for (; a < cur_left.size() && same(cur_left[a], head); ++a)
          cl.push_back(&cur_frame.loads[cur_left[a].index]);
        for (; b < prev_left.size() && same(prev_left[b], head); ++b)
          pl.push_back(&prev_frame.loads[prev_left[b].index]);
        u32 words = head.count;
        if (head.address <= XFMEM_SETPROJECTION + 6 && head.address + words > XFMEM_SETPROJECTION + 6)
          words = XFMEM_SETPROJECTION + 6 - head.address;
        const auto distance = [&](int i, int j) {
          const u8* c = cur_frame.stream.data() + cl[i]->data_offset;
          const u8* p = prev_frame.stream.data() + pl[j]->data_offset;
          float sum = 0.0f;
          for (u32 w = 0; w < words; ++w)
            sum += WordDistance(head.address + w, c + w * 4, p + w * 4);
          return std::isfinite(sum) ? sum : 1e30f;
        };
        ++s_stat_loads_by_object;
        Align(static_cast<int>(cl.size()), static_cast<int>(pl.size()), distance,
              [&](int i, int j) { blend_load(*cl[i], *pl[j]); });
      }
    }
  }

  // Draws with the same key are aligned by the distance between their first vertices; a draw left
  // without a partner is drawn as it is. partner[i]: the previous frame's draw for prims[i].
  static std::vector<const Prim*> partner;
  partner.assign(cur_frame.prims.size(), nullptr);
  // Two different objects' draws (DrawTask) are never paired
  const auto apart = [](const Prim& cur, const Prim& prev) { return cur.task != 0 && prev.task != 0 && cur.task != prev.task; };
  for (const Prim& prim : cur_frame.prims)
    s_stat_tasked += prim.task != 0 ? 1 : 0;
  const auto set_partner = [&](const Prim* cur, const Prim* prev) {
    if (apart(*cur, *prev))
    {
      ++s_stat_kept_apart;
      return;
    }
    partner[static_cast<size_t>(cur - cur_frame.prims.data())] = prev;
  };
  // [Skies] An object whose draws in the two frames are alike one for one (same count, and each the same vertex layout,
  // the texture free to change) pairs them by their numbers (Prim::ordinal), before and instead of the alignment by
  // position below: the game draws an object's parts and an effect's quads in the same order every frame, but sorts
  // alike sprites by depth, which swaps them (issues #18, #24). An object whose draws differ is aligned as before.
  // SKIES_INTERP_ORDINAL=0: no pairing by number (to compare).
  static std::vector<u8> cur_identity, prev_identity;
  cur_identity.assign(cur_frame.prims.size(), 0);
  prev_identity.assign(prev_frame.prims.size(), 0);
  static const bool ordinal_on = [] {
    const char* env = std::getenv("SKIES_INTERP_ORDINAL");
    return !env || env[0] != '0';
  }();
  if (ordinal_on)
  {
    static std::vector<u32> cur_by, prev_by;
    const auto by_object = [](const Frame& frame, std::vector<u32>* out) {
      out->clear();
      for (u32 i = 0; i < frame.prims.size(); ++i)
      {
        if (frame.prims[i].task != 0)
          out->push_back(i);
      }
      std::sort(out->begin(), out->end(), [&](u32 x, u32 y) {
        const Prim& p = frame.prims[x];
        const Prim& q = frame.prims[y];
        return std::tie(p.task, p.ordinal, x) < std::tie(q.task, q.ordinal, y);
      });
    };
    by_object(cur_frame, &cur_by);
    by_object(prev_frame, &prev_by);
    const auto alike = [](const Prim& p, const Prim& q) {
      return p.func == q.func && p.ordinal == q.ordinal && p.num_vertices == q.num_vertices &&
             p.vertex_size == q.vertex_size && p.pos_offset == q.pos_offset && p.pos_format == q.pos_format &&
             p.pos_count == q.pos_count;
    };
    size_t i = 0, j = 0;
    while (i < cur_by.size() && j < prev_by.size())
    {
      const u32 task = cur_frame.prims[cur_by[i]].task, prev_task = prev_frame.prims[prev_by[j]].task;
      size_t i_end = i, j_end = j;
      while (i_end < cur_by.size() && cur_frame.prims[cur_by[i_end]].task == task)
        ++i_end;
      while (j_end < prev_by.size() && prev_frame.prims[prev_by[j_end]].task == prev_task)
        ++j_end;
      if (task != prev_task)
      {
        if (task < prev_task)
          i = i_end;
        else
          j = j_end;
        continue;
      }
      bool same = i_end - i == j_end - j;
      for (size_t k = 0; same && k < i_end - i; ++k)
        same = alike(cur_frame.prims[cur_by[i + k]], prev_frame.prims[prev_by[j + k]]);
      // A pair that jumps means the numbers moved (Maramba's floating rocks, one object: a rock left the view as
      // another came in, and every rock after it was paired with its neighbour): the object is aligned as before
      for (size_t k = 0; same && k < i_end - i; ++k)
      {
        const Prim& c = cur_frame.prims[cur_by[i + k]];
        const Prim& q = prev_frame.prims[prev_by[j + k]];
        same = c.pos_format != FORMAT_FLOAT ||
               !VertexJumps(cur_frame.stream.data() + c.data_offset + c.pos_offset,
                            prev_frame.stream.data() + q.data_offset + q.pos_offset, c.pos_count);
      }
      for (size_t k = 0; same && k < i_end - i; ++k)
      {
        const u32 c = cur_by[i + k], q = prev_by[j + k];
        partner[c] = &prev_frame.prims[q];
        cur_identity[c] = 1;
        prev_identity[q] = 1;
        ++s_stat_by_number;
      }
      i = i_end;
      j = j_end;
    }
  }
  static std::vector<const Prim*> cur_list, prev_list;
  static std::vector<std::array<float, 3>> cur_first, prev_first;
  const std::vector<u32>& cur_order = cur_frame.prim_order;
  const std::vector<u32>& prev_order = prev_frame.prim_order;
  size_t a = 0, b = 0;
  while (a < cur_order.size() && b < prev_order.size())
  {
    // The next key both frames draw.
    const u64 key = cur_frame.prims[cur_order[a]].key;
    const u64 prev_key = prev_frame.prims[prev_order[b]].key;
    if (key != prev_key)
    {
      if (key < prev_key)
        ++a;
      else
        ++b;
      continue;
    }
    cur_list.clear();
    prev_list.clear();
    for (; a < cur_order.size() && cur_frame.prims[cur_order[a]].key == key; ++a)
    {
      if (!cur_identity[cur_order[a]])
        cur_list.push_back(&cur_frame.prims[cur_order[a]]);
    }
    for (; b < prev_order.size() && prev_frame.prims[prev_order[b]].key == key; ++b)
    {
      if (!prev_identity[prev_order[b]])
        prev_list.push_back(&prev_frame.prims[prev_order[b]]);
    }
    if (cur_list.empty() || prev_list.empty())
      continue;
    const int n_cur = static_cast<int>(cur_list.size()), n_prev = static_cast<int>(prev_list.size());
    if (cur_list[0]->pos_format != FORMAT_FLOAT || (n_cur == 1 && n_prev == 1))
    {
      for (int i = 0; i < std::min(n_cur, n_prev); ++i)
        set_partner(cur_list[i], prev_list[i]);
      continue;
    }
    // [Skies] Each draw's mean vertex, decoded once (the Dolphin build: the first vertex). Two planks of Maramba's pier
    // (issue #24) shared a first vertex and were paired, the pair jumped and the pier's whole texture snapped a frame
    // ahead. SKIES_INTERP_CENTROID=0: the first vertex (to compare).
    const auto first_vertices = [](const Frame& frame, const std::vector<const Prim*>& list,
                                   std::vector<std::array<float, 3>>* firsts) {
      firsts->resize(list.size());
      for (size_t i = 0; i < list.size(); ++i)
      {
        const u8* p = frame.stream.data() + list[i]->data_offset + list[i]->pos_offset;
        std::array<float, 3>& f = (*firsts)[i];
        f = {0.0f, 0.0f, 0.0f};
        static const bool centroid = [] {
          const char* env = std::getenv("SKIES_INTERP_CENTROID");
          return !env || env[0] != '0';
        }();
        const u32 nv = centroid ? list[i]->num_vertices : 1u;
        for (u32 v = 0; v < nv; ++v)
          for (u32 k = 0; k < list[i]->pos_count; ++k)
            f[k] += ReadFloat(p + v * list[i]->vertex_size + k * 4);
        for (u32 k = 0; k < 3; ++k)
          f[k] /= static_cast<float>(nv);
      }
    };
    first_vertices(cur_frame, cur_list, &cur_first);
    first_vertices(prev_frame, prev_list, &prev_first);
    const auto distance = [&](int i, int j) {
      if (apart(*cur_list[i], *prev_list[j]))
        return 1e30f;
      const float sum = std::fabs(cur_first[i][0] - prev_first[j][0]) +
                        std::fabs(cur_first[i][1] - prev_first[j][1]) +
                        std::fabs(cur_first[i][2] - prev_first[j][2]);
      return std::isfinite(sum) ? sum : 1e30f;
    };
    // [Skies] A gap costs at least a tenth of the item's distance from the camera (VertexJumps' slack): the typical
    // motion is the median, and when most of a kind doesn't move it is 0 and any motion cost more than leaving the
    // item out. Maramba's sky haze (issue #24): the game sends its culled strips as zeros, 14 of 16 per pass, so the
    // one visible strip went unpaired, was drawn a frame ahead over its neighbour (a band) and left a gap.
    static const bool gap_floor_on = [] {
      const char* env = std::getenv("SKIES_INTERP_GAP_FLOOR");
      return !env || env[0] != '0';
    }();
    const auto gap_floor = [&](bool is_cur, int index) {
      if (!gap_floor_on)
        return 0.0f;
      const std::array<float, 3>& f = is_cur ? cur_first[index] : prev_first[index];
      return 0.1f * std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    };
    Align(n_cur, n_prev, distance, [&](int i, int j) { set_partner(cur_list[i], prev_list[j]); }, gap_floor);
  }
  // SKIES_INTERP_REPAIR=0 skips the two passes below (to compare).
  static const bool repair_on = [] {
    const char* env = std::getenv("SKIES_INTERP_REPAIR");
    return !env || env[0] != '0';
  }();
  const auto first_position = [](const Frame& frame, const Prim& prim) {
    return frame.stream.data() + prim.data_offset + prim.pos_offset;
  };
  // Pairs whose first vertices jump, let go to be paired again below (issues #18 and #24). The alignment
  // keeps draw order, but the game sorts see-through sprites by depth (the pyramid's flames, Pirate Isle's
  // rocks), so two alike sprites that change places between frames were paired each with the other one:
  // a jump, which snapped the pair and every draw with its texture to the new frame while the rest of the
  // picture moved halfway (a jump forward at every in-between, more often seen at the display's rate).
  // A released draw that finds no partner below snaps its texture's group as before.
  static std::vector<u8> cur_released, prev_released;
  cur_released.assign(cur_frame.prims.size(), 0);
  prev_released.assign(prev_frame.prims.size(), 0);
  for (size_t index = 0; index < cur_frame.prims.size() && repair_on; ++index)
  {
    const Prim& prim = cur_frame.prims[index];
    const Prim* old = partner[index];
    if (!old || prim.pos_format != FORMAT_FLOAT || old->pos_format != FORMAT_FLOAT || old->pos_count != prim.pos_count)
      continue;
    if (VertexJumps(first_position(cur_frame, prim), first_position(prev_frame, *old), prim.pos_count))
    {
      cur_released[index] = 1;
      prev_released[static_cast<size_t>(old - prev_frame.prims.data())] = 1;
      partner[index] = nullptr;
    }
  }
  // Draws still without a partner, paired again without the texture: a draw whose texture changed
  // between the frames (Fina's face changing expression, a flame's next picture) has a new key, and
  // drawn as it is while the rest of the model moved halfway, the model's edges showed around it
  // (issue #18). Per shape (vertex format, count and size), the closest pairs of draws left over in
  // the two frames are taken first, if their first vertices don't jump (VertexJumps), between
  // another texture or a released pair. Every candidate counts: an animated texture changes on all
  // its sprites at once (the pyramid has ~140 flame quads), where looking at the first 64 left half
  // of them unpaired, drawn a whole frame ahead.
  {
    static std::vector<u8> prev_taken;
    prev_taken.assign(prev_frame.prims.size(), 0);
    for (const Prim* p : partner)
    {
      if (p)
        prev_taken[static_cast<size_t>(p - prev_frame.prims.data())] = 1;
    }
    const auto shape = [](const Prim& prim) { return prim.key ^ (u64{prim.texture} << 20); };
    static std::vector<std::pair<u64, u32>> lone_prev, lone_cur;  // shape, index
    lone_prev.clear();
    lone_cur.clear();
    for (u32 j = 0; j < prev_frame.prims.size(); ++j)
    {
      const Prim& prim = prev_frame.prims[j];
      if (!prev_taken[j] && prim.pos_format == FORMAT_FLOAT && !prev_identity[j])
        lone_prev.emplace_back(shape(prim), j);
    }
    for (u32 i = 0; i < cur_frame.prims.size(); ++i)
    {
      const Prim& prim = cur_frame.prims[i];
      if (!partner[i] && prim.pos_format == FORMAT_FLOAT && !cur_identity[i])
        lone_cur.emplace_back(shape(prim), i);
    }
    std::sort(lone_prev.begin(), lone_prev.end());
    std::sort(lone_cur.begin(), lone_cur.end());
    // A scene change leaves everything unpaired: a shape with more candidate pairs than this is left so
    constexpr size_t MAX_PAIRS = 1u << 16;
    struct Candidate
    {
      float distance;
      u32 cur, prev;
      bool operator<(const Candidate& o) const
      {
        return std::tie(distance, cur, prev) < std::tie(o.distance, o.cur, o.prev);
      }
    };
    static std::vector<Candidate> candidates;
    // Objects that draw at most a few draws of a key this frame (a particle: one task each)
    static std::unordered_map<u64, u32> per_object;
    per_object.clear();
    for (const Prim& prim : cur_frame.prims)
    {
      if (prim.task != 0)
        ++per_object[(u64{prim.task} << 32) ^ prim.key];
    }
    const auto small_object = [&](const Prim& prim) {
      const auto it = per_object.find((u64{prim.task} << 32) ^ prim.key);
      return it != per_object.end() && it->second <= 4;
    };
    u32 repaired = 0;
    size_t a = 0, b = 0;
    while (a < lone_cur.size() && b < lone_prev.size() && repair_on)
    {
      const u64 key = lone_cur[a].first;
      if (key != lone_prev[b].first)
      {
        if (key < lone_prev[b].first)
          ++a;
        else
          ++b;
        continue;
      }
      const size_t a_end = std::upper_bound(lone_cur.begin() + a, lone_cur.end(), std::make_pair(key, ~0u)) - lone_cur.begin();
      const size_t b_end = std::upper_bound(lone_prev.begin() + b, lone_prev.end(), std::make_pair(key, ~0u)) - lone_prev.begin();
      if ((a_end - a) * (b_end - b) <= MAX_PAIRS)
      {
        candidates.clear();
        for (size_t i = a; i < a_end; ++i)
        {
          const u32 ci = lone_cur[i].second;
          const Prim& prim = cur_frame.prims[ci];
          const u8* cur = first_position(cur_frame, prim);
          for (size_t j = b; j < b_end; ++j)
          {
            const u32 pj = lone_prev[j].second;
            const Prim& old = prev_frame.prims[pj];
            // Same texture: the alignment above left it unpaired on purpose (a particle that went,
            // another that came), unless both were a released pair's
            // (or the same small object's: the alignment's band can miss a particle that moved far in the draw
            // order; not an object drawing many of these, such as the map, whose pieces are told apart by
            // nothing but their place)
            if (old.texture == prim.texture && !(cur_released[ci] && prev_released[pj]) &&
                !(prim.task != 0 && prim.task == old.task && small_object(prim)))
              continue;
            if (apart(prim, old))
              continue;
            const u8* prev = first_position(prev_frame, old);
            if (VertexJumps(cur, prev, prim.pos_count))
              continue;
            // [Skies] Every vertex, when the shapes match: a flame is two crossed quads whose first vertices are 3
            // apart, so by the first vertex alone one flame's quads swapped (and collapsed, issue #18).
            float distance = 0.0f;
            const u32 nv = old.num_vertices == prim.num_vertices && old.vertex_size == prim.vertex_size &&
                                   old.pos_offset == prim.pos_offset
                               ? prim.num_vertices
                               : 1u;
            for (u32 v = 0; v < nv; ++v)
              for (u32 k = 0; k < prim.pos_count; ++k)
                distance += std::fabs(ReadFloat(cur + v * prim.vertex_size + k * 4) -
                                      ReadFloat(prev + v * old.vertex_size + k * 4));
            distance /= static_cast<float>(nv);
            candidates.push_back(Candidate{distance, ci, pj});
          }
        }
        std::sort(candidates.begin(), candidates.end());
        for (const Candidate& c : candidates)
        {
          if (partner[c.cur] || prev_taken[c.prev])
            continue;
          partner[c.cur] = &prev_frame.prims[c.prev];
          prev_taken[c.prev] = 1;
          ++repaired;
        }
      }
      a = a_end;
      b = b_end;
    }
    s_stat_repaired += repaired;
  }
  // [Skies] An object's draws left without a partner of the same shape (issue #24: the game clips pier pieces of the
  // map to another vertex count as the camera turns, so they have no counterpart) were drawn a frame ahead, and a
  // released one snapped its texture's whole group with it. When the object's paired draws moved as one rigid
  // body (the map, transformed into view space on the CPU, moves only with the camera), the leftovers move back
  // by that motion instead. SKIES_INTERP_RIGID=0 turns it off.
  static const bool rigid_moves = [] {
    const char* env = std::getenv("SKIES_INTERP_RIGID");
    return !env || env[0] != '0';
  }();
  std::vector<u8> moved_prim(cur_frame.prims.size(), 0);
  const auto layout_matches = [](const Prim& prim, const Prim& old) {
    return old.num_vertices == prim.num_vertices && old.vertex_size == prim.vertex_size &&
           old.pos_offset == prim.pos_offset && old.pos_format == prim.pos_format && old.pos_count == prim.pos_count;
  };
  // The rigid motion of an object's paired same-shape float draws (cached per object), false when they didn't move
  // as one body (a character, particles) or are too few
  static std::unordered_map<u32, std::pair<bool, BlendPlan::Moved>> fits;
  fits.clear();
  // Each frame's float draws of an object: (task, index) sorted, a task's run found by binary search
  static std::vector<std::pair<u32, u32>> cur_tasks, prev_tasks;
  cur_tasks.clear();
  prev_tasks.clear();
  if (rigid_moves)
  {
    for (size_t index = 0; index < cur_frame.prims.size(); ++index)
      if (cur_frame.prims[index].task != 0 && cur_frame.prims[index].pos_format == FORMAT_FLOAT)
        cur_tasks.emplace_back(cur_frame.prims[index].task, static_cast<u32>(index));
    for (size_t index = 0; index < prev_frame.prims.size(); ++index)
      if (prev_frame.prims[index].task != 0 && prev_frame.prims[index].pos_format == FORMAT_FLOAT)
        prev_tasks.emplace_back(prev_frame.prims[index].task, static_cast<u32>(index));
    std::sort(cur_tasks.begin(), cur_tasks.end());
    std::sort(prev_tasks.begin(), prev_tasks.end());
  }
  struct Run
  {
    const std::pair<u32, u32>* first;
    const std::pair<u32, u32>* last;
    size_t size() const { return static_cast<size_t>(last - first); }
    const std::pair<u32, u32>* begin() const { return first; }
    const std::pair<u32, u32>* end() const { return last; }
  };
  const auto task_run = [](const std::vector<std::pair<u32, u32>>& list, u32 task) {
    const auto lo = std::lower_bound(list.begin(), list.end(), std::make_pair(task, 0u));
    const auto hi = std::lower_bound(lo, list.end(), std::make_pair(task + 1, 0u));
    return Run{list.data() + (lo - list.begin()), list.data() + (hi - list.begin())};
  };
  std::vector<std::array<float, 3>> pc, pp;
  std::vector<u8> keep;
  std::vector<float> residual, sorted;
  const auto fit_for = [&](u32 task, BlendPlan::Moved* out) {
    const auto found = fits.find(task);
    if (found != fits.end())
    {
      *out = found->second.second;
      return found->second.first;
    }
    pc.clear();
    pp.clear();
    size_t vertices = 0;
    const auto usable = [&](size_t index) {
      const Prim& prim = cur_frame.prims[index];
      return prim.task == task && partner[index] && prim.pos_format == FORMAT_FLOAT && prim.pos_count == 3 &&
             layout_matches(prim, *partner[index]);
    };
    const Run mine = task_run(cur_tasks, task);
    for (const auto& [unused, index] : mine)
      vertices += usable(index) ? cur_frame.prims[index].num_vertices : 0;
    // Every vertex, up to about 512 points (plenty for a rigid motion; the fit's cost goes with them)
    const size_t stride = std::max<size_t>(1, vertices / 512);
    size_t counter = 0;
    for (const auto& [unused, index] : mine)
    {
      if (!usable(index))
        continue;
      const Prim& prim = cur_frame.prims[index];
      const Prim& old = *partner[index];
      for (u32 v = 0; v < prim.num_vertices; ++v, ++counter)
      {
        if (counter % stride)
          continue;
        const u8* c = cur_frame.stream.data() + prim.data_offset + v * prim.vertex_size + prim.pos_offset;
        const u8* p = prev_frame.stream.data() + old.data_offset + v * old.vertex_size + old.pos_offset;
        pc.push_back({ReadFloat(c), ReadFloat(c + 4), ReadFloat(c + 8)});
        pp.push_back({ReadFloat(p), ReadFloat(p + 4), ReadFloat(p + 8)});
      }
    }
    BlendPlan::Moved fit{};
    bool good = pc.size() >= 24;
    if (good)
    {
      // Fit, drop the points left far off (pieces that moved on their own, a mispaired draw), fit again
      keep.assign(pc.size(), 1);
      good = FitRigid(pc, pp, keep, &fit);
      for (int round = 0; round < 3 && good; ++round)
      {
        residual.resize(pc.size());
        double spread = 0.0;
        for (size_t i = 0; i < pc.size(); ++i)
        {
          const std::array<float, 3> m = MoveBack(fit, pc[i].data());
          const float dx = m[0] - pp[i][0], dy = m[1] - pp[i][1], dz = m[2] - pp[i][2];
          residual[i] = std::sqrt(dx * dx + dy * dy + dz * dz);
          spread += std::sqrt(pc[i][0] * pc[i][0] + pc[i][1] * pc[i][1] + pc[i][2] * pc[i][2]);
        }
        spread /= static_cast<double>(pc.size());
        sorted = residual;
        std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
        const float median = sorted[sorted.size() / 2];
        // Within 1% of the points' distance, or three times the typical miss
        const float limit = std::max(static_cast<float>(0.01 * spread), 3.0f * median);
        size_t kept = 0;
        for (size_t i = 0; i < pc.size(); ++i)
          kept += (keep[i] = residual[i] <= limit) ? 1 : 0;
        // A rigid body: most points (70%) follow it closely
        // A rigid body: after two refits without the far points, most points (70%) follow it closely
        good = kept * 10 >= pc.size() * 7 && (round < 2 ? FitRigid(pc, pp, keep, &fit) : median <= 0.005 * spread);
        fit.limit = limit;
      }
    }
    fits.emplace(task, std::make_pair(good, fit));
    *out = fit;
    return good;
  };
  // [Skies] The camera's motion: the rigid motion of the largest object that moved as one body (the map, in view
  // space), which everything that sits still in the world shares (issue #24). Objects by their number of float
  // draws, most first; the camera is looked for once, when first needed (below).
  static std::vector<std::pair<u32, u32>> by_size;  // (count, task)
  by_size.clear();
  for (size_t a = 0; a < cur_tasks.size();)
  {
    size_t b = a;
    while (b < cur_tasks.size() && cur_tasks[b].first == cur_tasks[a].first)
      ++b;
    by_size.emplace_back(static_cast<u32>(b - a), cur_tasks[a].first);
    a = b;
  }
  std::sort(by_size.begin(), by_size.end(), std::greater<>());
  int camera_state = -1;  // not looked for yet, 0 none, 1 found
  u32 camera_task = 0;
  BlendPlan::Moved camera;
  const auto camera_fit = [&](BlendPlan::Moved* out) {
    if (camera_state < 0)
    {
      camera_state = 0;
      for (size_t c = 0; c < by_size.size() && c < 3 && camera_state == 0; ++c)
        if (by_size[c].first >= 16 && fit_for(by_size[c].second, &camera))
          camera_state = 1, camera_task = by_size[c].second;
    }
    *out = camera;
    return camera_state == 1;
  };
  static const bool camera_pair_on = [] {
    const char* env = std::getenv("SKIES_INTERP_CAMERA_PAIR");
    return !env || env[0] != '0';
  }();
  const auto move_with_object = [&](size_t index) {
    const Prim& prim = cur_frame.prims[index];
    if (!rigid_moves || prim.task == 0 || prim.pos_format != FORMAT_FLOAT || prim.pos_count != 3)
      return false;
    BlendPlan::Moved fit;
    if (!fit_for(prim.task, &fit))
      return false;
    fit.index = static_cast<u32>(index);
    plan->moved.push_back(fit);
    moved_prim[index] = 1;
    return true;
  };
  // [Skies] Alike pieces of one object told apart only by their place (Maramba's reef rocks, issue #24: camera-facing
  // quads in view space): in a fast camera move (into first person) each piece moved further than the gap to its
  // neighbour, so pairing by place took a neighbour's old place. Then either the object's motion didn't fit and its
  // jumps snapped every rock a frame ahead, or it fitted on most pieces and the rest swept across the picture. Each
  // object of 4 to 256 float draws is paired again by its own motion, or when that doesn't fit by the camera's (the
  // largest object that moved as one body: the map; things that sit still in the world move with it): a piece takes
  // the previous piece that motion brings it onto, clearly nearer than any other. When most pieces (70%) find one,
  // none claimed twice, those become the partners and the rest follow the object's motion (pieces coming into the
  // picture); a piece with no clear match keeps a partner that motion agrees with. Objects paired by number are left
  // as they are. SKIES_INTERP_CAMERA_PAIR=0 turns it off (to compare).
  if (rigid_moves && camera_pair_on && !cur_tasks.empty())
  {
    const auto centroid = [](const Frame& frame, const Prim& prim) {
      std::array<float, 3> m = {0.0f, 0.0f, 0.0f};
      for (u32 v = 0; v < prim.num_vertices; ++v)
      {
        const u8* d = frame.stream.data() + prim.data_offset + v * prim.vertex_size + prim.pos_offset;
        for (int k = 0; k < 3; ++k)
          m[k] += ReadFloat(d + k * 4);
      }
      for (int k = 0; k < 3; ++k)
        m[k] /= static_cast<float>(prim.num_vertices);
      return m;
    };
    static std::vector<std::array<float, 3>> prev_centre;
    static std::vector<u32> chosen;
    for (const auto& [count, task] : by_size)
    {
      if (count < 4 || count > 256)
        continue;
      const Run mine = task_run(cur_tasks, task);
      const Run theirs = task_run(prev_tasks, task);
      if (theirs.size() < 4)
        continue;
      // An object paired by number (the same draws as before, none jumping) is right as it is
      if (cur_identity[mine.first[0].second])
        continue;
      BlendPlan::Moved own;
      const bool own_good = fit_for(task, &own);
      BlendPlan::Moved cam;
      if (!own_good && (!camera_fit(&cam) || task == camera_task))
        continue;
      const BlendPlan::Moved& motion = own_good ? own : cam;
      prev_centre.clear();
      for (const auto& [unused, j] : theirs)
        prev_centre.push_back(centroid(prev_frame, prev_frame.prims[j]));
      chosen.assign(mine.size(), ~0u);
      size_t matched = 0, considered = 0;
      for (size_t i = 0; i < mine.size(); ++i)
      {
        const Prim& prim = cur_frame.prims[mine.first[i].second];
        if (prim.pos_format != FORMAT_FLOAT || prim.pos_count != 3)
          continue;
        ++considered;
        const std::array<float, 3> c = centroid(cur_frame, prim);
        const std::array<float, 3> back = MoveBack(motion, c.data());
        float best = 1e30f, second = 1e30f;
        u32 best_j = ~0u;
        for (size_t j = 0; j < theirs.size(); ++j)
        {
          const Prim& old = prev_frame.prims[theirs.first[j].second];
          if (old.key != prim.key || !layout_matches(prim, old))
            continue;
          const float dx = back[0] - prev_centre[j][0], dy = back[1] - prev_centre[j][1], dz = back[2] - prev_centre[j][2];
          const float miss = std::sqrt(dx * dx + dy * dy + dz * dz);
          if (miss < best)
            second = best, best = miss, best_j = static_cast<u32>(j);
          else if (miss < second)
            second = miss;
        }
        const float length = std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
        const float near = std::max(3.0f * motion.limit, 0.01f * length);
        if (best_j != ~0u && best <= near && 4.0f * best < second)
        {
          chosen[i] = best_j, ++matched;
          continue;
        }
        // No clear match: a partner that motion agrees with stays (alike pieces in one place)
        const Prim* had = partner[mine.first[i].second];
        if (had && had->task == task && had->key == prim.key && layout_matches(prim, *had))
        {
          const size_t j = static_cast<size_t>(had - prev_frame.prims.data());
          for (size_t k = 0; k < theirs.size(); ++k)
            if (theirs.first[k].second == j)
            {
              const float dx = back[0] - prev_centre[k][0], dy = back[1] - prev_centre[k][1],
                          dz = back[2] - prev_centre[k][2];
              if (std::sqrt(dx * dx + dy * dy + dz * dz) <= near)
                chosen[i] = static_cast<u32>(k), ++matched;
              break;
            }
        }
      }
      // Most pieces that can have a partner (70% of the fewer of the two frames': pieces come into the picture, Maramba's
      // second rock object going from 4 to 6) follow the motion, none claimed twice
      if (considered < 4 || matched * 10 < std::min(considered, theirs.size()) * 7)
        continue;
      static std::vector<u32> claimed;
      claimed = chosen;
      std::sort(claimed.begin(), claimed.end());
      const auto valid_end = std::lower_bound(claimed.begin(), claimed.end(), ~0u);
      if (std::adjacent_find(claimed.begin(), valid_end) != valid_end)
        continue;
      bool changed = false;
      for (size_t i = 0; i < mine.size(); ++i)
      {
        const u32 index = mine.first[i].second;
        const Prim& prim = cur_frame.prims[index];
        if (prim.pos_format != FORMAT_FLOAT || prim.pos_count != 3)
          continue;
        const Prim* now = chosen[i] != ~0u ? &prev_frame.prims[theirs.first[chosen[i]].second] : nullptr;
        if (now != partner[index])
          ++s_stat_camera_paired, changed = true;
        partner[index] = now;
      }
      if (changed)
        fits.erase(task);  // refitted from the new pairs
    }
  }
  for (size_t index = 0; index < cur_frame.prims.size(); ++index)
    if (!partner[index] || !layout_matches(cur_frame.prims[index], *partner[index]))
      move_with_object(index);
  // A released draw left without a partner jumped: its texture's group snaps, as when it was paired
  for (size_t index = 0; index < cur_frame.prims.size(); ++index)
  {
    if (cur_released[index] && !partner[index] && !moved_prim[index])
    {
      plan->jumping_textures.push_back(cur_frame.prims[index].texture);
      plan->triggers.push_back(MakeTrigger(cur_frame, cur_frame.prims[index]));
    }
  }
  // Which paired draws jump (VertexJumps; with the two added, a full-screen quad Skies moves from
  // z = 100 to 0.001 after a menu passed covered the scene halfway). When one draw jumps, every draw
  // with the same texture snaps rather than smears: deciding per draw tore objects made of many
  // draws apart (the battle-entry shatter's card of strips). Collapses, which depends on t, is ApplyPlan's.
  const auto same_layout = [](const Prim& prim, const Prim& old) {
    return old.num_vertices == prim.num_vertices && old.vertex_size == prim.vertex_size &&
           old.pos_offset == prim.pos_offset && old.pos_format == prim.pos_format &&
           old.pos_count == prim.pos_count;
  };
  for (size_t index = 0; index < cur_frame.prims.size(); ++index)
  {
    if (!partner[index])
      continue;
    const Prim& prim = cur_frame.prims[index];
    if (!same_layout(prim, *partner[index]))
      continue;
    // [Skies] A draw of a rigidly moving object that doesn't follow its motion was paired with another piece (the
    // map's alike pier planks, issue #24): the previous frame's piece of that object that the motion takes it to is
    // its partner. None (a limb moving on its own): the pairing stands.
    const Prim* better = nullptr;
    // (only an object of many draws: alike pieces told apart by nothing but their place; a fit costs)
    if (rigid_moves && prim.task != 0 && prim.pos_format == FORMAT_FLOAT && prim.pos_count == 3 &&
        task_run(cur_tasks, prim.task).size() >= 32)
    {
      BlendPlan::Moved fit;
      if (fit_for(prim.task, &fit))
      {
        const auto miss = [&](const Prim& candidate) {
          float sum = 0.0f;
          for (u32 v = 0; v < prim.num_vertices; ++v)
          {
            const u8* c = cur_frame.stream.data() + prim.data_offset + v * prim.vertex_size + prim.pos_offset;
            const u8* p = prev_frame.stream.data() + candidate.data_offset + v * candidate.vertex_size + candidate.pos_offset;
            const float cv[3] = {ReadFloat(c), ReadFloat(c + 4), ReadFloat(c + 8)};
            const std::array<float, 3> m = MoveBack(fit, cv);
            const float dx = m[0] - ReadFloat(p), dy = m[1] - ReadFloat(p + 4), dz = m[2] - ReadFloat(p + 8);
            sum += std::sqrt(dx * dx + dy * dy + dz * dz);
          }
          return sum;
        };
        const float allowed = fit.limit * static_cast<float>(prim.num_vertices);
        if (miss(*partner[index]) > allowed)
        {
          float best = allowed;
          for (const auto& [unused, j] : task_run(prev_tasks, prim.task))
          {
            const Prim& candidate = prev_frame.prims[j];
            if (candidate.key != prim.key || !same_layout(prim, candidate))
              continue;
            const float m = miss(candidate);
            if (m <= best)
              best = m, better = &candidate;
          }
          s_stat_rigid_repaired += better ? 1 : 0;
        }
      }
    }
    const Prim& old = better ? *better : *partner[index];
    bool jump = false;
    if (prim.pos_format == FORMAT_FLOAT)
    {
      for (u32 v = 0; v < prim.num_vertices && !jump; ++v)
      {
        const u32 at = v * prim.vertex_size + prim.pos_offset;
        jump = VertexJumps(cur_frame.stream.data() + prim.data_offset + at,
                           prev_frame.stream.data() + old.data_offset + at, prim.pos_count);
      }
      // [Skies] A jump its object's motion explains (or the camera's, for an object that didn't fit) isn't one: a fast
      // camera move (into first person, issue #24) moves view-space draws far in a frame, and moving them with the
      // fitted motion instead of their own partner put them a little off (outlines, rocks that shift)
      if (jump && camera_pair_on && rigid_moves && prim.task != 0 && prim.pos_count == 3)
      {
        BlendPlan::Moved motion;
        const bool own = fit_for(prim.task, &motion);
        if (own || (camera_fit(&motion) && prim.task != camera_task))
        {
          float miss = 0.0f, size = 0.0f;
          for (u32 v = 0; v < prim.num_vertices; ++v)
          {
            const u8* c = cur_frame.stream.data() + prim.data_offset + v * prim.vertex_size + prim.pos_offset;
            const u8* p = prev_frame.stream.data() + old.data_offset + v * old.vertex_size + old.pos_offset;
            const float cv[3] = {ReadFloat(c), ReadFloat(c + 4), ReadFloat(c + 8)};
            const std::array<float, 3> m = MoveBack(motion, cv);
            const float dx = m[0] - ReadFloat(p), dy = m[1] - ReadFloat(p + 4), dz = m[2] - ReadFloat(p + 8);
            miss = std::max(miss, std::sqrt(dx * dx + dy * dy + dz * dz));
            size = std::max(size, std::sqrt(cv[0] * cv[0] + cv[1] * cv[1] + cv[2] * cv[2]));
          }
          if (miss <= std::max(3.0f * motion.limit, 0.01f * size))
            jump = false, ++s_stat_jumps_explained;
        }
      }
      if (jump && !move_with_object(index))
      {
        plan->jumping_textures.push_back(prim.texture);
        plan->triggers.push_back(MakeTrigger(cur_frame, prim));
      }
    }
    plan->prims.push_back(BlendPlan::PrimPair{static_cast<u32>(index), &old, jump,
                                              prim.pos_format == FORMAT_FLOAT && !jump});
    s_stat_prims_changed += std::memcmp(cur_frame.stream.data() + prim.data_offset,
                                        prev_frame.stream.data() + old.data_offset,
                                        static_cast<size_t>(prim.num_vertices) * prim.vertex_size) != 0 ? 1 : 0;
  }
  std::sort(plan->jumping_textures.begin(), plan->jumping_textures.end());
  plan->jumping_textures.erase(std::unique(plan->jumping_textures.begin(), plan->jumping_textures.end()),
                               plan->jumping_textures.end());
  // [Skies] Colour and light loads (ambient, material, lights) go with the draw after them: each is paired with the load
  // of the same register before that draw's partner. Aligned by kind and order like matrices, a frame with one draw more
  // shifted them all (the Red Alarm room's pulsing ambient blended from another draw's: paler walls).
  {
    static std::vector<std::pair<u64, u32>> prev_colour;  // (partner prim << 32 | address << 8 | count, load index)
    prev_colour.clear();
    for (u32 j = 0; j < prev_frame.loads.size(); ++j)
    {
      const Load& l = prev_frame.loads[j];
      if (l.prim != ~0u && IsColourLoad(l.address, l.count))
        prev_colour.emplace_back((u64{l.prim} << 32) | (u64{l.address} << 8) | l.count, j);
    }
    std::sort(prev_colour.begin(), prev_colour.end());
    for (const Load& l : cur_frame.loads)
    {
      if (l.prim == ~0u || !IsColourLoad(l.address, l.count) || !partner[l.prim])
        continue;
      const u32 j = static_cast<u32>(partner[l.prim] - prev_frame.prims.data());
      const u64 want = (u64{j} << 32) | (u64{l.address} << 8) | l.count;
      const auto it = std::lower_bound(prev_colour.begin(), prev_colour.end(), std::make_pair(want, 0u));
      // the last such load before the partner (the one in effect), as the current load is the last before its draw
      auto last = prev_colour.end();
      for (auto k = it; k != prev_colour.end() && k->first == want; ++k)
        last = k;
      if (last != prev_colour.end())
        plan->loads.emplace_back(&l, &prev_frame.loads[last->second]);
    }
  }
  // [Skies] A matrix with no partner: an object drawn by matrix that came into the picture, or that the game drew
  // another way a frame before (Maramba's reef haze, issue #24: view-space draws in third person, 46 matrix-drawn ones
  // once the first-person view settles) took the next frame's place, a frame ahead of everything around it. A
  // position or normal matrix of a known object gets the previous value the camera's motion gives it (prev =
  // R (cur - from) + to: R M, and the translation R t + to - R from), as new view-space draws follow their object.
  plan->synth.clear();
  static const bool synth_on = [] {
    const char* env = std::getenv("SKIES_INTERP_NEW_MATRICES");
    return !env || env[0] != '0';
  }();
  if (synth_on && rigid_moves)
  {
    BlendPlan::Moved cam;
    bool have = false, tried = false;
    for (u32 i = 0; i < cur_frame.loads.size(); ++i)
    {
      if (cur_load_aligned[i])
        continue;
      const Load& load = cur_frame.loads[i];
      const bool position = load.address < 0x78 && load.address % 4 == 0 && load.count == 12;
      const bool normal = load.address >= XFMEM_NORMALMATRICES && load.address < XFMEM_NORMALMATRICES + 0x60 &&
                          (load.address - XFMEM_NORMALMATRICES) % 3 == 0 && load.count == 9;
      // An identity matrix isn't an object's place (its draws carry their own positions, paired on their own), and the
      // draws after it, paired, use it too: moved with the camera, they moved twice
      if (load.task == 0 || (!position && !normal) || IdentityMatrix(cur_frame.stream.data() + load.data_offset, load.count))
        continue;
      if (!tried)
        tried = true, have = camera_fit(&cam);
      if (!have)
        break;
      const u8* d = cur_frame.stream.data() + load.data_offset;
      BlendPlan::Synth out{i, {}};
      const float* r = cam.r.data();
      if (position)
      {
        float m[12];
        for (int w = 0; w < 12; ++w)
          m[w] = ReadFloat(d + w * 4);
        const float shift[3] = {cam.to[0] - (r[0] * cam.from[0] + r[1] * cam.from[1] + r[2] * cam.from[2]),
                                cam.to[1] - (r[3] * cam.from[0] + r[4] * cam.from[1] + r[5] * cam.from[2]),
                                cam.to[2] - (r[6] * cam.from[0] + r[7] * cam.from[1] + r[8] * cam.from[2])};
        for (int row = 0; row < 3; ++row)
          for (int col = 0; col < 4; ++col)
            out.words[row * 4 + col] = r[row * 3] * m[col] + r[row * 3 + 1] * m[4 + col] + r[row * 3 + 2] * m[8 + col] +
                                       (col == 3 ? shift[row] : 0.0f);
      }
      else
      {
        float m[9];
        for (int w = 0; w < 9; ++w)
          m[w] = ReadFloat(d + w * 4);
        for (int row = 0; row < 3; ++row)
          for (int col = 0; col < 3; ++col)
            out.words[row * 3 + col] = r[row * 3] * m[col] + r[row * 3 + 1] * m[3 + col] + r[row * 3 + 2] * m[6 + col];
      }
      bool finite = true;
      for (u32 w = 0; w < load.count; ++w)
        finite = finite && std::isfinite(out.words[w]);
      if (finite)
        plan->synth.push_back(out), ++s_stat_synth_matrices;
    }
  }
  if (StrictTest())
  {
    plan->new_loads.clear();
    plan->new_prims.clear();
    for (u32 i = 0; i < cur_frame.loads.size(); ++i)
      if (!load_had_partner[i])
        plan->new_loads.push_back(i);
    for (u32 i = 0; i < cur_frame.prims.size(); ++i)
      if (!partner[i] && !moved_prim[i])
        plan->new_prims.push_back(i);
  }
  plan->total = cur_frame.prims.size();
  for (const Load& load : cur_frame.loads)
    plan->total += IsColourLoad(load.address, load.count) ? 0 : 1;
  s_stat_loads += cur_frame.loads.size();
  s_stat_prims += cur_frame.prims.size();
  s_stat_draws += cur_frame.draws.size();
}

// The blend at t: the plan's matrices and positions written into a copy of the current stream. False
// when too few loads and draws have a partner in the previous frame to be worth it (a scene change).

u8 BlendByte(u8 prev, u8 cur, float t)
{
  return static_cast<u8>(std::clamp(std::lround(prev + (static_cast<float>(cur) - prev) * t), 0l, 255l));
}

// [Skies] A direct vertex colour (CP colour 0 or 1) blended like the position: per channel, for the formats of whole
// bytes and the 16-bit ones (RGB565, RGBA4444); RGBA6666 is left.
void BlendVertexColour(u8 format, const u8* cur, const u8* prev, u8* dst, float t)
{
  switch (format)
  {
  case 1:  // RGB8
  case 2:  // RGBX8
  case 5:  // RGBA8
  {
    const int n = format == 1 ? 3 : 4;
    for (int k = 0; k < n; ++k)
      dst[k] = BlendByte(prev[k], cur[k], t);
    break;
  }
  case 0:  // RGB565
  {
    const u32 c = BE16(cur), p = BE16(prev);
    const auto channel = [&](int shift, int bits) {
      const float max = static_cast<float>((1 << bits) - 1);
      const float cv = static_cast<float>((c >> shift) & ((1 << bits) - 1)), pv = static_cast<float>((p >> shift) & ((1 << bits) - 1));
      return static_cast<u32>(std::clamp(std::lround(pv + (cv - pv) * t), 0l, static_cast<long>(max)));
    };
    const u32 v = (channel(11, 5) << 11) | (channel(5, 6) << 5) | channel(0, 5);
    dst[0] = static_cast<u8>(v >> 8);
    dst[1] = static_cast<u8>(v);
    break;
  }
  case 3:  // RGBA4444
  {
    const u32 c = BE16(cur), p = BE16(prev);
    u32 v = 0;
    for (int shift = 0; shift < 16; shift += 4)
    {
      const float cv = static_cast<float>((c >> shift) & 15), pv = static_cast<float>((p >> shift) & 15);
      v |= static_cast<u32>(std::clamp(std::lround(pv + (cv - pv) * t), 0l, 15l)) << shift;
    }
    dst[0] = static_cast<u8>(v >> 8);
    dst[1] = static_cast<u8>(v);
    break;
  }
  default:
    break;
  }
}

// A texture coordinate component of a ComponentFormat with `frac` fraction bits, as a float, and back
float ReadTexComponent(u8 format, u8 frac, const u8* q)
{
  const float unit = frac ? 1.0f / static_cast<float>(1u << frac) : 1.0f;
  switch (format)
  {
  case FORMAT_FLOAT: return ReadFloat(q);
  case FORMAT_SHORT: return static_cast<float>(static_cast<s16>(BE16(q))) * unit;
  case FORMAT_USHORT: return static_cast<float>(BE16(q)) * unit;
  case FORMAT_BYTE: return static_cast<float>(static_cast<s8>(q[0])) * unit;
  default: return static_cast<float>(q[0]) * unit;
  }
}

void WriteTexComponent(u8 format, u8 frac, u8* q, float value)
{
  if (format == FORMAT_FLOAT)
    return WriteFloat(q, value);
  const long raw = std::lround(value * static_cast<float>(1u << frac));
  switch (format)
  {
  case FORMAT_SHORT:
  case FORMAT_USHORT:
  {
    const long r = std::clamp(raw, format == FORMAT_SHORT ? -32768l : 0l, format == FORMAT_SHORT ? 32767l : 65535l);
    q[0] = static_cast<u8>(r >> 8);
    q[1] = static_cast<u8>(r);
    break;
  }
  case FORMAT_BYTE: q[0] = static_cast<u8>(std::clamp(raw, -128l, 127l)); break;
  default: q[0] = static_cast<u8>(std::clamp(raw, 0l, 255l)); break;
  }
}

// [Skies] Whether a draw's texture coordinates stepped rather than moved: one of them moved more than half a texture (a
// scroll that wrapped), or all its vertices moved by the same offset, a quarter or more of the part of the texture the
// draw shows (a flipbook cell: the pyramid's torch flames and glows step through 0.25-wide cells; blended halfway they
// showed the gap between two cells, the flames split and the glow gone). c and p: per vertex, elems coordinates each.
bool TexStepped(const std::vector<float>& c, const std::vector<float>& p, u32 elems)
{
  if (elems == 0 || c.empty())
    return false;
  const size_t vertices = c.size() / elems;
  for (size_t i = 0; i < c.size(); ++i)
    if (!(std::fabs(c[i] - p[i]) <= 0.5f))
      return true;
  for (u32 k = 0; k < elems; ++k)
  {
    float lo = 1e30f, hi = -1e30f, dlo = 1e30f, dhi = -1e30f;
    for (size_t v = 0; v < vertices; ++v)
    {
      const float a = c[v * elems + k], b = p[v * elems + k];
      lo = std::min({lo, a, b});
      hi = std::max({hi, a, b});
      dlo = std::min(dlo, a - b);
      dhi = std::max(dhi, a - b);
    }
    const float span = hi - lo, shift = 0.5f * (dlo + dhi);
    if (vertices >= 2 && dhi - dlo <= 0.05f * span + 1.0f / 256.0f && std::fabs(shift) >= 0.25f * (span - std::fabs(shift)) &&
        std::fabs(shift) > 1.0f / 256.0f)
      return true;
  }
  return false;
}

// A paired draw's direct texture coordinates (vertex data) blended, vertex by vertex; a coordinate that moved by more
// than half a texture, or a flipbook step (TexStepped), takes the previous frame's value for the first half of the
// way and the current frame's after
void BlendDirectTexture(const Frame& cur_frame, const Frame& prev_frame, const Prim& prim, const Prim& old, u8* out, float t)
{
  for (u32 u = 0; u < 4; ++u)
  {
    const Prim::TexAttr& a = prim.tex[u];
    const Prim::TexAttr& b = old.tex[u];
    if (a.off == 0xFF || b.off != a.off || b.elems != a.elems || b.format != a.format || b.frac != a.frac)
      continue;
    const u32 comp = a.format <= FORMAT_BYTE ? 1u : a.format <= FORMAT_SHORT ? 2u : 4u;
    const u8* cur = cur_frame.stream.data() + prim.data_offset + a.off;
    const u8* prev = prev_frame.stream.data() + old.data_offset + a.off;
    static std::vector<float> cv, pv;
    cv.clear();
    pv.clear();
    for (u32 v = 0; v < prim.num_vertices; ++v)
      for (u32 k = 0; k < a.elems; ++k)
      {
        cv.push_back(ReadTexComponent(a.format, a.frac, cur + v * prim.vertex_size + k * comp));
        pv.push_back(ReadTexComponent(a.format, a.frac, prev + v * prim.vertex_size + k * comp));
      }
    const bool wrapped = TexStepped(cv, pv, a.elems);
    if (wrapped && t >= 0.5f)
      continue;
    for (u32 v = 0; v < prim.num_vertices; ++v)
      for (u32 k = 0; k < a.elems; ++k)
      {
        const u32 at = v * prim.vertex_size + k * comp;
        const float c = ReadTexComponent(a.format, a.frac, cur + at), p = ReadTexComponent(a.format, a.frac, prev + at);
        WriteTexComponent(a.format, a.frac, out + prim.data_offset + a.off + at, wrapped ? p : p + (c - p) * t);
      }
  }
}

// [Skies] The arrays' copies the replay reads (with blended colours and texture coordinates, when a draw's were
// blended: the pyramid's torch glow quads take their texture coordinates from an indexed array that changes every
// frame); empty when none was
std::vector<u8> s_blended_arrays;
bool s_arrays_blended = false;
// Which of the frame's array copies were blended (by Frame::array_copies index): only those are copied into
// s_blended_arrays and read from it (copying all the array data for each in-between cost the Pocket 5 ~0.5 ms)
std::vector<u8> s_copy_blended;

// A paired draw's indexed colours and float texture coordinates blended, vertex by vertex: the element a vertex of the
// current draw reads, and the one its partner read. A texture coordinate that moved by more than half a texture
// (a scroll that wrapped) stays the current one.
void BlendArrays(const Frame& cur_frame, const Frame& prev_frame, const Prim& prim, const Prim& old, float t)
{
  const auto find_draw = [](const Frame& frame, const Prim& p) -> const Draw* {
    const u32 offset = p.data_offset - 3;
    const auto it = std::lower_bound(frame.draws.begin(), frame.draws.end(), offset,
                                     [](const Draw& d, u32 value) { return d.offset < value; });
    return it != frame.draws.end() && it->offset == offset ? &*it : nullptr;
  };
  const Draw* cd = find_draw(cur_frame, prim);
  const Draw* pd = find_draw(prev_frame, old);
  if (!cd || !pd)
    return;
  for (u32 i = cd->arrays_begin; i < cd->arrays_end; ++i)
  {
    const ArrayCopy& ac = cur_frame.array_copies[i];
    if (ac.kind == 0)
      continue;
    const ArrayCopy* bc = nullptr;
    for (u32 j = pd->arrays_begin; j < pd->arrays_end; ++j)
      if (prev_frame.array_copies[j].array == ac.array)
        bc = &prev_frame.array_copies[j];
    if (!bc || bc->kind != ac.kind || bc->idx_off != ac.idx_off || bc->idx_size != ac.idx_size || bc->stride != ac.stride ||
        bc->colour_format != ac.colour_format || bc->elems != ac.elems || bc->tex_format != ac.tex_format || bc->frac != ac.frac || ac.stride == 0)
      continue;
    const u32 comp = ac.tex_format <= FORMAT_BYTE ? 1u : ac.tex_format <= FORMAT_SHORT ? 2u : 4u;
    const u32 span = ac.kind == 2 ? ac.elems * comp : (ac.colour_format == 1 ? 3u : ac.colour_format == 0 || ac.colour_format == 3 ? 2u : 4u);
    const u8* cur_stream = cur_frame.stream.data() + prim.data_offset;
    const u8* prev_stream = prev_frame.stream.data() + old.data_offset;
    // The byte offset in each copy of the element a vertex reads, or ~0 if it lies outside the copy
    const auto where = [&](const u8* stream, const ArrayCopy& copy, u32 v) -> u32 {
      const u8* q = stream + v * prim.vertex_size + copy.idx_off;
      const u32 index = copy.idx_size == 1 ? q[0] : BE16(q);
      const u64 at = static_cast<u64>(index) * copy.stride;
      return at < copy.lo || at - copy.lo + span > copy.size ? ~0u : static_cast<u32>(at - copy.lo);
    };
    const auto read = [&](const u8* q) { return ReadTexComponent(ac.tex_format, ac.frac, q); };
    // Nothing to blend when every vertex reads the same bytes in both frames (most draws: the map's texture coordinates;
    // checking costs a compare per vertex, blending them cost the Pocket 5 1.75 ms an in-between)
    {
      bool same = true;
      for (u32 v = 0; v < prim.num_vertices && same; ++v)
      {
        const u32 a = where(cur_stream, ac, v), b = where(prev_stream, *bc, v);
        same = a == ~0u || b == ~0u ||
               std::memcmp(cur_frame.array_data.data() + ac.data_offset + a, prev_frame.array_data.data() + bc->data_offset + b,
                           span) == 0;
      }
      if (same)
        continue;
    }

    const auto write = [&](u8* q, float value) { WriteTexComponent(ac.tex_format, ac.frac, q, value); };
    bool step_prev = false;
    if (ac.kind == 2)
    {
      static std::vector<float> cv, pv;
      cv.clear();
      pv.clear();
      for (u32 v = 0; v < prim.num_vertices; ++v)
      {
        const u32 a = where(cur_stream, ac, v), b = where(prev_stream, *bc, v);
        if (a == ~0u || b == ~0u)
          continue;
        for (u32 k = 0; k < ac.elems; ++k)
        {
          cv.push_back(read(cur_frame.array_data.data() + ac.data_offset + a + k * comp));
          pv.push_back(read(prev_frame.array_data.data() + bc->data_offset + b + k * comp));
        }
      }
      const bool wrapped = TexStepped(cv, pv, ac.elems);
      // A flipbook cell or a wrapped scroll can't be blended: the previous frame's coordinates for the first half of the
      // way, the current frame's after (the picture changes at the middle, as a frame of the game would)
      step_prev = wrapped && t < 0.5f;
      if (wrapped && !step_prev)
        continue;
    }
    for (u32 v = 0; v < prim.num_vertices; ++v)
    {
      const u32 a = where(cur_stream, ac, v), b = where(prev_stream, *bc, v);
      if (a == ~0u || b == ~0u)
        continue;
      if (!s_copy_blended[i])
      {
        if (s_blended_arrays.size() < cur_frame.array_data.size())
          s_blended_arrays.resize(cur_frame.array_data.size());
        std::memcpy(s_blended_arrays.data() + ac.data_offset, cur_frame.array_data.data() + ac.data_offset, ac.size);
        s_copy_blended[i] = 1;
        s_arrays_blended = true;
      }
      const u8* cur = cur_frame.array_data.data() + ac.data_offset + a;
      const u8* prev = prev_frame.array_data.data() + bc->data_offset + b;
      u8* dst = s_blended_arrays.data() + ac.data_offset + a;
      if (ac.kind == 2 && step_prev)
      {
        std::memcpy(dst, prev, span);
      }
      else if (ac.kind == 2)
      {
        for (u32 k = 0; k < ac.elems; ++k)
        {
          const float c = read(cur + k * comp), p = read(prev + k * comp);
          write(dst + k * comp, p + (c - p) * t);
        }
      }
      else
      {
        BlendVertexColour(ac.colour_format, cur, prev, dst, t);
      }
    }
  }
}

bool ApplyPlan(const Frame& cur_frame, const Frame& prev_frame, const BlendPlan& plan, std::vector<u8>* out, float t)
{
  *out = cur_frame.stream;
  s_arrays_blended = false;
  s_copy_blended.assign(cur_frame.array_copies.size(), 0);
  size_t paired = 0;
  for (const auto& [load_ptr, match_ptr] : plan.loads)
  {
    const Load& load = *load_ptr;
    const u8* cur = cur_frame.stream.data() + load.data_offset;
    const u8* prev = prev_frame.stream.data() + match_ptr->data_offset;
    paired += IsColourLoad(load.address, load.count) ? 0 : 1;
    for (u32 i = 0; i < load.count; ++i)
    {
      // The projection type (the word after the six parameters) isn't a float.
      if (load.address + i >= XFMEM_SETPROJECTION + 6)
        continue;
      const Word kind = WordKind(load.address + i);
      if (kind == Word::Keep)
        continue;
      if (kind == Word::Colour)
      {
        for (int k = 0; k < 4; ++k)
          out->data()[load.data_offset + i * 4 + k] = BlendByte(prev[i * 4 + k], cur[i * 4 + k], t);
        continue;
      }
      const float c = ReadFloat(cur + i * 4), p = ReadFloat(prev + i * 4);
      WriteFloat(out->data() + load.data_offset + i * 4, p + (c - p) * t);
    }
    KeepMatrixScale(load, cur, prev, t, out->data() + load.data_offset);
  }
  for (const BlendPlan::Synth& sy : plan.synth)
  {
    const Load& load = cur_frame.loads[sy.index];
    const u8* cur = cur_frame.stream.data() + load.data_offset;
    for (u32 i = 0; i < load.count; ++i)
    {
      const float c = ReadFloat(cur + i * 4), p = sy.words[i];
      WriteFloat(out->data() + load.data_offset + i * 4, p + (c - p) * t);
    }
  }
  // Draws turning round in one frame (Collapses, at this t) snap with their texture's group too
  static std::vector<u64> collapsing;
  static std::vector<u8> collapsed;
  static std::vector<BlendPlan::Trigger> collapse_triggers;
  collapse_triggers.clear();
  collapsing.clear();
  collapsed.assign(plan.prims.size(), 0);
  for (size_t i = 0; i < plan.prims.size(); ++i)
  {
    const BlendPlan::PrimPair& pair = plan.prims[i];
    if (!pair.check_collapse)
      continue;
    const Prim& prim = cur_frame.prims[pair.index];
    if (Collapses(cur_frame.stream.data() + prim.data_offset, prev_frame.stream.data() + pair.old->data_offset, prim,
                  t))
    {
      collapsed[i] = 1;
      collapsing.push_back(prim.texture);
      collapse_triggers.push_back(MakeTrigger(cur_frame, prim));
      ++s_stat_collapsed;
    }
  }
  std::sort(collapsing.begin(), collapsing.end());
  const auto group_snaps_texture = [&](u64 texture) {
    return std::binary_search(plan.jumping_textures.begin(), plan.jumping_textures.end(), texture) ||
           std::binary_search(collapsing.begin(), collapsing.end(), texture);
  };
  // [Skies] Local snapping (issue #18): a trigger snaps the draws with its texture of the same object (the battle
  // transition task's shatter card stays whole), or, for draws of no object (task 0: the pyramid's ~140 flame quads,
  // their glows), only those near it. Snapping every flame on that picture drew them all a frame ahead.
  static const bool local_snap = [] {
    const char* env = std::getenv("SKIES_INTERP_LOCAL_SNAP");
    return !env || env[0] != '0';
  }();
  const auto group_snaps_prim = [&](const Prim& prim) {
    if (!local_snap)
      return group_snaps_texture(prim.texture);
    if (!group_snaps_texture(prim.texture))
      return false;
    const BlendPlan::Trigger self = MakeTrigger(cur_frame, prim);
    const auto near = [&](const BlendPlan::Trigger& tr) {
      if (tr.texture != prim.texture)
        return false;
      if (tr.task != 0 || prim.task != 0)
        return tr.task == prim.task;
      const float dx = tr.at[0] - self.at[0], dy = tr.at[1] - self.at[1], dz = tr.at[2] - self.at[2];
      const float length = std::sqrt(tr.at[0] * tr.at[0] + tr.at[1] * tr.at[1] + tr.at[2] * tr.at[2]);
      const float radius = std::max(50.0f, 0.15f * length);
      return dx * dx + dy * dy + dz * dz <= radius * radius;
    };
    return std::any_of(plan.triggers.begin(), plan.triggers.end(), near) ||
           std::any_of(collapse_triggers.begin(), collapse_triggers.end(), near);
  };
  for (size_t i = 0; i < plan.prims.size(); ++i)
  {
    const BlendPlan::PrimPair& pair = plan.prims[i];
    const Prim& prim = cur_frame.prims[pair.index];
    const Prim& old = *pair.old;
    if (pair.jumps || collapsed[i] || group_snaps_prim(prim))
      continue;
    ++paired;
    ++s_stat_prims_blended;
    if ((ColourMask() & 8) && prim.texture == old.texture)
      BlendArrays(cur_frame, prev_frame, prim, old, t);
    if ((ColourMask() & 16) && prim.texture == old.texture)
      BlendDirectTexture(cur_frame, prev_frame, prim, old, out->data(), t);
    if ((ColourMask() & 4) && prim.texture == old.texture)
      for (int c = 0; c < 2; ++c)
        if (prim.col_off[c] != 0xFF && old.col_off[c] == prim.col_off[c] && old.col_fmt[c] == prim.col_fmt[c])
          for (u32 v = 0; v < prim.num_vertices; ++v)
          {
            const u32 at = v * prim.vertex_size + prim.col_off[c];
            BlendVertexColour(prim.col_fmt[c], cur_frame.stream.data() + prim.data_offset + at,
                              prev_frame.stream.data() + old.data_offset + at, out->data() + prim.data_offset + at, t);
          }
    for (u32 v = 0; v < prim.num_vertices; ++v)
    {
      const u32 at = v * prim.vertex_size + prim.pos_offset;
      const u8* cur = cur_frame.stream.data() + prim.data_offset + at;
      const u8* prev = prev_frame.stream.data() + old.data_offset + at;
      u8* dst = out->data() + prim.data_offset + at;
      if (prim.pos_format == FORMAT_FLOAT)
      {
        for (u32 k = 0; k < prim.pos_count; ++k)
        {
          const float c = ReadFloat(cur + k * 4), p = ReadFloat(prev + k * 4);
          WriteFloat(dst + k * 4, p + (c - p) * t);
        }
      }
      else if (prim.pos_format == FORMAT_SHORT || prim.pos_format == FORMAT_USHORT)
      {
        const bool is_signed = prim.pos_format == FORMAT_SHORT;
        for (u32 k = 0; k < prim.pos_count; ++k)
        {
          const u32 cu = BE16(cur + k * 2), pu = BE16(prev + k * 2);
          const float c = is_signed ? static_cast<s16>(cu) : static_cast<float>(cu);
          const float pf = is_signed ? static_cast<s16>(pu) : static_cast<float>(pu);
          const s32 value = static_cast<s32>(std::lround(pf + (c - pf) * t));
          dst[k * 2] = static_cast<u8>(value >> 8);
          dst[k * 2 + 1] = static_cast<u8>(value);
        }
      }
      else
      {
        const bool is_signed = prim.pos_format == FORMAT_BYTE;
        for (u32 k = 0; k < prim.pos_count; ++k)
        {
          const float c = is_signed ? static_cast<s8>(cur[k]) : static_cast<float>(cur[k]);
          const float pf = is_signed ? static_cast<s8>(prev[k]) : static_cast<float>(prev[k]);
          dst[k] = static_cast<u8>(static_cast<s32>(std::lround(pf + (c - pf) * t)));
        }
      }
    }
  }
  for (const BlendPlan::Moved& move : plan.moved)
  {
    const Prim& prim = cur_frame.prims[move.index];
    if (group_snaps_prim(prim))
      continue;
    ++s_stat_moved;
    for (u32 v = 0; v < prim.num_vertices; ++v)
    {
      const u32 at = prim.data_offset + v * prim.vertex_size + prim.pos_offset;
      const float c[3] = {ReadFloat(cur_frame.stream.data() + at), ReadFloat(cur_frame.stream.data() + at + 4),
                          ReadFloat(cur_frame.stream.data() + at + 8)};
      const std::array<float, 3> p = MoveBack(move, c);
      for (u32 k = 0; k < 3; ++k)
        WriteFloat(out->data() + at + k * 4, p[k] + (c[k] - p[k]) * t);
    }
  }
  if (StrictTest())
  {
    // A new position or texture matrix zeroed (its draws collapse to a point); a new draw's vertices all at its first
    for (const u32 i : plan.new_loads)
    {
      const Load& load = cur_frame.loads[i];
      if ((load.count == 12 || load.count == 9) && IdentityMatrix(cur_frame.stream.data() + load.data_offset, load.count))
        continue;  // used by the draws after it too: their own positions hide new ones
      if (load.address < XFMEM_POSMATRICES_END)
        for (u32 w = 0; w < load.count && load.address + w < XFMEM_POSMATRICES_END; ++w)
          WriteFloat(out->data() + load.data_offset + w * 4, 0.0f);
    }
    for (const u32 i : plan.new_prims)
    {
      const Prim& prim = cur_frame.prims[i];
      const u32 size = prim.pos_format == FORMAT_FLOAT ? 4u : (prim.pos_format >= FORMAT_USHORT ? 2u : 1u);
      const u8* first = out->data() + prim.data_offset + prim.pos_offset;
      for (u32 v = 1; v < prim.num_vertices; ++v)
        std::memmove(out->data() + prim.data_offset + v * prim.vertex_size + prim.pos_offset, first, size * prim.pos_count);
    }
  }
  return plan.total != 0 && paired * 2 >= plan.total;
}

u64 s_serial = 0;  // the game's thread: each finished recording's (BlendPlan's key)

bool BuildBlendedStream(const Frame& cur_frame, const Frame& prev_frame, std::vector<u8>* out, float t)
{
  static BlendPlan plan;
  if (plan.cur_serial != cur_frame.serial || plan.prev_serial != prev_frame.serial || cur_frame.serial == 0)
  {
    BuildPlan(cur_frame, prev_frame, &plan);
    plan.cur_serial = cur_frame.serial;
    plan.prev_serial = prev_frame.serial;
  }
  return ApplyPlan(cur_frame, prev_frame, plan, out, t);
}


// ---- recording -----------------------------------------------------------------------------------

u32 ComponentSize(u32 format)
{
  return format <= FORMAT_BYTE ? 1 : format <= FORMAT_SHORT ? 2 : 4;
}

// An XF load, immediate or turned into one, at the stream's end (its data after the 5-byte header).
void NoteXF(u16 address, u8 count)
{
  bool relevant = false;
  for (u32 i = 0; i < count && !relevant; ++i)
    relevant = IsMatrixOrProjection(address + i) ||
               (((address + i >= XFMEM_LIGHTS && address + i < XFMEM_LIGHTS_END && (ColourMask() & 1)) ||
                 (address + i >= XFMEM_AMBIENT0 && address + i <= XFMEM_MATERIAL1 && (ColourMask() & 2))) &&
                WordKind(address + i) != Word::Keep);
  if (!relevant)
    return;
  s_unkeyed.push_back(s_current->loads.size());
  s_current->loads.push_back(Load{static_cast<u32>(s_current->stream.size() + 5), address, count, 0, 0});
}

// A draw: its key, its direct positions, and the arrays it reads by index (copied: the replay
// would otherwise read RAM the game changed, as the Dolphin build's RecordArrays).
void NoteDraw(const u8* p, u32 len)
{
  Frame& frame = *s_current;
  const u32* cp = DffCp();
  const u32 vat = p[0] & 7;
  const u16 num_vertices = BE16(p + 1);
  const u8* vertex_data = p + 3;
  const u32 size = len - 3;
  const u32 vcd_lo = cp[0x50], vcd_hi = cp[0x60];
  const u32 va = cp[0x70 + vat], vb = cp[0x80 + vat], vc = cp[0x90 + vat];
  const u32 texture = DffBp()[0x94] & 0xFFFFFF;  // texture unit 0's image address (TX_SETIMAGE3)
  const u64 key = (u64{vat} << 56) ^ (u64{num_vertices} << 32) ^ (u64{size} << 8) ^ (u64{texture} << 20);

  // The attributes in vertex order: their CP array, how they're sent (0 not, 1 direct, 2 index8,
  // 3 index16), their direct size, the bytes one index reads, and indices per vertex.
  struct Indexed
  {
    u8 array;
    u32 offset;
    u8 index_size;
    u8 count;
    u32 span;
  };
  std::array<Indexed, 12> indexed;
  size_t n = 0;
  u32 offset = (vcd_lo & 1) ? 1 : 0;
  for (u32 i = 0; i < 8; ++i)
    offset += (vcd_lo >> (1 + i)) & 1;
  const u8 pos_offset = static_cast<u8>(offset);
  u8 direct_at[12];
  std::memset(direct_at, 0xFF, sizeof direct_at);
  const auto add = [&](u8 array, u32 type, u32 direct, u32 span, u8 count) {
    if (type == 1)
      direct_at[array] = static_cast<u8>(std::min(offset, 0xFEu));
    if (type >= 2)
    {
      indexed[n++] = Indexed{array, offset, static_cast<u8>(type == 3 ? 2 : 1), count, span};
      offset += (type == 3 ? 2 : 1) * count;
    }
    else if (type == 1)
    {
      offset += direct;
    }
  };
  const u32 pos_type = (vcd_lo >> 9) & 3;
  const u32 pos_format = (va >> 1) & 7;
  const u32 pos_count = (va & 1) ? 3 : 2;
  add(0, pos_type, pos_count * ComponentSize(pos_format), pos_count * ComponentSize(pos_format), 1);
  const u32 nrm_type = (vcd_lo >> 11) & 3;
  const bool nbt = (va >> 9) & 1;
  const u32 nrm_size = ComponentSize((va >> 10) & 7);
  const bool index3 = nbt && (va >> 31);
  add(1, nrm_type, (nbt ? 9 : 3) * nrm_size, 9 * nrm_size, index3 ? 3 : 1);
  static constexpr u32 COLOR_SIZE[8] = {2, 3, 4, 2, 3, 4, 4, 4};
  add(2, (vcd_lo >> 13) & 3, COLOR_SIZE[(va >> 14) & 7], COLOR_SIZE[(va >> 14) & 7], 1);
  add(3, (vcd_lo >> 15) & 3, COLOR_SIZE[(va >> 18) & 7], COLOR_SIZE[(va >> 18) & 7], 1);
  const u32 tex_elems[8] = {(va >> 21) & 1, vb & 1, (vb >> 9) & 1, (vb >> 18) & 1,
                            (vb >> 27) & 1, (vc >> 5) & 1, (vc >> 14) & 1, (vc >> 23) & 1};
  const u32 tex_fmt[8] = {(va >> 22) & 7, (vb >> 1) & 7, (vb >> 10) & 7, (vb >> 19) & 7,
                          (vb >> 28) & 7, (vc >> 6) & 7, (vc >> 15) & 7, (vc >> 24) & 7};
  for (u32 i = 0; i < 8; ++i)
  {
    const u32 bytes = (tex_elems[i] ? 2 : 1) * ComponentSize(tex_fmt[i]);
    add(static_cast<u8>(4 + i), (vcd_hi >> (2 * i)) & 3, bytes, bytes, 1);
  }

  Draw draw{static_cast<u32>(frame.stream.size()), len, static_cast<u32>(frame.array_copies.size()), 0};
  // A layout that doesn't add up means a format read differently here: leave the draw alone.
  if (num_vertices != 0 && offset * num_vertices == size)
  {
    for (size_t c = 0; c < n; ++c)
    {
      const Indexed& item = indexed[c];
      const u32 all_ones = item.index_size == 1 ? 0xFFu : 0xFFFFu;
      u32 lo_index = all_ones, hi_index = 0;
      bool skip_index = false;
      for (u32 v = 0; v < num_vertices; ++v)
      {
        const u8* q = vertex_data + v * offset + item.offset;
        for (u32 k = 0; k < item.count; ++k)
        {
          const u32 index = item.index_size == 1 ? q[k] : BE16(q + 2 * k);
          skip_index = skip_index || index == all_ones;
          lo_index = std::min(lo_index, index);
          hi_index = std::max(hi_index, index);
        }
      }
      // The loader reads a skipped position too, outside any range worth copying: leave that
      // array in RAM for this draw.
      if (skip_index && item.array == 0)
        continue;
      const u32 stride = cp[0xB0 + item.array] & 0xFF;
      const u32 lo = lo_index * stride;
      const u32 bytes = (hi_index - lo_index) * stride + item.span;
      if (bytes > 1024 * 1024)
        continue;
      const u32 base = cp[0xA0 + item.array];
      const u8* data = RamPtr(base + lo, bytes);
      if (!data)
        continue;
      frame.array_copies.push_back(
          ArrayCopy{item.array, static_cast<u32>(frame.array_data.size()), lo, bytes, base});
      {
        ArrayCopy& added = frame.array_copies.back();
        added.idx_off = static_cast<u8>(item.offset);
        added.idx_size = item.index_size;
        added.stride = stride;
        if (item.array == 2 || item.array == 3)
        {
          added.kind = item.count == 1 ? 1 : 0;
          added.colour_format = static_cast<u8>((va >> (item.array == 2 ? 14 : 18)) & 7);
        }
        else if (item.array >= 4 && item.array != 8 && item.count == 1)
        {
          // The VAT's fraction bits of each coordinate (texture 4's straddle two words: left alone)
          static constexpr u32 SHIFT_AT[8] = {25, 4, 13, 22, 0, 9, 18, 27};
          const u32 unit = item.array - 4;
          const u32 word = unit == 0 ? va : unit < 4 ? vb : vc;
          added.kind = 2;
          added.elems = tex_elems[unit] ? 2 : 1;
          added.tex_format = static_cast<u8>(tex_fmt[unit]);
          added.frac = static_cast<u8>(tex_fmt[unit] == FORMAT_FLOAT ? 0 : (word >> SHIFT_AT[unit]) & 31);
        }
      }
      frame.array_data.insert(frame.array_data.end(), data, data + bytes);
    }
  }
  draw.arrays_end = static_cast<u32>(frame.array_copies.size());
  frame.draws.push_back(draw);

  if (pos_type == 1 && num_vertices != 0)
  {
    const u32 task = s_in_dl ? s_dl_task : s_top_task;
    const u32 ordinal = !task ? 0u : s_in_dl ? 0x80000000u | (s_dl_block << 12) | (s_dl_draw & 0xFFFu) : s_task_draws[task]++;
    s_dl_draw += s_in_dl ? 1 : 0;
    frame.prims.push_back(Prim{static_cast<u32>(frame.stream.size() + 3), num_vertices,
                               static_cast<u16>(size / num_vertices), pos_offset,
                               static_cast<u8>(pos_format), static_cast<u8>(pos_count), key, texture,
                               task, task ? Read32(task) : 0u, ordinal});
    frame.prims.back().col_off[0] = direct_at[2];
    frame.prims.back().col_off[1] = direct_at[3];
    frame.prims.back().col_fmt[0] = static_cast<u8>((va >> 14) & 7);
    frame.prims.back().col_fmt[1] = static_cast<u8>((va >> 18) & 7);
    for (u32 u = 0; u < 4; ++u)
    {
      static constexpr u32 SHIFT_AT[4] = {25, 4, 13, 22};
      Prim::TexAttr& attr = frame.prims.back().tex[u];
      attr.off = direct_at[4 + u];
      attr.elems = tex_elems[u] ? 2 : 1;
      attr.format = static_cast<u8>(tex_fmt[u]);
      attr.frac = static_cast<u8>(tex_fmt[u] == FORMAT_FLOAT ? 0 : ((u == 0 ? va : vb) >> SHIFT_AT[u]) & 31);
    }
  }
  // Not the vertex data itself: CPU-skinned models change it every frame.
  const u32 load_task = TasksOn() ? (s_in_dl ? s_dl_task : s_top_task) : 0u;
  const u32 load_func = load_task ? Read32(load_task) : 0u;
  for (size_t index : s_unkeyed)
  {
    frame.loads[index].key = key;
    frame.loads[index].texture = texture;
    frame.loads[index].prim = (pos_type == 1 && num_vertices != 0) ? static_cast<u32>(frame.prims.size() - 1) : ~0u;
    frame.loads[index].task = load_task;
    frame.loads[index].func = load_func;
  }
  s_unkeyed.clear();
}

// Interpolation only where it fits (the Dolphin build's ReplayFits): a scene whose work plus replay
// doesn't fit its real-time budget would slow the game down. The work is the video thread's: its
// time on the stream and the host's work, without the waits in Aurora's present calls (VideoBusy);
// with SKIES_VIDEO_THREAD=0, the one thread's wall time less the throttle's sleep and those waits
// (the single-core budget). Each frame measures that work (without the replay) and the replay's
// cost; replays stop while the two together don't fit (95% of the budget, averaged over about ten
// frames) and come back after a second that would have fit with room (88%). The frames in between
// show at the game's own rate, at full speed.
// SKIES_INTERP_ADAPTIVE=0: always (to measure the full cost).
struct Budget
{
  bool have_last = false;
  Clock::time_point last_time;
  Clock::duration last_idle{};  // the throttle's sleeps and Aurora's present waits, so far (one thread)
  u64 last_cycles = 0;
  double last_replay = 0.0;  // seconds the previous frame's replay took
  double replay = 0.0;       // recent replay cost
  double work = 0.0;         // recent game work per frame
  bool on = true;
  u32 fitting = 0;  // frames in a row that would have fit while off
  double frame = 0.0;  // the last frame's real-time budget, seconds
  bool display = false;  // FrameMode 2: video.cpp makes fewer pictures instead (InterpDisplayAllowed)
};
Budget s_budget;
u64 s_skipped = 0;

bool ReplayFits(u64 cycles)
{
  static const bool adaptive = [] {
    const char* env = std::getenv("SKIES_INTERP_ADAPTIVE");
    return !env || env[0] != '0';
  }();
  Budget& b = s_budget;
  const bool threaded = VideoThreaded();
  const Clock::time_point now = threaded ? Clock::time_point(VideoBusy()) : Clock::now();
  const Clock::duration idle = threaded ? Clock::duration{} : ThrottleIdle() + VideoPresentTime();
  const Budget previous = b;
  b.have_last = true;
  b.last_time = now;
  b.last_idle = idle;
  b.last_cycles = cycles;
  b.last_replay = 0.0;
  if (!adaptive || !Throttled())
    return true;  // no frame limit: any frame rate is full speed
  if (!previous.have_last || cycles <= previous.last_cycles)
    return b.on;
  const double wall = std::chrono::duration<double>(now - previous.last_time).count();
  const double slept = std::chrono::duration<double>(idle - previous.last_idle).count();
  const double work = std::max(wall - slept - previous.last_replay, 0.0);
  const double budget = static_cast<double>(cycles - previous.last_cycles) / CPU_HZ;
  if (budget > 0.25)
    return b.on;  // a long gap (loading, a pause): says nothing about the scene
  b.frame = budget;
  // Averaged over about ten frames: the throttle catches up after a single slow frame
  b.work = b.work == 0.0 ? work : b.work * 0.9 + work * 0.1;
  // FrameMode 2: the in-betweens adapt (every few refreshes), so only the frame's own work counts here
  const double need = b.work + (b.display ? 0.0 : b.replay);
  if (b.on && need > 0.95 * budget)
  {
    b.on = false;
    b.fitting = 0;
    Log("[interp] replays off: %.1f ms of work and %.1f of replay in a %.1f ms frame\n", b.work * 1e3,
        b.replay * 1e3, budget * 1e3);
  }
  else if (!b.on)
  {
    b.fitting = need < 0.88 * budget ? b.fitting + 1 : 0;
    if (b.fitting >= 30)
    {
      b.on = true;
      Log("[interp] replays on again\n");
    }
  }
  return b.on;
}

void CountReplay(double seconds)
{
  s_budget.last_replay = seconds;
  s_budget.replay = s_budget.replay == 0.0 ? seconds : s_budget.replay * 0.8 + seconds * 0.2;
}

int Mode()
{
  static const int env = [] {
    const char* v = std::getenv("SKIES_INTERP");
    return v ? std::atoi(v) : -1;
  }();
  const int mode = env >= 0 ? env : SettingGet(SettingFile::Main, "Skies", "FrameMode", -1);
  return std::clamp(mode, 0, 2);
}

bool Wanted()
{
  return Mode() >= 1 && !MenuInterpHeld();
}

#ifdef SKIES_STANDALONE_AURORA
// The replay draws' array copies stay put until Aurora has parsed them (leaving the replay context
// drains its FIFO): a bump arena, reset at each replay. Aurora reads an array from its base, so a
// copy sits `lo` bytes into its buffer.
std::vector<std::vector<u8>> s_arena;
size_t s_arena_chunk = 0, s_arena_used = 0;
constexpr size_t ARENA_CHUNK = 4u << 20;

u8* ArenaAlloc(size_t bytes)
{
  if (s_arena.empty())
    s_arena.emplace_back(ARENA_CHUNK);
  if (s_arena_used + bytes > s_arena[s_arena_chunk].size())
  {
    ++s_arena_chunk;
    s_arena_used = 0;
    if (s_arena_chunk >= s_arena.size())
      s_arena.emplace_back(std::max(ARENA_CHUNK, bytes));
    else if (s_arena[s_arena_chunk].size() < bytes)
      s_arena[s_arena_chunk].resize(bytes);
  }
  u8* p = s_arena[s_arena_chunk].data() + s_arena_used;
  s_arena_used += (bytes + 63) & ~size_t{63};
  return p;
}

void Feed(const u8* data, size_t size)
{
  if (size == 0)
    return;
  aurora_backend_write_raw(data, static_cast<u32>(size));
  aurora_backend_drain_if_over(64 * 1024);
}

void PushBE(std::vector<u8>& out, u64 value, int bytes)
{
  for (int shift = 8 * (bytes - 1); shift >= 0; shift -= 8)
    out.push_back(static_cast<u8>(value >> shift));
}

// Replays the blended stream of `frame` into Aurora's replay context; keeps the in-between picture
// in `slot` just before the stream's own XFB copy (its last command).
bool Replay(const Frame& frame, const std::vector<u8>& stream, int slot)
{
  s_arena_chunk = 0;
  s_arena_used = 0;
  auto t0 = Clock::now();
  aurora_backend_replay_context(1);
  s_ms[2] += Ms(t0);
  t0 = Clock::now();
  if (!aurora_backend_in_replay_context())
  {
    static int s_logged = 0;
    if (s_logged++ < 5)
      Log("[interp] Aurora's replay context could not be entered: no in-between picture\n");
    return false;
  }
  aurora_backend_replay_texture_frame(frame.texture_frame);
  static std::vector<u8> cmd;
  cmd.clear();
  DffRegisterCommands(frame.start, cmd);
  Feed(cmd.data(), cmd.size());
  size_t pos = 0;
  for (const Draw& draw : frame.draws)
  {
    Feed(stream.data() + pos, draw.offset - pos);
    // GX_AURORA_LOAD_ARRAYBASE (0x50, sub 0x0010 + array): u64 host address, u32 size, u8
    // little-endian flag
    cmd.clear();
    for (u32 i = draw.arrays_begin; i < draw.arrays_end; ++i)
    {
      const ArrayCopy& copy = frame.array_copies[i];
      const size_t bytes = static_cast<size_t>(copy.lo) + copy.size + 64;
      u8* buffer = ArenaAlloc(bytes);
      const bool blended = s_arrays_blended && i < s_copy_blended.size() && s_copy_blended[i];
      std::memcpy(buffer + copy.lo, (blended ? s_blended_arrays : frame.array_data).data() + copy.data_offset, copy.size);
      cmd.push_back(0x50);
      PushBE(cmd, 0x0010 + copy.array, 2);
      PushBE(cmd, static_cast<u64>(reinterpret_cast<uintptr_t>(buffer)), 8);
      PushBE(cmd, static_cast<u32>(bytes), 4);
      cmd.push_back(0);
    }
    Feed(cmd.data(), cmd.size());
    Feed(stream.data() + draw.offset, draw.length);
    // The guest bases back (CP 0xA0 + array) for the draws that read RAM
    cmd.clear();
    for (u32 i = draw.arrays_begin; i < draw.arrays_end; ++i)
    {
      const ArrayCopy& copy = frame.array_copies[i];
      cmd.push_back(0x08);
      cmd.push_back(static_cast<u8>(0xA0 + copy.array));
      PushBE(cmd, copy.guest_base, 4);
    }
    Feed(cmd.data(), cmd.size());
    pos = draw.offset + draw.length;
  }
  // The XFB copy closes the stream (5 bytes, BP 0x52): the picture is kept before it goes in, and
  // its EFB clear leaves the spare EFB as the next replay expects it.
  const size_t copy_at = stream.size() >= 5 ? stream.size() - 5 : stream.size();
  Feed(stream.data() + pos, copy_at > pos ? copy_at - pos : 0);
  s_ms[3] += Ms(t0);
  t0 = Clock::now();
  const bool kept = aurora_backend_keep_picture(slot) != 0;
  s_ms[4] += Ms(t0);
  t0 = Clock::now();
  Feed(stream.data() + std::max(pos, copy_at), stream.size() - std::max(pos, copy_at));
  aurora_backend_replay_context(0);
  s_ms[5] += Ms(t0);
  return kept;
}
#endif
}  // namespace

void InterpDisplayListRecorded(u32 base)
{
  if (s_on_game && s_recording && TasksOn())
  {
    const u32 task = DrawTask();
    s_dl_tasks[base & 0x01FFFFFFu] = {task, task ? s_task_blocks[task]++ : 0u};
  }
}

void InterpDisplayListCall(u32 address, int depth, bool begin)
{
  if (depth != 0)
    return;
  s_in_dl = begin;
  s_dl_task = 0;
  s_dl_block = 0;
  s_dl_draw = 0;
  if (!begin)
    return;
  const auto it = s_dl_tasks.find(address & 0x01FFFFFFu);
  if (it != s_dl_tasks.end())
  {
    s_dl_task = it->second.first;
    s_dl_block = it->second.second;
  }
}

u32 InterpDrawTask()
{
  if (!s_on_game || !s_recording || !TasksOn())
    return 0;
  // At every 32-byte FIFO burst: while the same function (stack pointer, the two innermost return
  // addresses) is writing, the answer can't have changed
  static u32 last_sp = 0, last_ret = 0, last_ret2 = 0, last_running = 0, last_task = 0;
  static bool last_deferred = false;
  const u32 sp = g_cpu.gpr[1];
  const u32 back = sp >= 0x80000000u && sp < 0x81800000u ? Read32(sp) : 0;
  const bool back_ok = back >= 0x80000000u && back < 0x81800000u;
  const u32 ret = back_ok ? Read32(back + 4) : 0;
  const u32 back2 = back_ok ? Read32(back) : 0;
  const u32 ret2 = back2 >= 0x80000000u && back2 < 0x81800000u ? Read32(back2 + 4) : 0;
  const u32 running = Read32(Tasks().running);
  // In the deferred pass the running task stays as it was while the deferred ones change (same stack, same callers):
  // asked again each time there
  if (sp != last_sp || ret != last_ret || ret2 != last_ret2 || running != last_running || last_deferred)
  {
    last_sp = sp;
    last_ret = ret;
    last_ret2 = ret2;
    last_running = running;
    last_task = DrawTask(&last_deferred);
  }
  return last_task;
}

void InterpTopLevelTask(u32 task)
{
  s_top_task = task;
}

void InterpCommand(const u8* p, u32 len)
{
  if (!s_recording)
    return;
  Frame& frame = *s_current;
  switch (p[0])
  {
  case 0x40:
    return;  // the display list's commands come through here one by one
  case 0x10:
  {
    const u32 head = BE32(p + 1);
    NoteXF(static_cast<u16>(head & 0xFFFF), static_cast<u8>(((head >> 16) & 0xF) + 1));
    break;
  }
  case 0x20:
  case 0x28:
  case 0x30:
  case 0x38:
  {
    // An indexed XF load (array 12..15: index << 16 | (count - 1) << 12 | address): the replay
    // would read the matrix from RAM again, which by then holds the next frame's. Recorded as an
    // immediate load of the values read now, which can also be blended.
    const u32 value = BE32(p + 1);
    const u32 array = 12 + ((p[0] - 0x20) >> 3);
    const u32 index = value >> 16, address = value & 0xFFF, count = ((value >> 12) & 0xF) + 1;
    const u32* cp = DffCp();
    const u8* src = RamPtr(cp[0xA0 + array] + index * (cp[0xB0 + array] & 0xFF), count * 4);
    if (!src)
      break;  // recorded as it is
    NoteXF(static_cast<u16>(address), static_cast<u8>(count));
    const u32 header = ((count - 1) << 16) | address;
    frame.stream.push_back(0x10);
    for (int shift = 24; shift >= 0; shift -= 8)
      frame.stream.push_back(static_cast<u8>(header >> shift));
    frame.stream.insert(frame.stream.end(), src, src + count * 4);  // already big-endian
    return;
  }
  default:
    if (p[0] & 0x80)
      NoteDraw(p, len);
    break;
  }
  frame.stream.insert(frame.stream.end(), p, p + len);
  if (frame.stream.size() + frame.array_data.size() > MAX_RECORDED_BYTES)
  {
    frame.Clear();  // an empty frame is never replayed
    s_recording = false;
  }
}

InterpHandoff InterpFrameEndGame()
{
  InterpHandoff handoff;
  handoff.wanted = Wanted();
  handoff.display = handoff.wanted && Mode() == 2 && VideoThreaded();  // the timeline is the video thread's
  handoff.cycles = g_cycles;
  if (handoff.wanted != s_on_game)
  {
    s_on_game = handoff.wanted;
    InterpReset();
  }
  if (!s_on_game)
    return handoff;
  if (!s_current)
    s_current = TakeFrame();
  // Number the loads and draws now that every key is known.
  {
    // (Draws were numbered too once; nothing reads that.) A hash map, kept between frames
    struct LoadId
    {
      u64 key;
      u32 address_count;
      bool operator==(const LoadId&) const = default;
    };
    struct LoadIdHash
    {
      size_t operator()(const LoadId& id) const { return std::hash<u64>()(id.key * 0x9E3779B97F4A7C15ull ^ id.address_count); }
    };
    static std::unordered_map<LoadId, u32, LoadIdHash> seen;
    seen.clear();
    for (Load& load : s_current->loads)
      load.occurrence = seen[LoadId{load.key, (u32{load.address} << 8) | load.count}]++;
  }
  s_unkeyed.clear();
  s_dl_tasks.clear();
  s_task_blocks.clear();
  s_task_draws.clear();
  handoff.complete = s_recording && s_current->has_start && !s_current->stream.empty();
  s_current->serial = ++s_serial;
  handoff.frame = s_current;
  // The next frame starts with the registers as they are now (this XFB copy done)
  s_current = TakeFrame();
  DffSnapshot(s_current->start);
  s_current->has_start = true;
  // Recorded while replays run, and from two frames before they would come back (a replay needs two)
  s_recording = s_record_wanted.load(std::memory_order_relaxed);
  return handoff;
}

namespace
{
// The frame between its end and its replay (the video thread)
struct Pending
{
  Frame* frame = nullptr;
  bool complete = false;
  bool replay = false;  // the real picture kept: replay into real_slot + 1
  int real_slot = 0;
  double keep_seconds = 0.0;
  bool display = false;  // for the timeline (FrameMode 2) instead
  u64 id = 0;
};
Pending s_pending;

// FrameMode 2: the recent recordings, oldest first, for the timeline's blends (each with the one before)
struct DisplayFrame
{
  Frame* frame;
  bool complete;
  u64 id;
};
std::deque<DisplayFrame> s_display;
bool s_display_mode = false;   // the frames go to s_display
double s_display_replay = 0.0;  // replay seconds since the last frame end (the budget's)
u32 s_display_replays = 0;      // and how many
double s_display_cost = 0.0;    // seconds per replay (recent)
// In-betweens per frame that fit beside the frame's work (85% of the frame's time, the rest the video
// thread's work on the stream): video.cpp makes pictures every few refreshes when all of them don't
std::atomic<double> s_display_allowed{1e9};

void DisplayClear()
{
  for (DisplayFrame& f : s_display)
    GiveBack(f.frame);
  s_display.clear();
}

// The frame becomes the next one's previous (a recording cut short can't be blended with)
void Rotate(Frame* frame, bool complete)
{
  GiveBack(s_previous);
  s_previous = nullptr;
  if (complete)
    s_previous = frame;
  else
    GiveBack(frame);
  s_record_wanted.store(s_budget.on || s_budget.fitting >= 28, std::memory_order_relaxed);
}

void LogStats()
{
  if ((s_frames % 600) != 0)
    return;
  static const bool s_stats = std::getenv("SKIES_INTERP_STATS") != nullptr;
  Log("[interp] %llu of %llu frames interpolated (%llu skipped to keep full speed)\n",
      static_cast<unsigned long long>(s_interpolated), static_cast<unsigned long long>(s_frames),
      static_cast<unsigned long long>(s_skipped));
  if (s_stats)
  {
    Log("[interp] per blend %.1f draws, %.1f matrix loads (%.1f blended, %.1f changed), %.1f with "
        "positions (%.1f blended, %.1f changed, %.2f paired across a texture change, %.2f snapped turning, %.2f matrices snapped turning, %.1f with their object, %.1f paired by number, %.2f moved with their object, %.2f re-paired by it, %.2f re-paired by its or the camera's motion, %.2f kept apart, %.2f load groups by object, %.2f matrices from the camera, %.2f jumps explained); ms per frame: keep real %.2f, blend %.2f, enter "
        "%.2f, feed %.2f, keep in-between %.2f, leave %.2f\n",
        s_stat_draws / 600.0, s_stat_loads / 600.0, s_stat_loads_blended / 600.0,
        s_stat_words_changed / 600.0, s_stat_prims / 600.0, s_stat_prims_blended / 600.0,
        s_stat_prims_changed / 600.0, s_stat_repaired / 600.0, s_stat_collapsed / 600.0, s_stat_matrix_snaps / 600.0,
        s_stat_tasked / 600.0, s_stat_by_number / 600.0, s_stat_moved / 600.0, s_stat_rigid_repaired / 600.0, s_stat_camera_paired / 600.0, s_stat_kept_apart / 600.0, s_stat_loads_by_object / 600.0, s_stat_synth_matrices / 600.0, s_stat_jumps_explained / 600.0, s_ms[0] / 600, s_ms[1] / 600, s_ms[2] / 600, s_ms[3] / 600,
        s_ms[4] / 600, s_ms[5] / 600);
  }
  s_stat_loads = s_stat_prims = s_stat_loads_blended = s_stat_prims_blended = s_stat_words_changed =
      s_stat_draws = s_stat_prims_changed = s_stat_repaired = s_stat_collapsed = s_stat_matrix_snaps =
      s_stat_tasked = s_stat_by_number = s_stat_moved = s_stat_rigid_repaired = s_stat_camera_paired = s_stat_kept_apart = s_stat_loads_by_object = s_stat_synth_matrices = s_stat_jumps_explained = 0;
  std::fill(std::begin(s_ms), std::end(s_ms), 0.0);
}
}  // namespace

int InterpFrameEndVideo(const InterpHandoff& handoff, int real_slot, u64 id)
{
  Frame* const frame = static_cast<Frame*>(handoff.frame);
  // A replay that never came (no frame end can pass the next one's without it, but a state load)
  if (s_pending.frame)
    InterpReplayVideo();
  if (handoff.wanted != s_on || (handoff.wanted && handoff.display != s_display_mode))
  {
    s_on = handoff.wanted;
    InterpResetVideo();
    s_display_mode = handoff.wanted && handoff.display;
    Log("[interp] frame interpolation %s\n", !s_on ? "off" : s_display_mode ? "on (display rate)" : "on");
  }
  if (!s_on)
  {
    GiveBack(frame);
    return -1;
  }
  ++s_frames;
  s_budget.display = s_display_mode;
  if (s_display_mode)
  {
    // The in-betweens since the last frame end are its replay cost. The timeline's replays run outside
    // the video thread's items, so its busy time (the work) doesn't hold them: nothing to take off it.
    CountReplay(s_display_replay);
    s_budget.last_replay = 0.0;
    if (s_display_replays != 0)
    {
      const double cost = s_display_replay / s_display_replays;
      s_display_cost = s_display_cost == 0.0 ? cost : s_display_cost * 0.8 + cost * 0.2;
    }
    s_display_replay = 0.0;
    s_display_replays = 0;
  }
  // While replays don't fit, the frame is presented as at 30 fps (-1)
  const bool fits = ReplayFits(handoff.cycles);
  if (!fits)
    ++s_skipped;
  if (s_display_mode && s_display_cost > 0.0 && s_budget.frame > 0.0 && Throttled())
    s_display_allowed.store(std::max(0.85 * s_budget.frame - s_budget.work, 0.0) / s_display_cost,
                            std::memory_order_relaxed);
  else if (!s_display_mode || !Throttled())
    s_display_allowed.store(1e9, std::memory_order_relaxed);
  s_pending = Pending{frame, handoff.complete, false, real_slot, 0.0, s_display_mode, id};
#ifdef SKIES_STANDALONE_AURORA
  if (fits)
  {
    const auto t0 = Clock::now();
    s_pending.replay = aurora_backend_keep_picture(real_slot) != 0;
    s_pending.keep_seconds = std::chrono::duration<double>(Clock::now() - t0).count();
    s_ms[0] += s_pending.keep_seconds * 1e3;
  }
  frame->texture_frame = aurora_backend_texture_frame();
#else
  (void)real_slot;
#endif
  if (s_pending.replay && s_pending.display)
  {
    // For the timeline at once (its first pictures come before the next frame's tokens would pass); sorted
    // at its first blend
    s_display.push_back(DisplayFrame{frame, handoff.complete, id});
    while (s_display.size() > 4)
    {
      GiveBack(s_display.front().frame);
      s_display.pop_front();
    }
    s_record_wanted.store(s_budget.on || s_budget.fitting >= 28, std::memory_order_relaxed);
    LogStats();
    s_pending = {};
    return 0;
  }
  if (!s_pending.replay)
  {
    // Nothing to replay: the frame is only the next one's previous
    if (fits)
      CountReplay(s_pending.keep_seconds);
    LogStats();
    Rotate(frame, handoff.complete);
    s_pending = {};
    return -1;
  }
  return 0;
}

int InterpReplayVideo()
{
  if (!s_pending.frame)
    return -1;
  const Pending pending = s_pending;
  s_pending = {};
  int result = 0;
#ifdef SKIES_STANDALONE_AURORA
  const auto start = Clock::now();
  if (pending.complete && s_previous)
  {
    Sort(*s_previous);
    Sort(*pending.frame);
    static std::vector<u8> blended;
    auto t0 = Clock::now();
    const bool blend = BuildBlendedStream(*pending.frame, *s_previous, &blended, DefaultT());
    s_ms[1] += Ms(t0);
    if (blend && Replay(*pending.frame, blended, pending.real_slot + 1))
    {
      result = 1;
      ++s_interpolated;
    }
  }
  CountReplay(pending.keep_seconds + std::chrono::duration<double>(Clock::now() - start).count());
#endif
  LogStats();
  Rotate(pending.frame, pending.complete);
  return result;
}

void InterpReset()
{
  if (s_current)
    s_current->Clear();
  s_unkeyed.clear();
  s_recording = false;  // until the next frame end gives a start
}

void InterpResetVideo()
{
  GiveBack(s_pending.frame);
  s_pending = {};
  GiveBack(s_previous);
  s_previous = nullptr;
  DisplayClear();
  s_display_replay = 0.0;
  s_display_replays = 0;
}

int InterpMode()
{
  return Mode();
}

double InterpDisplayAllowed()
{
  return s_display_allowed.load(std::memory_order_relaxed);
}

int InterpDisplayReplay(u64 id, float t, int slot)
{
#ifdef SKIES_STANDALONE_AURORA
  const DisplayFrame* cur = nullptr;
  const DisplayFrame* prev = nullptr;
  for (size_t i = 1; i < s_display.size(); ++i)
  {
    if (s_display[i].id == id && s_display[i - 1].id + 1 == id)
    {
      cur = &s_display[i];
      prev = &s_display[i - 1];
    }
  }
  if (!cur)
    return -1;  // not here (yet)
  if (!cur->complete || !prev->complete)
    return -2;
  const auto start = Clock::now();
  Sort(*cur->frame);   // once each (Sort returns when sorted)
  Sort(*prev->frame);
  static std::vector<u8> blended;
  auto t0 = Clock::now();
  const bool blend = BuildBlendedStream(*cur->frame, *prev->frame, &blended, std::clamp(t, 0.0f, 1.0f));
  s_ms[1] += Ms(t0);
  int result = blend ? -4 : -3;  // -3: too different (a scene change), -4: no replay context
  if (blend && Replay(*cur->frame, blended, slot))
  {
    result = 1;
    ++s_interpolated;
  }
  s_display_replay += std::chrono::duration<double>(Clock::now() - start).count();
  ++s_display_replays;
  return result;
#else
  (void)id, (void)t, (void)slot;
  return 0;
#endif
}

void InterpDisplayDrop(u64 id)
{
  while (!s_display.empty() && s_display.front().id < id)
  {
    GiveBack(s_display.front().frame);
    s_display.pop_front();
  }
}
}  // namespace host
