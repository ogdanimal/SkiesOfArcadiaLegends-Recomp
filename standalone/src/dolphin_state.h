// The Dolphin build's save states, loaded by this host (dolphin_state.cpp): the state's stream is
// walked field by field (dolphin_state/dolphin_state_reader.cpp, Dolphin 10f7ac4fe4 with the
// project's patches, state version 191), and each part takes what it needs from the fields
// (its *FromDolphin). A part that meets something it can't take throws dsr::ParseError, and the
// load puts back the state from before it.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host.h"

#include "dolphin_state/dolphin_state_reader.h"

#include <cstring>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace host
{
class DolphinState
{
public:
  // A CoreTiming event as the state has it (time on the state's clock, which becomes this run's)
  struct Event
  {
    std::string name;
    u64 time;
    u64 userdata;
  };

  explicit DolphinState(const dsr::StateWalk& walk);

  bool Has(const std::string& path) const { return m_walk.find(path) != nullptr; }
  std::span<const u8> Bytes(const std::string& path) const;
  // A scalar, zero-extended
  u64 U(const std::string& path) const { return m_walk.u64(path); }
  bool Bool(const std::string& path) const { return U(path) != 0; }
  std::string Text(const std::string& path) const;
  // A field of exactly sizeof(T) bytes (a scalar or a POD blob), as the state's machine wrote it
  // (little-endian, as this host)
  template <typename T>
  T Get(const std::string& path) const
  {
    static_assert(std::is_trivially_copyable_v<T>);
    T value;
    Copy(path, &value, sizeof(T));
    return value;
  }
  template <typename T>
  void Array(const std::string& path, T* out, size_t count) const
  {
    static_assert(std::is_trivially_copyable_v<T>);
    Copy(path, out, sizeof(T) * count);
  }

  // The machine's time when the state was taken: CoreTiming's global timer plus what the CPU had
  // run of its slice (GetTicks)
  u64 Now() const { return m_now; }
  const std::vector<Event>& Events() const { return m_events; }
  // The first pending event of that name, or nullptr
  const Event* FindEvent(const std::string& name) const;

private:
  void Copy(const std::string& path, void* out, size_t size) const;

  const dsr::StateWalk& m_walk;
  u64 m_now = 0;
  std::vector<Event> m_events;
};

// A Dolphin build's .sav (the whole file) into the machine; false with the reason if it can't be
// taken (the caller puts back the state from before). *guest_seconds: the state's clock.
bool DolphinStateApply(const std::vector<u8>& file, double* guest_seconds, std::string* error);

// Each part from a Dolphin state, in the order dolphin_state.cpp runs them
void SchedFromDolphin(const DolphinState& d);
void CpuFromDolphin(const DolphinState& d);
void PiFromDolphin(const DolphinState& d);
void ViFromDolphin(const DolphinState& d);
void GpuFromDolphin(const DolphinState& d);
void DffFromDolphin(const DolphinState& d);
void DspFromDolphin(const DolphinState& d);
void DiFromDolphin(const DolphinState& d);
void SiFromDolphin(const DolphinState& d);
void ExiFromDolphin(const DolphinState& d);
void AiFromDolphin(const DolphinState& d);
void VideoFromDolphin(const DolphinState& d);
namespace dsphle
{
void FromDolphin(const DolphinState& d);
}
}  // namespace host
