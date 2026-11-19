// Output through GXRuntime's Aurora backend: the GX command stream, presentation at the game's
// display copy, controllers and sound. Headless builds and --headless runs keep the same entry
// points with no window.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dolphin_state.h"
#include "host.h"
#include "state.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifdef SKIES_STANDALONE_AURORA
#include "gxruntime/aurora_backend.h"
#include "gxruntime/platform.h"

#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_video.h>
#include <aurora/aurora.h>
#include <aurora/gfx.h>
#include <dolphin/gx.h>
#include <dolphin/vi.h>

#if defined(__ANDROID__)
#include <android/choreographer.h>
#include <android/looper.h>
#endif

extern bool aurora_debug_draw_log;  // GXRuntime: Aurora's per-draw log (AURORA_DRAW_LOG)

extern "C" {
void aurora_backend_present(void);
bool aurora_backend_should_quit(void);
void aurora_backend_write_raw(const void* data, u32 size);
bool aurora_backend_drain_if_over(u32 bytes);
int aurora_backend_fifo_over(u32 bytes);
void aurora_backend_set_guest_address_resolver(DolPlatformGuestAddressResolverFn resolve, void* user);
int aurora_backend_surface_size(unsigned* width, unsigned* height);
void aurora_backend_set_present_rect(int left, int top, int width, int height);
void aurora_backend_set_present_time(long long steady_ns);
void aurora_backend_set_present_callback(void (*callback)(void));
void aurora_backend_set_frame_buffer_aspect_fit(int fit);
void aurora_backend_present_kept(int slot);
void aurora_backend_synchronize(void);
void aurora_backend_set_event_poller(void (*poller)(void));
void aurora_backend_poll_events(void);
}
#endif

