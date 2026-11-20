#include "animation/animation.h"

#include <algorithm>
#include <cmath>

#include "animation/effect_internal.h"

namespace waku::screensaver {

double effectDuration(unsigned effect, size_t columns) {
  return effect == kChaseEffect ? 28.0 * std::max(1.0, static_cast<double>(columns) / kColumns)
                                : kEffectDuration;
}

Frame frameAt(double seconds, std::span<const unsigned> effects, size_t columns) {
  const size_t count = effects.empty() ? kEffectCount : effects.size();
  const auto effectAt = [&](size_t index) {
    return effects.empty() ? static_cast<unsigned>(index) : effects[index];
  };
  double cycleDuration = 0.0;
  for (size_t index = 0; index < count; ++index)
    cycleDuration += effectDuration(effectAt(index), columns);
  const auto cycles = static_cast<unsigned>(seconds / cycleDuration);
  double progress = std::fmod(seconds, cycleDuration);
  for (size_t index = 0; index < count; ++index) {
    const unsigned effect = effectAt(index);
    const double duration = effectDuration(effect, columns);
    if (progress < duration || index + 1 == count)
      return {effect, static_cast<unsigned>(cycles * count + index), seconds, progress, columns};
    progress -= duration;
  }
  return {};
}

Cell cellAt(size_t row, size_t column, const Frame& frame, float entryDistance, const Art& art) {
  if (row >= art.rows.size() || column >= art.rows[row].size() || art.rows[row][column] != L'#')
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
    case kBlackholeEffect:
      return blackholeCell(row, column, frame);
    case kStarfieldEffect:
      return starfieldCell(row, column);
    case kRetroStarsEffect:
      return retroStarsCell(row, column);
    case kParallaxEffect:
      return parallaxCell(row, column);
    case kChaseEffect:
      return chaseCell(row, column, frame, art);
    default:
      return {L' ', {0.0f, 0.0f, 0.0f}};
  }
}

ParticleFrame particlesAt(const Frame& frame, const Art& art, ParticleViewport viewport) {
  if (viewport.width <= 0.0f) viewport.width = static_cast<float>(art.columns()) + 14.0f;
  if (viewport.height <= 0.0f) viewport.height = 18.0f / 1.75f;
  switch (frame.effect) {
    case kBlackholeEffect:
      return blackholeParticles(frame, art, viewport);
    case kStarfieldEffect:
      return starfieldParticles(frame, art, viewport);
    case kRetroStarsEffect:
      return retroStarsParticles(frame, art, viewport);
    case kParallaxEffect:
      return parallaxParticles(frame, art);
    case kChaseEffect:
      return chaseParticles(frame, art);
    default:
      return {};
  }
}

ChaseScene chaseSceneAt(const Frame& frame, const Art& art) {
  return frame.effect == kChaseEffect ? chaseScene(frame, art) : ChaseScene{};
}

unsigned nextFrameDelay(const Frame& frame) {
  if (frame.progress >= 6.0 &&
      (frame.effect == 0 || frame.effect == 1 || frame.effect == 5 || frame.effect == 6)) {
    return static_cast<unsigned>(std::ceil((kEffectDuration - frame.progress) * 1000.0));
  }
  return kFrameIntervalMs;
}

}  // namespace waku::screensaver
