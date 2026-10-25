// The CPU side: the recompiled module, its CPUState hooks, the dispatch loop, exceptions and the
// special-purpose registers the module leaves to its host.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "host.h"
#include "state.h"

#include "StaticRecompABI.h"
#include "interp/interp.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace host
{
CPUState g_cpu;
u8* g_ram = nullptr;
bool g_log_mmio = false;

void Log(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  std::vfprintf(stderr, fmt, args);
  va_end(args);
}

void Fatal(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  std::fprintf(stderr, "[fatal] ");
  std::vfprintf(stderr, fmt, args);
  va_end(args);
  std::fprintf(stderr, "[fatal] pc=%08x lr=%08x r1=%08x msr=%08x srr0=%08x srr1=%08x cycles=%llu\n",
               g_cpu.pc, g_cpu.lr, g_cpu.gpr[1], g_cpu.msr, g_cpu.srr0, g_cpu.srr1,
               static_cast<unsigned long long>(g_cycles));
  std::_Exit(1);  // no static destructors: Dawn aborts when torn down mid-frame
}

u32 Read32(u32 address)
{
  const u8* p = RamPtr(address, 4);
  return p ? BE32(p) : static_cast<u32>(MmioRead(address, 4));
}
u16 Read16(u32 address)
{
  const u8* p = RamPtr(address, 2);
  return p ? BE16(p) : static_cast<u16>(MmioRead(address, 2));
}
u8 Read8(u32 address)
{
  const u8* p = RamPtr(address, 1);
  return p ? *p : static_cast<u8>(MmioRead(address, 1));
}
void Write32(u32 address, u32 value)
{
  if (u8* p = RamPtr(address, 4))
  {
    p[0] = u8(value >> 24);
    p[1] = u8(value >> 16);
    p[2] = u8(value >> 8);
    p[3] = u8(value);
    return;
  }
  MmioWrite(address, value, 4);
}
void Write16(u32 address, u16 value)
{
  if (u8* p = RamPtr(address, 2))
  {
    p[0] = u8(value >> 8);
    p[1] = u8(value);
    return;
  }
  MmioWrite(address, value, 2);
}
void Write8(u32 address, u8 value)
{
  if (u8* p = RamPtr(address, 1))
  {
    *p = value;
    return;
  }
  MmioWrite(address, value, 1);
}

namespace
{
// SPR numbers (Gekko user manual; Dolphin's Gekko.h)
enum : u16
{
  SPR_DEC = 22,
  SPR_SPRG0 = 272,
  SPR_TL_W = 284,
  SPR_TU_W = 285,
  SPR_PVR = 287,
  SPR_WPAR = 921,
  SPR_DMAU = 922,
  SPR_DMAL = 923,
  SPR_UMMCR0 = 936,
  SPR_UPMC1 = 937,
  SPR_UPMC2 = 938,
  SPR_UMMCR1 = 940,
  SPR_UPMC3 = 941,
  SPR_UPMC4 = 942,
  SPR_MMCR0 = 952,
  SPR_PMC1 = 953,
  SPR_PMC2 = 954,
  SPR_MMCR1 = 956,
  SPR_PMC3 = 957,
  SPR_PMC4 = 958,
  SPR_HID0 = 1008,
  SPR_HID1 = 1009,
};

const StaticRecompModuleDesc* s_module = nullptr;
u32 s_spr[1024];
u8 s_locked_cache[0x4000];
constexpr u32 LC_BASE = 0xE0000000u;

bool s_quit = false;
u32 s_idle_pc = 0;
bool s_external_asserted = false;
bool s_decrementer_pending = false;
u64 s_dec_start_cycles = 0;
u32 s_dec_start_value = 0;
u64 s_tb_remainder = 0;
double s_overclock_cycles = 0.0;  // the fraction of a cycle the overclock left over

// The OS exception vectors (SDK __OSExceptionLocations order = exception number).
constexpr u32 kVectors[] = {0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700, 0x800,
                            0x900, 0xC00, 0xD00, 0xF00, 0x1300, 0x1400, 0x1700};

u64 HookExternalRead(CPUState*, u32 ea, u8 size)
{
  return MmioRead(ea, size);
}
void HookExternalWrite(CPUState*, u32 ea, u64 value, u8 size)
{
  MmioWrite(ea, value, size);
}
u32 HookExternalRead32(CPUState*, u32 ea, u8)
{
  return static_cast<u32>(MmioRead(ea, 4));
}
void HookExternalWrite32(CPUState*, u32 ea, u32 value, u8)
{
  MmioWrite(ea, value, 4);
}
void* HookExternalPointer(CPUState*, u32 ea, u32 size)
{
  if (ea >= LC_BASE && ea - LC_BASE + size <= sizeof(s_locked_cache))
    return s_locked_cache + (ea - LC_BASE);
  return nullptr;
}

// The module resumes mid-block (an rfi into a non-entry address) one instruction at a time
// through here, as RecompCore does with Dolphin's interpreter.
u64 s_fallback_instructions = 0;
void HookInstructionFallback(CPUState* cpu, u32 raw, u32 cia)
{
  ++s_fallback_instructions;
  if (!interp::ExecuteOne(cpu, raw, cia))
    Fatal("unknown instruction %08x at %08x\n", raw, cia);
}

void LockedCacheDma()
{
  const u32 dmau = s_spr[SPR_DMAU];
  const u32 dmal = s_spr[SPR_DMAL];
  const u32 mem = dmau & ~0x1Fu;
  const u32 lc = (dmal & ~0x1Fu) & 0x3FFFu;
  u32 lines = ((dmau & 0x1F) << 2) | ((dmal >> 2) & 3);
  if (lines == 0)
    lines = 128;
  const u32 bytes = lines * 32;
  u8* ram = RamPtr(mem, bytes);
  if (!ram || lc + bytes > sizeof(s_locked_cache))
  {
    Log("[cpu] locked cache DMA out of range: mem %08x lc %08x %u bytes\n", mem, dmal, bytes);
    return;
  }
  if (dmal & 0x10)
    std::memcpy(s_locked_cache + lc, ram, bytes);
  else
    std::memcpy(ram, s_locked_cache + lc, bytes);
}

u32 HookSprRead(CPUState* cpu, u16 spr, u32 cia)
{
  if (spr >= 1024)
    Fatal("mfspr %u at %08x\n", spr, cia);
  switch (spr)
  {
  case SPR_DEC:
    return DecrementerValue();
  case SPR_PVR:
    return 0x00083214u;  // Gekko
  case SPR_WPAR:
    // bit 0 (BNE) reads clear, as Dolphin's GPFifo::IsBNE: GX's redirect/restore of the pipe
    // (display lists) waits for it, and a partial burst never empties on its own
    return 0x0C008000u;
  case SPR_UPMC1:
    return s_spr[SPR_PMC1];
  case SPR_UPMC2:
    return s_spr[SPR_PMC2];
  case SPR_UPMC3:
    return s_spr[SPR_PMC3];
  case SPR_UPMC4:
    return s_spr[SPR_PMC4];
  case SPR_UMMCR0:
    return s_spr[SPR_MMCR0];
  case SPR_UMMCR1:
    return s_spr[SPR_MMCR1];
  default:
    return s_spr[spr];
  }
}

void HookSprWrite(CPUState* cpu, u16 spr, u32 value, u32 cia)
{
  if (spr >= 1024)
    Fatal("mtspr %u at %08x\n", spr, cia);
  const u32 old_value = s_spr[spr];
  s_spr[spr] = value;
  switch (spr)
  {
  case SPR_TL_W:
  case SPR_TU_W:
    // The module already moved cpu->timebase.
    return;
  case SPR_PVR:
    s_spr[spr] = old_value;
    return;
  case SPR_HID0:
    s_spr[spr] &= ~0x800u;  // ICFI self-clears
    return;
  case SPR_HID1:
    s_spr[spr] &= 0xF8000000u;
    return;
  case SPR_WPAR:
    if (cpu->gather_pipe_ptr_ref)
      *cpu->gather_pipe_ptr_ref = cpu->gather_pipe_base;
    return;
  case SPR_DMAL:
    if (value & 2)
      LockedCacheDma();
    s_spr[spr] &= ~2u;
    return;
  case SPR_DEC:
    if ((old_value >> 31) == 0 && (value >> 31) != 0)
      s_decrementer_pending = true;
    DecrementerWritten();
    return;
  default:
    return;
  }
}

void HookCacheControl(CPUState*, u8 operation, u32 ea, u32)
{
  // No caches are modelled: RAM is coherent, and an icbi has nothing to invalidate in the
  // recompiled code (its SMC sites are listed by the module and not patched by Skies).
  (void)operation;
  (void)ea;
}

u32 ExceptionMsr(u32 msr)
{
  const u32 clear = PPC_MSR_POW | PPC_MSR_EE | PPC_MSR_PR | PPC_MSR_FP | PPC_MSR_FE0 | PPC_MSR_SE |
                    PPC_MSR_BE | PPC_MSR_FE1 | PPC_MSR_IR | PPC_MSR_DR | PPC_MSR_PM | PPC_MSR_RI |
                    PPC_MSR_LE;
  u32 next = msr & ~clear;
  if (msr & PPC_MSR_ILE)
    next |= PPC_MSR_LE;
  return next;
}

void TakeInterrupt(u32 vector)
{
  g_cpu.srr0 = g_cpu.pc;
  g_cpu.srr1 = g_cpu.msr & PPC_MSR_RFI_MASK;
  g_cpu.msr = ExceptionMsr(g_cpu.msr);
  g_cpu.pc = vector;
}

void Rfi()
{
  g_cpu.msr = ((g_cpu.msr & ~PPC_MSR_RFI_MASK) | (g_cpu.srr1 & PPC_MSR_RFI_MASK)) & ~PPC_MSR_POW;
  g_cpu.pc = g_cpu.srr0 & ~3u;
}

// Run the vector's code. The stub runs with translation off; the host does what its 38
// instructions do and leaves the CPU at the handler, translation on, r3 = exception number,
// r4 = OSCurrentContext. Before OSInit installs the stubs a vector holds BS2's lone rfi.
void RunVector(u32 vector)
{
  const u8* code = RamPtr(vector, 0x98);
  if (!code)
    Fatal("exception vector %08x outside RAM\n", vector);
  if (BE32(code) == 0x4C000064)
  {
    Rfi();
    return;
  }
  // __OSInitSystemCall's handler at 0xC00 (SystemCallVector): mfspr r9,HID0; ori r10,r9,8;
  // mtspr HID0,r10; isync; sync; mtspr HID0,r9; rfi
  static const u32 kSyscall[] = {0x7D30FAA6, 0x612A0008, 0x7D50FBA6, 0x4C00012C,
                                 0x7C0004AC, 0x7D30FBA6, 0x4C000064};
  if (std::memcmp(code, "\x7D\x30\xFA\xA6", 4) == 0)
  {
    for (u32 i = 0; i < 7; ++i)
    {
      if (BE32(code + i * 4) != kSyscall[i])
        Fatal("vector %04x: unknown system call handler (word %u: %08x)\n", vector, i, BE32(code + i * 4));
    }
    g_cpu.gpr[9] = s_spr[SPR_HID0];
    g_cpu.gpr[10] = g_cpu.gpr[9] | 8;
    Rfi();
    return;
  }
  // The SDK's stub (OSExceptionVector, __OSEVStart..__OSEVEnd) word by word as OSExceptionInit
  // copies it; 0 marks the words that differ per copy or per region (the debugger hook nop, the
  // li r3,N, OSDefaultExceptionHandler's address).
  static const u32 kStubWords[] = {
      0x7C9043A6, 0x808000C0, 0x9064000C, 0x7C7042A6, 0x90640010, 0x90A40014, 0xA06401A2,
      0x60630002, 0xB06401A2, 0x7C600026, 0x90640080, 0x7C6802A6, 0x90640084, 0x7C6902A6,
      0x90640088, 0x7C6102A6, 0x9064008C, 0x7C7A02A6, 0x90640198, 0x7C7B02A6, 0x9064019C,
      0x7C651B78, 0,          0x7C6000A6, 0x60630030, 0x7C7B03A6, 0,          0x808000D4,
      0x54A507BD, 0x40820014, 0,          0,          0x7CBA03A6, 0x4C000064, 0x546515BA,
      0x80A53000, 0x7CBA03A6, 0x4C000064,
  };
  for (u32 i = 0; i < sizeof(kStubWords) / 4; ++i)
  {
    if (kStubWords[i] != 0 && BE32(code + i * 4) != kStubWords[i])
      Fatal("vector %04x holds unknown code (word %u: %08x)\n", vector, i, BE32(code + i * 4));
  }
  if (BE32(code + 0x58) != 0x60000000)
    Fatal("vector %04x has the debugger hook installed\n", vector);
  const u32 li = BE32(code + 0x68);
  if ((li & 0xFFFF0000u) != 0x38600000u)
    Fatal("vector %04x: no li r3,N at its number slot (%08x)\n", vector, li);
  const u32 number = li & 0xFFFF;
  const u32 lis = BE32(code + 0x78), addi = BE32(code + 0x7C);
  const u32 default_handler = ((lis & 0xFFFF) << 16) + static_cast<u32>(static_cast<s16>(addi & 0xFFFF));

  CPUState& c = g_cpu;
  s_spr[SPR_SPRG0] = c.gpr[4];
  const u32 context = Read32(0x800000C0);  // OSPhysicalContext (a physical address)
  Write32(context + 0x0C, c.gpr[3]);
  Write32(context + 0x10, c.gpr[4]);
  Write32(context + 0x14, c.gpr[5]);
  Write16(context + 0x1A2, Read16(context + 0x1A2) | 2);
  Write32(context + 0x80, c.cr);
  Write32(context + 0x84, c.lr);
  Write32(context + 0x88, c.ctr);
  Write32(context + 0x8C, c.xer);
  Write32(context + 0x198, c.srr0);
  Write32(context + 0x19C, c.srr1);
  c.gpr[5] = c.srr1;
  c.srr1 = c.msr | 0x30;
  c.gpr[3] = number;
  c.gpr[4] = Read32(0x800000D4);  // OSCurrentContext
  if (c.gpr[5] & 2)  // MSR[RI]: recoverable, take the OS handler from __OSExceptionTable
    c.srr0 = Read32(0x80003000 + number * 4);
  else
    c.srr0 = default_handler;
  c.gpr[5] &= 2;  // rlwinm. r5,r5,0,30,30
  Rfi();
}
}  // namespace

bool LoadModule(const char* path)
{
#ifdef _WIN32
  HMODULE handle = LoadLibraryA(path);
  if (!handle)
  {
    Log("[cpu] cannot load %s: error %lu\n", path, GetLastError());
    return false;
  }
  auto get = reinterpret_cast<StaticRecompGetModuleFn>(
      reinterpret_cast<void*>(GetProcAddress(handle, STATICRECOMP_GET_MODULE_SYMBOL)));
#else
  void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!handle)
  {
    Log("[cpu] cannot load %s: %s\n", path, dlerror());
    return false;
  }
  auto get = reinterpret_cast<StaticRecompGetModuleFn>(dlsym(handle, STATICRECOMP_GET_MODULE_SYMBOL));
#endif
  if (!get)
  {
    Log("[cpu] %s has no %s\n", path, STATICRECOMP_GET_MODULE_SYMBOL);
    return false;
  }
  const StaticRecompModuleDesc* desc = get();
  if (!desc || desc->abi_version != STATICRECOMP_ABI_VERSION ||
      desc->cpu_abi_version != GXRUNTIME_CPU_ABI_VERSION || desc->cpu_state_size != sizeof(CPUState))
  {
    Log("[cpu] %s: ABI mismatch (module abi %u cpu %u size %u; host abi %u cpu %u size %zu)\n", path,
        desc ? desc->abi_version : 0, desc ? desc->cpu_abi_version : 0, desc ? desc->cpu_state_size : 0,
        STATICRECOMP_ABI_VERSION, GXRUNTIME_CPU_ABI_VERSION, sizeof(CPUState));
    return false;
  }
  s_module = desc;
  Log("[cpu] module %s for %s: %u code ranges, %u chunks, entry %08x\n", path, desc->game_id,
      desc->num_code_ranges, desc->num_chunk_ranges, desc->entry_point);
  return true;
}

