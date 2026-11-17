#include "animation/effect_internal.h"

namespace waku::screensaver {

Cell matrixCell(size_t row, size_t column, const Frame& frame, float) {
  const auto offset = hash(static_cast<uint32_t>(column)) % 15U;
  const auto step = static_cast<unsigned>(frame.progress * 5.0);
  const auto head = (step + offset) % 15U;
  if (head == row || head == row + 1) {
    const uint32_t seed =
        hash(static_cast<uint32_t>(row * frame.columns + column) + frame.segment * 521U);
    return {static_cast<wchar_t>(L'0' + (seed + step) % 10), {0.65f, 1.0f, 0.60f}};
  }
  return {L'\x2588', {0.04f, 0.37f, 0.18f}};
}

}  // namespace waku::screensaver
