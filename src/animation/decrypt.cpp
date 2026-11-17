#include "animation/effect_internal.h"

namespace waku::screensaver {

Cell decryptCell(size_t row, size_t column, const Frame& frame, float) {
  const uint32_t seed = hash(static_cast<uint32_t>(row * kColumns + column) + frame.segment * 521U);
  const double reveal = static_cast<double>(row) * 0.55 + (seed % 1800U) / 1000.0;
  if (frame.progress < reveal) {
    return {static_cast<wchar_t>(L'A' + (seed + static_cast<unsigned>(frame.seconds * 15)) % 26),
            {0.12f, 0.85f, 0.25f}};
  }
  return {L'\x2588', {0.95f, 0.62f, 0.13f}};
}

}  // namespace waku::screensaver