u8* LockedCache()
{
  return s_locked_cache;
}

// The module's chunk hashes against the DOL in RAM (FNV-1a 64 of each chunk's original text, as
// RecompCore's SMC check): a module built for another DOL (a different game patch) would run the
// wrong code, so most chunks must match.
bool VerifyModule()
{
  u32 matched = 0, checked = 0;
  for (u32 i = 0; i < s_module->num_chunk_ranges; ++i)
  {
    const StaticRecompRange& chunk = s_module->chunk_ranges[i];
    const u64 expected = s_module->chunk_hashes ? s_module->chunk_hashes[i] : 0;
    const u8* bytes = RamPtr(chunk.start, chunk.end - chunk.start);
    if (expected == 0 || !bytes)
      continue;
    u64 hash = 0xCBF29CE484222325ull;
    for (u32 k = 0; k < chunk.end - chunk.start; ++k)
      hash = (hash ^ bytes[k]) * 0x100000001B3ull;
    ++checked;
    matched += hash == expected;
  }
  Log("[cpu] module chunks matching the DOL: %u of %u\n", matched, checked);
  if (checked != 0 && matched < checked - checked / 50)
  {
    Log("[cpu] the module was built for another DOL (another game patch?)\n");
    return false;
  }
  return true;
}

const char* ModuleGameId()
{
  return s_module ? s_module->game_id : "";
}