namespace host
{
namespace
{
bool s_active = false;
u64 s_frames = 0;
std::string s_dump_dir;
u64 s_dump_every = 0;
u64 s_dump_from = 0;  // SKIES_DUMP_FROM: first frame dumped
std::vector<u64> s_dump_frames;  // SKIES_DUMP_FRAMES=N,N,...: just these
std::vector<s16> s_audio;
double s_drain_ms = 0;  // SKIES_PRESENT_PACING: time this frame in aurora_backend_drain_if_over

// ---- the video thread ----------------------------------------------------------------------------
// The game (the CPU, the hardware models, the throttle) runs on a thread of its own (VideoRun); this
// one, the main thread, owns the window and every Aurora call, as Dolphin's dual core has its GPU
// thread: Aurora parses the GX stream here, the frame ends replay the 60 fps blend (interp60.cpp)
// and the presents wait here, beside the game instead of in its time. The game's thread hands over
// the stream in order with the host work that belongs between its commands (VideoPost: the frame
// ends, the retrace presents, the menu's overlay and settings), staged and passed on in chunks.
//
// It waits where the game would wait for the GPU: at a token or a draw done, until the video thread
// has parsed every command before it that reads guest RAM (VideoFence). Skies sets two tokens at the
// start of each frame and rebuilds its display lists once they are passed; without the wait Aurora
// read a display list the game had already rebuilt. Host work queued in between (presents) isn't
// waited for, and work that may run behind the game (the 60 fps replay) is held back until the game
// has moved on by a chunk of commands (VideoPostDeferred): the frame's four tokens come in a row, and
// a replay posted at the first would be waited for at the next three. It also waits for what needs
// Aurora caught up (VideoSync: save states), and to stay at most a frame ahead.
//
// The guest sees none of it: its GPU interrupts happen at the same guest time as before (hw_gpu.cpp),
// so runs stay the same. SKIES_VIDEO_THREAD=0: everything on the game's thread at once, in the same
// order (frame-exact comparisons: the pictures match either way).
bool s_threaded = false;
struct VideoItem
{
  std::vector<u8> bytes;       // GX commands
  std::function<void()> work;  // or host work
  bool stop = false;           // or the end: the game's thread is done
};
std::mutex s_queue_mutex;
std::condition_variable s_queue_items;  // the video thread waits for items
std::condition_variable s_queue_done;   // the game's thread waits for them done
std::deque<VideoItem> s_queue;
u64 s_posted = 0;                  // items posted (the game's thread)
u64 s_done = 0;                    // items done (the video thread; under the mutex)
size_t s_queued_bytes = 0;         // command bytes waiting
std::vector<std::vector<u8>> s_spare_buffers;  // command buffers to use again
std::vector<u8> s_stage;           // the game's thread: commands not yet posted
bool s_stage_reads_ram = false;    // some of them read guest RAM
u64 s_reads_ram_item = 0;          // the last item posted with such commands
std::vector<std::function<void()>> s_deferred;  // the game's thread: work for after the next wait
bool s_release_deferred = false;  // a wait passed: the deferred work goes after STAGE_BYTES more
size_t s_since_fence = 0;         // command bytes since
u64 s_previous_frame_end = 0;      // the item of the previous frame end
constexpr size_t STAGE_BYTES = 16 * 1024;
constexpr size_t MAX_QUEUED_BYTES = 8u << 20;  // a long load without frame ends
std::atomic<s64> s_busy_ns{0};  // the video thread's work so far (VideoBusy)
std::atomic<bool> s_quit{false};             // the window asked to close (from the video thread)
std::atomic<u32> s_surface_width{0}, s_surface_height{0};
std::recursive_mutex s_input_mutex;

// Phones clock a cluster by its cores' load, and the video thread alone, busy a quarter of the time, left
// the Pocket 5's gold cores at 1.3 of 2.4 GHz: it parsed at half the game thread's speed and the game
// waited ~2 ms a frame for it at the tokens (uncapped save point 1.64x; 1.77x with its cores at full
// clock). So while the game has no time to spare (its throttle slept under 15% of the last 30 frames,
// or it runs uncapped), the video thread polls for work instead of sleeping, which keeps its cores'
// clock up; with time to spare it sleeps (the power the full-speed game is played at).
// SKIES_VIDEO_SPIN=auto (Android's default) | 1 | 0 (the desktop's: its cores clock up on their own).
std::atomic<bool> s_spin_now{false};
std::atomic<u64> s_posted_count{0};  // s_posted, for the video thread's polling
u64 s_taken = 0;                     // the video thread: items taken

bool VideoStats();

void UpdateSpin()
{
  static const int mode = [] {
    const char* v = std::getenv("SKIES_VIDEO_SPIN");
    const std::string m = v ? v : "";
#if defined(__ANDROID__)
    const int platform = -1;
#else
    const int platform = 0;
#endif
    return m == "auto" ? -1 : m.empty() ? platform : std::atoi(m.c_str()) != 0 ? 1 : 0;
  }();
  if (mode >= 0)
  {
    s_spin_now.store(mode == 1, std::memory_order_relaxed);
    return;
  }
  static u32 frames = 0;
  static std::chrono::steady_clock::time_point since = std::chrono::steady_clock::now();
  static std::chrono::steady_clock::duration idle_since = ThrottleIdle();
  if (++frames < 30)
    return;
  const auto now = std::chrono::steady_clock::now();
  const double wall = std::chrono::duration<double>(now - since).count();
  const double idle = std::chrono::duration<double>(ThrottleIdle() - idle_since).count();
  frames = 0;
  since = now;
  idle_since = ThrottleIdle();
  const bool busy = !Throttled() || (wall > 0 && idle < 0.15 * wall);
  if (busy != s_spin_now.load(std::memory_order_relaxed))
  {
    s_spin_now.store(busy, std::memory_order_relaxed);
    if (VideoStats())
      Log("[video] video thread %s (the game slept %.0f%% of the last 30 frames)\n",
          busy ? "polls" : "sleeps", wall > 0 ? 100.0 * idle / wall : 0.0);
  }
}

u64 Push(VideoItem&& item)
{
  std::lock_guard lock(s_queue_mutex);
  s_queued_bytes += item.bytes.size();
  s_queue.push_back(std::move(item));
  s_queue_items.notify_one();
  s_posted_count.store(s_posted + 1, std::memory_order_release);
  return ++s_posted;
}

// SKIES_VIDEO_STATS=1: every 600 frames, the game's thread's waits and the video thread's work (read
// at first use: Android sets the environment in main, after static initialisation)
bool VideoStats()
{
  static const bool on = std::getenv("SKIES_VIDEO_STATS") != nullptr;
  return on;
}
std::chrono::steady_clock::duration s_waited[3]{};  // fences, frame ends, full queue (the game's thread)

void WaitDone(u64 item, int why)
{
  std::unique_lock lock(s_queue_mutex);
  if (s_done >= item)
    return;
  const auto start = std::chrono::steady_clock::now();
  s_queue_done.wait(lock, [item] { return s_done >= item; });
  s_waited[why] += std::chrono::steady_clock::now() - start;
}

// The staged commands to the video thread
void FlushStage()
{
  if (s_stage.empty())
    return;
  VideoItem item;
  item.bytes.swap(s_stage);
  const u64 posted = Push(std::move(item));
  if (s_stage_reads_ram)
    s_reads_ram_item = posted;
  s_stage_reads_ram = false;
  std::unique_lock lock(s_queue_mutex);
  if (!s_spare_buffers.empty())
  {
    s_stage.swap(s_spare_buffers.back());
    s_spare_buffers.pop_back();
  }
  if (s_queued_bytes > MAX_QUEUED_BYTES)
  {
    const auto start = std::chrono::steady_clock::now();
    s_queue_done.wait(lock, [] { return s_queued_bytes <= MAX_QUEUED_BYTES; });
    s_waited[2] += std::chrono::steady_clock::now() - start;
  }
}

#ifdef SKIES_STANDALONE_AURORA
void UpdateSurfaceSize()
{
  unsigned w = 0, h = 0;
  if (!aurora_backend_surface_size(&w, &h))
    w = h = 0;
  s_surface_width.store(w, std::memory_order_relaxed);
  s_surface_height.store(h, std::memory_order_relaxed);
}

// The window's events, polled by the present where it would (fork 0046), under the input lock
void PollEvents()
{
  std::lock_guard lock(s_input_mutex);
  aurora_backend_poll_events();
  s_quit.store(aurora_backend_should_quit(), std::memory_order_relaxed);
}
#endif

// SKIES_DRAIN=N: let Aurora's parsing fall at most N bytes behind (debug). The game's thread lets it
// gather 64 KB at a time; the video thread parses each chunk as it comes, so the waits at the tokens
// find little left to parse.
u32 DrainBytes()
{
  static const u32 drain = [] {
    const char* v = std::getenv("SKIES_DRAIN");
    return v ? static_cast<u32>(std::strtoul(v, nullptr, 0)) : s_threaded ? 0u : 64u * 1024;
  }();
  return drain;
}

// The deferred work to the queue (or done, without a video thread)
void PostDeferred()
{
  s_release_deferred = false;
  std::vector<std::function<void()>> work;
  work.swap(s_deferred);
  for (auto& w : work)
    VideoPost(std::move(w));
}

void ParseCommands(const u8* data, u32 size);

// The video thread until the game's thread is done
void VideoLoop()
{
  for (;;)
  {
    VideoItem item;
    if (s_spin_now.load(std::memory_order_relaxed))
    {
      // Polling for the next item, at most a while (the game may be loading or quitting)
      const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
      while (s_posted_count.load(std::memory_order_acquire) == s_taken &&
             std::chrono::steady_clock::now() < until)
        std::this_thread::yield();
    }
    {
      std::unique_lock lock(s_queue_mutex);
      s_queue_items.wait(lock, [] { return !s_queue.empty(); });
      item = std::move(s_queue.front());
      s_queue.pop_front();
      s_queued_bytes -= item.bytes.size();
      ++s_taken;
    }
    if (!item.stop)
    {
      const auto start = std::chrono::steady_clock::now();
      const auto presenting = VideoPresentTime();
      if (!item.bytes.empty())
        ParseCommands(item.bytes.data(), static_cast<u32>(item.bytes.size()));
      else if (item.work)
        item.work();
      s_busy_ns.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(
                              std::chrono::steady_clock::now() - start - (VideoPresentTime() - presenting))
                              .count(),
                          std::memory_order_relaxed);
    }
    {
      std::lock_guard lock(s_queue_mutex);
      if (item.bytes.capacity() != 0)
      {
        item.bytes.clear();
        s_spare_buffers.push_back(std::move(item.bytes));
      }
      ++s_done;
    }
    s_queue_done.notify_all();
    if (item.stop)
      return;
  }
}

#ifdef SKIES_STANDALONE_AURORA
// The picture's aspect: the widescreen game patch keeps its target in ws_config at the start of its
// data section (patches/game/cave.ld, widescreen.c: magic 'WSCF', then display_aspect; 0 = 4:3).
// It defaults to 16:9 and nothing here writes it, so a ws DOL shows 16:9 and the plain DOL 4:3.
constexpr u32 WS_CONFIG = 0x80002A00;
double PictureAspect()
{
  if (Read32(WS_CONFIG) != 0x57534346)
    return 4.0 / 3.0;
  const u32 bits = Read32(WS_CONFIG + 8);
  float aspect;
  std::memcpy(&aspect, &bits, sizeof(aspect));
  return aspect > 0.5f && aspect < 8.0f ? aspect : 4.0 / 3.0;
}

bool ResolveGuestAddress(void*, u32 address, u32, DolGuestAddressSpace, DolGuestResourceKind,
                         const void** data, u32* available)
{
  const u32 physical = address & 0x1FFFFFFFu;
  if (physical >= RAM_SIZE)
  {
    static int s_logged = 0;
    if (s_logged < 8)
    {
      ++s_logged;
      Log("[video] unresolved guest address %08x\n", address);
    }
    return false;
  }
  *data = g_ram + physical;
  *available = RAM_SIZE - physical;
  return true;
}

// 60 fps (interp60.cpp): Aurora keeps each frame's real picture in slot frame % 4 * 2 and the
// in-between one leading up to it in the slot after; the retraces present them (s_retrace_presents):
// the first retrace after the frame's XFB copy the in-between picture (the real one when there is
// none), the next the real one, as the Dolphin build's single-core order. Counted from the copy, as
// the 30 fps presents are, not from the VI's flip to the frame's XFB: the game flips a field late
// now and then, and a field then went without a picture (about 15 intervals of 33 ms in 20 s on
// Windows where the game itself takes a third field 2-3 times).
struct Shown
{
  u32 address;  // the frame's XFB, physical
  u64 frame;
  bool interp;
  u32 retraces = 0;
};
std::deque<Shown> s_shown;
std::atomic<bool> s_retrace_presents{false};  // the video thread's (and the render worker's)
bool s_retrace_presents_wanted = false;  // the game's thread: 60 fps on at the last frame end
// SKIES_INTERP_TRACE=1: each frame end and retrace present in the log (read at first use, as above)
bool InterpTrace()
{
  static const bool on = std::getenv("SKIES_INTERP_TRACE") != nullptr;
  return on;
}

int RealSlot(u64 frame)
{
  return static_cast<int>(frame % 4) * 2;
}

// The time the video thread spent in Aurora's present calls, which wait for the render worker
// when it holds the previous picture until its scheduled time: not work (interp60.cpp's budget)
std::atomic<s64> s_present_ns{0};

void Present(int slot)
{
  const auto start = std::chrono::steady_clock::now();
  if (slot < 0)
    aurora_backend_present();
  else
    aurora_backend_present_kept(slot);
  s_present_ns.fetch_add(
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count(),
      std::memory_order_relaxed);
  UpdateSurfaceSize();
}

// On the render worker after each present: the interval between presents, as the Dolphin build's
// [present-pacing] (>20 / >25 ms counts with 60 fps; Skies presents 30 frames a second otherwise, so
// then >40 / >50 ms)
void PresentDone()
{
  static std::chrono::steady_clock::time_point last{};
  static PacingWindow window;
  const auto now = std::chrono::steady_clock::now();
  const bool sixty = s_retrace_presents.load(std::memory_order_relaxed);
  if (last.time_since_epoch().count() != 0)
    PacingAdd(window, std::chrono::duration<double, std::milli>(now - last).count(), "present-pacing", "interval",
              sixty ? 20.0 : 40.0, sixty ? 25.0 : 50.0);
  last = now;
}

#if defined(__ANDROID__)
// The display's refresh grid, from Choreographer's frame callbacks on a thread of their own (their
// timestamps are the app's vsync on CLOCK_MONOTONIC, steady_clock here). The period is measured: the
// span since the first callback over the refreshes in it, refreshed as callbacks come; a callback off
// the grid (a new refresh rate, a long gap) starts the measurement again.
std::atomic<s64> s_vsync_last_ns{0};
std::atomic<s64> s_vsync_period_ns{0};

void VsyncFrame(long frame_ns, void*);

void VsyncPost()
{
  AChoreographer_postFrameCallback(AChoreographer_getInstance(), VsyncFrame, nullptr);
}

void VsyncFrame(long frame_ns, void*)
{
  static s64 s_first = 0, s_last = 0, s_period = 0;
  const s64 now = frame_ns;
  const s64 since = now - s_last;
  if (s_last == 0 || since <= 0 || since > 200'000'000)
  {
    s_first = now;
    s_period = 0;
  }
  else if (s_period == 0)
  {
    s_period = since;  // a first guess: one refresh, or a few when the thread was late
  }
  else
  {
    const double refreshes = static_cast<double>(since) / static_cast<double>(s_period);
    if (std::abs(refreshes - std::round(refreshes)) > 0.15 || std::round(refreshes) < 1)
    {
      s_first = now;  // off the grid: measure again
      s_period = since;
    }
    else
    {
      const s64 span = now - s_first;
      const double n = std::round(static_cast<double>(span) / static_cast<double>(s_period));
      if (n >= 1)
        s_period = static_cast<s64>(static_cast<double>(span) / n);
    }
  }
  s_last = now;
  s_vsync_last_ns.store(now, std::memory_order_relaxed);
  // Usable once it has a second of refreshes behind it
  s_vsync_period_ns.store(now - s_first >= 1'000'000'000 ? s_period : 0, std::memory_order_relaxed);
  VsyncPost();
}

void VsyncGridStart()
{
  static bool s_started = false;
  if (s_started)
    return;
  s_started = true;
  std::thread([] {
    ALooper_prepare(0);
    VsyncPost();
    while (true)
      ALooper_pollOnce(-1, nullptr, nullptr, nullptr);
  }).detach();
}

// A present time moved to a refresh: Choreographer's vsync plus SKIES_VSYNC_OFFSET_MS (10), so a frame always
// reaches the compositor at the same point of a refresh. Unsnapped, present times that drift across the
// compositor's latch flipped frames between two and three refreshes for seconds at a time (on the Pocket 5
// ~28 frames of 50 ms a minute at 30 fps). On the Pocket 5 offsets 4-12 ms are clean, 0 and 15 not (the GPU
// takes 7-15 ms from the present call). The present times themselves wobble by a few ms (a copy's guest
// time follows the game's work; the auto delay moves), so a frame keeps the last one's refresh plus the
// refreshes between their times, and is snapped again only when that slot ends up more than 5 ms before its
// time or a refresh and 5 ms after it. With the game paced to the display (main.cpp FollowDisplay) that
// leaves 2-3 frames a minute a refresh longer. SKIES_VSYNC_SNAP=0 turns it off.
s64 SnapToRefresh(s64 steady_ns)
{
  static s64 s_last_time = 0, s_last_slot = 0;
  static const bool s_on = [] {
    const char* v = std::getenv("SKIES_VSYNC_SNAP");
    return !v || std::atoi(v) != 0;
  }();
  static const s64 s_offset = [] {
    const char* v = std::getenv("SKIES_VSYNC_OFFSET_MS");
    return static_cast<s64>((v ? std::atof(v) : 10.0) * 1e6);
  }();
  const s64 period = s_vsync_period_ns.load(std::memory_order_relaxed);
  if (!s_on || period == 0)
    return steady_ns;
  constexpr s64 SLACK = 5'000'000;
  if (s_last_slot != 0)
  {
    const s64 refreshes = (steady_ns - s_last_time + period / 2) / period;
    const s64 slot = s_last_slot + refreshes * period;
    if (refreshes >= 1 && slot >= steady_ns - SLACK && slot <= steady_ns + period + SLACK)
    {
      s_last_time = steady_ns;
      s_last_slot = slot;
      return slot;
    }
  }
  const s64 base = s_vsync_last_ns.load(std::memory_order_relaxed) + s_offset;
  const s64 refreshes = steady_ns > base ? (steady_ns - base + period - 1) / period : 0;
  if (PacingOn() && s_last_slot != 0)
  {
    const s64 kept = s_last_slot + (steady_ns - s_last_time + period / 2) / period * period;
    Log("[video] refresh slot again: the kept one was %.1f ms from the present time (%.1f ms since the last)\n",
        (kept - steady_ns) / 1e6, (steady_ns - s_last_time) / 1e6);
  }
  s_last_time = steady_ns;
  s_last_slot = base + refreshes * period;
  return s_last_slot;
}
#endif

// Presents on the throttle's clock: the render worker presents each frame at its copy's guest time plus a
// delay instead of when the emulation thread got there, so the intervals keep the game's rhythm while the
// threads' own timing wobbles (on the Pocket 5 the copy comes 6-11 ms after its guest time, p99 up to 18:
// the drawing field runs slower than real time and the next one catches up; Windows 0.7-1.5 ms, p99 6-8).
// SKIES_PRESENT_DELAY_MS=auto (the default) keeps the delay at the last 300 copies' 99th percentile lateness
// + 1.5 ms (at most 30), N fixes it, off presents at once. Hitches over 50 ms (state loads) are left out;
// unthrottled runs present at once. `wall` is the copy's (or the retrace's) guest time on the throttle's
// clock (WallForGuest, from the game's thread); how late it is, is how late the video thread got here.
void SchedulePresent(std::chrono::steady_clock::time_point wall)
{
  static const double s_mode = [] {  // -2 auto, -1 off, else fixed ms
    const char* v = std::getenv("SKIES_PRESENT_DELAY_MS");
    const std::string mode = v ? v : "auto";
    return mode == "auto" ? -2.0 : mode == "off" ? -1.0 : std::atof(mode.c_str());
  }();
  if (wall.time_since_epoch().count() == 0)
    return;
  const double late = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wall).count();
  if (PacingOn())
  {
    static PacingWindow window;
    PacingAdd(window, late, "present-late", "copy after its guest time", 5.0, 10.0);
  }
  if (s_mode == -1.0 || late > 50.0)
    return;
  double delay = s_mode;
  if (s_mode == -2.0)
  {
    constexpr unsigned HISTORY = 300;
    static double s_late[HISTORY];
    static unsigned s_count = 0;
    static double s_delay = 20.0;  // from above: a delay that grows makes late presents, one that shrinks doesn't
    s_late[s_count++ % HISTORY] = late;
    if (s_count >= 60 && s_count % 30 == 0)
    {
      const unsigned n = std::min(s_count, HISTORY);
      std::vector<double> sorted(s_late, s_late + n);
      const size_t at = static_cast<size_t>(n * 0.99);
      std::nth_element(sorted.begin(), sorted.begin() + at, sorted.end());
      const double next = std::clamp(sorted[at] + 1.5, 0.0, 30.0);
      if (std::abs(next - s_delay) >= 1.0 && PacingOn())
        Log("[video] present delay %.1f ms\n", next);
      s_delay = next;
    }
    delay = s_delay;
  }
  s64 at = std::chrono::duration_cast<std::chrono::nanoseconds>(
               (wall + std::chrono::microseconds(static_cast<s64>(delay * 1000))).time_since_epoch())
               .count();
#if defined(__ANDROID__)
  at = SnapToRefresh(at);
#endif
  aurora_backend_set_present_time(at);
}

// The picture centred in the window at PictureAspect (Dolphin's aspect ratio letterbox), from the
// game's thread when it changes.
void ApplyPresentRect()
{
  unsigned width = 0, height = 0;
  if (!VideoSurfaceSize(&width, &height) || width == 0 || height == 0)
    return;
  const double aspect = PictureAspect();
  int w = static_cast<int>(width), h = static_cast<int>(height);
  if (static_cast<double>(width) / height > aspect)
    w = static_cast<int>(height * aspect + 0.5);
  else
    h = static_cast<int>(width / aspect + 0.5);
  const int left = (static_cast<int>(width) - w) / 2, top = (static_cast<int>(height) - h) / 2;
  static int s_applied[4] = {-1, -1, -1, -1};
  if (s_applied[0] == left && s_applied[1] == top && s_applied[2] == w && s_applied[3] == h)
    return;
  s_applied[0] = left;
  s_applied[1] = top;
  s_applied[2] = w;
  s_applied[3] = h;
  VideoPost([=] { aurora_backend_set_present_rect(left, top, w, h); });
}

bool DumpWanted(u64 frame)
{
  if (!s_dump_frames.empty())
    return std::find(s_dump_frames.begin(), s_dump_frames.end(), frame) != s_dump_frames.end();
  return s_dump_every != 0 && frame >= s_dump_from && (frame % s_dump_every) == 0;
}

void DumpFrame(u64 frame, const char* suffix)
{
  const u8* rgba = nullptr;
  u32 width = 0, height = 0;
  for (int attempt = 0; attempt < 20000; ++attempt)
  {
    if (aurora_take_framebuffer_readback(&rgba, &width, &height))
      break;
    aurora_pump_framebuffer_readback();
    std::this_thread::sleep_for(std::chrono::microseconds(250));
    rgba = nullptr;
  }
  if (!rgba)
  {
    Log("[video] frame %llu: readback never completed\n", static_cast<unsigned long long>(frame));
    return;
  }
  char path[512];
  std::snprintf(path, sizeof(path), "%s/frame_%06llu%s.ppm", s_dump_dir.c_str(),
                static_cast<unsigned long long>(frame), suffix);
  FILE* f = std::fopen(path, "wb");
  if (!f)
    return;
  std::fprintf(f, "P6\n%u %u\n255\n", width, height);
  std::vector<u8> rgb(static_cast<size_t>(width) * height * 3);
  for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i)
  {
    rgb[i * 3] = rgba[i * 4];
    rgb[i * 3 + 1] = rgba[i * 4 + 1];
    rgb[i * 3 + 2] = rgba[i * 4 + 2];
  }
  std::fwrite(rgb.data(), 1, rgb.size(), f);
  std::fclose(f);
}

