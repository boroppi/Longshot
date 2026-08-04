#include "fsic/probe.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
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

// Dense edge-to-edge random-colored bands: no row is ever blank, so a
// motion-diff comparator always sees a real difference when the underlying
// scroll offset changes (see tools/gen_testdata.cpp for the same recipe).
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

bool near(int a, int b, int tol) { return std::abs(a - b) <= tol; }

// A per-row grayscale value assigned by modular arithmetic: color(y) =
// (y*mult) mod 256. For any FIXED row offset d, color(y+d) - color(y) is a
// constant (either d*mult mod 256, or that value minus 256 if it wraps) --
// deterministic and independent of y, so picking mult/d so that value stays
// far from 0 and 256 guarantees every single row shows a large, reliable
// difference. No randomness, no chance of coincidental near-matches.
uint8_t scanline_color(int y, int mult) { return static_cast<uint8_t>(((y % 256) * mult) % 256); }

void fill_scanline_block(Image& img, int dx, int w, int row_offset, int mult) {
    for (int y = 0; y < img.h; ++y) {
        const uint8_t v = scanline_color(row_offset + y, mult);
        for (int x = dx; x < dx + w; ++x) set_px(img, x, y, v, v, v);
    }
}

void test_two_scroller_page() {
    const int W = 350, H = 500;
    const int sidebar_w = 100, main_x = 100, main_w = 200, static_x = 300, static_w = 50;
    const int kFrames = 15;
    // Rates chosen (with mult=97, notches=3) so the per-step offset maps to
    // a modular color delta far from 0/256 for every row: sidebar delta=83,
    // main delta=61 (see scanline_color's guarantee above).
    const int sidebar_rate = 17, main_rate = 31, mult = 97;

    // Fallback frame: only the static block matters here (the sidebar/main
    // columns get overwritten every capture by whichever track is current).
    Image fallback;
    fallback.resize(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = static_x; x < static_x + static_w; ++x) set_px(fallback, x, y, 128, 128, 128);

    const std::string dir = "test_probe_two_scroller";
    write_frames(dir, {fallback});

    SyntheticBackend backend(dir);
    FSIC_CHECK(backend.init());

    // Sidebar and main are registered as INDEPENDENT tracks: inject_scroll at
    // a point inside one track's rect only advances that track, exactly
    // matching how a real browser scrolls only the container under the
    // cursor. Without this, both would move in lockstep and the probe could
    // never distinguish them by anchor (see the two failed test iterations
    // this test went through before track support was added).
    std::vector<Image> sidebar_track, main_track;
    for (int i = 0; i < kFrames; ++i) {
        Image s;
        s.resize(sidebar_w, H);
        fill_scanline_block(s, 0, sidebar_w, i * sidebar_rate, mult);
        sidebar_track.push_back(std::move(s));

        Image m;
        m.resize(main_w, H);
        fill_scanline_block(m, 0, main_w, i * main_rate, mult);
        main_track.push_back(std::move(m));
    }
    backend.set_tracks({Rect{0, 0, sidebar_w, H}, Rect{main_x, 0, main_w, H}},
                        {sidebar_track, main_track});

    ProbeOptions opts;
    opts.search_bounds = Rect{0, 0, W, H};

    ProbeResult main_result;
    Status st = probe_scroll_region(backend, Point{200, 100}, opts, &main_result);
    FSIC_CHECK(st);
    if (st) {
        FSIC_CHECK(near(main_result.region.x, main_x, 4));
        FSIC_CHECK(near(main_result.region.y, 0, 4));
        FSIC_CHECK(near(main_result.region.right(), main_x + main_w, 4));
        FSIC_CHECK(near(main_result.region.bottom(), H, 4));
    }

    ProbeResult side_result;
    st = probe_scroll_region(backend, Point{50, 100}, opts, &side_result);
    FSIC_CHECK(st);
    if (st) {
        FSIC_CHECK(near(side_result.region.x, 0, 4));
        FSIC_CHECK(near(side_result.region.y, 0, 4));
        FSIC_CHECK(near(side_result.region.right(), sidebar_w, 4));
        FSIC_CHECK(near(side_result.region.bottom(), H, 4));
    }
}

// Full-width scanline pattern that scrolls every row EXCEPT a fixed band
// [gap_y0, gap_y1) held at a constant value regardless of frame -- stands in
// for a paragraph/section gap inside an otherwise-scrolling page.
Image make_scrolling_frame_with_gap(int w, int h, int row_offset, int mult, int gap_y0, int gap_y1) {
    Image img;
    img.resize(w, h);
    for (int y = 0; y < h; ++y) {
        const uint8_t v = (y >= gap_y0 && y < gap_y1) ? static_cast<uint8_t>(128)
                                                        : scanline_color(row_offset + y, mult);
        for (int x = 0; x < w; ++x) set_px(img, x, y, v, v, v);
    }
    return img;
}

