// The standalone host: shared state and the interfaces between its parts.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Hardware models follow Dolphin's (GPL-2.0-or-later; HW/*.cpp, VideoCommon/CommandProcessor.cpp,
// PixelEngine.cpp), cut down to what a retail game drives.
#pragma once

#include "core/cpu.h"

#include <cstdarg>
#include <cstdint>
#include <chrono>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace host
{
// ---- clocks ------------------------------------------------------------------------------------
constexpr u64 CPU_HZ = 486000000;  // Gekko core clock; all event times are in these cycles
constexpr u32 TIMER_RATIO = 12;    // core cycles per time base tick (40.5 MHz)

// ---- logging -----------------------------------------------------------------------------------
extern bool g_log_mmio;
void Log(const char* fmt, ...);
// From now on the log also goes to this file (the data folder's Logs/skies-standalone.log), as well as stderr
void LogAlsoToFile(const std::string& path);
int LogFileFd();  // the log file's descriptor (-1: none; stderr is 2)
// crash.cpp: a crash's report into the log (and a minidump on Windows) beside USER/Logs
void CrashHandlerInstall(const std::string& user);
// main.cpp: USER/Logs/skies-standalone.log kept as .1.log before a start writes a new one (.2, .3 older)
void RotateLogs(const std::string& logs_dir);
// The game has shown its first frames: the renderer started (main.cpp, Vulkan's crash safeguard on Android)
void GpuStartSurvived();
// The graphics API the first frame went out with ("Direct3D 12", ...): the desktop crash safeguard notes it
void GpuStartApi(const char* name);
// A tap or click at (x, y), fractions of the screen: true when the in-game menu is open and takes it (menu.cpp)
bool MenuTap(float x, float y);
[[noreturn]] void Fatal(const char* fmt, ...);

// ---- frame timing (SKIES_PRESENT_PACING=1, or the Dolphin build's RECOMP_PRESENT_PACING=1) -----
// Every 600 values a line "[TAG] WHAT ms over 600: p50 p90 p99 max; >A ms N, >B ms N", the Dolphin
// build's [present-pacing] format.
bool PacingOn();
struct PacingWindow
{
  double values[600];
  unsigned count = 0;
};
void PacingAdd(PacingWindow& window, double ms, const char* tag, const char* what, double over_a, double over_b);

// ---- guest memory ------------------------------------------------------------------------------
constexpr u32 RAM_SIZE = 0x01800000;  // 24 MB MEM1
extern u8* g_ram;
extern CPUState g_cpu;

inline u8* RamPtr(u32 address, u32 size)
{
  const u32 physical = address & 0x1FFFFFFFu;
  return physical + size <= RAM_SIZE ? g_ram + physical : nullptr;
}
u32 Read32(u32 address);
u16 Read16(u32 address);
u8 Read8(u32 address);
void Write32(u32 address, u32 value);
void Write16(u32 address, u16 value);
void Write8(u32 address, u8 value);
inline u32 BE32(const u8* p)
{
  return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3];
}
inline u16 BE16(const u8* p)
{
  return u16((p[0] << 8) | p[1]);
}

