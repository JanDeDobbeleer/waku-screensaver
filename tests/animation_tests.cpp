#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "animation/animation.h"
#include "config.h"

using namespace waku::screensaver;

int main() {
  int failures = 0;
  const auto check = [&failures](bool condition, const char* message) {
    if (!condition) {
      std::fprintf(stderr, "%s\n", message);
      ++failures;
    }
  };
  const auto defaults = parseConfig("");
  check(defaults.word == "WAKU" && defaults.effects.size() == kEffectCount, "config defaults");
  const auto configured = parseConfig(
      "\xef\xbb\xbf; settings\r\n[screensaver]\r\n word = hello 42 \r\n"
      "animations = blocks, waves, blocks\r\n[other]\r\nword=IGNORED\r\n");
  check(configured.word == "HELLO 42", "config word normalization and section");
  check(configured.effects == std::vector<unsigned>({6, 2}),
        "config selection order and duplicates");
  for (const auto invalid :
       {"", "   ", "hello!", "caf\xc3\xa9", "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567"})
    check(parseConfig(std::string("[screensaver]\nword=") + invalid).word == "WAKU",
          "invalid word fallback");
  for (const auto invalid : {"", "rain,unknown", "blocks,", ",waves", "RAIN"})
    check(parseConfig(std::string("[screensaver]\nanimations=") + invalid).effects ==
              defaults.effects,
          "invalid animations fallback");
  const auto repeated =
      parseConfig("[screensaver]\nword=OK\nword=!\nanimations=burn\nanimations=unknown\n");
  check(repeated.word == "OK" && repeated.effects == std::vector<unsigned>({5}),
        "invalid settings preserve previous valid value");
  check(parseConfig("[screensaver]\nanimations=chase").effects == std::vector<unsigned>({11}),
        "parse arcade chase animation");
  check(parseConfig(kDefaultConfig).effects == defaults.effects, "default config round trip");
  const auto testPath =
      std::filesystem::temp_directory_path() /
      ("waku-screensaver-test-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".ini");
  check(loadConfig(testPath).word == "WAKU", "missing config fallback");
  {
    std::ofstream output(testPath, std::ios::binary);
    output << "[screensaver]\nword=FILE\nanimations=matrix\n";
  }
  const auto fromFile = loadConfig(testPath);
  check(fromFile.word == "FILE" && fromFile.effects == std::vector<unsigned>({4}),
        "load runtime config file");
  {
    std::ofstream output(testPath, std::ios::binary);
    output << "[screensaver]\nword=FILE\n" << std::string(16385, ' ');
  }
  check(loadConfig(testPath).word == "WAKU", "oversized config fallback");
  std::filesystem::remove(testPath);
  check(makeArt("WAKU").rows == defaultArt().rows, "original artwork preserved");
  const auto customArt = makeArt(configured.word);
  check(customArt.columns() == configured.word.size() * 6 - 1, "custom art width");
  check(
      customArt.rows[0][0] == L'#' && customArt.rows[0][1] == L' ' && customArt.rows[0][4] == L'#',
      "custom letter H");
  for (const auto& row : customArt.rows) {
    check(row.size() == customArt.columns(), "custom art uniform rows");
    check(row.substr(30, 5) == L"     ", "custom art space");
  }
  const auto checkMaze = [&](const Art& art, const char* label) {
    const auto& maze = art.chase;
    size_t lit = 0;
    for (size_t row = 0; row < art.rows.size(); ++row) {
      for (size_t column = 0; column < art.columns(); ++column) {
        if (art.rows[row][column] != L'#') continue;
        ++lit;
        const uint32_t node = maze.cellNode[row * art.columns() + column];
        check(node != kNoNode && maze.nodes[node].kind == ChaseMaze::kLetter &&
                  maze.nodes[node].x == static_cast<int>(column) &&
                  maze.nodes[node].y == static_cast<int>(row) && std::isfinite(maze.eatTime[node]),
              label);
      }
    }
    const auto adjacent = [&](uint32_t a, uint32_t b) {
      return std::abs(maze.nodes[a].x - maze.nodes[b].x) +
                 std::abs(maze.nodes[a].y - maze.nodes[b].y) ==
             1;
    };
    for (size_t step = 1; step < maze.pacWalk.size(); ++step) {
      check(adjacent(maze.pacWalk[step - 1], maze.pacWalk[step]),
            "pacman only moves to a neighbouring cell");
      const float cost = maze.pacTime[step] - maze.pacTime[step - 1];
      check(cost == 1.0f || cost == 1.75f, "pacman step cost follows the display aspect ratio");
    }
    for (size_t step = 0; step <= maze.feastSteps; ++step) {
      if (step > 0)
        check(maze.ghostWalk[step] == maze.ghostWalk[step - 1] ||
                  adjacent(maze.ghostWalk[step - 1], maze.ghostWalk[step]),
              "ghost only moves to a neighbouring cell");
      const auto& p = maze.nodes[maze.pacWalk[step]];
      const auto& g = maze.nodes[maze.ghostWalk[step]];
      check(std::abs(p.x - g.x) + std::abs(p.y - g.y) >= 2, "ghost never touches pacman");
      if (step < maze.feastSteps)
        check(maze.nodes[maze.pacWalk[step]].kind != ChaseMaze::kHouse,
              "pacman never enters the ghost house during the feast");
    }
    check(maze.feastSteps <= 3 * lit, "pacman sweep stays efficient");
    check(!maze.walls.empty(), "maze has walls");
  };
  checkMaze(customArt, "custom chase maze covers each lit cell");
  checkMaze(defaultArt(), "default chase maze covers each lit cell");
  check(makeArt("").chase.pacWalk.empty() && makeArt("  ").chase.nodes.empty(),
        "empty word has no maze");
  for (unsigned segment = 0; segment < 8; ++segment)
    check(frameAt(segment * kEffectDuration, configured.effects).effect ==
              configured.effects[segment % configured.effects.size()],
          "configured animation cycle");
  const std::vector<unsigned> single{5};
  check(frameAt(90.0, single).effect == 5, "single animation cycle");
  const auto longArt = makeArt("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345");
  for (unsigned effect = 0; effect < kEffectCount; ++effect) {
    const auto frame = frameAt(effect * kEffectDuration + 6.0, {}, longArt.columns());
    const auto later = frameAt(effect * kEffectDuration + 8.9, {}, longArt.columns());
    check(cellAt(0, longArt.columns(), frame, 10.0f, longArt).character == L' ',
          "custom art column bounds");
    check(cellAt(7, 0, frame, 10.0f, longArt).character == L' ', "custom art row bounds");
    for (size_t row = 0; row < longArt.rows.size(); ++row) {
      for (size_t column = 0; column < longArt.columns(); ++column) {
        const auto first = cellAt(row, column, frame, 10.0f, longArt);
        const auto last = cellAt(row, column, later, 10.0f, longArt);
        if (longArt.rows[row][column] == L' ') {
          check(first.character == L' ', "custom art background mask");
        } else if (effect == 0 || effect == 1 || effect == 5 || effect == 6) {
          check(first.character == L'\x2588' && first.character == last.character &&
                    first.color.red == last.color.red && first.color.green == last.color.green &&
                    first.color.blue == last.color.blue && first.verticalOffset == 0.0f &&
                    last.verticalOffset == 0.0f,
                "wide custom art fully settles before redraw pauses");
        }
      }
    }
  }
  for (auto line : kArt) check(line.size() == kColumns, "art width");
  for (unsigned effect = 0; effect < kEffectCount; ++effect) {
    for (double progress : {0.0, 0.5, 5.0, 8.9}) {
      const auto frame = frameAt(effect * kEffectDuration + progress);
      for (size_t row = 0; row < kArt.size(); ++row) {
        for (size_t column = 0; column < kColumns; ++column) {
          if (kArt[row][column] != L'#')
            check(cellAt(row, column, frame).character == L' ', "background mask");
        }
      }
      check(cellAt(kArt.size(), 0, frame).character == L' ', "row bounds");
      check(cellAt(0, kColumns, frame).character == L' ', "column bounds");
    }
    check(frameAt(effect * kEffectDuration).effect == effect, "effect start");
    check(frameAt(effect * kEffectDuration + effectDuration(effect)).effect ==
              (effect + 1) % kEffectCount,
          "effect transition");
    const bool animated = effect == 2 || effect == 3 || effect == 4 || effect >= 7;
    check(nextFrameDelay(frameAt(effect * kEffectDuration + 5.9)) == kFrameIntervalMs,
          "active frame delay");
    check(nextFrameDelay(frameAt(effect * kEffectDuration + 7.0)) ==
              (animated ? kFrameIntervalMs : 2000),
          "settled frame delay");
    check(nextFrameDelay(frameAt(effect * kEffectDuration + 8.999)) > 0, "transition wakeup");
    if (!animated) {
      const auto settled = frameAt(effect * kEffectDuration + 6.0);
      const auto later = frameAt(effect * kEffectDuration + 8.9);
      for (size_t row = 0; row < kArt.size(); ++row) {
        for (size_t column = 0; column < kColumns; ++column) {
          const auto first = cellAt(row, column, settled);
          const auto last = cellAt(row, column, later);
          check(first.character == last.character && first.color.red == last.color.red &&
                    first.color.green == last.color.green && first.color.blue == last.color.blue &&
                    first.verticalOffset == last.verticalOffset,
                "settled effect remains unchanged");
        }
      }
    }
  }
  const auto falling = cellAt(0, 0, frameAt(58.9), 10.0f);
  const auto neighbor = cellAt(1, 0, frameAt(58.9), 11.0f);
  check(falling.character == L'\x2588' && falling.verticalOffset < 0.0f, "falling block");
  check(falling.verticalOffset == neighbor.verticalOffset, "grouped blocks");
  check(cellAt(0, 0, frameAt(59.2), 10.0f).verticalOffset > falling.verticalOffset, "block motion");
  check(cellAt(0, 0, frameAt(61.0), 10.0f).verticalOffset == 0.0f, "block settled");
  check(cellAt(6, 0, frameAt(9.1)).character != L'\x2588', "decrypt starts as letters");
  check(cellAt(6, 0, frameAt(15.0)).character == L'\x2588', "decrypt settles");
  for (unsigned effect = kBlackholeEffect; effect <= kParallaxEffect; ++effect) {
    const auto first = particlesAt(frameAt(effect * kEffectDuration + 1.0));
    const auto later = particlesAt(frameAt(effect * kEffectDuration + 2.0));
    check(first.count == first.particles.size(), "star effect particle count");
    check(later.count == later.particles.size(), "star effect particle count at later frame");
    bool changed = false;
    for (size_t index = 0; index < first.count; ++index) {
      const auto& particle = first.particles[index];
      check(std::isfinite(particle.x) && std::isfinite(particle.y) &&
                std::isfinite(particle.radius) && std::isfinite(particle.alpha) &&
                particle.radius > 0.0f && particle.alpha >= 0.0f && particle.alpha <= 1.0f,
            "valid particle coordinates and appearance");
      changed = changed || particle.x != later.particles[index].x ||
                particle.y != later.particles[index].y ||
                particle.alpha != later.particles[index].alpha;
    }
    check(changed, "star particles animate over time");
    check(cellAt(0, 0, frameAt(effect * kEffectDuration + 5.0)).character != L' ',
          "star effects render within the art");
  }
  const auto noParticles = particlesAt(frameAt(0.0));
  check(noParticles.count == 0, "non-particle effects do not emit particles");
  for (const ParticleViewport viewport :
       {ParticleViewport{100.0f, 20.0f}, ParticleViewport{50.0f, 60.0f}}) {
    for (unsigned effect : {kBlackholeEffect, kStarfieldEffect}) {
      float reachX = 0.0f;
      float reachY = 0.0f;
      for (double progress : {0.0, 0.5, 1.0, 2.0, 3.8}) {
        const Frame frame{effect, 0, progress, progress};
        const auto particles = particlesAt(frame, defaultArt(), viewport);
        const auto larger =
            particlesAt(frame, defaultArt(), {viewport.width * 2.0f, viewport.height * 2.0f});
        for (size_t index = 0; index < particles.count; ++index) {
          const auto& particle = particles.particles[index];
          const auto& scaled = larger.particles[index];
          const float dx = particle.x - static_cast<float>(kColumns) * 0.5f;
          const float dy = particle.y - static_cast<float>(kArt.size()) * 0.5f;
          reachX = std::max(reachX, std::abs(dx) / (viewport.width * 0.5f));
          reachY = std::max(reachY, std::abs(dy) / (viewport.height * 0.5f));
          check(std::abs(scaled.x - static_cast<float>(kColumns) * 0.5f - dx * 2.0f) < 0.0001f &&
                    std::abs(scaled.y - static_cast<float>(kArt.size()) * 0.5f - dy * 2.0f) <
                        0.0001f &&
                    std::abs(scaled.tailX - particle.tailX * 2.0f) < 0.0001f &&
                    std::abs(scaled.tailY - particle.tailY * 2.0f) < 0.0001f &&
                    scaled.radius == particle.radius,
                "fullscreen particles and trails resize without enlarging the stars");
        }
      }
      check(reachX > 0.85f && reachY > 0.85f,
            "blackhole and starfield reach screen edges in landscape and portrait");
    }
    for (const Art& art : {defaultArt(), makeArt("I"), makeArt("FULLSCREEN STARS")}) {
      const float left = (static_cast<float>(art.columns()) - viewport.width) * 0.5f;
      const float top = (static_cast<float>(art.rows.size()) - viewport.height) * 0.5f;
      for (double progress : {0.0, 4.5, 8.9}) {
        const Frame frame{kRetroStarsEffect, 0, progress, progress};
        const auto particles = particlesAt(frame, art, viewport);
        float minX = viewport.width;
        float maxX = 0.0f;
        float minY = viewport.height;
        float maxY = 0.0f;
        for (size_t index = 0; index < particles.count; ++index) {
          const auto& particle = particles.particles[index];
          const float x = particle.x - left;
          const float y = particle.y - top;
          check(x >= -0.0001f && x <= viewport.width + 0.0001f && y >= -0.0001f &&
                    y <= viewport.height + 0.0001f,
                "retro stars wrap within the centered viewport");
          minX = std::min(minX, x);
          maxX = std::max(maxX, x);
          minY = std::min(minY, y);
          maxY = std::max(maxY, y);
        }
        check(minX < viewport.width * 0.1f && maxX > viewport.width * 0.9f &&
                  minY < viewport.height * 0.1f && maxY > viewport.height * 0.9f,
              "retro stars fill landscape and portrait viewports regardless of word width");
      }
    }
  }
  const Frame twinkles{kParallaxEffect, 0, 1.0, 1.0};
  const auto localTwinkles = particlesAt(twinkles);
  const auto wideTwinkles = particlesAt(twinkles, defaultArt(), {100.0f, 60.0f});
  for (size_t index = 0; index < localTwinkles.count; ++index)
    check(localTwinkles.particles[index].x == wideTwinkles.particles[index].x &&
              localTwinkles.particles[index].y == wideTwinkles.particles[index].y,
          "letter-bound twinkles are unaffected by the fullscreen viewport");
  const std::vector<unsigned> chaseOnly{kChaseEffect};
  const auto chaseFrame = [&](double phase, const Art& art = defaultArt()) {
    return frameAt(phase * effectDuration(kChaseEffect, art.columns()) / kEffectDuration, chaseOnly,
                   art.columns());
  };
  const std::vector<unsigned> mixed{kChaseEffect, 2, kChaseEffect, 0};
  const double chaseDuration = effectDuration(kChaseEffect);
  check(frameAt(chaseDuration - 0.01, mixed).effect == kChaseEffect &&
            frameAt(chaseDuration, mixed).effect == 2 &&
            frameAt(chaseDuration + kEffectDuration, mixed).effect == kChaseEffect &&
            frameAt(2 * chaseDuration + kEffectDuration, mixed).effect == 0,
        "mixed playlist respects chase duration");
  check(frameAt(2 * chaseDuration + 2 * kEffectDuration, mixed).segment == 4,
        "mixed playlist wraps with the correct segment");
  check(frameAt(chaseDuration, chaseOnly).segment == 1 &&
            frameAt(chaseDuration, chaseOnly).progress == 0.0,
        "chase-only demo repeats after its longer duration");
  check(effectDuration(kChaseEffect, longArt.columns()) > chaseDuration &&
            effectDuration(0, longArt.columns()) == kEffectDuration,
        "wide words extend only chase timing");
  checkMaze(longArt, "wide chase maze covers each lit cell");
  const Art& art = defaultArt();
  const auto& maze = art.chase;
  const auto chaseStart = chaseFrame(0.0);
  check(cellAt(0, 0, chaseStart).character == L'.', "chase starts with dots across the word");
  check(cellAt(6, 34, chaseStart).character == L'.', "chase starts with dots across the word");
  check(particlesAt(chaseStart).count == 0 && particlesAt(chaseFrame(3.0)).count == 0,
        "runners are sprites without trails");
  const auto early = chaseSceneAt(chaseFrame(0.2));
  check(early.active && early.pacman.visible && early.ghost.visible && !early.ghost.scared,
        "pacman and ghost wait for the run");
  const auto midway = chaseSceneAt(chaseFrame(3.0));
  check(midway.pacman.visible && midway.ghost.visible && !midway.ghost.scared &&
            std::abs(midway.pacman.dirX) + std::abs(midway.pacman.dirY) == 1.0f,
        "pacman faces along an axis during the feast");
  check(chaseSceneAt(Frame{2, 0, 1.0, 1.0}).active == false, "other effects have no chase scene");
  // Dots vanish exactly when pacman arrives.
  const size_t probe = maze.pacWalk.size() / 3;
  const uint32_t probeNode = maze.pacWalk[probe];
  if (maze.nodes[probeNode].kind == ChaseMaze::kLetter) {
    const double arrival = (0.5 + maze.eatTime[probeNode] / maze.pacTime[maze.feastSteps] * 5.456) *
                           chaseDuration / kEffectDuration;
    const auto& at = maze.nodes[probeNode];
    const auto row = static_cast<size_t>(at.y);
    const auto column = static_cast<size_t>(at.x);
    const auto before = cellAt(row, column, frameAt(arrival - 0.05, chaseOnly));
    check(before.character == L'.' && before.radius > 0.1f, "dot keeps its size until eaten");
    check(cellAt(row, column, frameAt(arrival + 0.05, chaseOnly)).character == L' ',
          "dot disappears when pacman reaches it");
    const auto scene = chaseSceneAt(frameAt(arrival, chaseOnly));
    check(std::abs(scene.pacman.x - (static_cast<float>(at.x) + 0.5f)) < 0.02f &&
              std::abs(scene.pacman.y - (static_cast<float>(at.y) + 0.5f)) < 0.02f,
          "pacman is on the dot when it is eaten");
  }
  const double pelletPhase = 0.5 + 5.456;
  check(!chaseSceneAt(chaseFrame(pelletPhase - 0.05)).ghost.scared &&
            chaseSceneAt(chaseFrame(pelletPhase + 0.05)).ghost.scared,
        "power pellet frightens the ghost");
  const auto caught = chaseSceneAt(chaseFrame(6.69));
  check(std::abs(caught.pacman.x - caught.ghost.x) < 0.2f &&
            std::abs(caught.pacman.y - caught.ghost.y) < 0.2f,
        "pacman catches the ghost");
  check(cellAt(0, 0, chaseFrame(6.6)).character == L' ', "chase removes eaten dots");
  check(particlesAt(chaseFrame(6.9)).count == 24, "catching the ghost produces a burst");
  check(!chaseSceneAt(chaseFrame(6.9)).pacman.visible, "runners vanish with the burst");
  check(cellAt(0, 0, chaseFrame(8.7)).character == L'\x2588',
        "chase shockwave restores the word after the catch");
  check(chaseSceneAt(chaseFrame(2.0, longArt), longArt).pacman.visible,
        "chase supports custom word mazes");
  return failures ? 1 : 0;
}
