#include <cstdio>

#include "animation/animation.h"

using namespace waku::screensaver;

int main() {
  int failures = 0;
  const auto check = [&failures](bool condition, const char* message) {
    if (!condition) {
      std::fprintf(stderr, "%s\n", message);
      ++failures;
    }
  };
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
