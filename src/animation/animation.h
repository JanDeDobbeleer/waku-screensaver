#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace waku::screensaver {

inline constexpr std::array<std::wstring_view, 7> kArt{
    L"##    ##    ###    ##   ##  ##    ##", L"##    ##   ## ##   ##  ##   ##    ##",
    L"##    ##  ##   ##  ## ##    ##    ##", L"## ## ##  #######  ####     ##    ##",
    L"###  ###  ##   ##  ## ##    ##    ##", L"###  ###  ##   ##  ##  ##   ##    ##",
    L"##    ##  ##   ##  ##   ##   ###### ",
};
inline constexpr size_t kColumns = 36;
inline constexpr std::array<std::wstring_view, 12> kEffectNames{
    L"rain",   L"decrypt",   L"waves",     L"beams",       L"matrix",   L"burn",
    L"blocks", L"blackhole", L"starfield", L"retro-stars", L"parallax", L"chase"};
inline constexpr unsigned kEffectCount = static_cast<unsigned>(kEffectNames.size());
inline constexpr unsigned kBlackholeEffect = 7;
inline constexpr unsigned kStarfieldEffect = 8;
inline constexpr unsigned kRetroStarsEffect = 9;
inline constexpr unsigned kParallaxEffect = 10;
inline constexpr unsigned kChaseEffect = 11;
inline constexpr double kEffectDuration = 9.0;
inline constexpr unsigned kFrameIntervalMs = 33;

struct Art {
  std::array<std::wstring, 7> rows;
  std::vector<size_t> chasePath;
  std::vector<size_t> chaseRank;
  std::vector<double> chaseDistances;
  size_t columns() const { return rows[0].size(); }
};

const Art& defaultArt();
Art makeArt(std::string_view word);

struct Color {
  float red;
  float green;
  float blue;
};

struct Particle {
  float x = 0.0f;
  float y = 0.0f;
  float tailX = 0.0f;
  float tailY = 0.0f;
  float radius = 0.0f;
  float alpha = 0.0f;
  Color color{0.0f, 0.0f, 0.0f};
};

struct ParticleFrame {
  std::array<Particle, 96> particles{};
  size_t count = 0;
};

struct ParticleViewport {
  float width = 0.0f;
  float height = 0.0f;
};

struct Cell {
  wchar_t character;
  Color color;
  float verticalOffset = 0.0f;
  float radius = 0.0f;
};

struct Frame {
  unsigned effect;
  unsigned segment;
  double seconds;
  double progress;
  size_t columns = kColumns;
};

Frame frameAt(double seconds, std::span<const unsigned> effects = {}, size_t columns = kColumns);
double effectDuration(unsigned effect, size_t columns = kColumns);
Cell cellAt(size_t row, size_t column, const Frame& frame, float entryDistance = 10.0f,
            const Art& art = defaultArt());
ParticleFrame particlesAt(const Frame& frame, const Art& art = defaultArt(),
                          ParticleViewport viewport = {});
unsigned nextFrameDelay(const Frame& frame);

}  // namespace waku::screensaver
