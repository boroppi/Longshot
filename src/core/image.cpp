#include "fsic/image.h"

#include <algorithm>
#include <limits>
#include <cstring>

namespace fsic {

uint8_t* Image::row(int y) { return rgba.data() + static_cast<size_t>(y) * w * 4; }
const uint8_t* Image::row(int y) const {
    return rgba.data() + static_cast<size_t>(y) * w * 4;
}
void Image::resize(int new_w, int new_h) {
    this->w = new_w;
    this->h = new_h;
    rgba.assign(static_cast<size_t>(new_w) * new_h * 4, 0);
}
bool Image::valid() const { return w > 0 && h > 0 && rgba.size() == static_cast<size_t>(w) * h * 4; }

Image crop(const Image& src, const Rect& r) {
    Rect clamped = Rect{0, 0, src.w, src.h}.intersect(r);
    Image out;
    if (clamped.empty()) return out;
    out.resize(clamped.w, clamped.h);
    for (int y = 0; y < clamped.h; ++y) {
        const uint8_t* sp = src.row(clamped.y + y) + static_cast<size_t>(clamped.x) * 4;
        std::memcpy(out.row(y), sp, static_cast<size_t>(clamped.w) * 4);
    }
    return out;
}

void blit(Image& dst, int dx, int dy, const Image& src, const Rect& src_rect) {
    Rect s = Rect{0, 0, src.w, src.h}.intersect(src_rect);
    if (s.empty()) return;
    Rect d = Rect{0, 0, dst.w, dst.h}.intersect(Rect{dx, dy, s.w, s.h});
    if (d.empty()) return;
    for (int y = 0; y < d.h; ++y) {
        const uint8_t* sp = src.row(s.y + (y + d.y - dy)) + static_cast<size_t>(s.x + (d.x - dx)) * 4;
        std::memcpy(dst.row(d.y + y) + static_cast<size_t>(d.x) * 4, sp,
                     static_cast<size_t>(d.w) * 4);
    }
}

void draw_rect_outline(Image& img, const Rect& r, uint8_t red, uint8_t green,
                       uint8_t blue, int thickness) {
    if (!img.valid() || thickness <= 0) return;
    int x0 = std::max(r.x, 0);
    int y0 = std::max(r.y, 0);
    int x1 = std::min(r.right(), img.w);
    int y1 = std::min(r.bottom(), img.h);
    if (x0 >= x1 || y0 >= y1) return;
    int t = thickness;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            bool border = x < x0 + t || x >= x1 - t || y < y0 + t || y >= y1 - t;
            if (border) {
                uint8_t* p = img.row(y) + static_cast<size_t>(x) * 4;
                p[0] = red;
                p[1] = green;
                p[2] = blue;
                p[3] = 255;
            }
        }
    }
}

float mean_abs_diff(const Image& a, const Image& b) {
    if (a.w != b.w || a.h != b.h) return std::numeric_limits<float>::max();
    if (!a.valid() || !b.valid()) return std::numeric_limits<float>::max();
    size_t n = static_cast<size_t>(a.w) * a.h;
    if (n == 0) return 0.0f;
    double total = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* pa = a.rgba.data() + i * 4;
        const uint8_t* pb = b.rgba.data() + i * 4;
        int dr = static_cast<int>(pa[0]) - pb[0];
        int dg = static_cast<int>(pa[1]) - pb[1];
        int db = static_cast<int>(pa[2]) - pb[2];
        total += std::abs(dr) + std::abs(dg) + std::abs(db);
    }
    return static_cast<float>(total / (n * 3));
}

} // namespace fsic


