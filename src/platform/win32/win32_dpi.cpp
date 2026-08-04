#include "win32_backend.h"

#include <windows.h>

#include <shellscalingapi.h>

namespace fsic::win32 {

namespace {

using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
using SetProcessDpiAwarenessFn = HRESULT(WINAPI*)(PROCESS_DPI_AWARENESS);
using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);

// Loaded once, kept for the process lifetime -- these are core system DLLs
// already resident in essentially every Windows process, so there is no
// benefit to FreeLibrary-ing them between calls.
HMODULE shcore_module() {
    static HMODULE h = LoadLibraryW(L"shcore.dll");
    return h;
}

} // namespace

// MUST run before any HWND/geometry API (enumerate_windows, capture_rect,
// etc). If this runs late, or a caller "helpfully" moves it into a
// constructor that fires after some other Win32 call, Windows silently
// virtualizes every subsequent coordinate on any non-100%-scaled display --
// no error, just a capture of the wrong screen rectangle. Do not reorder.
void ensure_dpi_awareness() {
    // Preferred: per-monitor V2 (Windows 10 1703+), resolved dynamically so
    // this still runs (falling through) on older Windows.
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        auto fn = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (fn && fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;
    }
    // Fallback: per-monitor V1 (Windows 8.1+).
    if (HMODULE shcore = shcore_module()) {
        auto fn =
            reinterpret_cast<SetProcessDpiAwarenessFn>(GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (fn && SUCCEEDED(fn(PROCESS_PER_MONITOR_DPI_AWARE))) return;
    }
    // Last resort: system-DPI-aware only (Windows Vista+).
    SetProcessDPIAware();
}

double scale_for_point(Point screen_px) {
    const POINT pt{screen_px.x, screen_px.y};
    const HMONITOR hmon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    UINT dpi_x = 96, dpi_y = 96;
    if (HMODULE shcore = shcore_module()) {
        auto fn = reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore, "GetDpiForMonitor"));
        if (fn) fn(hmon, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y);
    }
    return static_cast<double>(dpi_x) / 96.0;
}

Rect virtual_desktop_bounds() {
    return Rect{GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)};
}

} // namespace fsic::win32