static void DecrementerDue(u64);

// Guest MEM1, page-aligned (a static array: MSVC has no std::aligned_alloc)
alignas(4096) static u8 s_ram_storage[RAM_SIZE];

void CpuInit()
{
  std::memset(&g_cpu, 0, sizeof(g_cpu));
  std::memset(s_spr, 0, sizeof(s_spr));
  g_ram = s_ram_storage;
  std::memset(g_ram, 0, RAM_SIZE);
  CPUState& c = g_cpu;
  c.ram = g_ram;
  c.ram_size = RAM_SIZE;
  c.external_read = HookExternalRead;
  c.external_write = HookExternalWrite;
  c.external_read32 = HookExternalRead32;
  c.external_write32 = HookExternalWrite32;
  c.external_pointer = HookExternalPointer;
  c.instruction_fallback = HookInstructionFallback;
  c.host_call = nullptr;
  c.spr_read = HookSprRead;
  c.spr_write = HookSprWrite;
  c.cache_control = HookCacheControl;
  GatherPipeInstall(&c);
  // The state Dolphin's BS2 emulation leaves (Boot_BS2Emu.cpp SetupMSR / SetupHID)
  c.msr = PPC_MSR_RI | PPC_MSR_DR | PPC_MSR_IR | PPC_MSR_FP;
  s_spr[SPR_HID0] = 0x0011C464u;
  s_spr[SPR_HID1] = 0x80000000u;
  c.hid2 = 0xE0000000u;  // LSQE | WPE | PSE
  s_spr[SPR_WPAR] = 0x0C008000u;
  SchedRegister(EV_DEC, DecrementerDue);
}

