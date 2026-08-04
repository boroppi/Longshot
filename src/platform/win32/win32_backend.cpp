#include "win32_backend.h"

#include <windows.h>

namespace fsic {

Status Win32Backend::init() {
    win32::ensure_dpi_awareness();
    if (GetSystemMetrics(SM_CMONITORS) < 1) {
        return Status::Error("win32: no monitors detected");
    }
    return Status::Ok();
}

Status Win32Backend::check_permissions(PermissionStatus* out) {
    out->screen_capture = true;
    out->input_injection = true;
    out->detail.clear();
    // SendInput cannot deliver events into an elevated foreground window
    // from a non-elevated process (UIPI). We don't have a cheap way to
    // detect the TARGET window's elevation ahead of time, so just warn
    // generally when we ourselves are not elevated.
    BOOL is_elevated = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation{};
        DWORD size = sizeof(elevation);
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
            is_elevated = elevation.TokenIsElevated;
        }
        CloseHandle(token);
    }
    if (!is_elevated) {
        out->detail =
            "not running elevated; capture/injection into an elevated target window will fail "
            "silently (Windows UIPI)";
    }
    return Status::Ok();
}

Status Win32Backend::request_permissions() { return Status::Ok(); }

Status Win32Backend::enumerate_windows(std::vector<WindowInfo>* out) {
    return win32::enumerate_windows(out);
}

Status Win32Backend::window_at_point(Point screen_px, WindowInfo* out) {
    return win32::window_at_point(screen_px, out);
}

Status Win32Backend::get_window_bounds(uint64_t window_id, Rect* frame_px, Rect* client_px) {
    return win32::get_window_bounds(window_id, frame_px, client_px);
}

Status Win32Backend::activate_window(uint64_t window_id) { return win32::activate_window(window_id); }

Status Win32Backend::get_virtual_desktop_bounds(Rect* out_px) {
    *out_px = win32::virtual_desktop_bounds();
    return Status::Ok();
}

double Win32Backend::get_scale_for_point(Point screen_px) { return win32::scale_for_point(screen_px); }

Status Win32Backend::capture_rect(const Rect& rect_px, Image* out) {
    return win32::capture_rect(rect_px, out);
}

Status Win32Backend::get_cursor_position(Point* out_px) { return win32::get_cursor_position(out_px); }

Status Win32Backend::move_cursor(Point screen_px) { return win32::move_cursor(screen_px); }

Status Win32Backend::inject_scroll(Point screen_px, ScrollDir dir, int notches) {
    return win32::inject_scroll(screen_px, dir, notches);
}

Status Win32Backend::inject_key(uint64_t window_id, Key key) { return win32::inject_key(window_id, key); }

void Win32Backend::sleep_ms(int ms) { Sleep(static_cast<DWORD>(ms > 0 ? ms : 0)); }

} // namespace fsic