// ---- scheduler (scheduler.cpp) -------------------------------------------------------------------
enum EventId
{
  EV_VI,          // next VI half-line
  EV_DEC,         // decrementer underflow
  EV_DI,          // disc command completion
  EV_DSP,         // DSP HLE: 1 ms update
  EV_DSP_INT,     // DSP interrupts the HLE raised with a delay
  EV_ARAM,        // ARAM DMA completion
  EV_AUDIO_DMA,   // AI DMA block
  EV_AI,          // AI sample counter interrupt
  EV_SI,          // SI transfer completion
  EV_EXI,         // EXI transfer completion
  EV_CARD_CMD,    // memory card: erase / program done
  EV_CARD_XFER,   // memory card: DMA transfer complete
  EV_CARD_FLUSH,  // memory card: write changed saves back to the GCI folder
  EV_THROTTLE,    // wall-clock pacing
  EV_COUNT,
};
using EventFn = void (*)(u64 late);
void SchedInit();
// Each slot has one handler; registering it at init lets a loaded state's pending events fire.
void SchedRegister(EventId id, EventFn fn);
void Schedule(EventId id, u64 cycles_from_now, EventFn fn);
void Deschedule(EventId id);
bool IsScheduled(EventId id);
u64 Now();
std::chrono::steady_clock::time_point WallForGuest(u64 cycles);  // {} unthrottled
std::chrono::steady_clock::duration ThrottleIdle();  // main.cpp: the throttle's sleeps so far
std::chrono::steady_clock::duration VideoPresentTime();  // video.cpp: time in Aurora's present calls so far
std::chrono::steady_clock::duration VideoBusy();  // video.cpp: the video thread's work so far, less those
bool VideoThreaded();  // video.cpp: Aurora on the video thread (not SKIES_VIDEO_THREAD=0, not headless)
bool Throttled();
void PinVideoThread();  // main.cpp: the video thread's cores (SKIES_PIN_VIDEO)
// The run loop's clock: Advance() moves time by dispatched cycles; RunDue() fires due events.
extern u64 g_cycles;
// Where this run started on the guest clock: 0 from boot, the state's time after the first --load-state.
// Test scripts give times relative to it: --seconds +N, SKIES_PRESS "+T:BUTTON", SKIES_POKE "@+T:...".
extern double g_run_start_seconds;
extern u64 g_next_event;
void RunDue();

// ---- CPU (cpu.cpp) -----------------------------------------------------------------------------
bool LoadModule(const char* path);
const char* ModuleGameId();
bool VerifyModule();  // after Boot: the module's chunk hashes against the DOL in RAM
void CpuInit();
void RunCpu();          // until quit
void RequestQuit();
bool QuitRequested();
void SetIdlePc(u32 pc);
// The game's frame-end wait (main.cpp per disc): a loop that calls the loader and reads the VI retrace
// count until it moves. While the count equals the last one seen and the loader's queue is empty,
// nothing the loop looks at changes before the next event, so time jumps there without running it.
struct FrameWait
{
  u32 head = 0;          // the loop's first instruction (li r3,0)
  u32 loader = 0;        // the loader it calls, skipped when entered from the loop
  u32 after_loader = 0;  // the return address (the VIGetRetraceCount call)
  u32 counter = 0;       // the VI retrace count
  u32 last = 0;          // the count the game last handled
  u32 queue = 0;         // the loader's queue (0 = empty)
};
void SetFrameWait(const FrameWait& wait);
u32 GetSpr(u16 spr);
void SetSpr(u16 spr, u32 value);
// Interrupt lines from the PI (level) and the decrementer (edge).
void SetExternalInterrupt(bool asserted);
void RaiseDecrementer();
void DecrementerWritten();
u32 DecrementerValue();
u8* LockedCache();  // 16 KB at 0xE0000000

// ---- boot (boot.cpp) ---------------------------------------------------------------------------
bool Boot(const std::string& game_dir);
// The real-time clock in seconds since 2000-01-01: the wall clock at boot (or SKIES_RTC, for
// repeatable runs) plus the guest time since, as Dolphin's emulated RTC.
u32 RtcSeconds();

// ---- disc (disc.cpp) ---------------------------------------------------------------------------
// dol_path: a DOL to boot instead of sys/main.dol (a game-patched one), or empty
bool DiscOpen(const std::string& game_dir, const std::string& dol_path);
bool DiscRead(u64 offset, void* out, u32 length);
std::string DiscPathAt(u64 offset);  // the file at a disc offset, relative to the game folder (traces)
const u8* DiscHeader();  // boot.bin (0x440 bytes)
// The console region the game boots on: the disc's, except the European game, which runs as an NTSC-U
// game as in the Dolphin build (SConfig::SetPathsAndGameMetadata): its render mode follows the TV format
// and only NTSC gives it the 60 Hz mode the port is built for. Its card lives in GC/USA there too.
bool DiscRunsAsNtsc();
const std::vector<u8>& DiscDol();  // sys/main.dol
void DiscAdoptGuestFst();  // after a load: the files at the offsets of the game's own FST in RAM

// ---- MMIO (mmio.cpp) ---------------------------------------------------------------------------
u64 MmioRead(u32 address, u8 size);
void MmioWrite(u32 address, u64 value, u8 size);

