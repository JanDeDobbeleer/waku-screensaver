#include <cmath>

#include "animation/particle_internal.h"

namespace waku::screensaver {

Cell parallaxCell(size_t row, size_t column) {
  const uint32_t seed = hash(static_cast<uint32_t>(row * kColumns + column));
  const float brightness = 0.7f + static_cast<float>(seed % 300U) / 1000.0f;
  return {L'\x2588', {0.025f * brightness, 0.17f * brightness, 0.2f * brightness}};
}

ParticleFrame parallaxParticles(const Frame& frame, const Art& art) {
  ParticleFrame result;
  for (uint32_t index = 0; index < result.particles.size(); ++index) {
    const uint32_t seed = hash(index + frame.segment * 313U);
    const unsigned layer = seed % 3U;
    const auto point = pointOnArt(art, seed + 1U);
    const float phase = unitValue(seed + 2U) * 2.0f * kPi;
    const float speed = 0.55f + static_cast<float>(layer) * 0.35f;
    const float x = point.first + std::sin(static_cast<float>(frame.progress) * speed + phase) *
                                      (0.04f + static_cast<float>(layer) * 0.035f);
    const float y =
        point.second + std::cos(static_cast<float>(frame.progress) * speed * 0.7f + phase) *
                           (0.04f + static_cast<float>(layer) * 0.035f);
    const float alpha =
        0.2f + 0.72f * (0.5f + 0.5f * std::sin(static_cast<float>(frame.progress) *
                                                   (1.0f + static_cast<float>(layer) * 0.7f) +
                                               phase));
    const Color color = layer == 0   ? Color{0.38f, 0.74f, 1.0f}
                        : layer == 1 ? Color{0.75f, 0.9f, 1.0f}
                                     : Color{0.48f, 1.0f, 0.84f};
    addParticle(result, x, y, 0.06f + static_cast<float>(layer) * 0.035f, alpha, color);
  }
  return result;
}

}  // namespace waku::screensaver