// Aurora's window (the only one)
SDL_Window* MainWindow()
{
  int count = 0;
  SDL_Window** windows = SDL_GetWindows(&count);
  SDL_Window* window = windows && count > 0 ? windows[0] : nullptr;
  SDL_free(windows);
  return window;
}

// SKIES_RESIZE_AT=SECONDS:WxH[,SECONDS:WxH...]: the window resized at those guest times (tests: a size
// change keeps the EFB copies)
void ApplyTestResize()
{
  struct Resize
  {
    double seconds;
    int width, height;
  };
  static std::vector<Resize> s_resizes = [] {
    std::vector<Resize> list;
    if (const char* v = std::getenv("SKIES_RESIZE_AT"))
      for (const char* p = v; *p;)
      {
        Resize r{};
        int used = 0;
        if (std::sscanf(p, "%lf:%dx%d%n", &r.seconds, &r.width, &r.height, &used) != 3)
        {
          Log("[video] SKIES_RESIZE_AT=SECONDS:WxH[,...], not '%s'\n", v);
          break;
        }
        list.push_back(r);
        p += used;
        if (*p == ',')
          ++p;
      }
    return list;
  }();
  const double now = static_cast<double>(Now()) / CPU_HZ;
  while (!s_resizes.empty() && now >= s_resizes.front().seconds)
  {
    const Resize r = s_resizes.front();
    s_resizes.erase(s_resizes.begin());
    Log("[video] resizing the window to %dx%d at %.2f s guest\n", r.width, r.height, now);
    VideoPost([r] {
      if (SDL_Window* window = MainWindow())
        SDL_SetWindowSize(window, r.width, r.height);
    });
  }
}
#endif
}  // namespace

