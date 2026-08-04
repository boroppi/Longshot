#pragma once
#include <cstdint>
#include <vector>
#include "fsic/image.h"
#include "fsic/status.h"
#include "fsic/types.h"

namespace fsic {

class IBackend {
public:
    virtual ~IBackend() = default;

    // ---- lifecycle ----
    virtual const char* name() const = 0;
    virtual Status init() = 0;
    virtual void shutdown() = 0;

    // ---- permissions (macOS is the only OS that can deny; others report granted) ----
    virtual Status check_permissions(PermissionStatus* out) = 0;
    virtual Status request_permissions() = 0;

    // ---- window discovery ----
    virtual Status enumerate_windows(std::vector<WindowInfo>* out) = 0;
    virtual Status window_at_point(Point screen_px, WindowInfo* out) = 0;
    virtual Status get_window_bounds(uint64_t window_id, Rect* frame_px, Rect* client_px) = 0;
    virtual Status activate_window(uint64_t window_id) = 0;

    // ---- geometry / DPI ----
    virtual Status get_virtual_desktop_bounds(Rect* out_px) = 0;
    virtual double get_scale_for_point(Point screen_px) = 0;

    // ---- capture (the core primitive) ----
    // rect is physical device pixels in global desktop space.
    // out is RGBA8, exactly rect.w x rect.h, alpha forced to 255.
    virtual Status capture_rect(const Rect& rect_px, Image* out) = 0;

    // ---- input injection ----
    virtual Status get_cursor_position(Point* out_px) = 0;
    virtual Status move_cursor(Point screen_px) = 0;
    virtual Status inject_scroll(Point screen_px, ScrollDir dir, int notches) = 0;
    virtual Status inject_key(uint64_t window_id, Key key) = 0;

    // ---- clock (virtualised so the synthetic backend runs tests instantly) ----
    virtual void sleep_ms(int ms) = 0;
};

} // namespace fsic
