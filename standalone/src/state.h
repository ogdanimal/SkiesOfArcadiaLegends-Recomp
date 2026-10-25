// Save states: the reader / writer every part's DoState goes through (as Dolphin's PointerWrap),
// and the requests the menu and the command line make (state.cpp).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/types.h"

#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace host
{
class StateIO
{
public:
  // Writing appends to *buffer; reading takes it from the start.
  StateIO(std::vector<u8>* buffer, bool reading) : m_buffer(buffer), m_reading(reading) {}

  bool Reading() const { return m_reading; }
  // false: the backup a load keeps in memory, which leaves out what only the GPU holds
  bool with_gpu = true;
  // A read that ran past the end or met the wrong section: everything after it reads as zeros.
  bool Ok() const { return m_ok; }
  const std::string& Error() const { return m_error; }

  void DoBytes(void* data, size_t size)
  {
    if (!m_reading)
    {
      const u8* p = static_cast<const u8*>(data);
      m_buffer->insert(m_buffer->end(), p, p + size);
      return;
    }
    if (!m_ok || m_pos + size > m_buffer->size())
    {
      Fail("the state ends early");
      std::memset(data, 0, size);
      return;
    }
    std::memcpy(data, m_buffer->data() + m_pos, size);
    m_pos += size;
  }

  template <typename T>
  void Do(T& value)
  {
    static_assert(std::is_trivially_copyable_v<T>);
    DoBytes(&value, sizeof(T));
  }

  template <typename T>
  void DoArray(T* values, size_t count)
  {
    static_assert(std::is_trivially_copyable_v<T>);
    DoBytes(values, sizeof(T) * count);
  }

  template <typename T>
  void Do(std::vector<T>& values)
  {
    static_assert(std::is_trivially_copyable_v<T>);
    u64 count = values.size();
    Do(count);
    if (m_reading)
    {
      if (!m_ok || count * sizeof(T) > m_buffer->size() - m_pos)
      {
        Fail("a list longer than the state");
        values.clear();
        return;
      }
      values.resize(static_cast<size_t>(count));
    }
    if (count)
      DoBytes(values.data(), sizeof(T) * values.size());
  }

  void Do(std::string& text)
  {
    std::vector<char> chars(text.begin(), text.end());
    Do(chars);
    if (m_reading)
      text.assign(chars.begin(), chars.end());
  }

  // A section's name: a state read with other code than it was written with stops at the first
  // section that doesn't line up.
  void Marker(const char* name)
  {
    std::string text = name;
    Do(text);
    if (m_reading && m_ok && text != name)
      Fail(std::string("section ") + name + " not found (the state has " + text + ")");
  }

  // A part that can't take the state it read (the first reason is kept)
  void Fail(const std::string& why)
  {
    if (m_ok)
      m_error = why;
    m_ok = false;
  }

private:
  std::vector<u8>* m_buffer;
  bool m_reading;
  size_t m_pos = 0;
  bool m_ok = true;
  std::string m_error;
};
}  // namespace host
