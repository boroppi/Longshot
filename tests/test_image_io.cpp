#include "fsic/image.h"
#include "fsic/image_io.h"
#include "test_util.h"

using namespace fsic;

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

void fill_pixel(Image& img, int x, int y) {
    uint8_t* p = img.row(y) + static_cast<size_t>(x) * 4;
    p[0] = static_cast<uint8_t>((x * 37 + y * 11) % 256);
    p[1] = static_cast<uint8_t>((x * 7) % 256);
    p[2] = static_cast<uint8_t>((y * 13) % 256);
    p[3] = 255;
}

Image make_image(int w, int h) {
    Image img;
    img.resize(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) fill_pixel(img, x, y);
    return img;
}

bool check_bmp_pixels(const Image& expected, const Image& got) {
    if (got.w != expected.w || got.h != expected.h) return false;
    for (int y = 0; y < got.h; ++y) {
        for (int x = 0; x < got.w; ++x) {
            const uint8_t* e = expected.row(y) + static_cast<size_t>(x) * 4;
            const uint8_t* g = got.row(y) + static_cast<size_t>(x) * 4;
            if (e[0] != g[0] || e[1] != g[1] || e[2] != g[2]) return false;
        }
    }
    return true;
}

bool write_raw_bytes(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;
    ofs.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(ofs);
}