void VideoSetTitleFps(double fps)
{
#ifdef SKIES_STANDALONE_AURORA
  if (!g_window.fps_in_title || !s_active)
    return;
  // As the Dolphin build's runner: "<title> | 29.9 FPS"
  char text[32];
  std::snprintf(text, sizeof(text), " | %.1f FPS", std::isfinite(fps) && fps > 0 ? fps : 0.0);
  std::string title = g_window.title + text;
  VideoPost([title = std::move(title)] {
    if (SDL_Window* window = MainWindow())
      SDL_SetWindowTitle(window, title.c_str());
  });
#else
  (void)fps;
#endif
}

bool VideoInit(bool headless)
{
  if (const char* dir = std::getenv("SKIES_DUMP_DIR"))
    s_dump_dir = dir;
  s_dump_every = s_dump_dir.empty() ? 0 : 60;
  if (const char* every = std::getenv("SKIES_DUMP_EVERY"))
    s_dump_every = std::strtoull(every, nullptr, 0);
  if (const char* from = std::getenv("SKIES_DUMP_FROM"))
    s_dump_from = std::strtoull(from, nullptr, 0);
  if (const char* list = std::getenv("SKIES_DUMP_FRAMES"))
  {
    for (const char* p = list; *p;)
    {
      char* end = nullptr;
      s_dump_frames.push_back(std::strtoull(p, &end, 0));
      p = *end ? end + 1 : end;
    }
    s_dump_every = 1;
  }
#ifdef SKIES_STANDALONE_AURORA
  if (headless)
    return true;
  // Aurora's controller and keyboard bindings (keyboard_bindings.dat and the controller
  // mappings) in USER/Config/Aurora beside the Dolphin build's GCPadNew.ini; its shader caches stay
  // in the system's per-user app folder.
  static std::string s_aurora_user;
  if (!SettingsConfigDir().empty())
  {
    s_aurora_user = (std::filesystem::path(SettingsConfigDir()) / "Aurora").string();
    std::error_code ec;
    std::filesystem::create_directories(s_aurora_user, ec);
  }
  const char* w = std::getenv("SKIES_WIDTH");
  const char* h = std::getenv("SKIES_HEIGHT");
  // The DOL is in memory by now, so the window can open at the picture's shape (960 lines high).
  const AuroraBackendConfig config = {
      .app_name = g_window.title.c_str(),
      .window_width = w ? static_cast<unsigned>(std::atoi(w))
                        : static_cast<unsigned>(960.0 * PictureAspect() + 0.5),
      .window_height = h ? static_cast<unsigned>(std::atoi(h)) : 960u,
      .vsync = std::getenv("SKIES_VSYNC") != nullptr ||
               SettingGet(SettingFile::Gfx, "Hardware", "VSync", 0) != 0,
      .allow_texture_dumps = false,
      .info_logging = std::getenv("SKIES_AURORA_LOG") != nullptr,
      .graphics_logging = std::getenv("SKIES_AURORA_LOG") != nullptr,
      .force_untextured = false,
      .native_window = nullptr,
      .native_window_width = 0,
      .native_window_height = 0,
      .native_window_scale = 0.f,
      .user_path = s_aurora_user.empty() ? nullptr : s_aurora_user.c_str(),
      .cache_path = nullptr,
  };
  char arg0[] = "skies-standalone";
  char* argv[] = {arg0, nullptr};
  if (!dol_aurora_initialize(1, argv, &config))
  {
    Log("[video] Aurora could not initialize its window or GPU device\n");
    return false;
  }
  // No graphics API started (Aurora's last resort is Dawn's Null backend, which draws nothing): say so rather
  // than run behind a black window. DOL_AURORA_BACKEND=null shows it.
  if (aurora_get_backend() == BACKEND_NULL)
  {
    dol_aurora_shutdown();
#if defined(_WIN32)
    const char* apis = "Direct3D 12, Direct3D 11 or Vulkan";
#elif defined(__APPLE__)
    const char* apis = "Metal";
#else
    const char* apis = "Vulkan or OpenGL ES";
#endif
    FrontendError("No graphics support", std::string("The game couldn't start ") + apis +
                                             " on this device. Updating the graphics driver usually fixes this.");
    return false;
  }
  aurora_backend_set_guest_address_resolver(ResolveGuestAddress, nullptr);
  // config.ini's Screen (the launcher's Fullscreen)
  if (g_window.fullscreen)
    if (SDL_Window* window = MainWindow())
      SDL_SetWindowFullscreen(window, true);
  if (PacingOn())
    aurora_backend_set_present_callback(PresentDone);
#if defined(__ANDROID__)
  VsyncGridStart();
#endif
  // As the Dolphin-side backend (ApplyEfbScale): the EFB keeps 640x480 times the scale whatever the
  // window's shape (the ws patch draws anamorphic) and the picture is stretched at the present.
  // The scale is the Resolution setting (menu.cpp; SKIES_EFB_SCALE=N overrides it), 0 = the window's.
  aurora_backend_set_frame_buffer_aspect_fit(1);
  // SDK baseline before the game's own GXInit, as the Dolphin-side backend does.
  static std::vector<u8> s_gx_fifo(64 * 1024);
  GXInit(s_gx_fifo.data(), static_cast<u32>(s_gx_fifo.size()));
  GXFlush();
  dol_platform_pad_init();
  dol_platform_audio_set_sample_rate(32000);
  s_active = true;
  const char* threaded = std::getenv("SKIES_VIDEO_THREAD");
  s_threaded = !threaded || std::atoi(threaded) != 0;
  if (s_threaded)
    aurora_backend_set_event_poller(PollEvents);
  Log("[video] Aurora on %s\n", s_threaded ? "the video thread" : "the game's thread");
  UpdateSurfaceSize();
  MenuApplyGame();
  return true;
#else
  (void)headless;
  return true;
#endif
}

