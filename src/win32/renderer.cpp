#include "win32/renderer.h"

#include <algorithm>
#include <cmath>

namespace waku::screensaver {

Renderer::Renderer(ID2D1Factory* d2d, IDWriteFactory* write) : d2d_(d2d), write_(write) {}

void Renderer::resize(unsigned width, unsigned height) {
  if (target_) target_->Resize(D2D1::SizeU(width, height));
}

bool Renderer::createChaseGeometry() {
  if (!roundStroke_ && FAILED(d2d_->CreateStrokeStyle(
                           D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                                                       D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND),
                           nullptr, 0, &roundStroke_)))
    return false;
  // Unit-radius shapes facing +x (Pacman) and upright (ghost); scaled and moved per frame.
  constexpr std::array<float, 4> mouth{0.09f, 0.35f, 0.62f, 0.87f};
  for (size_t index = 0; index < pacman_.size(); ++index) {
    if (pacman_[index]) continue;
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(d2d_->CreatePathGeometry(&pacman_[index])) || FAILED(pacman_[index]->Open(&sink)))
      return false;
    const float x = std::cos(mouth[index]);
    const float y = std::sin(mouth[index]);
    sink->BeginFigure(D2D1::Point2F(0.0f, 0.0f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(x, -y));
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x, y), D2D1::SizeF(1.0f, 1.0f), 0.0f,
                                  D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close())) return false;
  }
  for (size_t index = 0; index < ghost_.size(); ++index) {
    if (ghost_[index]) continue;
    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(d2d_->CreatePathGeometry(&ghost_[index])) || FAILED(ghost_[index]->Open(&sink)))
      return false;
    const std::array<float, 5> skirt = index == 0
                                           ? std::array<float, 5>{1.0f, 0.68f, 1.0f, 0.68f, 1.0f}
                                           : std::array<float, 5>{0.68f, 1.0f, 0.68f, 1.0f, 0.68f};
    sink->BeginFigure(D2D1::Point2F(-1.0f, skirt[4]), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(-1.0f, 0.0f));
    sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(1.0f, 0.0f), D2D1::SizeF(1.0f, 1.0f), 0.0f,
                                  D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    for (size_t point = 0; point < 4; ++point)
      sink->AddLine(D2D1::Point2F(1.0f - 0.5f * static_cast<float>(point), skirt[point]));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    if (FAILED(sink->Close())) return false;
  }
  return true;
}