void put16(std::vector<uint8_t>& v, uint16_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}
void put32(std::vector<uint8_t>& v, uint32_t val) {
    v.push_back(static_cast<uint8_t>(val & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    v.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void test_roundtrip() {
    const int widths[] = {1, 3, 4, 7, 640};
    const int heights[] = {1, 2, 100};
    for (int w : widths) {
        for (int h : heights) {
            Image img = make_image(w, h);
            std::string path = "test_tmp_" + std::to_string(w) + "x" + std::to_string(h) + ".bmp";
            Status st = write_bmp(path, img);
            FSIC_CHECK(st);
            Image back;
            st = read_bmp(path, &back);
            FSIC_CHECK(st);
            FSIC_CHECK_EQ(back.w, w);
            FSIC_CHECK_EQ(back.h, h);
            if (st && back.w == w && back.h == h) {
                for (int y = 0; y < h; ++y) {
                    for (int x = 0; x < w; ++x) {
                        const uint8_t* e = img.row(y) + static_cast<size_t>(x) * 4;
                        const uint8_t* g = back.row(y) + static_cast<size_t>(x) * 4;
                        FSIC_CHECK_EQ(static_cast<int>(e[0]), static_cast<int>(g[0]));
                        FSIC_CHECK_EQ(static_cast<int>(e[1]), static_cast<int>(g[1]));
                        FSIC_CHECK_EQ(static_cast<int>(e[2]), static_cast<int>(g[2]));
                        FSIC_CHECK_EQ(g[3], 255);
                    }
                }
            }
            std::remove(path.c_str());
        }
    }
}

void test_top_down() {
    // Hand-construct a minimal 2x2 top-down (biHeight < 0) 24-bit BMP.
    const int w = 2, h = 2;
    const int row_bytes = ((w * 3) + 3) & ~3;  // padded to 4-byte boundary
    const int data_size = row_bytes * h;
    std::vector<uint8_t> bmp;
    bmp.reserve(14 + 40 + data_size);
    // file header
    put16(bmp, 0x4D42);
    put32(bmp, 14 + 40 + static_cast<uint32_t>(data_size));
    put16(bmp, 0);
    put16(bmp, 0);
    put32(bmp, 54);
    // info header
    put32(bmp, 40);
    put32(bmp, static_cast<uint32_t>(w));
    put32(bmp, static_cast<uint32_t>(-h));  // top-down
    put16(bmp, 1);
    put16(bmp, 24);
    put32(bmp, 0);
    put32(bmp, static_cast<uint32_t>(data_size));
    put32(bmp, 0);
    put32(bmp, 0);
    put32(bmp, 0);
    put32(bmp, 0);
    // pixel data, top row first (BGR). Row 0: (0,0)=red, (1,0)=cyan.
    // Row 1: (0,1)=green, (1,1)=magenta.
    bmp.push_back(0); bmp.push_back(0); bmp.push_back(255);      // red
    bmp.push_back(255); bmp.push_back(255); bmp.push_back(0);   // cyan
    bmp.push_back(0); bmp.push_back(0);                         // row 0 padding
    bmp.push_back(0); bmp.push_back(255); bmp.push_back(0);     // green
    bmp.push_back(255); bmp.push_back(0); bmp.push_back(255);   // magenta
    bmp.push_back(0); bmp.push_back(0);                         // row 1 padding

    const std::string path = "test_tmp_topdown.bmp";
    FSIC_CHECK(write_raw_bytes(path, bmp));
    Image got;
    Status fs = read_bmp(path, &got);
    FSIC_CHECK(fs);
    if (fs) {
        FSIC_CHECK_EQ(got.w, 2);
        FSIC_CHECK_EQ(got.h, 2);
        // Expect decoded rows in top-down order: y=0 -> red, cyan; y=1 -> green, magenta.
        const uint8_t* p00 = got.row(0) + 0;
        const uint8_t* p01 = got.row(0) + 4;
        const uint8_t* p10 = got.row(1) + 0;
        const uint8_t* p11 = got.row(1) + 4;
        FSIC_CHECK_EQ(p00[0], 255); FSIC_CHECK_EQ(p00[1], 0); FSIC_CHECK_EQ(p00[2], 0);
        FSIC_CHECK_EQ(p01[0], 0);   FSIC_CHECK_EQ(p01[1], 255); FSIC_CHECK_EQ(p01[2], 255);
        FSIC_CHECK_EQ(p10[0], 0);   FSIC_CHECK_EQ(p10[1], 255); FSIC_CHECK_EQ(p10[2], 0);
        FSIC_CHECK_EQ(p11[0], 255); FSIC_CHECK_EQ(p11[1], 0); FSIC_CHECK_EQ(p11[2], 255);
        FSIC_CHECK_EQ(got.row(0)[3], 255);
    }
    std::remove(path.c_str());
}

void test_reject_8bit() {
    const int w = 2, h = 2;
    const int row_bytes = w;      // 8-bit indexed, one byte per pixel
    const int data_size = w * h;  // 8-bit indexed
    std::vector<uint8_t> bmp;
    put16(bmp, 0x4D42);
    put32(bmp, 14 + 40 + 64 + 8);  // file size incl. palette
    put16(bmp, 0);
    put16(bmp, 0);
    put32(bmp, 14 + 40 + 64);  // off bits after palette
    put32(bmp, 40);
    put32(bmp, static_cast<uint32_t>(w));
    put32(bmp, static_cast<uint32_t>(h));
    put16(bmp, 1);
    put16(bmp, 8);
    put32(bmp, 0);
    put32(bmp, static_cast<uint32_t>(row_bytes));
    put32(bmp, 0);
    put32(bmp, 0);
    put32(bmp, 0);
    put32(bmp, 0);
    // palette (256 entries * 4 bytes), all zero
    for (int i = 0; i < 256; ++i) { put32(bmp, 0); }
    // index data
    for (int i = 0; i < row_bytes; ++i) bmp.push_back(0);

    const std::string path = "test_tmp_8bit.bmp";
    FSIC_CHECK(write_raw_bytes(path, bmp));
    Image got;
    Status fs = read_bmp(path, &got);
    FSIC_CHECK(!fs);
    std::remove(path.c_str());
}

void test_crop() {
    Image src;
    src.resize(10, 10);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) fill_pixel(src, x, y);

    Image c = crop(src, Rect{2, 2, 4, 4});
    FSIC_CHECK_EQ(c.w, 4);
    FSIC_CHECK_EQ(c.h, 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            const uint8_t* e = src.row(y + 2) + static_cast<size_t>(x + 2) * 4;
            const uint8_t* g = c.row(y) + static_cast<size_t>(x) * 4;
            FSIC_CHECK_EQ(static_cast<int>(e[0]), static_cast<int>(g[0]));
            FSIC_CHECK_EQ(static_cast<int>(e[1]), static_cast<int>(g[1]));
            FSIC_CHECK_EQ(static_cast<int>(e[2]), static_cast<int>(g[2]));
        }
}

void test_crop_clamp() {
    Image src;
    src.resize(10, 10);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) fill_pixel(src, x, y);

    Image c = crop(src, Rect{8, 8, 10, 10});
    FSIC_CHECK_EQ(c.w, 2);
    FSIC_CHECK_EQ(c.h, 2);
    const uint8_t* g = c.row(0);
    const uint8_t* e = src.row(8) + static_cast<size_t>(8) * 4;
    FSIC_CHECK_EQ(static_cast<int>(e[0]), static_cast<int>(g[0]));
    FSIC_CHECK_EQ(static_cast<int>(e[1]), static_cast<int>(g[1]));
    FSIC_CHECK_EQ(static_cast<int>(e[2]), static_cast<int>(g[2]));
}

void test_blit() {
    Image dst;
    dst.resize(10, 10);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) {
            uint8_t* p = dst.row(y) + static_cast<size_t>(x) * 4;
            p[0] = 1; p[1] = 2; p[2] = 3; p[3] = 255;
        }
    Image src;
    src.resize(4, 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            uint8_t* p = src.row(y) + static_cast<size_t>(x) * 4;
            p[0] = static_cast<uint8_t>(100 + x);
            p[1] = static_cast<uint8_t>(150 + y);
            p[2] = static_cast<uint8_t>(200);
            p[3] = 255;
        }

    blit(dst, 3, 3, src, Rect{0, 0, 4, 4});
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) {
            const uint8_t* p = dst.row(y) + static_cast<size_t>(x) * 4;
            if (x >= 3 && x < 7 && y >= 3 && y < 7) {
                const uint8_t* s = src.row(y - 3) + static_cast<size_t>(x - 3) * 4;
                FSIC_CHECK_EQ(static_cast<int>(p[0]), static_cast<int>(s[0]));
                FSIC_CHECK_EQ(static_cast<int>(p[1]), static_cast<int>(s[1]));
                FSIC_CHECK_EQ(static_cast<int>(p[2]), static_cast<int>(s[2]));
            } else {
                FSIC_CHECK_EQ(p[0], 1);
                FSIC_CHECK_EQ(p[1], 2);
                FSIC_CHECK_EQ(p[2], 3);
            }
        }
}

