#include <algorithm>
#include <cmath>

#include "animation/particle_internal.h"

namespace waku::screensaver {

Cell starfieldCell(size_t row, size_t column) {
  const uint32_t seed = hash(static_cast<uint32_t>(row * kColumns + column));
  const float brightness = 0.75f + static_cast<float>(seed % 250U) / 1000.0f;
  return {L'\x2588', {0.035f * brightness, 0.2f * brightness, 0.28f * brightness}};
}

ParticleFrame starfieldParticles(const Frame& frame, const Art& art, ParticleViewport viewport) {
  ParticleFrame result;
  const float centerX = static_cast<float>(art.columns()) * 0.5f;
  const float centerY = static_cast<float>(art.rows.size()) * 0.5f;
  for (uint32_t index = 0; index < result.particles.size(); ++index) {
    const uint32_t seed = hash(index + frame.segment * 719U);
    const float angle = unitValue(seed) * 2.0f * kPi;
    const float directionX = std::cos(angle);
    const float directionY = std::sin(angle) / 1.75f;
    const float edge = 0.55f / std::max(std::abs(directionX) / viewport.width,
                                        std::abs(directionY) / viewport.height);
    const float speed = 0.12f + unitValue(seed + 1U) * 0.14f;
    const float depth =
        std::fmod(unitValue(seed + 2U) + static_cast<float>(frame.progress) * speed, 1.0f);
    const float previousDepth = std::max(0.0f, depth - speed * 0.11f);
    const auto project = [&](float distance) {
      const float perspective = distance * distance;
      return std::pair<float, float>{centerX + directionX * edge * perspective,
                                     centerY + directionY * edge * perspective};
    };
    const auto point = project(depth);
    const auto previous = project(previousDepth);
    const float tailScale = depth > speed * 0.11f ? 1.0f : 0.0f;
    addParticle(result, point.first, point.second, 0.035f + depth * 0.14f, 0.35f + depth * 0.65f,
                {0.56f + depth * 0.44f, 0.78f + depth * 0.22f, 1.0f},
                (point.first - previous.first) * tailScale,
                (point.second - previous.second) * tailScale);
  }
  return result;
}

}  // namespace waku::screensaver
