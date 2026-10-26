// The Dolphin build's save states, loaded by this host (dolphin_state.h): an update from the
// Dolphin build keeps a player's states. The .sav is unpacked (LZ4) and walked field by field as
// the Dolphin build wrote it (dolphin_state/dolphin_state_reader.cpp), then each part takes its
// fields. What the Dolphin build's state holds that this host keeps elsewhere or not at all: the
// EFB itself (the next frame draws it again, as after this host's own loads), the XFB copies (the
// display copy is presented), the memory card (the GCI files on disk, which the Dolphin build
// doesn't keep in its states either) and the RAM a palette was loaded from (the game loads its
// palettes again as it draws).
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dolphin_state.h"

#include <cstring>
#include <exception>

namespace host
{
DolphinState::DolphinState(const dsr::StateWalk& walk) : m_walk(walk)
{
  // CoreTiming::GetTicks: the global timer plus the part of the slice the CPU had run
  const s64 downcount = m_walk.s64("ppc/ppcState/downcount");
  m_now = m_walk.u64("coretiming/global_timer") + m_walk.u64("coretiming/slice_length") - downcount;
  const u64 count = m_walk.u64("coretiming/event_count");
  for (u64 i = 0; i < count; ++i)
  {
    const std::string at = "coretiming/event[" + std::to_string(i) + "]/";
    m_events.push_back({Text(at + "name"), m_walk.u64(at + "time"), m_walk.u64(at + "userdata")});
  }
}

std::span<const u8> DolphinState::Bytes(const std::string& path) const
{
  if (!Has(path))
    throw dsr::ParseError("the state has no " + path);
  return m_walk.bytes(path);
}

std::string DolphinState::Text(const std::string& path) const
{
  const dsr::Field* field = m_walk.find(path);
  if (!field || field->kind != dsr::Kind::String)
    throw dsr::ParseError("the state has no text " + path);
  return field->text;
}

const DolphinState::Event* DolphinState::FindEvent(const std::string& name) const
{
  for (const Event& e : m_events)
  {
    if (e.name == name)
      return &e;
  }
  return nullptr;
}

void DolphinState::Copy(const std::string& path, void* out, size_t size) const
{
  const std::span<const u8> bytes = Bytes(path);
  if (bytes.size() != size)
    throw dsr::ParseError(path + " is " + std::to_string(bytes.size()) + " bytes, not " +
                          std::to_string(size));
  std::memcpy(out, bytes.data(), size);
}

// Into the machine as it runs (state.cpp keeps the state from before, for a load that fails)
bool DolphinStateApply(const std::vector<u8>& file, double* guest_seconds, std::string* error)
{
  try
  {
    dsr::SavInfo info;
    const std::vector<u8> raw = dsr::unpack_sav_data(file, &info);
    if (info.state_version != 191)
    {
      *error = "a Dolphin state of another version (" + std::to_string(info.state_version) + ")";
      return false;
    }
    const dsr::StateWalk walk = dsr::walk_state(raw.data(), raw.size());
    if (!walk.ended_exactly)
    {
      *error = "a Dolphin state this host can't read to its end";
      return false;
    }
    if (walk.is_wii || walk.u64("mem1_size") != RAM_SIZE)
    {
      *error = "not a GameCube state";
      return false;
    }
    const DolphinState d(walk);
    SchedFromDolphin(d);
    const std::span<const u8> ram = d.Bytes("hw/memory/ram");
    if (ram.size() < RAM_SIZE)
      throw dsr::ParseError("the state's RAM is smaller than the GameCube's");
    std::memcpy(g_ram, ram.data(), RAM_SIZE);
    CpuFromDolphin(d);
    PiFromDolphin(d);
    ViFromDolphin(d);
    GpuFromDolphin(d);
    DffFromDolphin(d);
    DspFromDolphin(d);
    DiFromDolphin(d);
    SiFromDolphin(d);
    ExiFromDolphin(d);
    AiFromDolphin(d);
    VideoFromDolphin(d);
    *guest_seconds = static_cast<double>(d.Now()) / CPU_HZ;
    return true;
  }
  catch (const std::exception& e)
  {
    *error = std::string("a Dolphin state this host can't take: ") + e.what();
    return false;
  }
}
}  // namespace host
