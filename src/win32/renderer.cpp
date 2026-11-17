#include "win32/renderer.h"

#include <algorithm>
#include <cmath>

namespace waku::screensaver {

Renderer::Renderer(ID2D1Factory* d2d, IDWriteFactory* write) : d2d_(d2d), write_(write) {}

void Renderer::resize(unsigned width, unsigned height) {
  if (target_) target_->Resize(D2D1::SizeU(width, height));
}

void Renderer::render(HWND hwnd, const Frame& frame, const Art& art) {
  RECT rect{};
  if (!GetClientRect(hwnd, &rect)) return;
  const UINT width = static_cast<UINT>(std::max<LONG>(0, rect.right - rect.left));
  const UINT height = static_cast<UINT>(std::max<LONG>(0, rect.bottom - rect.top));
  if (!width || !height) return;
  if (!target_) {
    const auto properties = D2D1::RenderTargetProperties();
    const auto windowProperties =
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(width, height));
    if (FAILED(d2d_->CreateHwndRenderTarget(properties, windowProperties, &target_))) return;
    target_->SetDpi(96.0f, 96.0f);
    if (FAILED(target_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush_))) {
      target_.Reset();
      return;
    }
  }
  const float cellWidth =
      std::min(static_cast<float>(width) / (static_cast<float>(art.columns()) + 14.0f),
               static_cast<float>(height) / 18.0f);
  const float nextFontSize = std::max(1.0f, cellWidth * 1.8f);
  if (!text_ || std::abs(nextFontSize - fontSize_) > 0.1f) {
    text_.Reset();
    for (auto& glyph : glyphs_) glyph.Reset();
    fontSize_ = nextFontSize;
    if (FAILED(write_->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                        fontSize_, L"en-us", &text_)))
      return;
    text_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    text_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    text_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  }
  const float cellHeight = cellWidth * (frame.effect == 6 ? 1.0f : 1.75f);
  const float left = (static_cast<float>(width) - art.columns() * cellWidth) / 2.0f;
  const float top = (static_cast<float>(height) - art.rows.size() * cellHeight) / 2.0f;
  target_->BeginDraw();
  target_->Clear(D2D1::ColorF(0.005f, 0.012f, 0.015f));
  for (size_t row = 0; row < art.rows.size(); ++row) {
    const float entryDistance = top / cellHeight + static_cast<float>(row) + 1.0f;
    for (size_t column = 0; column < art.columns(); ++column) {
      const Cell cell = cellAt(row, column, frame, entryDistance, art);
      if (cell.character == L' ') continue;
      brush_->SetColor(D2D1::ColorF(cell.color.red, cell.color.green, cell.color.blue));
      const float x = left + static_cast<float>(column) * cellWidth;
      const float y = top + (static_cast<float>(row) + cell.verticalOffset) * cellHeight;
      if (cell.character == L'\x2588') {
        const bool rainDrop = frame.effect == 0 && cell.verticalOffset < -0.02f;
        if (rainDrop) {
          target_->FillRectangle(D2D1::RectF(x + cellWidth * 0.4f, y + cellHeight * 0.3f,
                                             x + cellWidth * 0.6f, y + cellHeight * 0.7f),
                                 brush_.Get());
        } else {
          target_->FillRectangle(D2D1::RectF(x, y, x + cellWidth, y + cellHeight), brush_.Get());
        }
      } else {
        const unsigned glyphIndex = cell.character >= L'0' && cell.character <= L'9'
                                        ? static_cast<unsigned>(cell.character - L'0')
                                        : static_cast<unsigned>(cell.character - L'A') + 10U;
        if (glyphIndex >= glyphs_.size()) continue;
        auto& glyph = glyphs_[glyphIndex];
        if (!glyph && FAILED(write_->CreateTextLayout(&cell.character, 1, text_.Get(), cellWidth,
                                                      cellHeight, glyph.GetAddressOf())))
          continue;
        target_->DrawTextLayout(D2D1::Point2F(x, y), glyph.Get(), brush_.Get(),
                                D2D1_DRAW_TEXT_OPTIONS_CLIP);
      }
    }
  }
  if (FAILED(target_->EndDraw())) {
    brush_.Reset();
    target_.Reset();
  }
}

}  // namespace waku::screensaver
