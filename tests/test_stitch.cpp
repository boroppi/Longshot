#include "fsic/stitch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "fsic/image.h"
#include "fsic/tuning.h"
#include "test_util.h"

using namespace fsic;

namespace {

struct Lcg {
    uint32_t state;
    explicit Lcg(uint32_t seed) : state(seed) {}
    uint32_t next() {
        state = state * 1103515245u + 12345u;
        return state;
    }
    int range(int lo, int hi_exclusive) {
        if (hi_exclusive <= lo) return lo;
        return lo + static_cast<int>(next() % static_cast<uint32_t>(hi_exclusive - lo));
    }
};

// A tall page made of horizontal 15px color bands with seeded random colors.
Image make_striped_page(int w, int h, Lcg& rng) {
    Image img;
    img.resize(w, h);
    std::fill(img.rgba.begin(), img.rgba.end(), static_cast<uint8_t>(255));
    for (int y = 0; y < h; y += 15) {
        const uint8_t r = static_cast<uint8_t>(rng.range(0, 256));
        const uint8_t g = static_cast<uint8_t>(rng.range(0, 256));
        const uint8_t b = static_cast<uint8_t>(rng.range(0, 256));
        const int band_h = std::min(15, h - y);
        for (int yy = y; yy < y + band_h; ++yy) {
            for (int x = 0; x < w; ++x) {
                uint8_t* p = img.row(yy) + static_cast<size_t>(x) * 4;
                p[0] = r;
                p[1] = g;
                p[2] = b;
                p[3] = 255;
            }
        }
    }
    return img;
}

Image make_noise(int w, int h, Lcg& rng) {
    Image img;
    img.resize(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t* p = img.row(y) + static_cast<size_t>(x) * 4;
            p[0] = static_cast<uint8_t>(rng.range(0, 256));
            p[1] = static_cast<uint8_t>(rng.range(0, 256));
            p[2] = static_cast<uint8_t>(rng.range(0, 256));
            p[3] = 255;
        }
    }
    return img;
}

std::vector<Image> slice_frames(const Image& page, int viewport_h, int step, int count) {
    std::vector<Image> frames;
    frames.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        frames.push_back(crop(page, Rect{0, i * step, page.w, viewport_h}));
    }
    return frames;
}

void add_jitter(Image& img, int jitter, Lcg& rng) {
    if (jitter <= 0) return;
    const size_t n = static_cast<size_t>(img.w) * static_cast<size_t>(img.h);
    for (size_t i = 0; i < n; ++i) {
        uint8_t* p = img.rgba.data() + i * 4;
        for (int c = 0; c < 3; ++c) {
            const int delta = rng.range(-jitter, jitter + 1);
            int v = static_cast<int>(p[c]) + delta;
            v = std::max(0, std::min(255, v));
            p[c] = static_cast<uint8_t>(v);
        }
    }
}

float psnr(const Image& a, const Image& b) {
    if (a.w != b.w || a.h != b.h) return -1.0f;
    double se = 0.0;
    const size_t n = static_cast<size_t>(a.w) * static_cast<size_t>(a.h);
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* pa = a.rgba.data() + i * 4;
        const uint8_t* pb = b.rgba.data() + i * 4;
        for (int c = 0; c < 3; ++c) {
            const double d = static_cast<double>(pa[c]) - static_cast<double>(pb[c]);
            se += d * d;
        }
    }
    const double mse = se / static_cast<double>(n * 3);
    if (mse <= 0.0) return 1000.0f;
    return static_cast<float>(10.0 * std::log10(255.0 * 255.0 / mse));
}

void test_uniform_no_jitter() {
    Lcg rng(999);
    const int w = 200, page_h = 1500, viewport_h = 300, step = 60;
    Image page = make_striped_page(w, page_h, rng);
    const int count = (page_h - viewport_h) / step + 1;
    std::vector<Image> frames = slice_frames(page, viewport_h, step, count);

    StitchOptions opts;
    Image out;
    StitchReport report;
    Status st = stitch_frames(frames, opts, &out, &report);
    FSIC_CHECK(st);
    for (int off : report.offsets) FSIC_CHECK_EQ(off, step);
    FSIC_CHECK_EQ(out.w, page.w);
    FSIC_CHECK_EQ(out.h, page.h);
    for (int y = 0; y < page.h; ++y) {
        for (int x = 0; x < page.w; ++x) {
            const uint8_t* e = page.row(y) + static_cast<size_t>(x) * 4;
            const uint8_t* g = out.row(y) + static_cast<size_t>(x) * 4;
            FSIC_CHECK_EQ(e[0], g[0]);
            FSIC_CHECK_EQ(e[1], g[1]);
            FSIC_CHECK_EQ(e[2], g[2]);
        }
    }
}