// The registers up to the host's hooks (CPUState's pointers stay this run's), the SPRs, the
// locked cache and the decrementer / time base bookkeeping.
void CpuDoState(StateIO& p)
{
  p.DoBytes(&g_cpu, offsetof(CPUState, external_read));
  p.DoArray(s_spr, 1024);
  p.DoArray(s_locked_cache, sizeof(s_locked_cache));
  p.Do(s_external_asserted);
  p.Do(s_decrementer_pending);
  p.Do(s_dec_start_cycles);
  p.Do(s_dec_start_value);
  p.Do(s_tb_remainder);
  p.Do(s_overclock_cycles);
  if (p.Reading())
  {
    g_cpu.downcount = 0;
    g_cpu.exception = 0;
    g_cpu.program_exception = 0;
    if (s_module && s_module->on_state_loaded)
      s_module->on_state_loaded(&g_cpu);
  }
}

u64 ModuleFingerprint()
{
  u64 hash = 0xCBF29CE484222325ull;
  const auto mix = [&](u64 value) {
    for (int i = 0; i < 8; ++i)
      hash = (hash ^ ((value >> (8 * i)) & 0xFF)) * 0x100000001B3ull;
  };
  if (!s_module)
    return 0;
  mix(s_module->entry_point);
  mix(s_module->num_chunk_ranges);
  for (u32 i = 0; i < s_module->num_chunk_ranges; ++i)
  {
    mix(s_module->chunk_ranges[i].start);
    mix(s_module->chunk_hashes ? s_module->chunk_hashes[i] : 0);
  }
  return hash;
}

