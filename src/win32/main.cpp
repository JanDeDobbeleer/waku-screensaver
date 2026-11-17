#include <windows.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <string_view>
#include <vector>

#include "animation/animation.h"
#include "win32/renderer.h"

using Microsoft::WRL::ComPtr;

namespace waku::screensaver {
namespace {

constexpr wchar_t kClassName[] = L"WakuScreensaverWindow";
constexpr UINT_PTR kTimer = 1;

enum class Mode { Config, Fullscreen, Preview, Demo, SelfTest, Invalid };

struct Arguments {
  Mode mode = Mode::Config;
  HWND parent = nullptr;
  unsigned effect = 0;
};

Arguments parseArguments(int argc, wchar_t** argv) {
  if (argc <= 1) return {};
  std::wstring_view arg(argv[1]);
  if (arg == L"--demo") {
    if (argc == 2) return {Mode::Demo};
    if (argc != 3) return {Mode::Invalid};
    const std::wstring_view option(argv[2]);
    constexpr std::wstring_view prefix = L"--effect=";
    if (!option.starts_with(prefix)) return {Mode::Invalid};
    const auto name = option.substr(prefix.size());
    for (unsigned i = 0; i < kEffectCount; ++i)
      if (name == kEffectNames[i]) return {Mode::Demo, nullptr, i};
    return {Mode::Invalid};
  }
  if (arg == L"--self-test") return {Mode::SelfTest};
  if (arg.size() < 2 || (arg[0] != L'/' && arg[0] != L'-')) return {Mode::Invalid};
  const wchar_t option = static_cast<wchar_t>(towlower(arg[1]));
  if (option == L's' && arg.size() == 2) return {Mode::Fullscreen};
  if (option == L'c' && (arg.size() == 2 || arg[2] == L':')) return {Mode::Config};
  if (option != L'p') return {Mode::Invalid};
  const wchar_t* value = nullptr;
  if (arg.size() > 3 && arg[2] == L':') value = argv[1] + 3;
  if (arg.size() == 2 && argc > 2) value = argv[2];
  if (!value || *value < L'0' || *value > L'9') return {Mode::Invalid};
  wchar_t* end = nullptr;
  const auto handle = std::wcstoull(value, &end, 10);
  if (*end || handle == 0 || handle > static_cast<unsigned long long>(UINTPTR_MAX))
    return {Mode::Invalid};
  return {Mode::Preview, reinterpret_cast<HWND>(static_cast<uintptr_t>(handle))};
}

bool selfTest() {
  wchar_t program[] = L"test";
  wchar_t preview[] = L"/p";
  wchar_t handle[] = L"123";
  wchar_t* args[] = {program, preview, handle};
  const auto parsed = parseArguments(3, args);
  if (parsed.mode != Mode::Preview || reinterpret_cast<uintptr_t>(parsed.parent) != 123)
    return false;
  wchar_t demo[] = L"--demo";
  wchar_t effect[] = L"--effect=blocks";
  wchar_t* demoArgs[] = {program, demo, effect};
  const auto demoParsed = parseArguments(3, demoArgs);
  return demoParsed.mode == Mode::Demo && demoParsed.effect == 6;
}

struct App;

struct Window {
  Window(App* owner, ID2D1Factory* d2d, IDWriteFactory* write)
      : app_(owner), renderer_(d2d, write) {}
  App* app_;
  Renderer renderer_;
  HWND hwnd_ = nullptr;
  bool preview_ = false;
  bool demo_ = false;
  bool haveMouse_ = false;
  POINT mouse_{};
  unsigned timerDelay_ = kFrameIntervalMs;
};

struct App {
  ComPtr<ID2D1Factory> d2d_;
  ComPtr<IDWriteFactory> write_;
  std::vector<std::unique_ptr<Window>> windows_;
  std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
  double initialOffset_ = 0.0;
  bool closing_ = false;
  bool active_ = false;

  ~App() {
    closing_ = true;
    for (auto& window : windows_)
      if (IsWindow(window->hwnd_)) DestroyWindow(window->hwnd_);
  }

