// Ported from Dolphin Core/DSP/DSPAccelerator.h
// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// (this port: GPL-3.0-or-later; BitField/PointerWrap/logging replaced)

#pragma once

#include <string>
#include "core/types.h"

namespace host
{
class StateIO;
class DolphinState;
}

namespace host::dsphle
{
class Accelerator
{
public:
  virtual ~Accelerator() = default;

  u16 ReadSample(const s16* coefs);
  // Zelda ucode reads ARAM through 0xffd3.
  u16 ReadRaw();
  void WriteRaw(u16 value);

  u32 GetStartAddress() const { return m_start_address; }
  u32 GetEndAddress() const { return m_end_address; }
  u32 GetCurrentAddress() const { return m_current_address; }
  u16 GetSampleFormat() const { return m_sample_format.hex; }
  s16 GetGain() const { return m_gain; }
  s16 GetYn1() const { return m_yn1; }
  s16 GetYn2() const { return m_yn2; }
  u16 GetPredScale() const { return m_pred_scale; }
  u16 GetInput() const { return m_input; }
  void SetStartAddress(u32 address);
  void SetEndAddress(u32 address);
  void SetCurrentAddress(u32 address);
  void SetSampleFormat(u16 format);
  void SetGain(s16 gain);
  void SetYn1(s16 yn1);
  void SetYn2(s16 yn2);
  void SetPredScale(u16 pred_scale);
  void SetInput(u16 input);
  void DoState(StateIO& p);
  void FromDolphin(const DolphinState& d, const std::string& at);

protected:
  virtual void OnRawReadEndException() = 0;
  virtual void OnRawWriteEndException() = 0;
  virtual void OnSampleReadEndException() = 0;
  virtual u8 ReadMemory(u32 address) = 0;
  virtual void WriteMemory(u32 address, u8 value) = 0;
  u16 GetCurrentSample();

  // DSP accelerator registers.
  u32 m_start_address = 0;
  u32 m_end_address = 0;
  u32 m_current_address = 0;

  enum class FormatSize : u16
  {
    Size4Bit = 0,
    Size8Bit = 1,
    Size16Bit = 2,
    SizeInvalid = 3
  };

  enum class FormatDecode : u16
  {
    ADPCM = 0,         // ADPCM reads from ARAM, ACCA increments
    MMIOPCMNoInc = 1,  // PCM Reads from ACIN, ACCA doesn't increment
    PCM = 2,           // PCM reads from ARAM, ACCA increments
    MMIOPCMInc = 3     // PCM reads from ACIN, ACCA increments
  };

  // When reading samples (at least in PCM mode), they are multiplied by the gain, then divided by
  // the value specified here
  enum class FormatGainScale : u16
  {
    GainScale2048 = 0,
    GainScale1 = 1,
    GainScale65536 = 2,
    GainScaleInvalid = 3
  };

  // Dolphin uses a BitField union here; the same layout as plain accessors.
  struct SampleFormat
  {
    u16 hex = 0;
    FormatSize size() const { return static_cast<FormatSize>(hex & 3); }
    FormatDecode decode() const { return static_cast<FormatDecode>((hex >> 2) & 3); }
    FormatGainScale gain_scale() const { return static_cast<FormatGainScale>((hex >> 4) & 3); }
    u16 unk() const { return static_cast<u16>(hex >> 6); }
  } m_sample_format;

  s16 m_gain = 0;
  s16 m_yn1 = 0;
  s16 m_yn2 = 0;
  u16 m_pred_scale = 0;
  u16 m_input = 0;

  // When an ACCOV is triggered, the accelerator stops reading back anything
  // and updating the current address register, unless the YN2 register is written to.
  // This is kept track of internally; this state is not exposed via any register.
  bool m_reads_stopped = false;
};
}  // namespace host::dsphle
