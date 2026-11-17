#include <algorithm>
#include <cmath>

#include "animation/particle_internal.h"

namespace waku::screensaver {

Cell blackholeCell(size_t row, size_t column, const Frame& frame) {
  if (frame.progress < 3.0) return {L' ', {0.0f, 0.0f, 0.0f}};
  const float centerX = static_cast<float>(frame.columns) * 0.5f;
  const float centerY = static_cast<float>(kArt.size()) * 0.5f;
  const float dx = (static_cast<float>(column) + 0.5f - centerX) / std::max(1.0f, centerX);
  const float dy = (static_cast<float>(row) + 0.5f - centerY) / 3.5f;
  const float reveal = 3.0f + std::sqrt(dx * dx + dy * dy) * 1.1f;
  if (frame.progress < reveal) {
    const uint32_t seed =
        hash(static_cast<uint32_t>(row * frame.columns + column) + frame.segment * 521U);
    return {static_cast<wchar_t>(L'A' + seed % 26U), {0.32f, 0.92f, 1.0f}};
  }
  const float warmth = static_cast<float>(std::clamp(frame.progress - reveal, 0.0, 1.0));
  return {L'\x2588', {0.95f, 0.38f + 0.42f * warmth, 0.16f + 0.78f * warmth}};
}

ParticleFrame blackholeParticles(const Frame& frame, const Art& art, ParticleViewport viewport) {
  ParticleFrame result;
  const float centerX = static_cast<float>(art.columns()) * 0.5f;
  const float centerY = static_cast<float>(art.rows.size()) * 0.5f;
  const float radiusX = viewport.width * 0.6f;
  const float radiusY = viewport.height * 0.6f;
  for (uint32_t index = 0; index < result.particles.size(); ++index) {
    const uint32_t seed = hash(index + frame.segment * 521U);
    const float phase = unitValue(seed) * 2.0f * kPi;
    const float outer = 0.55f + unitValue(seed + 1U) * 0.45f;
    float radius = outer;
    float alpha = 0.9f;
    float x = centerX;
    float y = centerY;
    Color color{0.62f, 0.82f, 1.0f};
    if (frame.progress < 3.0) {
      radius *= static_cast<float>(1.0 - frame.progress / 3.0);
      const float angle = phase + static_cast<float>(frame.progress) * 2.4f;
      x += std::cos(angle) * radiusX * radius;
      y += std::sin(angle) * radiusY * radius;
    } else if (frame.progress < 4.2) {
      radius *= static_cast<float>((frame.progress - 3.0) / 1.2);
      const float angle = phase + 7.2f;
      x += std::cos(angle) * radiusX * radius;
      y += std::sin(angle) * radiusY * radius;
      color = {1.0f, 0.72f, 0.32f};
      alpha = 1.0f;
    } else {
      const auto point = pointOnArt(art, seed);
      x = point.first;
      y = point.second;
      alpha = 0.25f +
              0.55f * (0.5f + 0.5f * std::sin(static_cast<float>(frame.progress) * 2.1f + phase));
      color = {0.38f, 0.9f, 1.0f};
    }
    const float tailX =
        frame.progress >= 3.0 && frame.progress < 4.2 ? (x - centerX) * 0.025f : 0.0f;
    const float tailY =
        frame.progress >= 3.0 && frame.progress < 4.2 ? (y - centerY) * 0.025f : 0.0f;
    addParticle(result, x, y, 0.06f + unitValue(seed + 2U) * 0.12f, alpha, color, tailX, tailY);
  }
  return result;
}

}  // namespace waku::screensaver
