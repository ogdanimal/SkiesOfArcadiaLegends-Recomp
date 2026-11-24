// A first-order DC blocker (about 7 Hz) on a stereo s16 stream, standing in for the coupling capacitors of
// an AC-coupled analog output. Used on the game's own audio (video.cpp, 32 kHz) and on the Dreamcast music
// chip's output (DCMusic.cpp, 44.1 kHz), each with its own state (issue #26).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

class DcBlocker
{
public:
  static constexpr float POLE_32K = 0.99863f;   // exp(-2 pi 7 / 32000)
  static constexpr float POLE_44K = 0.999003f;  // exp(-2 pi 7 / 44100)

  explicit DcBlocker(float pole) : m_pole(pole) {}

  // in and out may be the same buffer.
  void Run(const int16_t* in, int16_t* out, std::size_t frames)
  {
    for (std::size_t i = 0; i < frames; ++i)
      for (int c = 0; c < 2; ++c)
      {
        const float x = in[i * 2 + c];
        const float y = x - m_x[c] + m_pole * m_y[c];
        m_x[c] = x;
        m_y[c] = y;
        out[i * 2 + c] = static_cast<int16_t>(std::clamp(std::lround(y), -32768L, 32767L));
      }
  }

private:
  float m_pole;
  float m_x[2] = {}, m_y[2] = {};
};
