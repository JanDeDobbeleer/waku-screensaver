#include "animation/animation.h"

#include <cmath>

#include "animation/effect_internal.h"

namespace waku::screensaver {

Frame frameAt(double seconds) {
  const auto segment = static_cast<unsigned>(seconds / kEffectDuration);
  return {segment % kEffectCount, segment, seconds, std::fmod(seconds, kEffectDuration)};
}

Cell cellAt(size_t row, size_t column, const Frame& frame, float entryDistance) {
  if (row >= kArt.size() || column >= kColumns || kArt[row][column] != L'#')
    return {L' ', {0.0f, 0.0f, 0.0f}};
  switch (frame.effect) {
    case 0:
      return rainCell(row, column, frame, entryDistance);
    case 1:
      return decryptCell(row, column, frame, entryDistance);
    case 2:
      return wavesCell(row, column, frame, entryDistance);
    case 3:
      return beamsCell(row, column, frame, entryDistance);
    case 4:
      return matrixCell(row, column, frame, entryDistance);
    case 5:
      return burnCell(row, column, frame, entryDistance);
    case 6:
      return blocksCell(row, column, frame, entryDistance);
    default:
      return {L' ', {0.0f, 0.0f, 0.0f}};
  }
}

unsigned nextFrameDelay(const Frame& frame) {
  if (frame.progress >= 6.0 &&
      (frame.effect == 0 || frame.effect == 1 || frame.effect == 5 || frame.effect == 6)) {
    return static_cast<unsigned>(std::ceil((kEffectDuration - frame.progress) * 1000.0));
  }
  return kFrameIntervalMs;
}

}  // namespace waku::screensaver
