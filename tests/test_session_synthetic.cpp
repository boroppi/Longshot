#include "fsic/session.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "fsic/backend.h"
#include "fsic/image.h"
#include "fsic/image_io.h"
#include "fsic/tuning.h"
#include "platform/synthetic/synthetic_backend.h"
#include "test_util.h"

using namespace fsic;
namespace fs = std::filesystem;

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

void set_px(Image& img, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t* p = img.row(y) + static_cast<size_t>(x) * 4;
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = 255;
}

Image make_dense_page(int w, int h, Lcg& rng) {
    Image img;
    img.resize(w, h);
    int y = 0;
    while (y < h) {
        const int bar_h = rng.range(8, 13);
        const uint8_t r = static_cast<uint8_t>(rng.range(0, 256));
        const uint8_t g = static_cast<uint8_t>(rng.range(0, 256));
        const uint8_t b = static_cast<uint8_t>(rng.range(0, 256));
        for (int yy = y; yy < std::min(h, y + bar_h); ++yy) {
            for (int xx = 0; xx < w; ++xx) set_px(img, xx, yy, r, g, b);
        }
        y += bar_h;
    }
    return img;
}

void write_frames(const std::string& dir, const std::vector<Image>& frames) {
    fs::create_directories(dir);
    for (size_t i = 0; i < frames.size(); ++i) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "frame_%03d.bmp", static_cast<int>(i));
        write_bmp((fs::path(dir) / buf).string(), frames[i]);
    }
}

} // namespace

int main() {
    const int viewport_w = 150, page_h = 1200, viewport_h = 200, step = 100;
    Lcg rng(77);
    Image page = make_dense_page(viewport_w, page_h, rng);
    const int count = (page_h - viewport_h) / step + 1;  // 11, exact division

    std::vector<Image> frames;
    frames.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        frames.push_back(crop(page, Rect{0, i * step, viewport_w, viewport_h}));
    }

    const std::string dir = "test_session_synthetic";
    write_frames(dir, frames);

    SyntheticBackend backend(dir);
    FSIC_CHECK(backend.init());

    // Simulate "already scrolled partway down" before the session starts.
    backend.inject_scroll(Point{75, 100}, ScrollDir::Down, 5);

    CaptureConfig cfg;
    cfg.region = Rect{0, 0, viewport_w, viewport_h};
    cfg.wheel_point = Point{75, 100};
    cfg.window_id = 1;
    cfg.notches = 1;
    cfg.max_frames = 200;
    cfg.max_canvas_height = 30000;
    cfg.use_keys = false;
    cfg.scroll_to_top = true;

    CaptureResult result;
    Status st = run_capture_session(backend, cfg, &result);
    FSIC_CHECK(st);

    // Stopped early via STOP_CONFIRM_FRAMES, not by exhausting max_frames.
    FSIC_CHECK(result.frames_captured < cfg.max_frames);
    FSIC_CHECK_EQ(result.frames_captured, count + STOP_CONFIRM_FRAMES);
    FSIC_CHECK_EQ(result.report.frames_used, count);

    // Exact reconstruction of the full page proves scroll_to_top correctly
    // rewound to the real top (index 0) before capturing: if it had not,
    // the stitched output would be shorter and/or offset from page content.
    FSIC_CHECK_EQ(result.stitched.w, page.w);
    FSIC_CHECK_EQ(result.stitched.h, page.h);
    if (result.stitched.w == page.w && result.stitched.h == page.h) {
        for (int y = 0; y < page.h; ++y) {
            for (int x = 0; x < page.w; ++x) {
                const uint8_t* e = page.row(y) + static_cast<size_t>(x) * 4;
                const uint8_t* g = result.stitched.row(y) + static_cast<size_t>(x) * 4;
                FSIC_CHECK_EQ(e[0], g[0]);
                FSIC_CHECK_EQ(e[1], g[1]);
                FSIC_CHECK_EQ(e[2], g[2]);
            }
        }
    }

    if (g_fsic_test_failures == 0) std::printf("test_session_synthetic: all checks passed\n");
    FSIC_TEST_MAIN_END
}