void WavClose();

void VideoShutdown()
{
  WavClose();
#ifdef SKIES_STANDALONE_AURORA
  if (s_active)
    dol_aurora_shutdown();
#endif
  s_active = false;
}

namespace
{
// Commands into Aurora, parsed as they come (at most DrainBytes behind)
void ParseCommands(const u8* data, u32 size)
{
#ifdef SKIES_STANDALONE_AURORA
  aurora_backend_write_raw(data, size);
  const u32 drain = DrainBytes();
  // Timed only when it drains: a clock read per command chunk cost the Pocket 5 15% of its speed
  if (PacingOn() && aurora_backend_fifo_over(drain))
  {
    const auto t0 = std::chrono::steady_clock::now();
    aurora_backend_drain_if_over(drain);
    s_drain_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  }
  else
    aurora_backend_drain_if_over(drain);
#else
  (void)data;
  (void)size;
#endif
}
}  // namespace

void VideoCommands(const u8* data, u32 size, bool reads_ram)
{
  if (!s_active)
    return;
  if (!s_threaded)
    ParseCommands(data, size);
  else
  {
    s_stage.insert(s_stage.end(), data, data + size);
    s_stage_reads_ram |= reads_ram;
    if (s_stage.size() >= STAGE_BYTES)
      FlushStage();
  }
  s_since_fence += size;
  if (s_release_deferred && s_since_fence >= STAGE_BYTES)
    PostDeferred();
}

void VideoPostDeferred(std::function<void()> work)
{
  s_deferred.push_back(std::move(work));
}

void VideoFence()
{
  if (s_threaded)
  {
    if (s_stage_reads_ram)
      FlushStage();
    WaitDone(s_reads_ram_item, 0);
  }
  s_release_deferred = !s_deferred.empty();
  s_since_fence = 0;
}

void VideoPost(std::function<void()> work)
{
  if (!s_threaded)
  {
    work();
    return;
  }
  FlushStage();
  VideoItem item;
  item.work = std::move(work);
  Push(std::move(item));
}

void VideoSync(const std::function<void()>& work)
{
  PostDeferred();
  if (!s_threaded)
  {
    work();
    return;
  }
  FlushStage();
  VideoItem item;
  item.work = work;
  WaitDone(Push(std::move(item)), 1);
}

std::unique_lock<std::recursive_mutex> VideoInputLock()
{
  return std::unique_lock(s_input_mutex);
}

bool VideoThreaded()
{
  return s_threaded;
}

std::chrono::steady_clock::duration VideoBusy()
{
  return std::chrono::nanoseconds(s_busy_ns.load(std::memory_order_relaxed));
}

void VideoRun(const std::function<void()>& game)
{
#ifdef SKIES_STANDALONE_AURORA
  if (s_threaded)
  {
    // The game's thread: recompiled guest calls nest on the host stack, so a large one
    struct Run
    {
      const std::function<void()>* game;
    } run{&game};
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER,
                           reinterpret_cast<void*>(+[](void* data) -> int {
                             (*static_cast<Run*>(data)->game)();
                             FlushStage();
                             VideoItem stop;
                             stop.stop = true;
                             Push(std::move(stop));
                             return 0;
                           }));
    SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, "Skies game");
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, &run);
    SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, 16 << 20);
    SDL_Thread* thread = SDL_CreateThreadWithProperties(props);
    SDL_DestroyProperties(props);
    if (thread)
    {
      PinVideoThread();
      VideoLoop();
      SDL_WaitThread(thread, nullptr);
      return;
    }
    Log("[video] no game thread (%s): everything on this one\n", SDL_GetError());
    s_threaded = false;
    aurora_backend_set_event_poller(nullptr);
  }
#endif
  game();
}

// SKIES_POKE=ADDR=WORDS[;ADDR=WORDS...] (hex; WORDS: 8 hex digits each, big-endian guest words)
// written to RAM at every frame, as a frontend writes the game patch's input blocks every poll
// (e.g. qol_input at 80002EA0: 'QOLC', encounter percent, ...). An entry as @SECONDS:ADDR=WORDS is
// written once, at the first frame end at or after that guest time (tests: a map change is
// "@110:80305CF0=6D65323938612E7363740000;@110:80311AEC=0000000F", the script name then field state
// 0xF, USA addresses).
void ApplyPokes()
{
  struct Poke
  {
    u32 address;
    std::vector<u32> words;
    double at = -1;  // once, at this guest time; < 0: every frame
    bool done = false;
  };
  static std::vector<Poke> pokes = [] {
    std::vector<Poke> list;
    const char* spec = std::getenv("SKIES_POKE");
    for (std::string rest = spec ? spec : ""; !rest.empty();)
    {
      const size_t semi = rest.find(';');
      std::string one = rest.substr(0, semi);
      rest = semi == std::string::npos ? "" : rest.substr(semi + 1);
      double at = -1;
      if (!one.empty() && one[0] == '@')
      {
        const size_t colon = one.find(':');
        if (colon == std::string::npos)
          continue;
        at = std::strtod(one.c_str() + 1, nullptr);
        one = one.substr(colon + 1);
      }
      const size_t eq = one.find('=');
      if (eq == std::string::npos)
        continue;
      Poke p{static_cast<u32>(std::strtoul(one.substr(0, eq).c_str(), nullptr, 16)), {}, at};
      const std::string hex = one.substr(eq + 1);
      for (size_t i = 0; i + 8 <= hex.size(); i += 8)
        p.words.push_back(static_cast<u32>(std::strtoul(hex.substr(i, 8).c_str(), nullptr, 16)));
      list.push_back(std::move(p));
    }
    return list;
  }();
  const double now = static_cast<double>(Now()) / CPU_HZ;
  for (Poke& p : pokes)
  {
    if (p.at >= 0)
    {
      if (p.done || now < p.at)
        continue;
      p.done = true;
      Log("[video] poke %08x (%zu words) at %.3f s guest\n", p.address, p.words.size(), now);
    }
    for (size_t i = 0; i < p.words.size(); ++i)
      Write32(p.address + static_cast<u32>(4 * i), p.words[i]);
  }
}

#ifdef SKIES_STANDALONE_AURORA
static void ArmDrawLog(u64 frame);

