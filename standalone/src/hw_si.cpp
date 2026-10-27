// Serial interface with GameCube controllers on its four channels (SI.cpp, SI_DeviceGCController.cpp):
// polling into the channel input buffers each field, and the command transfers PADInit sends
// (status, origin, recalibrate). Controller state comes from video.cpp (Aurora's SDL pads), and the
// poll command's motor bits go back to them as rumble.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dolphin_state.h"
#include "host.h"
#include "state.h"

#include <cstring>

namespace host
{
namespace
{
struct Channel
{
  u32 out = 0, in_hi = 0, in_lo = 0;
  u8 mode = 3;
};
Channel s_ch[4];
u32 s_poll = 0, s_comcsr = 0, s_status = 0, s_exilk = 0;
u8 s_buffer[128];
// The motor command each channel's game last latched (0 stop, 1 rumble, 2 hard stop) and the one
// the controller has now (the game's, or stop while the menu is open or Rumble is off). Not in
// save states: a loaded state starts still and the game's next command sets it.
u8 s_motor[4];
u8 s_motor_sent[4];

constexpr u32 COMCSR_TSTART = 1u;
constexpr u32 COMCSR_RDSTINTMSK = 1u << 27;
constexpr u32 COMCSR_RDSTINT = 1u << 28;
constexpr u32 COMCSR_COMERR = 1u << 29;
constexpr u32 COMCSR_TCINTMSK = 1u << 30;
constexpr u32 COMCSR_TCINT = 1u << 31;
constexpr u16 PAD_USE_ORIGIN = 0x0080;

u32 RdstBit(u32 ch)
{
  return 1u << (29 - 8 * ch);
}
u32 NorepBit(u32 ch)
{
  return 1u << (27 - 8 * ch);
}

using Pad = PadStatus;

// The controllers as of the last poll (SiPollDevices, once per field): controller 1 through the
// menu's filter (menu.cpp), which takes the input while the menu is open.
Pad s_pads[4];
bool s_polled = false;

void Snapshot()
{
  // The menu's filter reads controllers through Aurora and SDL too
  const auto input = VideoInputLock();
  u16 buttons[4];
  s8 sticks[4][4];
  u8 triggers[4][2];
  bool connected[4];
  VideoPadRead(buttons, sticks, triggers, connected);
  for (u32 ch = 0; ch < 4; ++ch)
  {
    Pad& p = s_pads[ch];
    p.connected = connected[ch];
    p.buttons = buttons[ch];
    p.stick_x = static_cast<u8>(0x80 + sticks[ch][0]);
    p.stick_y = static_cast<u8>(0x80 + sticks[ch][1]);
    p.sub_x = static_cast<u8>(0x80 + sticks[ch][2]);
    p.sub_y = static_cast<u8>(0x80 + sticks[ch][3]);
    p.trig_l = triggers[ch][0];
    p.trig_r = triggers[ch][1];
  }
  if (s_pads[0].connected)
    MenuFilterPad(s_pads[0]);
  s_polled = true;
}

Pad ReadPad(u32 ch)
{
  if (!s_polled)
    Snapshot();
  return s_pads[ch];
}

void UpdateInterrupts()
{
  bool rdst = false;
  for (u32 i = 0; i < 4; ++i)
    rdst |= (s_status & RdstBit(i)) != 0;
  if (rdst)
    s_comcsr |= COMCSR_RDSTINT;
  else
    s_comcsr &= ~COMCSR_RDSTINT;
  const bool set = ((s_comcsr & COMCSR_RDSTINT) && (s_comcsr & COMCSR_RDSTINTMSK)) ||
                   ((s_comcsr & COMCSR_TCINT) && (s_comcsr & COMCSR_TCINTMSK));
  PiSetInterrupt(PI_INT_SI, set);
}

bool GetData(u32 ch, u32& hi, u32& lo)
{
  const Pad p = ReadPad(ch);
  if (!p.connected)
    return false;
  hi = p.stick_y | (u32(p.stick_x) << 8) | (u32(p.buttons | PAD_USE_ORIGIN) << 16);
  switch (s_ch[ch].mode)
  {
  case 1:
    lo = (u32(p.trig_r) << 8) | (u32(p.trig_l) << 16) | (u32(p.sub_y >> 4) << 24) | (u32(p.sub_x >> 4) << 28);
    break;
  case 2:
    lo = (u32(p.trig_r >> 4) << 16) | (u32(p.trig_l >> 4) << 20) | (u32(p.sub_y >> 4) << 24) |
         (u32(p.sub_x >> 4) << 28);
    break;
  case 3:
    lo = p.trig_r | (u32(p.trig_l) << 8) | (u32(p.sub_y) << 16) | (u32(p.sub_x) << 24);
    break;
  case 4:
    lo = (u32(p.sub_y) << 16) | (u32(p.sub_x) << 24);
    break;
  default:
    lo = (u32(p.trig_r >> 4) << 8) | (u32(p.trig_l >> 4) << 12) | (u32(p.sub_y) << 16) |
         (u32(p.sub_x) << 24);
    break;
  }
  return true;
}

// One command transfer through the I/O buffer: the response length, or -1 for no device.
int RunBuffer(u32 ch, u8* buffer)
{
  const Pad p = ReadPad(ch);
  if (!p.connected)
    return -1;
  switch (buffer[0])
  {
  case 0x00:  // status
  case 0xFF:  // reset
    buffer[0] = 0x09;  // SI_GC_CONTROLLER
    buffer[1] = 0x00;
    buffer[2] = 0x00;
    return 3;
  case 0x40:  // direct read
  {
    u32 hi = 0, lo = 0;
    GetData(ch, hi, lo);
    for (int i = 0; i < 4; ++i)
    {
      buffer[i] = static_cast<u8>(hi >> (24 - 8 * i));
      buffer[4 + i] = static_cast<u8>(lo >> (24 - 8 * i));
    }
    return 8;
  }
  case 0x41:  // origin
  case 0x42:  // recalibrate
  {
    const u8 origin[10] = {0x00, PAD_USE_ORIGIN, 0x80, 0x80, 0x80, 0x80, 0x1F, 0x1F, 0, 0};
    std::memcpy(buffer, origin, sizeof(origin));
    return 10;
  }
  case 0x1D:  // game ID (adapter configuration): no reply
    return 0;
  default:
    Log("[si] unknown command %02x on channel %u\n", buffer[0], ch);
    return 0;
  }
}

void TransferDone(u64)
{
  const u32 ch = (s_comcsr >> 1) & 3;
  const int reply = RunBuffer(ch, s_buffer);
  s_comcsr &= ~(COMCSR_TSTART | COMCSR_COMERR);
  if (reply < 0)
  {
    s_comcsr |= COMCSR_COMERR;
    s_status |= NorepBit(ch);
  }
  s_comcsr |= COMCSR_TCINT;
  UpdateInterrupts();
}
}  // namespace

void SiInit()
{
  for (Channel& c : s_ch)
    c = {};
  s_poll = s_comcsr = s_status = s_exilk = 0;
  s_polled = false;
  std::memset(s_buffer, 0, sizeof(s_buffer));
  std::memset(s_motor, 0, sizeof(s_motor));
  std::memset(s_motor_sent, 0, sizeof(s_motor_sent));
  SchedRegister(EV_SI, TransferDone);
}

// The controllers as the guest last saw them too, so a loaded state plays on exactly as the run
// it was taken from until the next field's poll.
void SiDoState(StateIO& p)
{
  p.DoArray(s_ch, 4);
  p.Do(s_poll);
  p.Do(s_comcsr);
  p.Do(s_status);
  p.Do(s_exilk);
  p.DoArray(s_buffer, sizeof(s_buffer));
  p.DoArray(s_pads, 4);
  p.Do(s_polled);
  if (p.Reading())
    std::memset(s_motor, 0, sizeof(s_motor));
}

// A Dolphin state's SI: the channels' buffers and a controller's poll mode; the controllers
// themselves are read again at the next poll
void SiFromDolphin(const DolphinState& d)
{
  constexpr u64 GC_CONTROLLER = 6;  // SIDevices::SIDEVICE_GC_CONTROLLER
  for (u32 ch = 0; ch < 4; ++ch)
  {
    const std::string at = "hw/si/channel[" + std::to_string(ch) + "]/";
    s_ch[ch].out = static_cast<u32>(d.U(at + "out"));
    s_ch[ch].in_hi = static_cast<u32>(d.U(at + "in_hi"));
    s_ch[ch].in_lo = static_cast<u32>(d.U(at + "in_lo"));
    s_ch[ch].mode = d.U(at + "device_type") == GC_CONTROLLER ?
                        static_cast<u8>(d.U(at + "gc_controller/mode")) :
                        3;
  }
  s_poll = static_cast<u32>(d.U("hw/si/poll"));
  s_comcsr = static_cast<u32>(d.U("hw/si/com_csr"));
  s_status = static_cast<u32>(d.U("hw/si/status_reg"));
  s_exilk = static_cast<u32>(d.U("hw/si/exi_clock_count"));
  d.Array("hw/si/si_buffer", s_buffer, sizeof(s_buffer));
  s_polled = false;
  std::memset(s_motor, 0, sizeof(s_motor));
  UpdateInterrupts();
}

void SiPollDevices()
{
#ifdef SKIES_STANDALONE_AURORA
  // The Dreamcast music follows the game at each pad poll, as the Dolphin build's SkiesMusic::Poll
  DreamcastPoll();
#endif
  Snapshot();
  const bool rumble = MenuRumbleAllowed();
  for (u32 i = 0; i < 4; ++i)
  {
    const u8 motor = rumble ? s_motor[i] : 0;
    if (motor != s_motor_sent[i])
    {
      s_motor_sent[i] = motor;
      static bool s_logged = false;
      if (motor == 1 && !s_logged)
      {
        s_logged = true;
        Log("[si] controller %u: the game's first rumble\n", i + 1);
      }
      VideoPadMotor(i, motor);
    }
  }
  for (u32 i = 0; i < 4; ++i)
  {
    const u32 errlatch = s_ch[i].in_hi & 0x40000000u;
    u32 hi = 0, lo = 0;
    if (GetData(i, hi, lo))
    {
      s_ch[i].in_hi = hi | errlatch;
      s_ch[i].in_lo = lo;
      s_status |= RdstBit(i);
    }
    else
    {
      s_status |= NorepBit(i);
      s_ch[i].in_hi |= 0xC0000000u;  // ERRSTAT | ERRLATCH
    }
  }
  UpdateInterrupts();
}

u32 SiRead32(u32 offset)
{
  if (offset >= 0x80)
  {
    const u32 i = (offset - 0x80) & 0x7C;
    return BE32(s_buffer + i);
  }
  if (offset < 0x30)
  {
    const u32 ch = offset / 0xC;
    switch (offset % 0xC)
    {
    case 0:
      return s_ch[ch].out;
    case 4:
      s_status &= ~RdstBit(ch);
      UpdateInterrupts();
      return s_ch[ch].in_hi;
    default:
      s_status &= ~RdstBit(ch);
      UpdateInterrupts();
      return s_ch[ch].in_lo;
    }
  }
  switch (offset)
  {
  case 0x30:
    return s_poll;
  case 0x34:
    return s_comcsr;
  case 0x38:
    return s_status;
  case 0x3C:
    return s_exilk;
  default:
    return 0;
  }
}

void SiWrite32(u32 offset, u32 value)
{
  if (offset >= 0x80)
  {
    const u32 i = (offset - 0x80) & 0x7C;
    s_buffer[i] = static_cast<u8>(value >> 24);
    s_buffer[i + 1] = static_cast<u8>(value >> 16);
    s_buffer[i + 2] = static_cast<u8>(value >> 8);
    s_buffer[i + 3] = static_cast<u8>(value);
    return;
  }
  if (offset < 0x30)
  {
    const u32 ch = offset / 0xC;
    switch (offset % 0xC)
    {
    case 0:
      s_ch[ch].out = value;
      return;
    case 4:
      s_ch[ch].in_hi = value;
      return;
    default:
      s_ch[ch].in_lo = value;
      return;
    }
  }
  switch (offset)
  {
  case 0x30:
    s_poll = value;
    return;
  case 0x34:
  {
    const u32 keep = COMCSR_TCINT | COMCSR_RDSTINT | COMCSR_TSTART | COMCSR_COMERR;
    s_comcsr = (s_comcsr & keep) | (value & ~keep & ~0x80000000u);
    if (value & COMCSR_RDSTINT)
      s_comcsr &= ~COMCSR_RDSTINT;
    if (value & COMCSR_TCINT)
      s_comcsr &= ~COMCSR_TCINT;
    if (value & COMCSR_TSTART)
    {
      s_comcsr |= COMCSR_TSTART;
      // about 4 us per bit for request and response; a fixed 100 us covers PADInit's transfers
      Schedule(EV_SI, CPU_HZ / 10000, TransferDone);
    }
    UpdateInterrupts();
    return;
  }
  case 0x38:
    // error bits clear on 1; WR (bit 31) latches the output buffers: the poll command's mode
    // and motor (SI_DeviceGCController::SendCommand)
    s_status &= ~(value & 0x0F0F0F0Fu);
    if (value & 0x80000000u)
    {
      for (u32 i = 0; i < 4; ++i)
      {
        if (((s_ch[i].out >> 16) & 0xFF) == 0x40)
        {
          s_ch[i].mode = static_cast<u8>((s_ch[i].out >> 8) & 7);
          s_motor[i] = static_cast<u8>(s_ch[i].out & 3);
        }
      }
    }
    UpdateInterrupts();
    return;
  case 0x3C:
    s_exilk = value;
    return;
  default:
    return;
  }
}
}  // namespace host