// ---- devices -------------------------------------------------------------------------------------
// PI: interrupt cause/mask (bit numbers as Dolphin's ProcessorInterface)
enum PiCause : u32
{
  PI_INT_PI = 0x1,
  PI_INT_RSW = 0x2,
  PI_INT_DI = 0x4,
  PI_INT_SI = 0x8,
  PI_INT_EXI = 0x10,
  PI_INT_AI = 0x20,
  PI_INT_DSP = 0x40,
  PI_INT_MEM = 0x80,
  PI_INT_VI = 0x100,
  PI_INT_PE_TOKEN = 0x200,
  PI_INT_PE_FINISH = 0x400,
  PI_INT_CP = 0x800,
  PI_INT_DEBUG = 0x1000,
  PI_INT_HSP = 0x2000,
  PI_INT_RESET = 0x10000,
};
void PiInit();
void PiSetInterrupt(u32 cause, bool set);
u32 PiRead(u32 offset);
void PiWrite(u32 offset, u32 value);
// The CPU-side FIFO pointers (the gather pipe writes there).
extern u32 g_pi_fifo_base, g_pi_fifo_end, g_pi_fifo_wptr;

void ViInit();
u16 ViRead16(u32 offset);
void ViWrite16(u32 offset, u16 value);
bool ViIsPal();
u64 ViFrameCycles();  // guest cycles per frame (two fields) at the current timing
u32 ViXfbAddress();  // the frame buffer the VI scans out (top field), physical

// GPU: command processor, pixel engine, the gather pipe and the GX command stream into Aurora.
void GpuInit();
u64 GpuFrameCount();  // XFB copies so far
u32 GpuXfbCopyAddress();  // the destination of the latest XFB copy, physical
// dff.cpp: the GP registers the command stream left (and, debug, SKIES_DFF frame capture as a
// Dolphin FIFO log)
void DffInit();
void DffCommand(const u8* p, u32 len, bool top_level);
void DffFrameEnd(u64 frame);
struct GpRegisters
{
  u32 bp[256];
  u32 cp[256];
  u32 xf_mem[4096];
  u32 xf_regs[88];
  std::vector<std::pair<u32, u32>> tluts;  // the last TLUT load to each TMEM offset: BP 0x64, 0x65
};
void DffSnapshot(GpRegisters& out);
const u32* DffBp();
const u32* DffCp();
// The registers as the FIFO commands that set them (no draw done, tokens, copies or TMEM loads)
void DffRegisterCommands(const GpRegisters& regs, std::vector<u8>& fifo);
// interp60.cpp: 60 fps, the Dolphin build's frame interpolation (VideoCommon/FrameInterpolator).
// Every command of a game frame, top level and inside display lists (hw_gpu.cpp Apply).
void InterpCommand(const u8* p, u32 len);
// The frame's XFB copy on the game's thread: the frame's recording, for the video thread.
struct InterpHandoff
{
  void* frame = nullptr;  // the recording (interp60.cpp's), the video thread's from here on
  bool wanted = false;    // 60 fps (or the display's rate) on
  bool display = false;   // at the display's refresh rate ([Skies] FrameMode 2): the video thread's timeline
  bool complete = false;  // the whole frame recorded
  u64 cycles = 0;         // the guest time of the copy
};
InterpHandoff InterpFrameEndGame();
// The same frame end on the video thread (not yet sent to Aurora): with interpolation on, keeps the
// real picture in Aurora slot real_slot (0; -1: off or not fitting, present as usual); the replay
// then keeps the in-between picture in real_slot + 1 when the frame pairs with the previous one.
int InterpFrameEndVideo(const InterpHandoff& handoff, int real_slot, u64 id);
// The frame's replay, later on the video thread (after the copy and the next frame's tokens):
// 1 the in-between picture kept, 0 not, -1 nothing to replay (InterpFrameEndVideo gave -1).
int InterpReplayVideo();
// At the display's refresh rate (FrameMode 2): the frames kept for the video thread's timeline (video.cpp),
// numbered by the video thread's frame count. InterpReplayVideo gives 1 when the pending frame joined them.
// InterpDisplayReplay blends frame `id` with frame id - 1 at t (0..1) and keeps the picture in Aurora slot
// `slot`: 1 kept, else not: -1 the frame not here (yet), -2 a recording cut short, -3 a scene change, -4 no replay
// context. InterpDisplayDrop lets go of the
// frames before `id`.
int InterpDisplayReplay(u64 id, float t, int slot);
void InterpDisplayDrop(u64 id);
double InterpDisplayAllowed();  // in-betweens per frame that fit (the budget's; large: any number)
int InterpMode();
double VideoDisplayHz();
bool VideoRenderSize(u32* width, u32* height);  // the picture's drawn size (the EFB), as of the last frame  // FrameMode 2: the pictures' rate in use (0: not yet known)  // 0: 30 fps, 1: 60 fps, 2: the display's refresh rate (FrameMode, SKIES_INTERP)
void InterpReset();       // a state load, on the game's thread: this frame's recording goes
void InterpResetVideo();  // and on the video thread: the previous frame's
u16 CpRead16(u32 offset);
void CpWrite16(u32 offset, u16 value);
u16 PeRead16(u32 offset);
void PeWrite16(u32 offset, u16 value);
void GatherPipeFlush(CPUState* cpu);
void GatherPipeInstall(CPUState* cpu);
u32 EfbRead(u32 address);

