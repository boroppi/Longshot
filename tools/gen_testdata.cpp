// Deterministic synthetic "scrolling web page" generator used to produce
// ground-truth test fixtures for the stitching algorithm without any real
// screen capture. Output is byte-identical across runs (seeded LCG, no
// wall-clock/rand() dependency).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "fsic/image.h"
#include "fsic/image_io.h"

using fsic::Image;

namespace {

namespace fs = std::filesystem;

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

void fill_white(Image& img) { std::fill(img.rgba.begin(), img.rgba.end(), static_cast<uint8_t>(255)); }

void set_px(Image& img, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= img.w || y >= img.h) return;
    uint8_t* p = img.row(y) + static_cast<size_t>(x) * 4;
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = 255;
}

// Draws deterministic horizontal "text bars" down the page, spaced roughly
// every 10px, with seeded color/width/offset/height, tiled edge-to-edge so
// no row is ever blank.
void draw_bars(Image& img, Lcg& rng) {
    int y = 4;
    while (y < img.h) {
        const int bar_h = rng.range(8, 13);
        const int bar_w = (img.w * rng.range(85, 101)) / 100;
        const int max_x = std::max(1, img.w - bar_w);
        const int x0 = rng.range(0, max_x);

        const uint8_t r = static_cast<uint8_t>(rng.range(0, 256));
        const uint8_t g = static_cast<uint8_t>(rng.range(0, 256));
        const uint8_t b = static_cast<uint8_t>(rng.range(0, 256));

        for (int yy = y; yy < std::min(img.h, y + bar_h); ++yy) {
            for (int xx = x0; xx < std::min(img.w, x0 + bar_w); ++xx) {
                set_px(img, xx, yy, r, g, b);
            }
        }
        y += bar_h;  // tiled edge-to-edge: no blank rows for the motion-diff probe to miss
    }
}

Image make_band(int w, int h, uint8_t r, uint8_t g, uint8_t b, Lcg& rng) {
    Image band;
    band.resize(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) set_px(band, x, y, r, g, b);
    // A couple of seeded decorations so the band is visually distinguishable.
    for (int i = 0; i < 3; ++i) {
        const int dw = std::max(1, w / 8);
        const int x0 = rng.range(0, std::max(1, w - dw));
        const int y0 = rng.range(0, std::max(1, h));
        for (int x = x0; x < std::min(w, x0 + dw); ++x) set_px(band, x, y0, 255, 255, 255);
    }
    return band;
}

void apply_jitter(Image& img, int jitter, Lcg& rng) {
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

void copy_rect(Image& dst, int dx, int dy, const Image& src, int sx, int sy, int w, int h) {
    for (int y = 0; y < h; ++y) {
        const int srow = std::max(0, std::min(src.h - 1, sy + y));
        for (int x = 0; x < w; ++x) {
            const int scol = std::max(0, std::min(src.w - 1, sx + x));
            const uint8_t* p = src.row(srow) + static_cast<size_t>(scol) * 4;
            set_px(dst, dx + x, dy + y, p[0], p[1], p[2]);
        }
    }
}

struct Options {
    std::string out_dir;
    int viewport_w = 900;
    int viewport_h = 700;
    int page_h = 5000;
    int step = 130;
    int sticky_top = 0;
    int sticky_bottom = 0;
    int jitter = 0;
    int sidebar_w = 0;
};

bool parse_options(int argc, char** argv, Options* o) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](int& out_val) {
            if (i + 1 >= argc) return false;
            out_val = std::atoi(argv[++i]);
            return true;
        };
        if (a == "--out" && i + 1 < argc) {
            o->out_dir = argv[++i];
        } else if (a == "--viewport-w") {
            if (!next(o->viewport_w)) return false;
        } else if (a == "--viewport-h") {
            if (!next(o->viewport_h)) return false;
        } else if (a == "--page-h") {
            if (!next(o->page_h)) return false;
        } else if (a == "--step") {
            if (!next(o->step)) return false;
        } else if (a == "--sticky-top") {
            if (!next(o->sticky_top)) return false;
        } else if (a == "--sticky-bottom") {
            if (!next(o->sticky_bottom)) return false;
        } else if (a == "--jitter") {
            if (!next(o->jitter)) return false;
        } else if (a == "--sidebar-w") {
            if (!next(o->sidebar_w)) return false;
        } else {
            std::fprintf(stderr, "fsic_gen_testdata: unknown option: %s\n", a.c_str());
            return false;
        }
    }
    return !o->out_dir.empty();
}

std::string zero_pad(int value, int digits) {
    std::string s = std::to_string(value);
    while (static_cast<int>(s.size()) < digits) s = "0" + s;
    return s;
}

} // namespace

