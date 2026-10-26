// Dolphin's Common/MathUtil.h for the Dreamcast modules (dolphin_shim/README.h)
#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace MathUtil
{
// The value clamped to T's range (floating-point values rounded towards zero, as Dolphin's)
template <typename T, typename F>
constexpr T SaturatingCast(F value)
{
  if constexpr (std::is_floating_point_v<F>)
  {
    if (!(value > static_cast<F>(std::numeric_limits<T>::lowest())))
      return std::numeric_limits<T>::lowest();
    if (!(value < static_cast<F>(std::numeric_limits<T>::max())))
      return std::numeric_limits<T>::max();
    return static_cast<T>(value);
  }
  else
  {
    using C = std::common_type_t<T, F>;
    return static_cast<T>(std::clamp<C>(static_cast<C>(value), static_cast<C>(std::numeric_limits<T>::lowest()),
                                        static_cast<C>(std::numeric_limits<T>::max())));
  }
}
}  // namespace MathUtil