void DspInit();
u16 DspRead16(u32 offset);
void DspWrite16(u32 offset, u16 value);
u8* Aram();
constexpr u32 ARAM_SIZE = 0x01000000;

void DiInit();
u32 DiRead32(u32 offset);
void DiWrite32(u32 offset, u32 value);

void SiInit();
u32 SiRead32(u32 offset);
void SiWrite32(u32 offset, u32 value);
void SiPollDevices();

// EXI with the IPL (SRAM, RTC, UART) and a memory card in slot A over USER/GC/<region>/Card A
// (no card when no user folder is set).
void ExiSetUserDir(const std::string& user_dir);
void ExiInit();
void ExiShutdown();  // flushes the memory card
u32 ExiRead32(u32 offset);
void ExiWrite32(u32 offset, u32 value);

void AiInit();
u32 AiRead32(u32 offset);
void AiWrite32(u32 offset, u32 value);
void AiSetSampleRateFromDsp();
u32 AiDspSampleRate();

// ---- video (video.cpp) ---------------------------------------------------------------------------
bool VideoInit(bool headless);
void VideoShutdown();
// Whole GP commands, top level; reads_ram: some make Aurora read guest RAM (hw_gpu.cpp ReadsRam)
void VideoCommands(const u8* data, u32 size, bool reads_ram = true);
// The game's run on its own thread, this one (the main thread) the video thread until it ends
// (main.cpp); at once on this thread with SKIES_VIDEO_THREAD=0 or headless.
void VideoRun(const std::function<void()>& game);
// Host work for the video thread, in order with the GX stream (at once without one); VideoSync
// waits until it is done.
void VideoPost(std::function<void()> work);
void VideoSync(const std::function<void()>& work);
// Host work that may run behind the game: posted after the next VideoFence, retrace or frame end
void VideoPostDeferred(std::function<void()> work);
// The game's thread waits until the video thread has parsed everything posted (a token, a draw done)
void VideoFence();
// Held while the game's side reads controllers and the keyboard through Aurora and SDL (the video
// thread holds it while it handles the window's events, which change them)
std::unique_lock<std::recursive_mutex> VideoInputLock();
void VideoFrameEnd();                         // the game's display copy (EFB to XFB)
void VideoRetrace();                          // a VI field
// The display's refresh period in ns, 0 when not known (Android: Choreographer's; video.cpp)
s64 VideoRefreshPeriodNs();
bool VideoQuitRequested();
void VideoPadRead(u16 buttons[4], s8 sticks[4][4], u8 triggers[4][2], bool connected[4]);
const char* VideoRendererName();  // the graphics API in use ("Direct3D 12", ...), "" before the window
// Android's touch controls (SkiesTouchControls.java, any thread): controller 1's buttons added to a real
// controller's, the sticks used while touched (-127..127), the triggers the further in of the two
void TouchPadSet(u16 buttons, s8 stick_x, s8 stick_y, s8 c_x, s8 c_y, u8 trigger_l, u8 trigger_r);
// [Skies] TouchControls (the menu, any thread): 0 Auto, 1 Always, 2 Off; the touch controls read it
void TouchModeSet(int mode, int opacity_percent, int size_percent);
int TouchMode();  // mode | opacity << 8 | size << 16, for the view

