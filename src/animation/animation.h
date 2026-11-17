#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace waku::screensaver {

inline constexpr std::array<std::wstring_view, 7> kArt{
    L"##    ##    ###    ##   ##  ##    ##", L"##    ##   ## ##   ##  ##   ##    ##",
    L"##    ##  ##   ##  ## ##    ##    ##", L"## ## ##  #######  ####     ##    ##",
    L"###  ###  ##   ##  ## ##    ##    ##", L"###  ###  ##   ##  ##  ##   ##    ##",
    L"##    ##  ##   ##  ##   ##   ###### ",
};
inline constexpr size_t kColumns = 36;
inline constexpr std::array<std::wstring_view, 7> kEffectNames{
    L"rain", L"decrypt", L"waves", L"beams", L"matrix", L"burn", L"blocks"};
inline constexpr unsigned kEffectCount = static_cast<unsigned>(kEffectNames.size());
inline constexpr double kEffectDuration = 9.0;
inline constexpr unsigned kFrameIntervalMs = 33;

struct Color {
  float red;
  float green;
  float blue;
};

struct Cell {
  wchar_t character;
  Color color;
  float verticalOffset = 0.0f;
};

struct Frame {
  unsigned effect;
  unsigned segment;
  double seconds;
  double progress;
};

Frame frameAt(double seconds);
Cell cellAt(size_t row, size_t column, const Frame& frame, float entryDistance = 10.0f);
unsigned nextFrameDelay(const Frame& frame);

}  // namespace waku::screensaver