  Frame frame() const {
    return frameAt(
        initialOffset_ +
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count());
  }
};

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  try {
    auto* window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
      window = static_cast<Window*>(create->lpCreateParams);
      window->hwnd_ = hwnd;
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    }
    if (!window) return DefWindowProcW(hwnd, message, wparam, lparam);
    switch (message) {
      case WM_CREATE:
        if (!SetTimer(hwnd, kTimer, kFrameIntervalMs, nullptr)) return -1;
        return 0;
      case WM_TIMER:
        if (wparam != kTimer) break;
        if (window->preview_ && !IsWindow(GetParent(hwnd))) PostQuitMessage(0);
        {
          const unsigned delay = nextFrameDelay(window->app_->frame());
          if (delay != window->timerDelay_ && SetTimer(hwnd, kTimer, delay, nullptr))
            window->timerDelay_ = delay;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
      case WM_SIZE:
        window->renderer_.resize(LOWORD(lparam), HIWORD(lparam));
        return 0;
      case WM_ERASEBKGND:
        return 1;
      case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(hwnd, &paint);
        try {
          window->renderer_.render(hwnd, window->app_->frame());
        } catch (...) {
          EndPaint(hwnd, &paint);
          throw;
        }
        EndPaint(hwnd, &paint);
        return 0;
      }
      case WM_SETCURSOR:
        if (!window->preview_ && !window->demo_) {
          SetCursor(nullptr);
          return TRUE;
        }
        break;
      case WM_ACTIVATEAPP:
        if (!wparam && !window->preview_ && !window->demo_ && window->app_->active_)
          PostQuitMessage(0);
        break;
      case WM_MOUSEMOVE:
        if (!window->preview_ && !window->demo_) {
          POINT position{};
          GetCursorPos(&position);
          if (window->haveMouse_ && (std::abs(position.x - window->mouse_.x) > 4 ||
                                     std::abs(position.y - window->mouse_.y) > 4))
            PostQuitMessage(0);
          if (!window->haveMouse_) window->mouse_ = position;
          window->haveMouse_ = true;
        }
        return 0;
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
      case WM_LBUTTONDOWN:
      case WM_RBUTTONDOWN:
      case WM_MBUTTONDOWN:
        if (!window->preview_ && !window->demo_) PostQuitMessage(0);
        return 0;
      case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
      case WM_DESTROY:
        KillTimer(hwnd, kTimer);
        if (!window->app_->closing_) PostQuitMessage(0);
        return 0;
      case WM_NCDESTROY:
        window->hwnd_ = nullptr;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
  } catch (...) {
    OutputDebugStringW(L"WAKU Screensaver: window callback failed.\n");
    PostQuitMessage(1);
    return message == WM_CREATE ? -1 : 0;
  }
}

bool createWindow(App& app, HINSTANCE instance, const RECT& rect, HWND parent, bool preview,
                  bool demo) {
  auto window = std::make_unique<Window>(&app, app.d2d_.Get(), app.write_.Get());
  window->preview_ = preview;
  window->demo_ = demo;
  auto* windowPtr = window.get();
  app.windows_.push_back(std::move(window));
  const DWORD style = preview ? WS_CHILD | WS_VISIBLE
                      : demo  ? WS_OVERLAPPEDWINDOW | WS_VISIBLE
                              : WS_POPUP | WS_VISIBLE;
  const DWORD exStyle = !preview && !demo ? WS_EX_TOPMOST | WS_EX_TOOLWINDOW : 0;
  HWND hwnd = CreateWindowExW(exStyle, kClassName, L"WAKU Screensaver", style, rect.left, rect.top,
                              rect.right - rect.left, rect.bottom - rect.top, parent, nullptr,
                              instance, windowPtr);
  if (!hwnd) {
    app.windows_.pop_back();
    return false;
  }
  return true;
}

struct MonitorContext {
  App* app;
  HINSTANCE instance;
  bool ok = true;
};

BOOL CALLBACK addMonitor(HMONITOR, HDC, LPRECT rect, LPARAM data) {
  auto& context = *reinterpret_cast<MonitorContext*>(data);
  try {
    if (createWindow(*context.app, context.instance, *rect, nullptr, false, false)) return TRUE;
  } catch (...) {
    OutputDebugStringW(L"WAKU Screensaver: monitor callback failed.\n");
  }
  context.ok = false;
  return FALSE;
}

int run(HINSTANCE instance, const Arguments& args) {
  if (args.mode == Mode::SelfTest) return selfTest() ? 0 : 1;
  if (args.mode == Mode::Invalid) return 1;
  if (args.mode == Mode::Config) {
    MessageBoxW(nullptr,
                L"WAKU Screensaver is an early prototype. No settings are "
                L"available yet.",
                L"WAKU Screensaver", MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  App app;
  app.initialOffset_ = args.effect * kEffectDuration;
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, app.d2d_.GetAddressOf())))
    return 1;
  if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(app.write_.GetAddressOf()))))
    return 1;
  WNDCLASSW cls{};
  cls.hInstance = instance;
  cls.lpfnWndProc = windowProc;
  cls.lpszClassName = kClassName;
  cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  if (!RegisterClassW(&cls)) return 1;
  if (args.mode == Mode::Preview) {
    RECT rect{};
    if (!IsWindow(args.parent) || !GetClientRect(args.parent, &rect) ||
        !createWindow(app, instance, rect, args.parent, true, false))
      return 1;
  } else if (args.mode == Mode::Demo) {
    const RECT rect{100, 100, 1060, 640};
    if (!createWindow(app, instance, rect, nullptr, false, true)) return 1;
  } else {
    MonitorContext context{&app, instance};
    if (!EnumDisplayMonitors(nullptr, nullptr, addMonitor, reinterpret_cast<LPARAM>(&context)) ||
        !context.ok || app.windows_.empty())
      return 1;
    SetForegroundWindow(app.windows_.front()->hwnd_);
    app.active_ = true;
  }
  MSG message{};
  BOOL result = 0;
  while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return result < 0 ? 1 : static_cast<int>(message.wParam);
}

}  // namespace
}  // namespace waku::screensaver

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  try {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;
    const auto args = waku::screensaver::parseArguments(argc, argv);
    LocalFree(argv);
    return waku::screensaver::run(instance, args);
  } catch (...) {
    OutputDebugStringW(L"WAKU Screensaver: startup failed.\n");
    return 1;
  }
}