// Cheat codes (cheats.cpp): the enabled Action Replay / Gecko memory writes of Cheats/<disc ID>.ini (then GameSettings/)
void CheatsApply();  // every frame end, while [Skies] Cheats is on
void CheatsReload();
int CheatsEnabledCount();
int CheatsSkippedCount();
std::string CheatsFileName();
// A controller's motor: 0 stop, 1 rumble, 2 hard stop (the SDK's PAD_MOTOR_*)
void VideoPadMotor(u32 channel, u32 command);
void VideoPadColor(u8 red, u8 green, u8 blue);  // every controller with a colour light (Aurora's PADSetColor)
// HD textures (video.cpp): USER/Load/Textures/ with its separator ("" without --user), where the
// imported packs are (texture_pack.h); turning them on or off, live (Aurora's texture replacement).
std::string TexturesDir();
bool TexturePackImported();  // something to load in TexturesDir (TexturePackImport::Describe)
int TexturePackBc7Skipped();  // the pack's BC7 textures when this GPU can't draw them, else 0
void VideoHdTextures(bool on);
void HdTexturesLog();  // video.cpp, every 600 frames: how many are in use
// --import-texture-pack / --remove-texture-packs: the exit code
int TexturePackCommand(const std::string& pack, bool remove);
// The window's surface in pixels (false: no window; either pointer may be null)
bool VideoSurfaceSize(unsigned* width, unsigned* height);

// ---- Dreamcast (dreamcast.cpp) ---------------------------------------------------------------------
// The Dolphin build's Dreamcast features, its modules kept as they are (standalone/src/dreamcast/):
// Uncensored, the Dreamcast music and streamed songs, the European disc's 60 Hz audio.
void DreamcastSetSysDir(const std::string& dir);  // --sys: holds Skies/uncensor-manifest.json
// disc.cpp: every file of the extracted game as the disc is opened, and the file a read of one at
// offset_in_file comes from (the Dreamcast stream, the 60 Hz copy or the uncensored file, else itself)
void DreamcastDiscFile(const std::string& path);
std::string DreamcastRedirect(const std::string& path, u64 offset_in_file);
void DreamcastPoll();          // hw_si.cpp, each pad poll: the Dreamcast music follows the game
void DreamcastStateLoaded();   // the loaded game's song again
void DreamcastFieldTick();     // main.cpp, each field: the game runs (the music plays)
void DreamcastShutdown();      // the background conversions stopped, the music too
// SKIES_DC_MUSIC_SYNC=1: the Dreamcast music mixed into the game's audio (video.cpp, 32 kHz) in guest
// time, for deterministic runs and SKIES_WAV dumps, instead of its own real-time stream
bool DreamcastMusicSync();
void DreamcastAudio(s16* samples, u32 frames);
bool DreamcastMusicInstalled();
bool DreamcastUncensorAvailable();
// --import-dc IMAGE [--game DIR]: the exit code
int DreamcastImportCommand(const std::string& image, const std::string& game_dir);

// ---- the release runner's part (frontend.cpp) ------------------------------------------------------
// What the command line and the release give the start: the launchers' form (--game G --user-dir U,
// or nothing at all) goes through FrontendPrepare, the test form (--game G [--user U]) doesn't.
struct LaunchOptions
{
  enum class WindowSystem
  {
    Default,
    X11,
    Wayland,
  };
  std::string game, user, module, dsp_coef, sys_dir;
  std::string state_dir;     // "" = USER/StateSaves
  int efb_scale = 0;         // config.ini's resolution (0: none)
  bool frame_interpolation = false;  // config.ini's (the first start's 60 fps)
  WindowSystem window_system = WindowSystem::Default;
};
// The window as config.ini and --title say (video.cpp)
struct WindowOptions
{
  std::string title = "Skies of Arcadia Legends";
  bool fullscreen = false;
  bool fps_in_title = false;
};
extern WindowOptions g_window;
// Fills in what the launcher's runner did before the game started (data folder, default game,
// config.ini, the release's DOL patch and pin, the states' folder, the module, the Sys folder): -1 to
// go on, else the exit code, with a message box first where a player must act
int FrontendPrepare(const char* argv0, LaunchOptions& options);
void FrontendError(const char* title, const std::string& message);
// The in-game menu's Resolution, back into config.ini (which sets it at the next start)
void FrontendResolutionChanged(int scale);
void VideoSetTitleFps(double fps);  // FPS in the title, when config.ini says so (video.cpp)
u32 VideoTakeInbetweensShown();  // 60 fps in-between pictures shown since the last call (video.cpp)