int main(int argc, char** argv) {
    Options o;
    if (!parse_options(argc, argv, &o)) {
        std::fprintf(stderr,
                      "usage: fsic_gen_testdata --out DIR [--viewport-w W] [--viewport-h H] "
                      "[--page-h PH] [--step S] [--sticky-top T] [--sticky-bottom B] "
                      "[--jitter J] [--sidebar-w SW]\n");
        return 2;
    }

    std::error_code ec;
    fs::create_directories(o.out_dir, ec);
    if (ec) {
        std::fprintf(stderr, "fsic_gen_testdata: cannot create directory %s\n", o.out_dir.c_str());
        return 1;
    }

    Lcg rng(12345u);

    Image page;
    page.resize(o.viewport_w, o.page_h);
    fill_white(page);
    draw_bars(page, rng);

    Image sidebar;
    const bool has_sidebar = o.sidebar_w > 0;
    if (has_sidebar) {
        sidebar.resize(o.sidebar_w, o.page_h * 2);
        fill_white(sidebar);
        // Distinct palette: bias toward blue by drawing bars then tinting.
        draw_bars(sidebar, rng);
    }

    const int total_w = o.viewport_w + (has_sidebar ? o.sidebar_w : 0);

    Image band_top, band_bottom;
    if (o.sticky_top > 0) band_top = make_band(total_w, o.sticky_top, 40, 60, 90, rng);
    if (o.sticky_bottom > 0) band_bottom = make_band(total_w, o.sticky_bottom, 90, 60, 40, rng);

    const int CH = o.viewport_h - o.sticky_top - o.sticky_bottom;
    if (CH <= 0) {
        std::fprintf(stderr, "fsic_gen_testdata: sticky_top+sticky_bottom >= viewport_h\n");
        return 1;
    }
    const int max_scroll = std::max(0, o.page_h - CH);

    std::vector<int> scrolls;
    {
        int s = 0;
        while (true) {
            scrolls.push_back(std::min(s, max_scroll));
            if (s >= max_scroll) break;
            s += o.step;
        }
    }

    const int digits = std::max(3, static_cast<int>(std::to_string(scrolls.size()).size()));

    for (size_t i = 0; i < scrolls.size(); ++i) {
        Image frame;
        frame.resize(total_w, o.viewport_h);
        fill_white(frame);

        if (o.sticky_top > 0) copy_rect(frame, 0, 0, band_top, 0, 0, total_w, o.sticky_top);
        if (o.sticky_bottom > 0)
            copy_rect(frame, 0, o.viewport_h - o.sticky_bottom, band_bottom, 0, 0, total_w,
                      o.sticky_bottom);

        const int main_x = has_sidebar ? o.sidebar_w : 0;
        copy_rect(frame, main_x, o.sticky_top, page, 0, scrolls[i], o.viewport_w, CH);

        if (has_sidebar) {
            const int sidebar_max_scroll = std::max(0, sidebar.h - CH);
            const int sidebar_top = std::min(scrolls[i] / 2, sidebar_max_scroll);
            copy_rect(frame, 0, o.sticky_top, sidebar, 0, sidebar_top, o.sidebar_w, CH);
        }

        apply_jitter(frame, o.jitter, rng);

        const std::string path = o.out_dir + "/frame_" + zero_pad(static_cast<int>(i), digits) + ".bmp";
        fsic::Status st = fsic::write_bmp(path, frame);
        if (!st) {
            std::fprintf(stderr, "fsic_gen_testdata: %s\n", st.message.c_str());
            return 1;
        }
    }

    // expected.bmp: ground-truth main-column stitch (sticky bands + full page), width viewport_w.
    Image expected;
    expected.resize(o.viewport_w, o.sticky_top + o.page_h + o.sticky_bottom);
    int y = 0;
    if (o.sticky_top > 0) {
        copy_rect(expected, 0, 0, band_top, has_sidebar ? o.sidebar_w : 0, 0, o.viewport_w, o.sticky_top);
        y = o.sticky_top;
    }
    copy_rect(expected, 0, y, page, 0, 0, o.viewport_w, o.page_h);
    y += o.page_h;
    if (o.sticky_bottom > 0) {
        copy_rect(expected, 0, y, band_bottom, has_sidebar ? o.sidebar_w : 0, 0, o.viewport_w,
                  o.sticky_bottom);
    }
    {
        fsic::Status st = fsic::write_bmp(o.out_dir + "/expected.bmp", expected);
        if (!st) {
            std::fprintf(stderr, "fsic_gen_testdata: %s\n", st.message.c_str());
            return 1;
        }
    }

    {
        std::ofstream manifest(o.out_dir + "/manifest.txt", std::ios::trunc);
        manifest << "viewport_w " << o.viewport_w << "\n";
        manifest << "viewport_h " << o.viewport_h << "\n";
        manifest << "page_h " << o.page_h << "\n";
        manifest << "step " << o.step << "\n";
        manifest << "sticky_top " << o.sticky_top << "\n";
        manifest << "sticky_bottom " << o.sticky_bottom << "\n";
        manifest << "sidebar_w " << o.sidebar_w << "\n";
        manifest << "jitter " << o.jitter << "\n";
        manifest << "frame_count " << scrolls.size() << "\n";
    }

    std::printf("wrote %zu frames to %s (%dx%d viewport, page height %d)\n", scrolls.size(),
                o.out_dir.c_str(), o.viewport_w, o.viewport_h, o.page_h);
    return 0;
}
