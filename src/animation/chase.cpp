#include <algorithm>
#include <cmath>

#include "animation/particle_internal.h"

namespace waku::screensaver {
namespace {

constexpr double kPlayerStart = 0.6;
constexpr double kPlayerDuration = 4.4;
constexpr double kGhostStart = 2.1;
constexpr double kCollisionTime = kPlayerStart + kPlayerDuration;
constexpr double kWaveStart = kCollisionTime + 0.55;
constexpr double kWaveDuration = 1.35;

struct RoutePosition {
  double progress;
  float x;
  float y;
  float tailX;
  float tailY;
};

RoutePosition routePosition(const Art& art, double progress) {
  if (art.chasePath.empty()) return {};
  const double last = art.chaseDistances.back();
  const double cursor = std::clamp(progress, 0.0, last);
  const auto next = std::upper_bound(art.chaseDistances.begin(), art.chaseDistances.end(), cursor);
  const size_t firstIndex = static_cast<size_t>(next - art.chaseDistances.begin() - 1);
  const size_t secondIndex = std::min(firstIndex + 1, art.chasePath.size() - 1);
  const double length = art.chaseDistances[secondIndex] - art.chaseDistances[firstIndex];
  const float fraction =
      length > 0.0 ? static_cast<float>((cursor - art.chaseDistances[firstIndex]) / length) : 0.0f;
  const size_t columns = art.columns();
  const size_t first = art.chasePath[firstIndex];
  const size_t second = art.chasePath[secondIndex];
  const float firstX = static_cast<float>(first % columns) + 0.5f;
  const float firstY = static_cast<float>(first / columns) + 0.5f;
  const float secondX = static_cast<float>(second % columns) + 0.5f;
  const float secondY = static_cast<float>(second / columns) + 0.5f;
  const float x = firstX + (secondX - firstX) * fraction;
  const float y = firstY + (secondY - firstY) * fraction;
  const float tailScale = length > 0.0 ? static_cast<float>(0.6 / length) : 0.0f;
  return {cursor, x, y, (secondX - firstX) * tailScale, (secondY - firstY) * tailScale};
}

double playerPathProgress(double progress, const Art& art) {
  if (art.chasePath.empty()) return 0.0;
  const double last = art.chaseDistances.back();
  const double fraction = std::clamp((progress - kPlayerStart) / kPlayerDuration, 0.0, 1.0);
  return fraction * last;
}

double ghostPathProgress(double progress, const Art& art) {
  if (art.chasePath.empty()) return 0.0;
  const double fraction =
      std::clamp((progress - kGhostStart) / (kCollisionTime - kGhostStart), 0.0, 1.0);
  const double initialLead = playerPathProgress(kGhostStart, art);
  const double gap = initialLead * (1.0 - fraction) * (1.0 - fraction);
  return std::max(0.0, playerPathProgress(progress, art) - gap);
}

void addRunner(ParticleFrame& particles, const Art& art, const RoutePosition& position, Color color,
               float radius) {
  addParticle(particles, position.x, position.y, radius, 1.0f, color, position.tailX,
              position.tailY);
  for (unsigned trail = 1; trail <= 2; ++trail) {
    const auto afterimage =
        routePosition(art, std::max(0.0, position.progress - static_cast<double>(trail) * 0.7));
    addParticle(particles, afterimage.x, afterimage.y, radius * (trail == 1 ? 0.55f : 0.35f),
                trail == 1 ? 0.5f : 0.24f, color);
  }
}

}  // namespace

Cell chaseCell(size_t row, size_t column, const Frame& input, const Art& art) {
  if (art.chasePath.empty() || column >= art.columns()) return {L' ', {0.0f, 0.0f, 0.0f}};
  const double timeScale = effectDuration(kChaseEffect, input.columns) / kEffectDuration;
  Frame frame = input;
  frame.progress /= timeScale;

  if (frame.progress >= kWaveStart) {
    const auto collision = routePosition(art, art.chaseDistances.back());
    const float dx = static_cast<float>(column) + 0.5f - collision.x;
    const float dy = (static_cast<float>(row) + 0.5f - collision.y) * 1.75f;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float maxDistance =
        std::hypot(static_cast<float>(art.columns()), static_cast<float>(art.rows.size()) * 1.75f);
    const float wave =
        static_cast<float>(std::clamp((frame.progress - kWaveStart) / kWaveDuration, 0.0, 1.0));
    if (distance > wave * maxDistance) return {L' ', {0.0f, 0.0f, 0.0f}};
    const float pulse =
        frame.progress < 7.5
            ? 1.0f
            : 0.86f +
                  0.14f * static_cast<float>(0.5 + 0.5 * std::sin((frame.progress - 7.5) * 3.0));
    const float depth = 1.0f - distance / maxDistance;
    return {L'\x2588',
            {pulse * (0.06f + depth * 0.1f), pulse * (0.3f + depth * 0.28f),
             pulse * (0.48f + depth * 0.4f)}};
  }

  const size_t routeIndex = row * art.columns() + column;
  if (routeIndex >= art.chaseRank.size()) return {L' ', {0.0f, 0.0f, 0.0f}};
  const size_t rank = art.chaseRank[routeIndex];
  const double totalDistance = art.chaseDistances.back();
  const double arrival =
      (kPlayerStart +
       (totalDistance > 0.0 ? art.chaseDistances[rank] / totalDistance : 0.0) * kPlayerDuration) *
      timeScale;
  const double remaining = arrival - input.progress;
  if (remaining <= 0.0) return {L' ', {0.0f, 0.0f, 0.0f}};
  const float twinkle =
      0.8f +
      0.2f * (0.5f + 0.5f * std::sin(static_cast<float>(frame.progress) * 9.0f +
                                     unitValue(hash(static_cast<uint32_t>(rank))) * 2.0f * kPi));
  const float brightness = frame.progress < kPlayerStart ? 0.35f + 0.4f * twinkle : 1.0f;
  const float shrink = static_cast<float>(std::clamp(remaining / 0.18, 0.0, 1.0));
  return {L'.', {brightness * 0.36f, brightness * 0.78f, brightness}, 0.0f, 0.13f * shrink};
}

ParticleFrame chaseParticles(const Frame& input, const Art& art) {
  ParticleFrame result;
  if (art.chasePath.empty()) return result;
  const double timeScale = effectDuration(kChaseEffect, input.columns) / kEffectDuration;
  Frame frame = input;
  frame.progress /= timeScale;

  if (frame.progress >= kPlayerStart && frame.progress < kCollisionTime) {
    const auto player = routePosition(art, playerPathProgress(frame.progress, art));
    const float chew = 0.95f + 0.05f * static_cast<float>(std::sin(input.progress * 18.0));
    addRunner(result, art, player, {1.0f, 0.76f, 0.22f}, 0.32f * chew);
  }
  if (frame.progress >= kGhostStart && frame.progress < kCollisionTime) {
    const auto ghost = routePosition(art, ghostPathProgress(frame.progress, art));
    addRunner(result, art, ghost, {0.95f, 0.34f, 0.82f}, 0.28f);
  }

  if (frame.progress >= kPlayerStart &&
      frame.progress < kPlayerStart + kPlayerDuration + 0.22 / timeScale) {
    const double totalDistance = art.chaseDistances.back();
    const auto player = routePosition(art, playerPathProgress(frame.progress, art));
    for (size_t rank = 0; rank < art.chasePath.size() && result.count < result.particles.size();
         ++rank) {
      const double arrival =
          (kPlayerStart + (totalDistance > 0.0 ? art.chaseDistances[rank] / totalDistance : 0.0) *
                              kPlayerDuration) *
          timeScale;
      const double elapsed = input.progress - arrival;
      if (elapsed < 0.0 || elapsed >= 0.22) continue;
      const float age = static_cast<float>(elapsed / 0.22);
      const size_t index = art.chasePath[rank];
      const float x = static_cast<float>(index % art.columns()) + 0.5f;
      const float y = static_cast<float>(index / art.columns()) + 0.5f;
      addParticle(result, x + (player.x - x) * age, y + (player.y - y) * age, 0.13f * (1.0f - age),
                  1.0f - age, {1.0f, 0.94f, 0.68f});
    }
  }

  if (frame.progress >= kCollisionTime && frame.progress < kWaveStart) {
    const auto collision = routePosition(art, art.chaseDistances.back());
    const float age =
        static_cast<float>((frame.progress - kCollisionTime) / (kWaveStart - kCollisionTime));
    for (uint32_t index = 0; index < 24; ++index) {
      const uint32_t seed = hash(index + frame.segment * 521U);
      const float angle = unitValue(seed) * 2.0f * kPi;
      const float distance = age * (0.3f + unitValue(seed + 1U) * 1.6f);
      addParticle(result, collision.x + std::cos(angle) * distance,
                  collision.y + std::sin(angle) * distance, 0.06f + unitValue(seed + 2U) * 0.1f,
                  1.0f - age, {1.0f, 0.53f, 0.25f});
    }
  }
  return result;
}

}  // namespace waku::screensaver