void test_blit_clamp() {
    Image dst;
    dst.resize(10, 10);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) {
            uint8_t* p = dst.row(y) + static_cast<size_t>(x) * 4;
            p[0] = 0; p[1] = 0; p[2] = 0; p[3] = 255;
        }
    Image src;
    src.resize(4, 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) {
            uint8_t* p = src.row(y) + static_cast<size_t>(x) * 4;
            p[0] = static_cast<uint8_t>(200 + x);
            p[1] = static_cast<uint8_t>(200 + y);
            p[2] = 100;
            p[3] = 255;
        }

    // Blit at (8,8): only the 2x2 top-left corner of src fits.
    blit(dst, 8, 8, src, Rect{0, 0, 4, 4});
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) {
            const uint8_t* p = dst.row(y) + static_cast<size_t>(x) * 4;
            if (x >= 8 && x < 10 && y >= 8 && y < 10) {
                const uint8_t* s = src.row(y - 8) + static_cast<size_t>(x - 8) * 4;
                FSIC_CHECK_EQ(static_cast<int>(p[0]), static_cast<int>(s[0]));
                FSIC_CHECK_EQ(static_cast<int>(p[1]), static_cast<int>(s[1]));
                FSIC_CHECK_EQ(static_cast<int>(p[2]), static_cast<int>(s[2]));
            } else {
                FSIC_CHECK_EQ(p[0], 0);
                FSIC_CHECK_EQ(p[1], 0);
                FSIC_CHECK_EQ(p[2], 0);
            }
        }
}

void test_draw_rect_outline() {
    Image img;
    img.resize(10, 10);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) {
            uint8_t* p = img.row(y) + static_cast<size_t>(x) * 4;
            p[0] = 10; p[1] = 20; p[2] = 30; p[3] = 255;
        }

    draw_rect_outline(img, Rect{2, 2, 6, 6}, 200, 100, 50, 1);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x) {
            const uint8_t* p = img.row(y) + static_cast<size_t>(x) * 4;
            bool on_border = (x >= 2 && x < 8) && (y == 2 || y == 7);
            on_border |= (y >= 2 && y < 8) && (x == 2 || x == 7);
            if (on_border) {
                FSIC_CHECK_EQ(p[0], 200);
                FSIC_CHECK_EQ(p[1], 100);
                FSIC_CHECK_EQ(p[2], 50);
                FSIC_CHECK_EQ(p[3], 255);
            } else {
                FSIC_CHECK_EQ(p[0], 10);
                FSIC_CHECK_EQ(p[1], 20);
                FSIC_CHECK_EQ(p[2], 30);
            }
        }
}

}  // namespace

int main() {
    test_roundtrip();
    test_top_down();
    test_reject_8bit();
    test_crop();
    test_crop_clamp();
    test_blit();
    test_blit_clamp();
    test_draw_rect_outline();
    if (g_fsic_test_failures == 0) std::printf("test_image_io: all checks passed\n");
    FSIC_TEST_MAIN_END
}




