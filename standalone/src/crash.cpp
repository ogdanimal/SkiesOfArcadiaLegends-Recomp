// Crash reports: a crash writes what it knows into the log the README asks players to attach
// (USER/Logs/skies-standalone.log), and on Windows a minidump beside it. Nothing is sent anywhere.
// Dusklight (TwilitRealm/dusklight) reports crashes through Sentry once the player agrees
// (m_Do_main.cpp, ui/reporting.cpp); this is the part of that which needs no service: the signal or
// exception, the game CPU's last known registers, and the native stack as module + offset, which a
// build's symbols turn into functions.
//
// POSIX: SIGSEGV, SIGBUS, SIGILL, SIGFPE and SIGABRT on an alternate stack (a stack overflow), then the
// handler that was there before (Android's debuggerd writes its tombstone; the default ends the
// process), so the renderer's crash safeguard (main.cpp) still sees the process die. Windows: an
// unhandled-exception filter (the game's own exception handling comes first), then Windows' own.
// SKIES_CRASH_HANDLER=0 leaves them out; SKIES_CRASH_TEST=segv|abort crashes at the start (tests).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"

#include <atomic>
#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <io.h>
#else
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>
#if defined(__ANDROID__)
#include <unwind.h>
#elif defined(__GLIBC__) || defined(__APPLE__)
#include <execinfo.h>
#define SKIES_HAVE_EXECINFO 1
#endif
#endif

namespace host
{
namespace
{
int s_log_fd = -1;          // the log file (beside stderr), kept open by cpu.cpp
std::string s_logs_dir;     // USER/Logs (the minidump's folder)
std::atomic<int> s_crashed{0};

void Out(const char* text)
{
  const size_t n = std::strlen(text);
#if defined(_WIN32)
  _write(2, text, static_cast<unsigned>(n));
  if (s_log_fd >= 0 && s_log_fd != 2)
    _write(s_log_fd, text, static_cast<unsigned>(n));
#else
  ssize_t ignored = write(2, text, n);
  if (s_log_fd >= 0 && s_log_fd != 2)
    ignored = write(s_log_fd, text, n);
  (void)ignored;
#endif
}

void OutGuest()
{
  char line[256];
  std::snprintf(line, sizeof(line),
                "[crash] game CPU (last known): pc=%08x lr=%08x r1=%08x msr=%08x srr0=%08x cycles=%llu (%.2f s)\n",
                g_cpu.pc, g_cpu.lr, g_cpu.gpr[1], g_cpu.msr, g_cpu.srr0, static_cast<unsigned long long>(g_cycles),
                static_cast<double>(g_cycles) / CPU_HZ);
  Out(line);
}

void OutEnd()
{
  Out("[crash] please attach this log (and any crash-*.dmp beside it) to an issue: "
      "github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp/issues\n");
}

#if defined(_WIN32)
const char* Basename(const char* path)
{
  const char* name = path;
  for (const char* p = path; *p; ++p)
  {
    if (*p == '\\' || *p == '/')
      name = p + 1;
  }
  return name;
}

void OutAddress(int index, DWORD64 address)
{
  char line[512];
  HMODULE module = nullptr;
  char path[MAX_PATH] = {};
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(address), &module) &&
      GetModuleFileNameA(module, path, sizeof(path)) != 0)
    std::snprintf(line, sizeof(line), "[crash]   #%02d %s+0x%llx\n", index, Basename(path),
                  static_cast<unsigned long long>(address - reinterpret_cast<DWORD64>(module)));
  else
    std::snprintf(line, sizeof(line), "[crash]   #%02d 0x%llx\n", index, static_cast<unsigned long long>(address));
  Out(line);
}

