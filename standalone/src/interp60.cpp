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
// records which game object (task) drew it (DrawTask), and two objects' draws are never paired.
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
// times every 600 frames. SKIES_INTERP_TRACE=1 (video.cpp): each frame end and retrace present.
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
};

// Vertex array data a draw reads by index, copied when it was drawn.
struct ArrayCopy
{
  u8 array;         // CP array: 0 position, 1 normal, 2-3 colours, 4-11 texture coordinates
  u32 data_offset;  // in Frame::array_data
  u32 lo;           // bytes from the array base to the first byte copied
  u32 size;
  u32 guest_base;   // the array's base register when it was drawn, put back after the replay's draw
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
u64 s_stat_collapsed = 0, s_stat_matrix_snaps = 0;  // draws and matrices snapped as turning round (issue #35)
u64 s_stat_tasked = 0, s_stat_kept_apart = 0;  // draws with a known object; pairings refused as two objects'
// Which game object drew each draw (the game's thread). Skies runs every object as a task (decomp
// docs/draw.md: TaskRunAll calls each task's function, which updates and draws it), and the running task is
// at 0x80311A7C (USA), so a draw made inside TaskRunAll's call belongs to that task. Most draws go straight into
// the FIFO: hw_gpu.cpp takes the object at the FIFO burst that brought a command's first bytes, which is
// while the game writes it (the GPU runs synchronously). The draw queue's display lists (battles, cutscene
// models) are recorded inside the task's call and replayed later, sorted by depth: the object is taken
// when GXBeginDisplayList points the CPU FIFO at the block (the PI FIFO base) and looked up when the block
// is called. Draws made outside TaskRunAll (TaskRunDeferred doesn't update the running task; the 2D layer) get
// 0. The pairing below never pairs two different objects' draws: alike sprites of two objects that depth
// sorting swaps (#18, #24) stay apart; an object's own draws are paired as before.
std::unordered_map<u32, u32> s_dl_tasks;  // block address (physical) -> task, this frame
u32 s_dl_task = 0;                        // the top-level display list being replayed
bool s_in_dl = false;
u32 s_top_task = 0;                       // the top-level command being applied
bool s_tasks_on = [] {  // SKIES_INTERP_TASKS=0: don't use the objects (to compare)
  const char* env = std::getenv("SKIES_INTERP_TASKS");
  return !env || env[0] != '0';
}();

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

// The running task, if the guest is inside TaskRunAll's call of it now: a saved return address on the
// guest stack (each frame's back chain word, the caller's return address at the frame above +4) inside
// TaskRunAll and none inside TaskRunDeferred first.
u32 DrawTask()
{
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
// Returns how many items were left without a partner. The Dolphin build's Align.
template <typename Distance, typename PairFn>
int Align(int n_cur, int n_prev, const Distance& distance, const PairFn& pair)
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
        const float c = cost_table[at(i - 1, j)] + gap;
        if (c < best)
          best = c, move = 2;  // a new item
      }
      if (j > 0 && at(i, j - 1) >= 0)
      {
        const float c = cost_table[at(i, j - 1)] + gap;
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
  size_t total = 0;
};

void BuildPlan(const Frame& cur_frame, const Frame& prev_frame, BlendPlan* plan)
{
  plan->loads.clear();
  plan->prims.clear();
  plan->jumping_textures.clear();
  const auto blend_load = [&](const Load& load, const Load& match) {
    const u8* cur = cur_frame.stream.data() + load.data_offset;
    const u8* prev = prev_frame.stream.data() + match.data_offset;
    if (!Blendable(load, cur, prev))
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
        const u8* c = cur_frame.stream.data() + cur_loads[i]->data_offset;
        const u8* p = prev_frame.stream.data() + prev_loads[j]->data_offset;
        float sum = 0.0f;
        for (u32 w = 0; w < words; ++w)
          sum += std::fabs(ReadFloat(c + w * 4) - ReadFloat(p + w * 4));
        return std::isfinite(sum) ? sum : 1e30f;
      };
      Align(static_cast<int>(cur_loads.size()), static_cast<int>(prev_loads.size()), distance,
            [&](int i, int j) { blend_load(*cur_loads[i], *prev_loads[j]); });
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
      cur_list.push_back(&cur_frame.prims[cur_order[a]]);
    for (; b < prev_order.size() && prev_frame.prims[prev_order[b]].key == key; ++b)
      prev_list.push_back(&prev_frame.prims[prev_order[b]]);
    const int n_cur = static_cast<int>(cur_list.size()), n_prev = static_cast<int>(prev_list.size());
    if (cur_list[0]->pos_format != FORMAT_FLOAT || (n_cur == 1 && n_prev == 1))
    {
      for (int i = 0; i < std::min(n_cur, n_prev); ++i)
        set_partner(cur_list[i], prev_list[i]);
      continue;
    }
    // First vertex of each draw, decoded once.
    const auto first_vertices = [](const Frame& frame, const std::vector<const Prim*>& list,
                                   std::vector<std::array<float, 3>>* firsts) {
      firsts->resize(list.size());
      for (size_t i = 0; i < list.size(); ++i)
      {
        const u8* p = frame.stream.data() + list[i]->data_offset + list[i]->pos_offset;
        std::array<float, 3>& f = (*firsts)[i];
        f = {0.0f, 0.0f, 0.0f};
        for (u32 k = 0; k < list[i]->pos_count; ++k)
          f[k] = ReadFloat(p + k * 4);
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
    Align(n_cur, n_prev, distance, [&](int i, int j) { set_partner(cur_list[i], prev_list[j]); });
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
      if (!prev_taken[j] && prim.pos_format == FORMAT_FLOAT)
        lone_prev.emplace_back(shape(prim), j);
    }
    for (u32 i = 0; i < cur_frame.prims.size(); ++i)
    {
      const Prim& prim = cur_frame.prims[i];
      if (!partner[i] && prim.pos_format == FORMAT_FLOAT)
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
            float distance = 0.0f;
            for (u32 k = 0; k < prim.pos_count; ++k)
              distance += std::fabs(ReadFloat(cur + k * 4) - ReadFloat(prev + k * 4));
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
  // A released draw left without a partner jumped: its texture's group snaps, as when it was paired
  for (size_t index = 0; index < cur_frame.prims.size(); ++index)
  {
    if (cur_released[index] && !partner[index])
      plan->jumping_textures.push_back(cur_frame.prims[index].texture);
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
    const Prim& old = *partner[index];
    if (!same_layout(prim, old))
      continue;
    bool jump = false;
    if (prim.pos_format == FORMAT_FLOAT)
    {
      for (u32 v = 0; v < prim.num_vertices && !jump; ++v)
      {
        const u32 at = v * prim.vertex_size + prim.pos_offset;
        jump = VertexJumps(cur_frame.stream.data() + prim.data_offset + at,
                           prev_frame.stream.data() + old.data_offset + at, prim.pos_count);
      }
      if (jump)
        plan->jumping_textures.push_back(prim.texture);
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
  plan->total = cur_frame.loads.size() + cur_frame.prims.size();
  s_stat_loads += cur_frame.loads.size();
  s_stat_prims += cur_frame.prims.size();
  s_stat_draws += cur_frame.draws.size();
}

// The blend at t: the plan's matrices and positions written into a copy of the current stream. False
// when too few loads and draws have a partner in the previous frame to be worth it (a scene change).
bool ApplyPlan(const Frame& cur_frame, const Frame& prev_frame, const BlendPlan& plan, std::vector<u8>* out, float t)
{
  *out = cur_frame.stream;
  size_t paired = 0;
  for (const auto& [load_ptr, match_ptr] : plan.loads)
  {
    const Load& load = *load_ptr;
    const u8* cur = cur_frame.stream.data() + load.data_offset;
    const u8* prev = prev_frame.stream.data() + match_ptr->data_offset;
    ++paired;
    for (u32 i = 0; i < load.count; ++i)
    {
      // The projection type (the word after the six parameters) isn't a float.
      if (load.address + i >= XFMEM_SETPROJECTION + 6)
        continue;
      const float c = ReadFloat(cur + i * 4), p = ReadFloat(prev + i * 4);
      WriteFloat(out->data() + load.data_offset + i * 4, p + (c - p) * t);
    }
    KeepMatrixScale(load, cur, prev, t, out->data() + load.data_offset);
  }
  // Draws turning round in one frame (Collapses, at this t) snap with their texture's group too
  static std::vector<u64> collapsing;
  static std::vector<u8> collapsed;
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
      ++s_stat_collapsed;
    }
  }
  std::sort(collapsing.begin(), collapsing.end());
  const auto group_snaps = [&](u64 texture) {
    return std::binary_search(plan.jumping_textures.begin(), plan.jumping_textures.end(), texture) ||
           std::binary_search(collapsing.begin(), collapsing.end(), texture);
  };
  for (size_t i = 0; i < plan.prims.size(); ++i)
  {
    const BlendPlan::PrimPair& pair = plan.prims[i];
    const Prim& prim = cur_frame.prims[pair.index];
    const Prim& old = *pair.old;
    if (pair.jumps || collapsed[i] || group_snaps(prim.texture))
      continue;
    ++paired;
    ++s_stat_prims_blended;
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
    relevant = IsMatrixOrProjection(address + i);
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
  const auto add = [&](u8 array, u32 type, u32 direct, u32 span, u8 count) {
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
      frame.array_data.insert(frame.array_data.end(), data, data + bytes);
    }
  }
  draw.arrays_end = static_cast<u32>(frame.array_copies.size());
  frame.draws.push_back(draw);

  if (pos_type == 1 && num_vertices != 0)
  {
    frame.prims.push_back(Prim{static_cast<u32>(frame.stream.size() + 3), num_vertices,
                               static_cast<u16>(size / num_vertices), pos_offset,
                               static_cast<u8>(pos_format), static_cast<u8>(pos_count), key, texture,
                               s_in_dl ? s_dl_task : s_top_task});
  }
  // Not the vertex data itself: CPU-skinned models change it every frame.
  for (size_t index : s_unkeyed)
    frame.loads[index].key = key;
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
      std::memcpy(buffer + copy.lo, frame.array_data.data() + copy.data_offset, copy.size);
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
  if (s_on_game && s_recording && s_tasks_on)
    s_dl_tasks[base & 0x01FFFFFFu] = DrawTask();
}

void InterpDisplayListCall(u32 address, int depth, bool begin)
{
  if (depth != 0)
    return;
  s_in_dl = begin;
  s_dl_task = 0;
  if (!begin)
    return;
  const auto it = s_dl_tasks.find(address & 0x01FFFFFFu);
  if (it != s_dl_tasks.end())
    s_dl_task = it->second;
}

u32 InterpDrawTask()
{
  if (!s_on_game || !s_recording || !s_tasks_on)
    return 0;
  // At every 32-byte FIFO burst: while the same function (stack pointer, the two innermost return
  // addresses) is writing, the answer can't have changed
  static u32 last_sp = 0, last_ret = 0, last_ret2 = 0, last_running = 0, last_task = 0;
  const u32 sp = g_cpu.gpr[1];
  const u32 back = sp >= 0x80000000u && sp < 0x81800000u ? Read32(sp) : 0;
  const bool back_ok = back >= 0x80000000u && back < 0x81800000u;
  const u32 ret = back_ok ? Read32(back + 4) : 0;
  const u32 back2 = back_ok ? Read32(back) : 0;
  const u32 ret2 = back2 >= 0x80000000u && back2 < 0x81800000u ? Read32(back2 + 4) : 0;
  const u32 running = Read32(Tasks().running);
  if (sp != last_sp || ret != last_ret || ret2 != last_ret2 || running != last_running)
  {
    last_sp = sp;
    last_ret = ret;
    last_ret2 = ret2;
    last_running = running;
    last_task = DrawTask();
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
        "positions (%.1f blended, %.1f changed, %.2f paired across a texture change, %.2f snapped turning, %.2f matrices snapped turning, %.1f with their object, %.2f kept apart); ms per frame: keep real %.2f, blend %.2f, enter "
        "%.2f, feed %.2f, keep in-between %.2f, leave %.2f\n",
        s_stat_draws / 600.0, s_stat_loads / 600.0, s_stat_loads_blended / 600.0,
        s_stat_words_changed / 600.0, s_stat_prims / 600.0, s_stat_prims_blended / 600.0,
        s_stat_prims_changed / 600.0, s_stat_repaired / 600.0, s_stat_collapsed / 600.0, s_stat_matrix_snaps / 600.0,
        s_stat_tasked / 600.0, s_stat_kept_apart / 600.0, s_ms[0] / 600, s_ms[1] / 600, s_ms[2] / 600, s_ms[3] / 600,
        s_ms[4] / 600, s_ms[5] / 600);
  }
  s_stat_loads = s_stat_prims = s_stat_loads_blended = s_stat_prims_blended = s_stat_words_changed =
      s_stat_draws = s_stat_prims_changed = s_stat_repaired = s_stat_collapsed = s_stat_matrix_snaps =
      s_stat_tasked = s_stat_kept_apart = 0;
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