void Renderer::drawChase(const ChaseScene& scene, const Art& art, float left, float top,
                         float cellWidth, float cellHeight) {
  if (!createChaseGeometry()) return;
  const auto& maze = art.chase;
  const auto pixel = [&](float x, float y) {
    return D2D1::Point2F(left + x * cellWidth, top + y * cellHeight);
  };
  const float alpha = scene.wallAlpha;
  if (alpha > 0.0f) {
    // Glow pass, then a crisp core, so the word reads as a neon maze.
    for (const bool core : {false, true}) {
      for (const bool dim : {true, false}) {
        const float strength = (dim ? 0.45f : 1.0f) * alpha;
        brush_->SetColor(D2D1::ColorF(0.1f, 0.3f, 1.0f, core ? strength : strength * 0.18f));
        const float width = cellWidth * (core ? 0.1f : 0.34f);
        for (const auto& wall : maze.walls) {
          if (wall.dim != dim) continue;
          target_->DrawLine(pixel(wall.x0, wall.y0), pixel(wall.x1, wall.y1), brush_.Get(), width,
                            roundStroke_.Get());
        }
      }
    }
    brush_->SetColor(D2D1::ColorF(1.0f, 0.86f, 0.68f, 0.55f * alpha));
    for (size_t node = 0; node < maze.nodes.size(); ++node) {
      if (maze.nodes[node].kind != kTunnelNode || maze.eatTime[node] <= scene.cursor) continue;
      const float radius = cellWidth * 0.1f;
      target_->FillEllipse(D2D1::Ellipse(pixel(static_cast<float>(maze.nodes[node].x) + 0.5f,
                                               static_cast<float>(maze.nodes[node].y) + 0.5f),
                                         radius, radius),
                           brush_.Get());
    }
  }
  const float scale = cellWidth * 0.46f;
  if (scene.pacman.visible) {
    const auto& sprite = scene.pacman;
    const auto at = pixel(sprite.x, sprite.y);
    const float angle = std::atan2(sprite.dirY, sprite.dirX) * 57.29578f;
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
                          D2D1::Matrix3x2F::Rotation(angle, D2D1::Point2F(0.0f, 0.0f)) *
                          D2D1::Matrix3x2F::Translation(at.x, at.y));
    brush_->SetColor(D2D1::ColorF(1.0f, 0.85f, 0.1f));
    const size_t level = std::min<size_t>(static_cast<size_t>(sprite.phase * 4.0f), 3);
    target_->FillGeometry(pacman_[level].Get(), brush_.Get());
  }
  if (scene.ghost.visible) {
    const auto& sprite = scene.ghost;
    const auto at = pixel(sprite.x, sprite.y);
    target_->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
                          D2D1::Matrix3x2F::Translation(at.x, at.y));
    const bool white = sprite.scared && sprite.flash;
    brush_->SetColor(sprite.scared ? (white ? D2D1::ColorF(0.95f, 0.95f, 1.0f)
                                            : D2D1::ColorF(0.12f, 0.22f, 0.95f))
                                   : D2D1::ColorF(1.0f, 0.3f, 0.35f));
    target_->FillGeometry(ghost_[sprite.phase < 0.5f ? 0 : 1].Get(), brush_.Get());
    if (sprite.scared) {
      brush_->SetColor(white ? D2D1::ColorF(0.9f, 0.1f, 0.1f) : D2D1::ColorF(1.0f, 0.85f, 0.7f));
      for (const float eye : {-0.35f, 0.35f})
        target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(eye, -0.2f), 0.16f, 0.16f), brush_.Get());
    } else {
      for (const float eye : {-0.38f, 0.38f}) {
        brush_->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f));
        target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(eye, -0.2f), 0.3f, 0.36f), brush_.Get());
        brush_->SetColor(D2D1::ColorF(0.1f, 0.15f, 0.9f));
        target_->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(eye + sprite.dirX * 0.14f, -0.2f + sprite.dirY * 0.14f),
                          0.15f, 0.15f),
            brush_.Get());
      }
    }
  }
  target_->SetTransform(D2D1::Matrix3x2F::Identity());
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
      if (cell.character == L'.') {
        const float radius = cell.radius * cellWidth;
        target_->FillEllipse(
            D2D1::Ellipse(D2D1::Point2F(x + cellWidth * 0.5f, y + cellHeight * 0.5f), radius,
                          radius),
            brush_.Get());
      } else if (cell.character == L'\x2588') {
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
                                    : cell.character >= L'A' && cell.character <= L'Z'
                                        ? static_cast<unsigned>(cell.character - L'A') + 10U
                                    : cell.character == L'.' ? 36U
                                                             : 37U;
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
  if (frame.effect == kChaseEffect) {
    const ChaseScene scene = chaseSceneAt(frame, art);
    if (scene.active) drawChase(scene, art, left, top, cellWidth, cellHeight);
  }
  const ParticleFrame particles = particlesAt(
      frame, art, {static_cast<float>(width) / cellWidth, static_cast<float>(height) / cellHeight});
  for (size_t index = 0; index < particles.count; ++index) {
    const auto& particle = particles.particles[index];
    const float x = left + particle.x * cellWidth;
    const float y = top + particle.y * cellHeight;
    const float radius = std::max(1.0f, particle.radius * cellWidth);
    brush_->SetColor(D2D1::ColorF(particle.color.red, particle.color.green, particle.color.blue,
                                  particle.alpha));
    if (particle.tailX != 0.0f || particle.tailY != 0.0f) {
      target_->DrawLine(
          D2D1::Point2F(x - particle.tailX * cellWidth, y - particle.tailY * cellHeight),
          D2D1::Point2F(x, y), brush_.Get(), std::max(1.0f, radius * 0.65f));
    }
    target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius), brush_.Get());
  }
  if (FAILED(target_->EndDraw())) {
    brush_.Reset();
    target_.Reset();
  }
}

}  // namespace waku::screensaver