void test_probe_bridges_paragraph_gap() {
    const int W = 100, H = 400;
    const int kFrames = 15;
    const int mult = 97, rate = 31;  // same as test_two_scroller_page's main_rate: guaranteed large per-row delta
    const int gap_y0 = 180, gap_y1 = 220;  // 40px static band: > old 24px threshold, well under the new one

    std::vector<Image> frames;
    for (int i = 0; i < kFrames; ++i)
        frames.push_back(make_scrolling_frame_with_gap(W, H, i * rate, mult, gap_y0, gap_y1));

    const std::string dir = "test_probe_gap_bridge";
    write_frames(dir, frames);
    SyntheticBackend backend(dir);
    FSIC_CHECK(backend.init());

    ProbeOptions opts;
    opts.search_bounds = Rect{0, 0, W, H};
    ProbeResult result;
    Status st = probe_scroll_region(backend, Point{50, 50}, opts, &result);
    FSIC_CHECK(st);
    if (st) {
        // The static band sits inside a single scrolling region; the gap
        // threshold must bridge it so the detected region covers the full
        // moving extent, not just the segment above the gap (this is the
        // base44.com clipping bug: the old 24px threshold left the region
        // truncated right at a section gap).
        FSIC_CHECK(near(result.region.y, 0, 4));
        FSIC_CHECK(result.region.bottom() > gap_y1 + 20);
    }
}

void test_probe_does_not_bridge_huge_gap() {
    const int W = 100, H = 600;
    const int kFrames = 15;
    const int mult = 97, rate = 31;
    const int gap_y0 = 150;
    const int gap_y1 = gap_y0 + PROBE_MAX_GAP_PX + 50;  // deliberately past the bridge threshold

    std::vector<Image> frames;
    for (int i = 0; i < kFrames; ++i)
        frames.push_back(make_scrolling_frame_with_gap(W, H, i * rate, mult, gap_y0, gap_y1));

    const std::string dir = "test_probe_gap_too_big";
    write_frames(dir, frames);
    SyntheticBackend backend(dir);
    FSIC_CHECK(backend.init());

    ProbeOptions opts;
    opts.search_bounds = Rect{0, 0, W, H};
    ProbeResult result;
    Status st = probe_scroll_region(backend, Point{50, 50}, opts, &result);
    FSIC_CHECK(st);
    if (st) {
        // A gap wider than the bridge threshold must NOT be spanned: the
        // detected region should stay confined to one side of it.
        FSIC_CHECK(result.region.bottom() <= gap_y0 + 4 || result.region.y >= gap_y1 - 4);
    }
}

void test_one_shot_animation_rejected() {
    const int W = 100, H = 100;
    const int kFrames = 15;
    std::vector<Image> frames;

    Image base;
    base.resize(W, H);
    std::fill(base.rgba.begin(), base.rgba.end(), static_cast<uint8_t>(255));

    for (int i = 0; i < kFrames; ++i) {
        Image f = base;
        // The square changes ONCE (frame 0 -> frame 1) then freezes forever.
        const uint8_t v = (i == 0) ? 0 : 200;
        for (int y = 40; y < 60; ++y)
            for (int x = 40; x < 60; ++x) set_px(f, x, y, v, v, v);
        frames.push_back(std::move(f));
    }

    const std::string dir = "test_probe_one_shot";
    write_frames(dir, frames);

    SyntheticBackend backend(dir);
    FSIC_CHECK(backend.init());

    ProbeOptions opts;
    opts.search_bounds = Rect{0, 0, W, H};
    ProbeResult result;
    Status st = probe_scroll_region(backend, Point{50, 50}, opts, &result);
    FSIC_CHECK(!st);
}

void test_fully_static_rejected() {
    const int W = 80, H = 80;
    const int kFrames = 15;

    Lcg rng(44);
    Image page = make_dense_page(W, H, rng);
    std::vector<Image> frames(kFrames, page);  // byte-identical every frame

    const std::string dir = "test_probe_static";
    write_frames(dir, frames);

    SyntheticBackend backend(dir);
    FSIC_CHECK(backend.init());

    ProbeOptions opts;
    opts.search_bounds = Rect{0, 0, W, H};
    ProbeResult result;
    Status st = probe_scroll_region(backend, Point{40, 40}, opts, &result);
    FSIC_CHECK(!st);
}

} // namespace

int main() {
    test_two_scroller_page();
    test_probe_bridges_paragraph_gap();
    test_probe_does_not_bridge_huge_gap();
    test_one_shot_animation_rejected();
    test_fully_static_rejected();
    if (g_fsic_test_failures == 0) std::printf("test_probe: all checks passed\n");
    FSIC_TEST_MAIN_END
}
