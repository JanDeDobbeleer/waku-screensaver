#pragma once

#include "animation/animation.h"

namespace waku::screensaver {

inline uint32_t hash(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  return value ^ (value >> 16);
}

Cell rainCell(size_t row, size_t column, const Frame& frame, float entryDistance);
Cell decryptCell(size_t row, size_t column, const Frame& frame, float entryDistance);
Cell wavesCell(size_t row, size_t column, const Frame& frame, float entryDistance);
Cell beamsCell(size_t row, size_t column, const Frame& frame, float entryDistance);
Cell matrixCell(size_t row, size_t column, const Frame& frame, float entryDistance);
Cell burnCell(size_t row, size_t column, const Frame& frame, float entryDistance);
Cell blocksCell(size_t row, size_t column, const Frame& frame, float entryDistance);

}  // namespace waku::screensaver