// ---- settings (settings.cpp) -----------------------------------------------------------------------
// The Dolphin build's Config/Dolphin.ini, Config/GFX.ini and Config/GCPadNew.ini (controller 1's
// bindings) in the user folder (read at start, changed lines written back by SettingsSave).
enum class SettingFile
{
  Main,
  Gfx,
  Pad,
};
void SettingsInit(const std::string& user_dir);
// USER/Config (empty without --user)
const std::string& SettingsConfigDir();
// USER itself (empty without --user)
const std::string& SettingsUserDir();
int SettingGet(SettingFile file, const char* section, const char* key, int fallback);
void SettingSet(SettingFile file, const char* section, const char* key, int value, bool boolean = false);
std::string SettingGetString(SettingFile file, const char* section, const char* key);  // "" if unset
void SettingSetString(SettingFile file, const char* section, const char* key, const std::string& value);
void SettingsSave();

// ---- the in-game menu and the player options (menu.cpp) --------------------------------------------
// A GameCube controller's state as the SI sends it (sticks centred at 0x80).
struct PadStatus
{
  bool connected = false;
  u16 buttons = 0;
  u8 stick_x = 0x80, stick_y = 0x80, sub_x = 0x80, sub_y = 0x80, trig_l = 0, trig_r = 0;
};
// Applies the picture settings to the game patch's config words (before the window opens, and at
// every frame end).
void MenuApplyGame();
// Controller 1 at each SI poll: the menu (Z+Start, Esc, a controller's Back) takes the input while
// it's open; the camera, speed and QOL options read it while it's closed.
void MenuFilterPad(PadStatus& pad);
// The game's rumble reaches the controllers: Rumble is on and the menu isn't open.
bool MenuRumbleAllowed();
bool MenuInterpHeld();  // 60 fps held off: the game runs fast (field / battle speed)
// True (once) when the menu asked for the game to start again (a new language on the European disc,
// saved already): the run ends and main starts the same game without a state or Continue.
bool MenuTakeRestartRequest();
// Before each present: the menu, messages and the FPS counter through Aurora's overlay.
void MenuFrame();
void MenuShaderProgress(unsigned width, unsigned height, unsigned done, unsigned total, bool touch);
void MenuShaderProgressEnd();
void VideoPrepareShaders();
// While the start's shader preparation waits: a tap, Back or touch-control press ends it (true: taken)
bool VideoShaderPrepSkip();
void MenuMessage(const std::string& text, int milliseconds);