void WriteMinidump(EXCEPTION_POINTERS* info)
{
  if (s_logs_dir.empty())
    return;
  HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
  if (!dbghelp)
    return;
  using WriteFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
  const auto write = reinterpret_cast<WriteFn>(GetProcAddress(dbghelp, "MiniDumpWriteDump"));
  if (!write)
    return;
  SYSTEMTIME t;
  GetLocalTime(&t);
  char path[1200];
  std::snprintf(path, sizeof(path), "%s\\crash-%04d%02d%02d-%02d%02d%02d.dmp", s_logs_dir.c_str(), t.wYear, t.wMonth,
                t.wDay, t.wHour, t.wMinute, t.wSecond);
  HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return;
  MINIDUMP_EXCEPTION_INFORMATION exception{GetCurrentThreadId(), info, FALSE};
  // Threads, their stacks and the exception: enough for a stack in a debugger, a few MB at most (no heap:
  // the game's 24 MB of RAM and the GPU's buffers would make it huge and hold the player's own data)
  const BOOL ok = write(GetCurrentProcess(), GetCurrentProcessId(), file,
                        static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo), &exception, nullptr,
                        nullptr);
  CloseHandle(file);
  char line[1300];
  std::snprintf(line, sizeof(line), ok ? "[crash] minidump: %s\n" : "[crash] no minidump (%s)\n", path);
  Out(line);
}

constexpr DWORD ABORT_CODE = 0x40000015;  // STATUS_FATAL_APP_EXIT: abort(), reported from SIGABRT below

LONG WINAPI Filter(EXCEPTION_POINTERS* info)
{
  if (s_crashed.exchange(1) != 0)
    return EXCEPTION_CONTINUE_SEARCH;
  const EXCEPTION_RECORD& record = *info->ExceptionRecord;
  char line[256];
  if (record.ExceptionCode == ABORT_CODE)
    std::snprintf(line, sizeof(line), "[crash] abort() (an assertion, or an uncaught C++ exception) at:\n");
  else
    std::snprintf(line, sizeof(line), "[crash] exception 0x%08lx at:\n", static_cast<unsigned long>(record.ExceptionCode));
  Out(line);
  if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
  {
    std::snprintf(line, sizeof(line), "[crash] (%s address 0x%llx)\n",
                  record.ExceptionInformation[0] == 0 ? "reading" : record.ExceptionInformation[0] == 1 ? "writing"
                                                                                                         : "running",
                  static_cast<unsigned long long>(record.ExceptionInformation[1]));
    Out(line);
  }
#if defined(_M_X64)
  // The stack from the exception's context, unwound with the modules' own unwind tables
  CONTEXT context = *info->ContextRecord;
  for (int i = 0; i < 48 && context.Rip != 0; ++i)
  {
    OutAddress(i, context.Rip);
    DWORD64 image_base = 0;
    RUNTIME_FUNCTION* function = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
    if (!function)
    {
      // A leaf function: the return address is on top of the stack
      context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
      context.Rsp += 8;
      continue;
    }
    void* handler_data = nullptr;
    DWORD64 establisher = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip, function, &context, &handler_data, &establisher,
                     nullptr);
  }
#else
  OutAddress(0, reinterpret_cast<DWORD64>(record.ExceptionAddress));
#endif
  OutGuest();
  WriteMinidump(info);
  OutEnd();
  return EXCEPTION_CONTINUE_SEARCH;
}
// The C runtime's abort() (std::terminate too) ends the process without an exception: its SIGABRT handler
// reports from here, then the runtime goes on ending it
void AbortHandler(int)
{
  CONTEXT context{};
  RtlCaptureContext(&context);
  EXCEPTION_RECORD record{};
  record.ExceptionCode = ABORT_CODE;
#if defined(_M_X64)
  record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);
#endif
  EXCEPTION_POINTERS pointers{&record, &context};
  Filter(&pointers);
}
#else
struct sigaction s_previous[32];
constexpr int SIGNALS[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};

const char* SignalName(int sig)
{
  switch (sig)
  {
  case SIGSEGV:
    return "SIGSEGV";
  case SIGBUS:
    return "SIGBUS";
  case SIGILL:
    return "SIGILL";
  case SIGFPE:
    return "SIGFPE";
  case SIGABRT:
    return "SIGABRT";
  default:
    return "signal";
  }
}

