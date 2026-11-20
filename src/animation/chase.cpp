#include <algorithm>
#include <cmath>

#include "animation/particle_internal.h"

namespace waku::screensaver {
namespace {

// Normalized timeline of the effect, in units of kEffectDuration.
constexpr double kRunStart = 0.5;
constexpr double kRunEnd = 6.7;
constexpr double kWaveStart = 7.25;
constexpr double kWaveDuration = 1.35;
// Share of the run spent on the dot sweep; the rest is Pacman hunting the frightened ghost.
constexpr double kFeastShare = 0.88;
constexpr double kPulseStart = kWaveStart + kWaveDuration;

double normalized(const Frame& frame) {
  return frame.progress / (effectDuration(kChaseEffect, frame.columns) / kEffectDuration);
}

float totalTime(const ChaseMaze& maze) { return maze.pacTime.empty() ? 0.0f : maze.pacTime.back(); }

// Position on the walk in pacTime units; negative before the run starts.
float cursorAt(double progress, const ChaseMaze& maze) {
  if (maze.pacTime.empty()) return 0.0f;
  const double run = (progress - kRunStart) / (kRunEnd - kRunStart);
  const double pellet = static_cast<double>(maze.pacTime[maze.feastSteps]);
  const double total = static_cast<double>(totalTime(maze));
  return static_cast<float>(run < kFeastShare ? run / kFeastShare * pellet
                                              : pellet + (run - kFeastShare) / (1.0 - kFeastShare) *
                                                             (total - pellet));
}

struct Located {
  size_t index;
  float fraction;
};

Located locate(const std::vector<float>& times, float cursor) {
  const float clamped = std::clamp(cursor, 0.0f, times.back());
  const auto next = std::upper_bound(times.begin(), times.end(), clamped);
  const size_t index = static_cast<size_t>(std::max<ptrdiff_t>(next - times.begin() - 1, 0));
  if (index + 1 >= times.size()) return {times.size() - 1, 0.0f};
  return {index, (clamped - times[index]) / (times[index + 1] - times[index])};
}

struct Point {
  float x;
  float y;
};

Point center(const ChaseMaze& maze, uint32_t node) {
  return {static_cast<float>(maze.nodes[node].x) + 0.5f,
          static_cast<float>(maze.nodes[node].y) + 0.5f};
}

Point between(const ChaseMaze& maze, uint32_t from, uint32_t to, float fraction) {
  const Point a = center(maze, from);
  const Point b = center(maze, to);
  return {a.x + (b.x - a.x) * fraction, a.y + (b.y - a.y) * fraction};
}

Point collisionPoint(const ChaseMaze& maze) {
  return center(maze, maze.ghostWalk.empty() ? maze.pacWalk.back() : maze.ghostWalk.back());
}

}  // namespace

ChaseScene chaseScene(const Frame& frame, const Art& art) {
  const ChaseMaze& maze = art.chase;
  ChaseScene scene;
  if (maze.pacWalk.empty()) return scene;
  const double progress = normalized(frame);
  scene.active = true;
  const float cursor = cursorAt(progress, maze);
  scene.cursor = cursor;
  scene.wallAlpha = progress < kRunStart
                        ? static_cast<float>(progress / kRunStart)
                        : static_cast<float>(std::clamp(
                              1.0 - (progress - kRunEnd) / (kWaveStart - kRunEnd) * 2.0, 0.0, 1.0));
  if (progress >= kRunEnd) return scene;

  const auto& walk = maze.pacWalk;
  const auto here = locate(maze.pacTime, cursor);
  const size_t next = std::min(here.index + 1, walk.size() - 1);
  const Point pacman = between(maze, walk[here.index], walk[next], here.fraction);
  scene.pacman.visible = true;
  scene.pacman.x = pacman.x;
  scene.pacman.y = pacman.y;
  size_t from = here.index;
  size_t to = next;
  if (from == to && from > 0) from = to - 1;
  if (from != to) {
    scene.pacman.dirX = static_cast<float>(maze.nodes[walk[to]].x - maze.nodes[walk[from]].x);
    scene.pacman.dirY = static_cast<float>(maze.nodes[walk[to]].y - maze.nodes[walk[from]].y);
  }
  const float chew = cursor < 0.0f ? static_cast<float>(progress) * 6.0f
                                   : static_cast<float>(here.index) + here.fraction;
  scene.pacman.phase = std::abs(std::sin(kPi * chew));

  if (!maze.ghostWalk.empty()) {
    const size_t pellet = maze.feastSteps;
    const auto ghostHere = locate(maze.pacTime, std::min(cursor, maze.pacTime[pellet]));
    const size_t ghostIndex = std::min(ghostHere.index, pellet);
    const size_t ghostNext = std::min(ghostIndex + 1, pellet);
    const Point ghost =
        between(maze, maze.ghostWalk[ghostIndex], maze.ghostWalk[ghostNext], ghostHere.fraction);
    scene.ghost.visible = true;
    scene.ghost.x = ghost.x;
    scene.ghost.y = ghost.y;
    scene.ghost.scared = cursor >= maze.pacTime[pellet];
    scene.ghost.flash = scene.ghost.scared && std::fmod(progress * 6.0, 1.0) < 0.5;
    scene.ghost.phase = static_cast<float>(std::fmod(frame.seconds * 5.0, 1.0));
    float dx = static_cast<float>(maze.nodes[maze.ghostWalk[ghostNext]].x -
                                  maze.nodes[maze.ghostWalk[ghostIndex]].x);
    float dy = static_cast<float>(maze.nodes[maze.ghostWalk[ghostNext]].y -
                                  maze.nodes[maze.ghostWalk[ghostIndex]].y);
    if (dx == 0.0f && dy == 0.0f) {
      dx = pacman.x - ghost.x;
      dy = (pacman.y - ghost.y) * 1.75f;
      if (std::abs(dx) >= std::abs(dy)) {
        dy = 0.0f;
        dx = dx < 0.0f ? -1.0f : 1.0f;
      } else {
        dx = 0.0f;
        dy = dy < 0.0f ? -1.0f : 1.0f;
      }
    }
    scene.ghost.dirX = dx;
    scene.ghost.dirY = dy;
  }
  return scene;
}

Cell chaseCell(size_t row, size_t column, const Frame& input, const Art& art) {
  const ChaseMaze& maze = art.chase;
  if (maze.pacWalk.empty() || column >= art.columns()) return {L' ', {0.0f, 0.0f, 0.0f}};
  const double progress = normalized(input);

  if (progress >= kWaveStart) {
    const Point collision = collisionPoint(maze);
    const float dx = static_cast<float>(column) + 0.5f - collision.x;
    const float dy = (static_cast<float>(row) + 0.5f - collision.y) * 1.75f;
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float maxDistance =
        std::hypot(static_cast<float>(art.columns()), static_cast<float>(art.rows.size()) * 1.75f);
    const float wave =
        static_cast<float>(std::clamp((progress - kWaveStart) / kWaveDuration, 0.0, 1.0));
    if (distance > wave * maxDistance) return {L' ', {0.0f, 0.0f, 0.0f}};
    const float pulse =
        progress < kPulseStart
            ? 1.0f
            : 0.86f +
                  0.14f * static_cast<float>(0.5 + 0.5 * std::sin((progress - kPulseStart) * 3.0));
    const float depth = 1.0f - distance / maxDistance;
    return {L'\x2588',
            {pulse * (0.06f + depth * 0.1f), pulse * (0.3f + depth * 0.28f),
             pulse * (0.48f + depth * 0.4f)}};
  }

  const uint32_t node = maze.cellNode[row * art.columns() + column];
  if (node == kNoNode || maze.eatTime[node] <= cursorAt(progress, maze))
    return {L' ', {0.0f, 0.0f, 0.0f}};
  const float twinkle = 0.8f + 0.2f * (0.5f + 0.5f * std::sin(static_cast<float>(progress) * 9.0f +
                                                              unitValue(node) * 2.0f * kPi));
  const float brightness = progress < kRunStart ? 0.35f + 0.4f * twinkle : 1.0f;
  const Color dot{brightness, brightness * 0.86f, brightness * 0.68f};
  if (maze.pacWalk.size() > 1 && node == maze.pacWalk[maze.feastSteps]) {
    const float blink = 0.5f + 0.5f * std::sin(static_cast<float>(progress) * 12.0f);
    return {L'.', {1.0f, 0.9f, 0.75f}, 0.0f, 0.26f + 0.08f * blink};
  }
  return {L'.', dot, 0.0f, 0.13f};
}

ParticleFrame chaseParticles(const Frame& input, const Art& art) {
  ParticleFrame result;
  if (art.chase.pacWalk.empty()) return result;
  const double progress = normalized(input);
  if (progress < kRunEnd || progress >= kWaveStart) return result;
  const Point collision = collisionPoint(art.chase);
  const float age = static_cast<float>((progress - kRunEnd) / (kWaveStart - kRunEnd));
  for (uint32_t index = 0; index < 24; ++index) {
    const uint32_t seed = hash(index + input.segment * 521U);
    const float angle = unitValue(seed) * 2.0f * kPi;
    const float distance = age * (0.3f + unitValue(seed + 1U) * 1.6f);
    addParticle(result, collision.x + std::cos(angle) * distance,
                collision.y + std::sin(angle) * distance, 0.06f + unitValue(seed + 2U) * 0.1f,
                1.0f - age, {1.0f, 0.53f, 0.25f});
  }
  return result;
}

}  // namespace waku::screensaver