// ---- controllers through Aurora's PAD layer (input.cpp) ---------------------------------------------
// At each controller read with a window: the "Controller: <name>" message when port 1 changes hands.
void InputPoll();
std::string InputDeviceLabel(std::string_view name);
// The Controls tab's Button mapping page: a row per GameCube input (A ... D-pad right), each with a
// controller and a keyboard binding, kept in Aurora's mapping store (USER/Config/Aurora).
enum class InputSlot
{
  Controller,
  Keyboard,
};
int InputMappingRows();
const char* InputMappingName(int row);
std::string InputBindingLabel(int row, InputSlot slot);  // "-" for none
bool InputClearable(int row, InputSlot slot);           // not the controller's A and B
// A capture: false (and a message) without the device. Poll it at each controller read: true while
// it waits; the next new press binds (saved), Esc / Back or 6 s without one keep the old binding.
bool InputStartCapture(int row, InputSlot slot);
bool InputPollCapture();
void InputClear(int row, InputSlot slot);
bool InputResetDefaults(InputSlot slot);  // false (and a message) without the device
bool InputKeyboardInUse();                // the keyboard was used after controller 1
bool InputHasKeyboard();
bool InputKeyBound(int scancode);  // an SDL scancode bound to any GameCube button or stick
// The Menu (0) and Invincible (1) hotkeys, as controller 1's profile binds them ([SkiesHotkeys]; defaults
// Back / Select + Escape, and the stick clicks): a bound button held on this gamepad, the bound key
bool InputHotkeyHeld(void* sdl_gamepad, int hotkey);  // an SDL_Gamepad*
int InputHotkeyKey(int hotkey);  // an SDL scancode, or < 0 for none (any thread: as last loaded)
void InputHotkeysRefresh();        // load the hotkeys if they aren't (the game thread)
void InputHotkeysReload();         // read them again at the next refresh (the game thread)
// Continue from a card save ([Skies] AutoLoadCard, one shot, as the launchers set it): file 1..99, or
// CONTINUE_NEWEST for the game's newest save on card A; 0 leaves the key's value. The game patch
// (autoload.c, ws20+) loads it at boot, straight into the field. Before the first frame.
constexpr int CONTINUE_NEWEST = -1;
constexpr int CONTINUE_OFF = -2;  // none this boot (a state loads), and the key cleared
void MenuContinue(int file);
// At each display copy: the Continue request and its progress (headless too)
void MenuVideoFrame();
void MenuRequestBack();  // Android's Back: open or close the menu, as Esc does
int MenuVolume();  // percent
// The emulated CPU's clock factor for the speed options (1 = the Gekko's; cpu.cpp divides the
// cycles it charges by it)
extern float g_overclock;
// Guest time over wall time for the last half second (1 = full speed; main.cpp)
double EmulationSpeed();
// After a state load moved the guest clock: wall-clock pacing restarts from now (main.cpp)
void ThrottleRestart();

// ---- save states (state.cpp) -------------------------------------------------------------------
// The whole machine in one file, taken and put back between two dispatches of the run loop, after
// a frame's display copy (the picture is complete and the next frame not started).
class StateIO;
// user_dir/StateSaves, or state_dir when it's given (a romhack's or another version's folder)
void StateSetUserDir(const std::string& user_dir, const std::string& state_dir = {});
// Where the states are (empty without --user): the Dolphin build's folders and names, so the
// launchers list, back up and Continue from them as before
std::string StateDirectory();
// Done at the next frame end; the message is shown once it worked.
void StateRequestSave(const std::string& path, const std::string& message);
void StateRequestLoad(const std::string& path, const std::string& message);
// The command line's --load-state: at the game's first frame end, as the menu's loads are; a state
// that can't be loaded ends the run
void StateLoadAtStart(const std::string& path);
extern bool g_state_request;  // the run loop calls StateRun while set
void StateRun();
void StateFrameEnd();  // hw_gpu.cpp, after each display copy
void StateFinishWrites();  // the saves still being packed and written, before the run ends
void CpuRestoreBootDol();  // after a load: the DOL's code over a state's older code (cpu.cpp)
// The parts' state (each file's own), in the order state.cpp runs them
void SchedDoState(StateIO& p);
void CpuDoState(StateIO& p);
void PiDoState(StateIO& p);
void ViDoState(StateIO& p);
void GpuDoState(StateIO& p);
void DffDoState(StateIO& p);  // the GP registers Aurora has (BP, CP, XF, TLUT loads)
void DffReloadAurora();       // after a load: those registers to Aurora again
void DspDoState(StateIO& p);  // and ARAM, the DSP HLE
void DiDoState(StateIO& p);
void SiDoState(StateIO& p);
void ExiDoState(StateIO& p);  // and the memory card
void AiDoState(StateIO& p);
void VideoDoState(StateIO& p);  // and Aurora's EFB copies
void VideoStateLoaded();        // after a load: the state's EFB copies to Aurora
u64 ModuleFingerprint();  // the recompiled module's chunk hashes (a state needs the same module)
double CardSecondsSinceLastAccess();  // guest seconds; auto-saves wait for a quiet card
// The game's file number (1..99) of its newest save on card A by write time, as the launchers' Continue
// picks it ("8P-GEAE-SA_LEGENDS.000.gci" is file 1); 0 when there's none
int CardNewestSaveFile();
}  // namespace host
