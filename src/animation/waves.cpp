#include <cmath>

#include "animation/effect_internal.h"

namespace waku::screensaver {

Cell wavesCell(size_t row, size_t column, const Frame& frame, float) {
  const float wave =
      static_cast<float>((std::sin(frame.progress * 2.8 - column * 0.3 - row * 0.5) + 1.0) / 2.0);
  return {L'\x2588', {0.1f + 0.85f * wave, 0.45f + 0.45f * wave, 0.8f - 0.65f * wave}};
}

}  // namespace waku::screensaver
