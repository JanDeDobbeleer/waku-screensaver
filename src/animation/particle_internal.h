#pragma once

#include <cmath>
#include <utility>

#include "animation/effect_internal.h"

namespace waku::screensaver {

inline constexpr float kPi = 3.14159265358979323846f;

inline float unitValue(uint32_t value) {
  return static_cast<float>(hash(value) >> 8) / 16777215.0f;
}

inline void addParticle(ParticleFrame& frame, float x, float y, float radius, float alpha,
                        Color color, float tailX = 0.0f, float tailY = 0.0f) {
  if (frame.count >= frame.particles.size()) return;
  frame.particles[frame.count++] = {x, y, tailX, tailY, radius, alpha, color};
}

inline std::pair<float, float> pointOnArt(const Art& art, uint32_t seed) {
  const size_t columns = art.columns();
  const size_t total = columns * art.rows.size();
  if (!total) return {0.0f, 0.0f};
  const size_t start = hash(seed) % total;
  for (size_t offset = 0; offset < total; ++offset) {
    const size_t index = (start + offset) % total;
    const size_t row = index / columns;
    const size_t column = index % columns;
    if (column < art.rows[row].size() && art.rows[row][column] == L'#')
      return {static_cast<float>(column) + 0.5f, static_cast<float>(row) + 0.5f};
  }
  return {static_cast<float>(columns) / 2.0f, static_cast<float>(art.rows.size()) / 2.0f};
}

}  // namespace waku::screensaver
