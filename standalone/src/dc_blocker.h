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

  // One sample of channel c (0 or 1), not rounded.
  float Step(int c, float x)
  {
    float y = x - m_x[c] + m_pole * m_y[c];
    // A constant input decays y into the subnormals within seconds (7e-43), which are slow on most CPUs
    // (the Dreamcast music engine: 18 -> 320-460 ns per output frame). The output doesn't change: a y this
    // small rounds to 0, and next to a whole-step x - m_x it's lost in the float sum.
    if (std::abs(y) < 1e-15f)
      y = 0.0f;
    m_x[c] = x;
    m_y[c] = y;
    return y;
  }

  // in and out may be the same buffer.
  void Run(const int16_t* in, int16_t* out, std::size_t frames)
  {
    for (std::size_t i = 0; i < frames; ++i)
      for (int c = 0; c < 2; ++c)
        out[i * 2 + c] = static_cast<int16_t>(std::clamp(std::lround(Step(c, in[i * 2 + c])), -32768L, 32767L));
  }

private:
  float m_pole;
  float m_x[2] = {}, m_y[2] = {};
};
