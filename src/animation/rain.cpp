#include <algorithm>

#include "animation/effect_internal.h"

namespace waku::screensaver {

Cell rainCell(size_t row, size_t column, const Frame& frame, float entryDistance) {
  const uint32_t seed = hash(static_cast<uint32_t>(row * kColumns + column));
  const double delay = (seed % 4000U) / 1000.0;
  const double duration = 1.1 + (seed % 800U) / 1000.0;
  if (frame.progress < delay) return {L' ', {0.0f, 0.0f, 0.0f}};
  const double fraction = std::clamp((frame.progress - delay) / duration, 0.0, 1.0);
  const float fall = static_cast<float>(1.0 - fraction * fraction);
  const float brightness = 0.65f + static_cast<float>(seed % 350U) / 1000.0f;
  return {L'\x2588',
          {0.12f * brightness, 0.56f * brightness, 0.95f * brightness},
          -entryDistance * fall};
}

}  // namespace waku::screensaver
