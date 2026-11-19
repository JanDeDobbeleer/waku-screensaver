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
  size_t litCells = 0;
  for (size_t row = 0; row < customArt.rows.size(); ++row) {
    for (size_t column = 0; column < customArt.columns(); ++column) {
      if (customArt.rows[row][column] == L'#') {
        const size_t index = row * customArt.columns() + column;
        const size_t rank = customArt.chaseRank[index];
        check(rank < customArt.chasePath.size() && customArt.chasePath[rank] == index,
              "chase route covers each lit cell");
        ++litCells;
      }
    }
  }
  check(customArt.chasePath.size() == litCells, "chase route covers all lit cells once");
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
  const auto chaseStart = chaseFrame(0.0);
  check(cellAt(0, 0, chaseStart).character == L'.', "chase starts with dots across the word");
  check(cellAt(6, 34, chaseStart).character == L'.', "chase starts with dots across the word");
  check(particlesAt(chaseFrame(1.0)).count >= 3, "chase player has a particle trail");
  check(particlesAt(chaseFrame(2.5)).count >= 6, "chaser joins the route");
  const auto chaseEnd = chaseFrame(5.1);
  check(cellAt(0, 0, chaseEnd).character == L' ', "chase removes eaten dots");
  Art straight;
  for (auto& row : straight.rows) row = L"    ";
  straight.rows[0] = L"####";
  straight.chasePath = {0, 1, 2, 3};
  straight.chaseDistances = {0.0, 1.0, 2.0, 3.0};
  straight.chaseRank.assign(28, 0);
  double previousGap = 4.0;
  float previousPlayerX = 0.0f;
  for (double phase : {2.2, 3.0, 4.0, 4.9, 4.999}) {
    const auto runners = particlesAt(chaseFrame(phase, straight), straight);
    check(runners.count >= 6, "both runners remain visible until the catch");
    const auto& player = runners.particles[0];
    const auto& pursuer = runners.particles[3];
    const double gap = static_cast<double>(player.x - pursuer.x);
    check(gap >= 0.0 && gap < previousGap && player.x > previousPlayerX,
          "pursuer closes the gap while the player is still moving");
    previousGap = gap;
    previousPlayerX = player.x;
  }
  check(previousGap < 0.001, "pursuer reaches the player before the collision burst");
  const auto collision = chaseFrame(5.1);
  check(particlesAt(collision).count == 24, "chaser collision produces a burst");
  const auto restored = chaseFrame(7.6);
  check(cellAt(0, 0, restored).character == L'\x2588',
        "chase shockwave restores the word after the collision");
  const auto longChase = particlesAt(chaseFrame(2.5, longArt), longArt);
  check(longChase.count >= 6, "chase supports custom word paths");
  const Art& art = defaultArt();
  const size_t rank = art.chasePath.size() / 2;
  const size_t dot = art.chasePath[rank];
  const double arrival = (0.6 + art.chaseDistances[rank] / art.chaseDistances.back() * 4.4) *
                         chaseDuration / kEffectDuration;
  const auto dotBefore =
      cellAt(dot / art.columns(), dot % art.columns(), frameAt(arrival - 0.25, chaseOnly));
  const auto dotShrinking =
      cellAt(dot / art.columns(), dot % art.columns(), frameAt(arrival - 0.08, chaseOnly));
  check(dotBefore.character == L'.' && dotShrinking.character == L'.' &&
            dotBefore.radius > dotShrinking.radius && dotShrinking.radius > 0.0f,
        "centered dots shrink over multiple frames before being eaten");
  check(cellAt(dot / art.columns(), dot % art.columns(), frameAt(arrival + 0.01, chaseOnly))
                .character == L' ',
        "dot disappears when the player reaches it");
  const auto swallowed = particlesAt(frameAt(arrival + 0.15, chaseOnly));
  bool eatingVisible = false;
  for (size_t index = 0; index < swallowed.count; ++index)
    eatingVisible = eatingVisible || (swallowed.particles[index].color.green == 0.94f &&
                                      swallowed.particles[index].alpha > 0.0f);
  check(eatingVisible, "eating animation remains visible beyond a single frame");
  for (size_t index = 1; index < art.chasePath.size(); ++index) {
    const size_t previous = art.chasePath[index - 1];
    const size_t current = art.chasePath[index];
    const double length = std::hypot(static_cast<double>(current % art.columns()) -
                                         static_cast<double>(previous % art.columns()),
                                     (static_cast<double>(current / art.columns()) -
                                      static_cast<double>(previous / art.columns())) *
                                         1.75);
    check(std::abs(art.chaseDistances[index] - art.chaseDistances[index - 1] - length) < 0.0001,
          "route distance accounts for gaps and display aspect ratio");
    if (length > 2.0) {
      const double midpoint = (art.chaseDistances[index] + art.chaseDistances[index - 1]) * 0.5;
      const double time =
          (0.6 + midpoint / art.chaseDistances.back() * 4.4) * chaseDuration / kEffectDuration;
      const auto crossing = particlesAt(frameAt(time, chaseOnly));
      const float expectedX = (static_cast<float>(current % art.columns()) +
                               static_cast<float>(previous % art.columns())) *
                                  0.5f +
                              0.5f;
      const float expectedY = (static_cast<float>(current / art.columns()) +
                               static_cast<float>(previous / art.columns())) *
                                  0.5f +
                              0.5f;
      check(crossing.count >= 3 && std::abs(crossing.particles[0].x - expectedX) < 0.001f &&
                std::abs(crossing.particles[0].y - expectedY) < 0.001f,
            "runner crosses long gaps smoothly using actual distance");
    }
  }
  return failures ? 1 : 0;
}
