#pragma once
#include <cstdint>
#include <string>

namespace fsic {

struct Point {
    int x = 0;
    int y = 0;
};

struct Size {
    int w = 0;
    int h = 0;
};

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    int right() const { return x + w; }
    int bottom() const { return y + h; }
    bool empty() const { return w <= 0 || h <= 0; }

    Rect intersect(const Rect& o) const {
        int x0 = x > o.x ? x : o.x;
        int y0 = y > o.y ? y : o.y;
        int x1 = right() < o.right() ? right() : o.right();
        int y1 = bottom() < o.bottom() ? bottom() : o.bottom();
        if (x1 <= x0 || y1 <= y0) return Rect{0, 0, 0, 0};
        return Rect{x0, y0, x1 - x0, y1 - y0};
    }

    bool contains(Point p) const {
        return p.x >= x && p.x < right() && p.y >= y && p.y < bottom();
    }
};

struct WindowInfo {
    uint64_t id = 0;
    std::string title;
    std::string app_name;
    Rect frame_px;
    Rect client_px;
    double scale = 1.0;
    bool on_screen = true;
};

enum class ScrollDir { Up, Down };
enum class Key { PageUp, PageDown, Home, End };

struct PermissionStatus {
    bool screen_capture = true;
    bool input_injection = true;
    std::string detail;
};

struct Band {
    int y0 = 0;
    int y1 = 0;
};

} // namespace fsic