u32 GetSpr(u16 spr)
{
  return s_spr[spr];
}
void SetSpr(u16 spr, u32 value)
{
  s_spr[spr] = value;
}

void SetIdlePc(u32 pc)
{
  s_idle_pc = pc;
}

void RequestQuit()
{
  s_quit = true;
}
bool QuitRequested()
{
  return s_quit;
}

void SetExternalInterrupt(bool asserted)
{
  s_external_asserted = asserted;
}

void RaiseDecrementer()
{
  s_decrementer_pending = true;
}

u32 DecrementerValue()
{
  return s_dec_start_value - static_cast<u32>((g_cycles - s_dec_start_cycles) / TIMER_RATIO);
}

static void DecrementerDue(u64)
{
  s_decrementer_pending = true;
}

void DecrementerWritten()
{
  // As Dolphin's SystemTimers::DecrementerSet: count down from the written value; the
  // exception comes when it passes zero (bit 31 set).
  const u32 value = s_spr[SPR_DEC];
  s_dec_start_cycles = g_cycles;
  s_dec_start_value = value;
  Deschedule(EV_DEC);
  if ((value & 0x80000000u) == 0)
    Schedule(EV_DEC, static_cast<u64>(value) * TIMER_RATIO, DecrementerDue);
}

// Debug: SKIES_WATCH=ADDR (hex) logs the block (and lr) after which the word at ADDR changed (the
// first 40 changes, or SKIES_WATCH_MAX).
const u32 s_watch_address = [] {
  const char* v = std::getenv("SKIES_WATCH");
  return v ? static_cast<u32>(std::strtoul(v, nullptr, 16)) : 0u;
}();