namespace
{
// The frame end on the video thread: the 60 fps replay, the present
struct FrameEnd
{
  u64 frame;
  std::chrono::steady_clock::time_point wall;  // the copy's guest time on the throttle's clock
  InterpHandoff interp;
  u32 xfb, vi_xfb;  // for SKIES_INTERP_TRACE
};

void FrameEndVideo(const FrameEnd& end)
{
  if ((end.frame % 600) == 1)
    HdTexturesLog();
  ArmDrawLog(end.frame);
  // 60 fps: the real picture kept here, the in-between one by the replay after the frame's tokens
  // (ReplayVideo), both presented from the retraces
  if (InterpFrameEndVideo(end.interp, RealSlot(end.frame)) >= 0)
  {
    s_shown.push_back(Shown{end.xfb, end.frame, false});
    if (InterpTrace())
      Log("[interp-trace] frame %llu end: xfb %08x; VI shows %08x\n", static_cast<unsigned long long>(end.frame),
          end.xfb, end.vi_xfb);
    while (s_shown.size() > 3)
      s_shown.pop_front();
    s_retrace_presents = true;
    return;
  }
  s_retrace_presents = false;
  s_shown.clear();
  const bool dump = DumpWanted(end.frame);
  if (dump)
  {
    // As ReplayVideo: the readback goes with the next submitted present, so the frame before (still
    // queued when presents are scheduled, as on Android) must be out first, or its picture gets this
    // frame's number
    aurora_backend_synchronize();
    aurora_request_framebuffer_readback();
  }
  SchedulePresent(end.wall);
  if (PacingOn())
  {
    const auto now = std::chrono::steady_clock::now();
    static PacingWindow drains;
    PacingAdd(drains, s_drain_ms, "present-drain", "in drains per frame", 2.0, 8.0);
    s_drain_ms = 0;
    static PacingWindow call;
    Present(-1);
    PacingAdd(call, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - now).count(),
              "present-call", "in aurora_backend_present", 2.0, 8.0);
  }
  else
    Present(-1);
  if (dump)
    DumpFrame(end.frame, "");
}

// The frame's 60 fps replay (interp60.cpp), after the copy and the next frame's tokens
void ReplayVideo()
{
  const int kept = InterpReplayVideo();
  if (kept == 1 && !s_shown.empty())
    s_shown.back().interp = true;
  if (InterpTrace() && kept >= 0 && !s_shown.empty())
    Log("[interp-trace] frame %llu replayed: %s\n", static_cast<unsigned long long>(s_shown.back().frame),
        kept == 1 ? "in-between kept" : "no in-between");
}
}  // namespace
#endif

void VideoFrameEnd()
{
  ++s_frames;
  ApplyPokes();
  MenuVideoFrame();
  if ((s_frames % 600) == 1)
    Log("[video] frame %llu at %.2f s guest\n", static_cast<unsigned long long>(s_frames),
        static_cast<double>(Now()) / CPU_HZ);
#ifdef SKIES_STANDALONE_AURORA
  if (!s_active)
    return;
  ApplyTestResize();
  {
    const auto input = VideoInputLock();
    MenuApplyGame();
    MenuFrame();
  }
  ApplyPresentRect();
  // The previous frame's replay first, if no token or retrace came since
  PostDeferred();
  FrameEnd end{s_frames, WallForGuest(Now()), InterpFrameEndGame(), GpuXfbCopyAddress(),
               ViXfbAddress() & 0x01FFFFFF};
  s_retrace_presents_wanted = end.interp.wanted;
  VideoPost([end] { FrameEndVideo(end); });
  if (end.interp.wanted)
    VideoPostDeferred([] { ReplayVideo(); });
  // At most a frame ahead of the video thread: this frame end posted, the previous one done
  if (s_threaded)
  {
    UpdateSpin();
    WaitDone(s_previous_frame_end, 1);
    std::lock_guard lock(s_queue_mutex);
    s_previous_frame_end = s_posted;
  }
  if (VideoStats() && (s_frames % 600) == 0)
  {
    static std::chrono::steady_clock::time_point s_last_wall = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::duration s_last_busy{}, s_last_present{};
    const auto now = std::chrono::steady_clock::now();
    const auto ms = [](std::chrono::steady_clock::duration d) {
      return std::chrono::duration<double, std::milli>(d).count() / 600;
    };
    Log("[video-stats] ms per frame: wall %.2f; the game's thread waited %.2f at tokens, %.2f at frame "
        "ends, %.2f on a full queue; the video thread worked %.2f (%.2f in present calls)\n",
        ms(now - s_last_wall), ms(s_waited[0]), ms(s_waited[1]), ms(s_waited[2]), ms(VideoBusy() - s_last_busy),
        ms(VideoPresentTime() - s_last_present));
    s_last_wall = now;
    s_last_busy = VideoBusy();
    s_last_present = VideoPresentTime();
    s_waited[0] = s_waited[1] = s_waited[2] = {};
  }
  if (VideoQuitRequested())
    RequestQuit();
#endif
}

// The EFB copies Aurora holds (the picture the game copied to a texture lives on the GPU only,
// the memory under it is zeroed: hw_gpu.cpp MarkEfbCopy), read back into the state; a load hands
// them back once the rest is in place (VideoRestoreCopies).
static std::vector<u8> s_loaded_copies;

#ifdef SKIES_STANDALONE_AURORA
// SKIES_DRAW_LOG_FRAME=N: Aurora's draw log for frame N only (run with SKIES_DRAIN=0 so the
// frame's commands are parsed before the switch). Set at each frame end, and after a state load.
static void ArmDrawLog(u64 frame)
{
  static const u64 log_frame = [] {
    const char* v = std::getenv("SKIES_DRAW_LOG_FRAME");
    return v ? std::strtoull(v, nullptr, 0) : 0ull;
  }();
  if (log_frame != 0)
  {
    aurora_backend_drain_if_over(0);
    aurora_debug_draw_log = frame + 1 == log_frame;
  }
}
#endif

void VideoDoState(StateIO& p)
{
  p.Do(s_frames);  // the frame dumps' numbers
  std::vector<u8> copies;
#ifdef SKIES_STANDALONE_AURORA
  if (!p.Reading() && p.with_gpu && s_active)
  {
    // Aurora caught up with the stream, then the copies read back there
    VideoSync([&copies] {
      const u32 count = aurora_backend_copies_snapshot(
          g_ram, RAM_SIZE,
          [](const void* data, u32 size, void* user) {
            const u8* bytes = static_cast<const u8*>(data);
            static_cast<std::vector<u8>*>(user)->assign(bytes, bytes + size);
          },
          &copies);
      Log("[video] state: %u EFB copies (%.1f MB)\n", count, copies.size() / 1048576.0);
    });
  }
#endif
  p.Do(copies);
  if (p.Reading())
    s_loaded_copies = std::move(copies);
}

// A Dolphin state's EFB copies: its texture cache's (TextureCacheBase::DoState: each entry's
// texels as an RGBA8 / BGRA8 texture at the internal resolution, its guest address, base format and
// copy size), rebuilt in Aurora once the rest is in place (fork 0047). Its XFB copies aren't: the
// display copy is presented here, not kept.
struct DolphinCopy
{
  u32 offset, format, logical_width, logical_height, width, height;
  std::vector<u8> rgba;
};
static std::vector<DolphinCopy> s_dolphin_copies;

