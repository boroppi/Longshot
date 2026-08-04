#include "win32_backend.h"

#include <windows.h>

#include <dwmapi.h>

#include <string>
#include <vector>

namespace fsic::win32 {

namespace {

std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int len =
        WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string s(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), len, nullptr, nullptr);
    return s;
}

std::string window_title(HWND hwnd) {
    const int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return {};
    std::wstring w(static_cast<size_t>(len), L'\0');
    GetWindowTextW(hwnd, w.data(), len + 1);
    return wide_to_utf8(w);
}

std::string window_app_name(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return {};
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return {};
    wchar_t buf[MAX_PATH];
    DWORD size = MAX_PATH;
    std::string result;
    if (QueryFullProcessImageNameW(proc, 0, buf, &size)) {
        const std::wstring full(buf, size);
        const size_t slash = full.find_last_of(L"\\/");
        const std::wstring base = (slash == std::wstring::npos) ? full : full.substr(slash + 1);
        result = wide_to_utf8(base);
    }
    CloseHandle(proc);
    return result;
}

bool is_cloaked(HWND hwnd) {
    int cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
        return cloaked != 0;
    }
    return false;
}

Rect frame_bounds(HWND hwnd) {
    RECT r{};
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))) {
        return Rect{r.left, r.top, r.right - r.left, r.bottom - r.top};
    }
    GetWindowRect(hwnd, &r);
    return Rect{r.left, r.top, r.right - r.left, r.bottom - r.top};
}

Rect client_bounds(HWND hwnd) {
    RECT r{};
    GetClientRect(hwnd, &r);  // client coords: r.left==r.top==0
    POINT top_left{0, 0};
    ClientToScreen(hwnd, &top_left);
    POINT bottom_right{r.right, r.bottom};
    ClientToScreen(hwnd, &bottom_right);
    return Rect{top_left.x, top_left.y, bottom_right.x - top_left.x, bottom_right.y - top_left.y};
}

WindowInfo make_window_info(HWND hwnd) {
    WindowInfo w;
    w.id = reinterpret_cast<uint64_t>(hwnd);
    w.title = window_title(hwnd);
    w.app_name = window_app_name(hwnd);
    w.frame_px = frame_bounds(hwnd);
    w.client_px = client_bounds(hwnd);
    w.scale = scale_for_point(Point{w.frame_px.x, w.frame_px.y});
    w.on_screen = IsWindowVisible(hwnd) && !is_cloaked(hwnd);
    return w;
}

BOOL CALLBACK enum_proc(HWND hwnd, LPARAM lparam) {
    auto* out = reinterpret_cast<std::vector<WindowInfo>*>(lparam);
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindowTextLengthW(hwnd) == 0) return TRUE;
    const LONG_PTR ex_style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (ex_style & WS_EX_TOOLWINDOW) return TRUE;
    if (is_cloaked(hwnd)) return TRUE;
    out->push_back(make_window_info(hwnd));
    return TRUE;
}

} // namespace

Status enumerate_windows(std::vector<WindowInfo>* out) {
    out->clear();
    EnumWindows(enum_proc, reinterpret_cast<LPARAM>(out));
    return Status::Ok();
}

Status window_at_point(Point screen_px, WindowInfo* out) {
    const POINT pt{screen_px.x, screen_px.y};
    HWND hwnd = WindowFromPoint(pt);
    if (!hwnd) return Status::Error("win32: no window at point");
    hwnd = GetAncestor(hwnd, GA_ROOT);
    if (!hwnd) return Status::Error("win32: no window at point");
    *out = make_window_info(hwnd);
    return Status::Ok();
}

Status get_window_bounds(uint64_t window_id, Rect* frame_px, Rect* client_px) {
    HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window_id));
    if (!IsWindow(hwnd)) return Status::Error("win32: invalid window id");
    *frame_px = frame_bounds(hwnd);
    *client_px = client_bounds(hwnd);
    return Status::Ok();
}

Status activate_window(uint64_t window_id) {
    HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window_id));
    if (!IsWindow(hwnd)) return Status::Error("win32: invalid window id");

    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);

    AllowSetForegroundWindow(ASFW_ANY);
    if (!SetForegroundWindow(hwnd)) {
        const DWORD target_thread = GetWindowThreadProcessId(hwnd, nullptr);
        const DWORD cur_thread = GetCurrentThreadId();
        if (target_thread != 0 && target_thread != cur_thread) {
            AttachThreadInput(cur_thread, target_thread, TRUE);
            SetForegroundWindow(hwnd);
            AttachThreadInput(cur_thread, target_thread, FALSE);
        }
    }
    Sleep(120);
    if (GetForegroundWindow() != hwnd) {
        return Status::Error("win32: could not bring window to foreground");
    }
    return Status::Ok();
}

} // namespace fsic::win32
