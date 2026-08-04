#pragma once
#include <cstdint>
#include <vector>
#include "fsic/types.h"

namespace fsic {

struct Image {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> rgba;

    uint8_t* row(int y);
    const uint8_t* row(int y) const;
    void resize(int w, int h);
    bool valid() const;
};

Image crop(const Image& src, const Rect& r);
void blit(Image& dst, int dx, int dy, const Image& src, const Rect& src_rect);
void draw_rect_outline(Image& img, const Rect& r, uint8_t red, uint8_t green,
                       uint8_t blue, int thickness);
float mean_abs_diff(const Image& a, const Image& b);

} // namespace fsic