void VideoFromDolphin(const DolphinState& d)
{
  s_frames = d.U("video/presenter/frame_count");
  // An empty snapshot: the copies of the run before the load go
  s_loaded_copies.assign(8, 0);
  const u32 magic = 0x53504F43;  // "COPS"
  std::memcpy(s_loaded_copies.data(), &magic, 4);
  // Built apart and kept only once every entry parsed: a state that throws partway (DolphinStateApply
  // fails, state.cpp restores its backup) must not leave copies for the next load's VideoStateLoaded
  s_dolphin_copies.clear();
  std::vector<DolphinCopy> copies;
  const u64 count = d.U("video/texture_cache/entry_count");
  for (u64 i = 0; i < count; ++i)
  {
    const std::string at = "video/texture_cache/entry[" + std::to_string(i) + "]/";
    if (!d.Bool(at + "is_efb_copy") || d.Bool(at + "is_xfb_copy"))
      continue;
    // TextureConfig: width, height, levels, layers, samples, format (AbstractTextureFormat:
    // RGBA8 = 0, BGRA8 = 1)
    u32 config[8];
    d.Array(at + "config", config, 8);
    const u32 width = config[0], height = config[1], format = config[5];
    const std::span<const u8> texels = d.Bytes(at + "data/data");
    if (format > 1 || width == 0 || height == 0 || texels.size() < static_cast<size_t>(width) * height * 4)
    {
      Log("[video] state: EFB copy %llu skipped (texture format %u, %ux%u)\n",
          static_cast<unsigned long long>(i), format, width, height);
      continue;
    }
    DolphinCopy copy{static_cast<u32>(d.U(at + "addr")) & 0x01FFFFFF,
                     static_cast<u32>(d.U(at + "format_texfmt")),
                     static_cast<u32>(d.U(at + "native_width")),
                     static_cast<u32>(d.U(at + "native_height")),
                     width,
                     height,
                     std::vector<u8>(texels.begin(), texels.begin() + static_cast<long>(width) * height * 4)};
    if (format == 1)
    {
      for (size_t k = 0; k + 3 < copy.rgba.size(); k += 4)
        std::swap(copy.rgba[k], copy.rgba[k + 2]);
    }
    copies.push_back(std::move(copy));
  }
  s_dolphin_copies = std::move(copies);
}

void VideoStateLoaded()
{
  InterpReset();
#ifdef SKIES_STANDALONE_AURORA
  if (s_active)
  {
    const u64 frame = s_frames;
    VideoSync([frame] {
      InterpResetVideo();
      s_shown.clear();
      if (!s_loaded_copies.empty())
      {
        const int count = aurora_backend_copies_restore(g_ram, RAM_SIZE, s_loaded_copies.data(),
                                                        static_cast<u32>(s_loaded_copies.size()));
        Log("[video] state: %d EFB copies restored\n", count);
      }
      int converted = 0;
      for (const DolphinCopy& c : s_dolphin_copies)
        converted += aurora_backend_copy_restore_rgba(g_ram, RAM_SIZE, c.offset, c.format, c.logical_width,
                                                      c.logical_height, c.width, c.height, c.rgba.data());
      if (!s_dolphin_copies.empty())
        Log("[video] state: %d of the Dolphin build's %zu EFB copies rebuilt\n", converted,
            s_dolphin_copies.size());
      ArmDrawLog(frame);
    });
  }
#endif
  s_loaded_copies = {};
  s_dolphin_copies = {};
}

#ifdef SKIES_STANDALONE_AURORA
namespace
{
// A VI field on the video thread, with 60 fps on: the in-between picture, then the real one
void RetraceVideo(std::chrono::steady_clock::time_point wall)
{
  if (!s_retrace_presents)
    return;
  // The newest frame (a frame done in less than two fields gives up its real picture to the next)
  if (s_shown.empty())
    return;
  Shown* current = &s_shown.back();
  ++current->retraces;
  bool interp;
  if (current->retraces == 1)
    interp = current->interp;
  else if (current->retraces == 2 && current->interp)
    interp = false;
  else
    return;  // the real picture shows already
  if (InterpTrace())
    Log("[interp-trace] retrace %u of frame %llu: %s (slot %d)\n", current->retraces,
        static_cast<unsigned long long>(current->frame), interp ? "in-between" : "real",
        RealSlot(current->frame) + (interp ? 1 : 0));
  const bool dump = DumpWanted(current->frame);
  if (dump)
  {
    // The readback goes with the next submitted present: none may be queued before this one
    aurora_backend_synchronize();
    aurora_request_framebuffer_readback();
  }
  SchedulePresent(wall);
  Present(RealSlot(current->frame) + (interp ? 1 : 0));
  if (dump)
    DumpFrame(current->frame, interp ? "-interp" : "");
}
}  // namespace
#endif

void VideoRetrace()
{
#ifdef SKIES_STANDALONE_AURORA
  // Only while the last frame end had 60 fps on (the video thread knows whether it presents)
  if (!s_active || !s_retrace_presents_wanted)
    return;
  PostDeferred();  // the replay before the presents
  const auto wall = WallForGuest(Now());
  VideoPost([wall] { RetraceVideo(wall); });
#endif
}

s64 VideoRefreshPeriodNs()
{
#if defined(SKIES_STANDALONE_AURORA) && defined(__ANDROID__)
  return s_vsync_period_ns.load(std::memory_order_relaxed);
#else
  return 0;
#endif
}

std::chrono::steady_clock::duration VideoPresentTime()
{
#ifdef SKIES_STANDALONE_AURORA
  return std::chrono::nanoseconds(s_present_ns.load(std::memory_order_relaxed));
#else
  return {};
#endif
}

bool VideoSurfaceSize(unsigned* width, unsigned* height)
{
#ifdef SKIES_STANDALONE_AURORA
  // As of the last present (the video thread's)
  const unsigned w = s_surface_width.load(std::memory_order_relaxed);
  const unsigned h = s_surface_height.load(std::memory_order_relaxed);
  if (!s_active || w == 0 || h == 0)
    return false;
  if (width)
    *width = w;
  if (height)
    *height = h;
  return true;
#else
  (void)width;
  (void)height;
  return false;
#endif
}

bool VideoQuitRequested()
{
#ifdef SKIES_STANDALONE_AURORA
  return s_active && (s_threaded ? s_quit.load(std::memory_order_relaxed) : aurora_backend_should_quit());
#else
  return false;
#endif
}

// SKIES_PRESS="12.5:START,14:A,...": port 1 holds that button for 0.2 guest seconds from each time
// (START A B X Y Z L R UP DOWN LEFT RIGHT; CUP CDOWN CLEFT CRIGHT push the C-stick fully over, LT RT
// the analog triggers fully in), for unattended runs. The low 16 bits are buttons.
// While a script is set the host's own controllers and keyboard are left out of port 1-4, so an
// unattended run doesn't pick up a connected pad's resting stick (one unit off centre changes the
// encounter dice: two runs of one route met their battle 37 s apart); SKIES_PRESS_HOST_PADS=1 keeps them.
static bool s_script_active = false;
constexpr u32 SCRIPT_C_UP = 1u << 16, SCRIPT_C_DOWN = 1u << 17, SCRIPT_C_LEFT = 1u << 18,
              SCRIPT_C_RIGHT = 1u << 19, SCRIPT_LT = 1u << 20, SCRIPT_RT = 1u << 21;
u32 ScriptedButtons()
{
  struct Press
  {
    double at;
    u32 button;
  };
  static std::vector<Press> s_presses;
  static bool s_parsed = false;
  if (!s_parsed)
  {
    s_parsed = true;
    // SKIES_PRESS_FILE=FILE: the same script read from a file (long scripts; Windows' cmd cuts
    // environment values read with set /p at 1023 characters)
    static std::string s_file_script;
    const char* script = std::getenv("SKIES_PRESS");
    if (const char* path = std::getenv("SKIES_PRESS_FILE"))
    {
      if (std::FILE* f = std::fopen(path, "rb"))
      {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
          s_file_script.append(buf, n);
        std::fclose(f);
        while (!s_file_script.empty() && std::isspace(static_cast<unsigned char>(s_file_script.back())))
          s_file_script.pop_back();
        script = s_file_script.c_str();
      }
      else
        Log("[video] cannot read SKIES_PRESS_FILE %s\n", path);
    }
    static const struct
    {
      const char* name;
      u32 bit;
    } kNames[] = {{"LEFT", 0x1},           {"RIGHT", 0x2},      {"DOWN", 0x4},
                  {"UP", 0x8},             {"Z", 0x10},         {"R", 0x20},
                  {"L", 0x40},             {"A", 0x100},        {"B", 0x200},
                  {"X", 0x400},            {"Y", 0x800},        {"START", 0x1000},
                  {"CUP", SCRIPT_C_UP},    {"CDOWN", SCRIPT_C_DOWN}, {"CLEFT", SCRIPT_C_LEFT},
                  {"CRIGHT", SCRIPT_C_RIGHT}, {"LT", SCRIPT_LT}, {"RT", SCRIPT_RT}};
    while (script && *script)
    {
      char* end = nullptr;
      const double at = std::strtod(script, &end);
      if (!end || *end != ':')
        break;
      std::string name;
      for (script = end + 1; *script && *script != ','; ++script)
        name += *script;
      if (*script == ',')
        ++script;
      for (const auto& n : kNames)
        if (name == n.name)
          s_presses.push_back({at, n.bit});
    }
    if (!s_presses.empty())
    {
      const char* host = std::getenv("SKIES_PRESS_HOST_PADS");
      s_script_active = !(host && host[0] != '0');
      if (s_script_active)
        Log("[video] press script: %zu presses, the host's controllers and keyboard left out\n", s_presses.size());
    }
  }
  const double now = static_cast<double>(Now()) / CPU_HZ;
  u32 held = 0;
  for (const Press& p : s_presses)
    if (now >= p.at && now < p.at + 0.2)
      held |= p.button;
  return held;
}

