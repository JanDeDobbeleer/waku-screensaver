#include <algorithm>
#include <cmath>

#include "animation/effect_internal.h"

namespace waku::screensaver {

Cell beamsCell(size_t row, size_t column, const Frame& frame, float) {
  const double position = std::fmod(frame.progress / 3.0, 1.0) * (kColumns + 8.0) - 4.0;
  const double vertical = std::fmod(frame.progress / 2.2, 1.0) * 11.0 - 2.0;
  const float horizontalGlow = static_cast<float>(
      std::max(0.0, 1.0 - std::abs(static_cast<double>(column) - position) / 5.0));
  const float verticalGlow =
      static_cast<float>(std::max(0.0, 1.0 - std::abs(static_cast<double>(row) - vertical) / 2.0));
  const float glow = std::max(horizontalGlow, verticalGlow);
  return {L'\x2588', {0.08f + glow * 0.60f, 0.28f + glow * 0.65f, 0.38f + glow * 0.62f}};
}

}  // namespace waku::screensaver
