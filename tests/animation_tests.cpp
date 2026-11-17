#include <chrono>
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
    check(frameAt((effect + 1) * kEffectDuration).effect == (effect + 1) % kEffectCount,
          "effect transition");
    const bool animated = effect == 2 || effect == 3 || effect == 4;
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
  return failures ? 1 : 0;
}
