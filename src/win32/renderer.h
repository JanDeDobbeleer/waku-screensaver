#pragma once

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <array>

#include "animation/animation.h"

namespace waku::screensaver {

class Renderer {
 public:
  Renderer(ID2D1Factory* d2d, IDWriteFactory* write);
  void resize(unsigned width, unsigned height);
  void render(HWND hwnd, const Frame& frame);

 private:
  ID2D1Factory* d2d_;
  IDWriteFactory* write_;
  Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
  Microsoft::WRL::ComPtr<IDWriteTextFormat> text_;
  std::array<Microsoft::WRL::ComPtr<IDWriteTextLayout>, 36> glyphs_;
  float fontSize_ = 0.0f;
};

}  // namespace waku::screensaver
