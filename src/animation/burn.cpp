#include "animation/effect_internal.h"

namespace waku::screensaver {

Cell burnCell(size_t row, size_t column, const Frame& frame, float) {
  const double columnDelay =
      column * 0.08 *
      (frame.columns > kColumns ? static_cast<double>(kColumns) / frame.columns : 1.0);
  const double elapsed = frame.progress - columnDelay - row * 0.12;
  if (elapsed < 0.0) return {L'\x2588', {0.3f, 0.27f, 0.27f}};
  if (elapsed < 0.4) return {L'\x2588', {1.0f, 0.95f, 0.6f}};
  if (elapsed < 1.6) {
    const float cooling = static_cast<float>((elapsed - 0.4) / 1.2);
    return {L'\x2588', {1.0f - 0.4f * cooling, 0.65f - 0.55f * cooling, 0.05f}};
  }
  return {L'\x2588', {0.13f, 0.65f, 0.76f}};
}

}  // namespace waku::screensaver