void test_uniform_with_jitter() {
    Lcg rng(4242);
    const int w = 200, page_h = 1500, viewport_h = 300, step = 60;
    Image page = make_striped_page(w, page_h, rng);
    const int count = (page_h - viewport_h) / step + 1;
    std::vector<Image> frames = slice_frames(page, viewport_h, step, count);

    Lcg jrng(777);
    for (auto& f : frames) add_jitter(f, 2, jrng);

    StitchOptions opts;
    Image out;
    StitchReport report;
    Status st = stitch_frames(frames, opts, &out, &report);
    FSIC_CHECK(st);
    for (int off : report.offsets) FSIC_CHECK_EQ(off, step);

    const float p = psnr(out, page);
    FSIC_CHECK(p > 40.0f);
}

void test_sticky_top() {
    Lcg rng(55);
    const int sticky_h = 60;
    const int content_w = 200, content_h = 1200, viewport_h = 300, step = 60;
    const int CH = viewport_h - sticky_h;

    Image sticky_band;
    sticky_band.resize(content_w, sticky_h);
    for (int y = 0; y < sticky_h; ++y)
        for (int x = 0; x < content_w; ++x) {
            uint8_t* p = sticky_band.row(y) + static_cast<size_t>(x) * 4;
            p[0] = 40;
            p[1] = 60;
            p[2] = 90;
            p[3] = 255;
        }

    Image content_page = make_striped_page(content_w, content_h, rng);
    const int count = (content_h - CH) / step + 1;

    std::vector<Image> frames;
    frames.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        Image f;
        f.resize(content_w, viewport_h);
        blit(f, 0, 0, sticky_band, Rect{0, 0, content_w, sticky_h});
        Image slice = crop(content_page, Rect{0, i * step, content_w, CH});
        blit(f, 0, sticky_h, slice, Rect{0, 0, content_w, CH});
        frames.push_back(f);
    }

    Band top, bottom;
    detect_static_bands(frames, &top, &bottom);
    FSIC_CHECK_EQ(top.y0, 0);
    FSIC_CHECK_EQ(top.y1, sticky_h);
    FSIC_CHECK_EQ(bottom.y0, viewport_h);
    FSIC_CHECK_EQ(bottom.y1, viewport_h);

    StitchOptions opts;
    Image out;
    StitchReport report;
    Status st = stitch_frames(frames, opts, &out, &report);
    FSIC_CHECK(st);
    FSIC_CHECK_EQ(report.top_band_h, sticky_h);
    FSIC_CHECK_EQ(out.h, sticky_h + content_h);

    for (int y = 0; y < sticky_h; ++y) {
        for (int x = 0; x < content_w; ++x) {
            const uint8_t* e = sticky_band.row(y) + static_cast<size_t>(x) * 4;
            const uint8_t* g = out.row(y) + static_cast<size_t>(x) * 4;
            FSIC_CHECK_EQ(e[0], g[0]);
            FSIC_CHECK_EQ(e[1], g[1]);
            FSIC_CHECK_EQ(e[2], g[2]);
        }
    }
}

void test_end_of_scroll() {
    Lcg rng(321);
    const int w = 200, page_h = 900, viewport_h = 300, step = 60;
    Image page = make_striped_page(w, page_h, rng);
    const int count = (page_h - viewport_h) / step + 1;
    std::vector<Image> frames = slice_frames(page, viewport_h, step, count);
    frames.push_back(frames.back());  // duplicate final frame: no further movement

    StitchOptions opts;
    OffsetResult r = find_vertical_offset(frames[frames.size() - 2], frames[frames.size() - 1], opts);
    FSIC_CHECK(r.offset <= STOP_OFFSET_PX);

    Image out;
    StitchReport report;
    Status st = stitch_frames(frames, opts, &out, &report);
    FSIC_CHECK(st);
    FSIC_CHECK_EQ(report.frames_used, count);
}

void test_low_confidence_on_noise() {
    Lcg rngA(111), rngB(222);
    Image a = make_noise(64, 64, rngA);
    Image b = make_noise(64, 64, rngB);
    StitchOptions opts;
    OffsetResult r = find_vertical_offset(a, b, opts);
    FSIC_CHECK(!r.confident);
}

void test_max_canvas_height_guard() {
    Lcg rng(1);
    const int w = 100, page_h = 1000, viewport_h = 200, step = 60;
    Image page = make_striped_page(w, page_h, rng);
    const int count = (page_h - viewport_h) / step + 1;
    std::vector<Image> frames = slice_frames(page, viewport_h, step, count);

    StitchOptions opts;
    opts.max_canvas_height = 250;  // deliberately too small
    Image out;
    StitchReport report;
    Status st = stitch_frames(frames, opts, &out, &report);
    FSIC_CHECK(!st);
}

} // namespace

int main() {
    test_uniform_no_jitter();
    test_uniform_with_jitter();
    test_sticky_top();
    test_end_of_scroll();
    test_low_confidence_on_noise();
    test_max_canvas_height_guard();
    if (g_fsic_test_failures == 0) std::printf("test_stitch: all checks passed\n");
    FSIC_TEST_MAIN_END
}
