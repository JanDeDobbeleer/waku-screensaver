#include <cmath>

#include "animation/particle_internal.h"

namespace waku::screensaver {

Cell retroStarsCell(size_t row, size_t column) {
  const uint32_t seed = hash(static_cast<uint32_t>(row * kColumns + column));
  const float brightness = 0.8f + static_cast<float>(seed % 200U) / 1000.0f;
  return {L'\x2588', {0.06f * brightness, 0.13f * brightness, 0.31f * brightness}};
}

ParticleFrame retroStarsParticles(const Frame& frame, const Art& art) {
  ParticleFrame result;
  const float width = static_cast<float>(art.columns()) + 8.0f;
  for (uint32_t index = 0; index < result.particles.size(); ++index) {
    const uint32_t seed = hash(index + frame.segment * 997U);
    const unsigned layer = seed % 3U;
    const float speed = 0.12f + static_cast<float>(layer) * 0.11f;
    const float startX = unitValue(seed + 1U) * width;
    const float x = std::fmod(startX + static_cast<float>(frame.progress) * speed, width) - 4.0f;
    const float startY = unitValue(seed + 2U) * 11.0f - 2.0f;
    const float phase = unitValue(seed + 3U) * 2.0f * kPi;
    const float y = startY + std::sin(static_cast<float>(frame.progress) * 0.7f + phase) * 0.12f;
    const Color color = layer == 0   ? Color{0.54f, 0.72f, 1.0f}
                        : layer == 1 ? Color{0.8f, 0.88f, 1.0f}
                                     : Color{1.0f, 0.79f, 0.51f};
    addParticle(result, x, y, 0.05f + static_cast<float>(layer) * 0.035f,
                0.42f + static_cast<float>(layer) * 0.18f, color);
  }
  return result;
}

}  // namespace waku::screensaver