// The touch controls' state, packed: buttons (16 bits), stick x / y, C-stick x / y, L / R (8 bits each)
static std::atomic<u64> s_touch_pad{0};

void TouchPadSet(u16 buttons, s8 stick_x, s8 stick_y, s8 c_x, s8 c_y, u8 trigger_l, u8 trigger_r)
{
  s_touch_pad.store(u64{buttons} | u64{static_cast<u8>(stick_x)} << 16 | u64{static_cast<u8>(stick_y)} << 24 |
                        u64{static_cast<u8>(c_x)} << 32 | u64{static_cast<u8>(c_y)} << 40 | u64{trigger_l} << 48 |
                        u64{trigger_r} << 56,
                    std::memory_order_relaxed);
}

// Port 1 with the touch controls added: their buttons to the controller's, their sticks over its own while
// touched, each trigger the further in of the two
static void AddTouchPad(u16& buttons, s8 sticks[4], u8 triggers[2])
{
  const u64 t = s_touch_pad.load(std::memory_order_relaxed);
  if (t == 0)
    return;
  buttons |= static_cast<u16>(t);
  const s8 sx = static_cast<s8>(t >> 16), sy = static_cast<s8>(t >> 24), cx = static_cast<s8>(t >> 32),
           cy = static_cast<s8>(t >> 40);
  if (sx != 0 || sy != 0)
    sticks[0] = sx, sticks[1] = sy;
  if (cx != 0 || cy != 0)
    sticks[2] = cx, sticks[3] = cy;
  triggers[0] = std::max(triggers[0], static_cast<u8>(t >> 48));
  triggers[1] = std::max(triggers[1], static_cast<u8>(t >> 56));
}

void VideoPadRead(u16 buttons[4], s8 sticks[4][4], u8 triggers[4][2], bool connected[4])
{
  for (int i = 0; i < 4; ++i)
  {
    buttons[i] = 0;
    sticks[i][0] = sticks[i][1] = sticks[i][2] = sticks[i][3] = 0;
    triggers[i][0] = triggers[i][1] = 0;
    connected[i] = i == 0;  // port 1 always holds a controller
  }
  const u32 scripted = ScriptedButtons();
  buttons[0] = static_cast<u16>(scripted);
  sticks[0][2] = scripted & SCRIPT_C_LEFT ? -127 : scripted & SCRIPT_C_RIGHT ? 127 : 0;
  sticks[0][3] = scripted & SCRIPT_C_DOWN ? -127 : scripted & SCRIPT_C_UP ? 127 : 0;
  triggers[0][0] = scripted & SCRIPT_LT ? 255 : 0;
  triggers[0][1] = scripted & SCRIPT_RT ? 255 : 0;
#ifdef SKIES_STANDALONE_AURORA
  if (!s_active || s_script_active)
    return;
  const auto input = VideoInputLock();
  DolPadState state[4];
  dol_platform_pad_read(state);
  InputPoll();
  for (int i = 0; i < 4; ++i)
  {
    if (state[i].error != 0)
      continue;
    connected[i] = true;
    buttons[i] = static_cast<u16>(state[i].button | (i == 0 ? buttons[0] : 0));
    sticks[i][0] = state[i].stick_x;
    sticks[i][1] = state[i].stick_y;
    if (i != 0 || !(scripted & (SCRIPT_C_UP | SCRIPT_C_DOWN | SCRIPT_C_LEFT | SCRIPT_C_RIGHT)))
    {
      sticks[i][2] = state[i].substick_x;
      sticks[i][3] = state[i].substick_y;
    }
    triggers[i][0] = std::max(triggers[i][0], state[i].trigger_left);
    triggers[i][1] = std::max(triggers[i][1], state[i].trigger_right);
  }
  AddTouchPad(buttons[0], sticks[0], triggers[0]);
#endif
}

void VideoPadMotor(u32 channel, u32 command)
{
#ifdef SKIES_STANDALONE_AURORA
  if (s_active)
  {
    const auto input = VideoInputLock();
    dol_platform_pad_control_motor(channel, command);
  }
#else
  (void)channel;
  (void)command;
#endif
}

// SKIES_WAV=FILE: the audio DMA's output (32 kHz stereo, left first) as a WAV file, muted or not.
std::FILE* s_wav = nullptr;
u32 s_wav_bytes = 0;

void WavHeader(std::FILE* f, u32 data_bytes)
{
  auto u32le = [&](u32 v) { std::fwrite(&v, 4, 1, f); };
  auto u16le = [&](u16 v) { std::fwrite(&v, 2, 1, f); };
  std::fwrite("RIFF", 1, 4, f);
  u32le(36 + data_bytes);
  std::fwrite("WAVEfmt ", 1, 8, f);
  u32le(16);
  u16le(1);  // PCM
  u16le(2);
  u32le(32000);
  u32le(32000 * 4);
  u16le(4);
  u16le(16);
  std::fwrite("data", 1, 4, f);
  u32le(data_bytes);
}

void WavClose()
{
  if (!s_wav)
    return;
  std::fseek(s_wav, 0, SEEK_SET);
  WavHeader(s_wav, s_wav_bytes);
  std::fclose(s_wav);
  s_wav = nullptr;
}

void AudioPushSamples(const s16* samples, u32 frames)
{
#ifdef SKIES_STANDALONE_AURORA
  // SKIES_DC_MUSIC_SYNC=1: the Dreamcast music in the game's audio, in guest time (dreamcast.cpp)
  static std::vector<s16> s_with_music;
  if (DreamcastMusicSync())
  {
    s_with_music.assign(samples, samples + frames * 2);
    DreamcastAudio(s_with_music.data(), frames);
    samples = s_with_music.data();
  }
#endif
  static const char* s_wav_path = std::getenv("SKIES_WAV");
  if (s_wav_path && !s_wav && s_wav_bytes == 0)
  {
    s_wav = std::fopen(s_wav_path, "wb");
    if (s_wav)
      WavHeader(s_wav, 0);
    else
      s_wav_path = nullptr;
  }
  if (s_wav)
  {
    std::fwrite(samples, sizeof(s16), frames * 2, s_wav);  // the host is little-endian
    s_wav_bytes += frames * 4;
  }
#ifdef SKIES_STANDALONE_AURORA
  static const bool s_mute = std::getenv("SKIES_MUTE") != nullptr;
  if (!s_active || s_mute)
    return;
  const int volume = MenuVolume();
  if (volume >= 100)
  {
    s_audio.insert(s_audio.end(), samples, samples + frames * 2);
  }
  else
  {
    // The Volume setting (the WAV dump above stays the DSP's own output)
    for (u32 i = 0; i < frames * 2; ++i)
      s_audio.push_back(static_cast<s16>(samples[i] * volume / 100));
  }
  if (s_audio.size() >= 512 * 2)
  {
    dol_platform_audio_push(s_audio.data(), static_cast<u32>(s_audio.size() / 2));
    s_audio.clear();
  }
#else
  (void)samples;
  (void)frames;
#endif
}
}  // namespace host
