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

inline constexpr uint32_t kNoNode = 0xFFFFFFFFU;

inline constexpr uint8_t kLetterNode = 0;
inline constexpr uint8_t kTunnelNode = 1;
inline constexpr uint8_t kHouseNode = 2;

// The word is a maze: letter cells are corridors, carved tunnels join separate letters.
struct ChaseMaze {
  struct Node {
    int x;
    int y;
    uint8_t kind;
  };
  struct Wall {
    float x0;
    float y0;
    float x1;
    float y1;
    bool dim;
  };
  std::vector<Node> nodes;
  std::vector<uint32_t> cellNode;   // row * columns + column of the art, kNoNode if unlit
  std::vector<uint32_t> pacWalk;    // node per step; every step moves to a 4-neighbour
  std::vector<float> pacTime;       // cumulative cost per step, 1.0 horizontal and 1.75 vertical
  std::vector<uint32_t> ghostWalk;  // node per step of pacWalk[0..feastSteps]
  std::vector<float> eatTime;       // per node, pacTime when first visited, infinity if never
  size_t feastSteps = 0;            // pacWalk[feastSteps] is the power pellet
  std::vector<Wall> walls;
};

struct Art {
  std::array<std::wstring, 7> rows;
  ChaseMaze chase;
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

struct ChaseSprite {
  float x = 0.0f;
  float y = 0.0f;
  float dirX = 1.0f;
  float dirY = 0.0f;
  float phase = 0.0f;  // pacman: mouth openness 0..1, ghost: skirt wobble 0..1
  bool visible = false;
  bool scared = false;
  bool flash = false;
};

struct ChaseScene {
  bool active = false;
  float cursor = -1.0f;  // pacTime units; compare against ChaseMaze::eatTime
  float wallAlpha = 0.0f;
  ChaseSprite pacman;
  ChaseSprite ghost;
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
ChaseScene chaseSceneAt(const Frame& frame, const Art& art = defaultArt());
unsigned nextFrameDelay(const Frame& frame);

}  // namespace waku::screensaver
