// The DSP HLE from a Dolphin build's state (dolphin_state.h): the fields Dolphin's DSPHLE,
// UCodeInterface, AXUCode, Accelerator and CMailHandler DoStates wrote, into this port of them.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dolphin_state.h"
#include "dsphle/accelerator.h"
#include "dsphle/ax.h"
#include "dsphle/dsp_hle.h"
#include "dsphle/dsphle.h"
#include "dsphle/mail_handler.h"
#include "dsphle/rom.h"
#include "dsphle/ucodes.h"

#include <cstring>

namespace host::dsphle
{
namespace
{
// The reader's name for a ucode's fields, by its CRC (dolphin_state_reader.cpp do_ucode)
std::string UCodeKind(u32 crc)
{
  return crc == UCODE_ROM ? "rom" : crc == UCODE_INIT_AUDIO_SYSTEM ? "init" : crc == 0x65d6cc6f ? "card" :
         crc == 0xdd7e72d5 ? "gba" : "ax";
}
}  // namespace

void UCodeInterface::FromDolphinShared(const DolphinState& d, const std::string& at)
{
  static_assert(sizeof(m_next_ucode) == 28, "Dolphin's NextUCodeInfo is 28 bytes");
  m_upload_setup_in_progress = d.Bool(at + "upload_setup_in_progress");
  m_next_ucode = d.Get<NextUCodeInfo>(at + "next_ucode");
  m_next_ucode_steps = static_cast<int>(d.U(at + "next_ucode_steps"));
  m_needs_resume_mail = d.Bool(at + "needs_resume_mail");
}

void ROMUCode::FromDolphin(const DolphinState&, const std::string&)
{
  // Only before the game's own ucode runs (the boot's first second)
  throw dsr::ParseError("the state was saved while the DSP ROM ucode ran");
}

void Accelerator::FromDolphin(const DolphinState& d, const std::string& at)
{
  // Dolphin keeps neither the gain nor the input register: a voice sets them before it reads
  m_start_address = static_cast<u32>(d.U(at + "start_address"));
  m_end_address = static_cast<u32>(d.U(at + "end_address"));
  m_current_address = static_cast<u32>(d.U(at + "current_address"));
  m_sample_format.hex = static_cast<u16>(d.U(at + "sample_format"));
  m_yn1 = static_cast<s16>(d.U(at + "yn1"));
  m_yn2 = static_cast<s16>(d.U(at + "yn2"));
  m_pred_scale = static_cast<u16>(d.U(at + "pred_scale"));
  m_reads_stopped = d.Bool(at + "reads_stopped");
}

void AXUCode::FromDolphin(const DolphinState& d, const std::string& at)
{
  FromDolphinShared(d, at);
  d.Array(at + "cmdlist", m_cmdlist, 512);
  m_cmdlist_size = static_cast<u32>(d.U(at + "cmdlist_size"));
  m_mail_state = static_cast<MailState>(d.U(at + "mail_state"));
  d.Array(at + "samples_main_left", m_samples_main_left, 32 * 5);
  d.Array(at + "samples_main_right", m_samples_main_right, 32 * 5);
  d.Array(at + "samples_main_surround", m_samples_main_surround, 32 * 5);
  d.Array(at + "samples_auxA_left", m_samples_auxA_left, 32 * 5);
  d.Array(at + "samples_auxA_right", m_samples_auxA_right, 32 * 5);
  d.Array(at + "samples_auxA_surround", m_samples_auxA_surround, 32 * 5);
  d.Array(at + "samples_auxB_left", m_samples_auxB_left, 32 * 5);
  d.Array(at + "samples_auxB_right", m_samples_auxB_right, 32 * 5);
  d.Array(at + "samples_auxB_surround", m_samples_auxB_surround, 32 * 5);
  // Dolphin keeps the coefficients' checksum; they come from the DSP ROM file here too
  if (d.Bool(at + "coeffs_checksum_present") && !m_coeffs_valid)
    LoadResamplingCoefficients();
  m_compressor_pos = static_cast<u16>(d.U(at + "compressor_pos"));
  m_accelerator->FromDolphin(d, at + "accelerator/");
}

void CMailHandler::FromDolphin(const DolphinState& d, const std::string& at)
{
  m_pending_mails.clear();
  const u64 count = d.U(at + "pending_count");
  for (u64 i = 0; i < count; ++i)
  {
    const std::string mail = at + "mail[" + std::to_string(i) + "]/";
    m_pending_mails.emplace_back(static_cast<u32>(d.U(mail + "value")), d.Bool(mail + "is_last"));
  }
  m_last_mail = static_cast<u32>(d.U(at + "last_mail"));
  m_halted = d.Bool(at + "halted");
}

void DSPHLE::FromDolphin(const DolphinState& d)
{
  const std::string at = "hw/dsp/hle/";
  m_dsp_control.Hex = static_cast<u16>(d.U(at + "dsp_control"));
  // Dolphin clears DSPInitCode at a time-base time; here the reads count it down (m_init_code_reads_left)
  m_init_code_reads_left =
      m_dsp_control.DSPInitCode && g_cpu.timebase < d.U(at + "control_reg_init_code_clear_time") ? 2 : 0;
  m_dsp_state.cpu_mailbox = static_cast<u32>(d.U(at + "cpu_mailbox"));
  m_dsp_state.dsp_mailbox = static_cast<u32>(d.U(at + "dsp_mailbox"));
  const auto make = [this](u32 crc) {
    return crc == UCODE_NULL ? std::unique_ptr<UCodeInterface>() : UCodeFactory(crc, this);
  };
  const u32 crc = static_cast<u32>(d.U(at + "ucode_crc"));
  const u32 last_crc = static_cast<u32>(d.U(at + "last_ucode_crc"));
  if (crc != UCodeInterface::GetCRC(m_ucode.get()))
    m_ucode = make(crc);
  if (last_crc != UCodeInterface::GetCRC(m_last_ucode.get()))
    m_last_ucode = make(last_crc);
  if (crc != UCODE_NULL && !m_ucode)
    throw dsr::ParseError("the state's DSP ucode isn't one this host has");
  if (m_ucode)
    m_ucode->FromDolphin(d, at + "ucode/" + UCodeKind(crc) + "/");
  if (m_last_ucode)
    m_last_ucode->FromDolphin(d, at + "last_ucode/" + UCodeKind(last_crc) + "/");
  m_mail_handler.FromDolphin(d, at + "mail_handler/");
}
}  // namespace host::dsphle