static void WatchChanged(u32 pc, u32 address)
{
  static u32 last = Read32(address);
  static int logged = 0;
  static const int most = [] {
    const char* v = std::getenv("SKIES_WATCH_MAX");
    return v ? std::atoi(v) : 40;
  }();
  const u32 now = Read32(address);
  if (now != last && logged < most)
  {
    ++logged;
    Log("[watch] %08x: %08x -> %08x after block %08x (now pc %08x, lr %08x, %.3f s)\n", address,
        last, now, pc, g_cpu.pc, g_cpu.lr, static_cast<double>(g_cycles) / CPU_HZ);
  }
  last = now;
}

inline void Watch(u32 pc)
{
  if (s_watch_address)
    WatchChanged(pc, s_watch_address);
}

void RunCpu()
{
  CPUState& c = g_cpu;
  if (s_module->on_state_loaded)
    s_module->on_state_loaded(&c);
  u64 dispatches = 0;
  while (!s_quit)
  {
    // A save state taken or put back here, between dispatches
    if (g_state_request)
      StateRun();
    if (g_next_event <= g_cycles)
      RunDue();

    // External interrupts and the decrementer, when the guest has them enabled.
    if (c.msr & PPC_MSR_EE)
    {
      if (s_external_asserted)
        TakeInterrupt(0x500);
      else if (s_decrementer_pending)
      {
        s_decrementer_pending = false;
        TakeInterrupt(0x900);
      }
    }

    const u32 pc = c.pc;
    if (pc < 0x3000)
    {
      RunVector(pc);
      Watch(pc);
      continue;
    }

    if (!s_module->dispatch(&c, pc))
      Fatal("no recompiled code at %08x\n", pc);
    ++dispatches;
    Watch(pc);

    // The module's per-block cycle charges, at least one cycle per dispatch.
    const s64 charge = -c.downcount;
    c.downcount = 0;
    u64 cycles = charge > 0 ? static_cast<u64>(charge) : 1;
    if (g_overclock != 1.0f)
    {
      // The speed options' overclock (menu.cpp): the same code in fewer guest cycles
      s_overclock_cycles += static_cast<double>(cycles) / g_overclock;
      cycles = static_cast<u64>(s_overclock_cycles);
      s_overclock_cycles -= static_cast<double>(cycles);
    }
    g_cycles += cycles;
    s_tb_remainder += cycles;
    c.timebase += s_tb_remainder / TIMER_RATIO;
    s_tb_remainder %= TIMER_RATIO;

    // The module already moved pc/msr/srr to the vector; the flags only report it.
    c.exception = 0;
    c.program_exception = 0;

    // The scheduler's idle spin (game ini StaticRecompIdlePC): nothing can change until the next
    // event, so time jumps there.
    if (c.pc == s_idle_pc && s_idle_pc != 0 && g_next_event > g_cycles &&
        g_next_event != ~0ull)
    {
      const u64 skip = g_next_event - g_cycles;
      g_cycles = g_next_event;
      s_tb_remainder += skip;
      c.timebase += s_tb_remainder / TIMER_RATIO;
      s_tb_remainder %= TIMER_RATIO;
    }
  }
  Log("[cpu] stopped after %llu dispatches (%llu fallback instructions), %llu cycles (%.2f s guest)\n",
      static_cast<unsigned long long>(dispatches), static_cast<unsigned long long>(s_fallback_instructions), static_cast<unsigned long long>(g_cycles),
      static_cast<double>(g_cycles) / CPU_HZ);
}
}  // namespace host
