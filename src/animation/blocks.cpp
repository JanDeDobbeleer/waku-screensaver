#include <algorithm>
#include <array>

#include "animation/effect_internal.h"

namespace waku::screensaver {

Cell blocksCell(size_t row, size_t column, const Frame& frame, float entryDistance) {
  entryDistance -= static_cast<float>(row % 2);
  const uint32_t seed = hash(static_cast<uint32_t>((row / 2) * kColumns + column / 2));
  const double delay =
      (3 - row / 2) * 1.5 + (hash(static_cast<uint32_t>(column / 2)) % 400U) / 1000.0;
  if (frame.progress < delay) return {L' ', {0.0f, 0.0f, 0.0f}};
  const double fraction = std::clamp(frame.progress - delay, 0.0, 1.0);
  const float fall = static_cast<float>(1.0 - fraction * fraction);
  constexpr std::array<Color, 5> colors{{
      {0.22f, 0.84f, 0.95f},
      {0.90f, 0.35f, 0.70f},
      {0.98f, 0.70f, 0.20f},
      {0.42f, 0.86f, 0.45f},
      {0.62f, 0.48f, 0.96f},
  }};
  return {L'\x2588', colors[seed % colors.size()], -entryDistance * fall};
}

}  // namespace waku::screensaver
