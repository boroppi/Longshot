#pragma once
#include "fsic/backend.h"

namespace fsic {

class Win32Backend final : public IBackend {
public:
    Win32Backend() = default;
    ~Win32Backend() override = default;

    const char* name() const override { return "win32"; }
    Status init() override;
    void shutdown() override {}

    Status check_permissions(PermissionStatus* out) override;
    Status request_permissions() override;

    Status enumerate_windows(std::vector<WindowInfo>* out) override;
    Status window_at_point(Point screen_px, WindowInfo* out) override;
    Status get_window_bounds(uint64_t window_id, Rect* frame_px, Rect* client_px) override;
    Status activate_window(uint64_t window_id) override;

    Status get_virtual_desktop_bounds(Rect* out_px) override;
    double get_scale_for_point(Point screen_px) override;

    Status capture_rect(const Rect& rect_px, Image* out) override;

    Status get_cursor_position(Point* out_px) override;
    Status move_cursor(Point screen_px) override;
    Status inject_scroll(Point screen_px, ScrollDir dir, int notches) override;
    Status inject_key(uint64_t window_id, Key key) override;

    void sleep_ms(int ms) override;
};

// Free-function helpers, one implementation file per concern. Win32Backend's
// methods are thin delegates to these so each concern (DPI, capture, window
// enumeration, input injection) is independently readable/testable.
namespace win32 {

// win32_dpi.cpp -- MUST be called before any HWND/geometry API; see
// docs/permissions.md for why (a late call silently virtualizes every
// coordinate on non-100% displays).
void ensure_dpi_awareness();
double scale_for_point(Point screen_px);
Rect virtual_desktop_bounds();

// win32_capture.cpp
Status capture_rect(const Rect& rect_px, Image* out);

// win32_windows.cpp
Status enumerate_windows(std::vector<WindowInfo>* out);
Status window_at_point(Point screen_px, WindowInfo* out);
Status get_window_bounds(uint64_t window_id, Rect* frame_px, Rect* client_px);
Status activate_window(uint64_t window_id);

// win32_input.cpp
Status get_cursor_position(Point* out_px);
Status move_cursor(Point screen_px);
Status inject_scroll(Point screen_px, ScrollDir dir, int notches);
Status inject_key(uint64_t window_id, Key key);

} // namespace win32

} // namespace fsic
