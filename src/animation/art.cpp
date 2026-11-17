#include <cmath>

#include "animation/animation.h"

namespace waku::screensaver {
namespace {

void buildChasePath(Art& art) {
  const size_t columns = art.columns();
  art.chasePath.clear();
  art.chaseDistances.clear();
  art.chaseRank.assign(columns * art.rows.size(), 0);
  for (size_t column = 0; column < columns; ++column) {
    for (size_t step = 0; step < art.rows.size(); ++step) {
      const size_t row = column % 2 == 0 ? step : art.rows.size() - step - 1;
      if (column >= art.rows[row].size() || art.rows[row][column] != L'#') continue;
      const size_t index = row * columns + column;
      double distance = 0.0;
      if (!art.chasePath.empty()) {
        const size_t previous = art.chasePath.back();
        const double dx = static_cast<double>(column) - static_cast<double>(previous % columns);
        const double dy =
            (static_cast<double>(row) - static_cast<double>(previous / columns)) * 1.75;
        distance = art.chaseDistances.back() + std::hypot(dx, dy);
      }
      art.chaseDistances.push_back(distance);
      art.chaseRank[index] = art.chasePath.size();
      art.chasePath.push_back(index);
    }
  }
}

}  // namespace

const Art& defaultArt() {
  static const Art art = [] {
    Art result;
    for (size_t row = 0; row < kArt.size(); ++row) result.rows[row] = kArt[row];
    buildChasePath(result);
    return result;
  }();
  return art;
}

Art makeArt(std::string_view word) {
  if (word == "WAKU") return defaultArt();
  constexpr std::array<std::array<unsigned char, 7>, 36> glyphs{{
      {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
      {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
      {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {31, 4, 4, 4, 4, 4, 31},
      {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
      {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
      {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
      {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
      {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 27, 17}, {17, 17, 10, 4, 10, 17, 17},
      {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},     {14, 17, 19, 21, 25, 17, 14},
      {4, 12, 4, 4, 4, 4, 14},      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
      {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},   {14, 16, 16, 30, 17, 17, 14},
      {31, 1, 2, 4, 8, 8, 8},       {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14},
  }};
  Art art;
  for (char character : word) {
    const bool letter = character >= 'A' && character <= 'Z';
    const bool digit = character >= '0' && character <= '9';
    const size_t index = letter ? character - 'A' : digit ? character - '0' + 26 : 0;
    for (size_t row = 0; row < art.rows.size(); ++row) {
      auto& line = art.rows[row];
      if (!line.empty()) line += L' ';
      for (unsigned column = 0; column < 5; ++column)
        line += (letter || digit) && (glyphs[index][row] & (1U << (4 - column))) ? L'#' : L' ';
    }
  }
  buildChasePath(art);
  return art;
}

}  // namespace waku::screensaver