void OutAddress(int index, const void* address)
{
  char line[512];
  Dl_info info{};
  if (dladdr(address, &info) && info.dli_fname)
  {
    const char* name = std::strrchr(info.dli_fname, '/');
    name = name ? name + 1 : info.dli_fname;
    const unsigned long long offset =
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(info.dli_fbase));
    if (info.dli_sname)
      std::snprintf(line, sizeof(line), "[crash]   #%02d %s+0x%llx (%s+0x%llx)\n", index, name, offset, info.dli_sname,
                    static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(address) -
                                                    reinterpret_cast<uintptr_t>(info.dli_saddr)));
    else
      std::snprintf(line, sizeof(line), "[crash]   #%02d %s+0x%llx\n", index, name, offset);
  }
  else
  {
    std::snprintf(line, sizeof(line), "[crash]   #%02d %p\n", index, address);
  }
  Out(line);
}

#if defined(__ANDROID__)
struct Frames
{
  void* pcs[48];
  int count;
};

_Unwind_Reason_Code Collect(_Unwind_Context* context, void* arg)
{
  Frames* frames = static_cast<Frames*>(arg);
  const uintptr_t pc = _Unwind_GetIP(context);
  if (pc != 0 && frames->count < 48)
    frames->pcs[frames->count++] = reinterpret_cast<void*>(pc);
  return frames->count < 48 ? _URC_NO_REASON : _URC_END_OF_STACK;
}
#endif

void Handler(int sig, siginfo_t* info, void* context)
{
  if (s_crashed.exchange(1) == 0)
  {
    char line[160];
    std::snprintf(line, sizeof(line), "[crash] %s (code %d) at address %p\n", SignalName(sig), info ? info->si_code : 0,
                  info ? info->si_addr : nullptr);
    Out(line);
    // The stack as the unwinder sees it from here (the handler's own frames first)
#if defined(__ANDROID__)
    Frames frames{};
    _Unwind_Backtrace(Collect, &frames);
    for (int i = 0; i < frames.count; ++i)
      OutAddress(i, frames.pcs[i]);
#elif defined(SKIES_HAVE_EXECINFO)
    void* pcs[48];
    const int count = backtrace(pcs, 48);
    for (int i = 0; i < count; ++i)
      OutAddress(i, pcs[i]);
#endif
    OutGuest();
    OutEnd();
  }
  // The handler from before (debuggerd's tombstone on Android; the default: the process ends)
  struct sigaction& previous = s_previous[sig];
  if ((previous.sa_flags & SA_SIGINFO) && previous.sa_sigaction)
  {
    sigaction(sig, &previous, nullptr);
    previous.sa_sigaction(sig, info, context);
    return;
  }
  if (previous.sa_handler != SIG_IGN && previous.sa_handler != SIG_DFL && previous.sa_handler != nullptr)
  {
    sigaction(sig, &previous, nullptr);
    previous.sa_handler(sig);
    return;
  }
  signal(sig, SIG_DFL);
  raise(sig);
}
#endif
}  // namespace

void CrashHandlerInstall(const std::string& user)
{
  const char* off = std::getenv("SKIES_CRASH_HANDLER");
  if (off && std::atoi(off) == 0)
    return;
  s_log_fd = LogFileFd();
  if (!user.empty())
    s_logs_dir = (std::filesystem::path(user) / "Logs").string();
#if defined(_WIN32)
  SetUnhandledExceptionFilter(Filter);
  std::signal(SIGABRT, AbortHandler);
#else
  // A stack overflow leaves no stack for the handler: it gets its own
  static char s_stack[128 * 1024];
  stack_t alt{};
  alt.ss_sp = s_stack;
  alt.ss_size = sizeof(s_stack);
  sigaltstack(&alt, nullptr);
#if defined(SKIES_HAVE_EXECINFO)
  void* warm[4];
  backtrace(warm, 4);  // its first call loads libgcc's unwinder (not from a signal handler)
#endif
  for (int sig : SIGNALS)
  {
    struct sigaction action{};
    action.sa_sigaction = Handler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);
    sigaction(sig, &action, &s_previous[sig]);
  }
#endif
  if (const char* test = std::getenv("SKIES_CRASH_TEST"))
  {
    Log("[crash] SKIES_CRASH_TEST=%s\n", test);
    if (std::strcmp(test, "abort") == 0)
      std::abort();
    volatile int* null = nullptr;
    *null = 1;
  }
}
}  // namespace host
