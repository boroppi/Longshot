#include "win32_backend.h"

#include <windows.h>

#include <vector>

namespace fsic::win32 {

namespace {

struct ScreenDC {
    HDC hdc = GetDC(nullptr);
    ~ScreenDC() {
        if (hdc) ReleaseDC(nullptr, hdc);
    }
};

struct MemDC {
    HDC hdc;
    explicit MemDC(HDC screen) : hdc(CreateCompatibleDC(screen)) {}
    ~MemDC() {
        if (hdc) DeleteDC(hdc);
    }
};

struct GdiBitmap {
    HBITMAP h;
    explicit GdiBitmap(HBITMAP bmp) : h(bmp) {}
    ~GdiBitmap() {
        if (h) DeleteObject(h);
    }
};

} // namespace

Status capture_rect(const Rect& rect_px, Image* out) {
    if (rect_px.w <= 0 || rect_px.h <= 0) return Status::Error("win32: capture_rect: empty rect");

    ScreenDC screen;
    if (!screen.hdc) return Status::Error("win32: GetDC(NULL) failed");

    MemDC mem(screen.hdc);
    if (!mem.hdc) return Status::Error("win32: CreateCompatibleDC failed");

    GdiBitmap bmp(CreateCompatibleBitmap(screen.hdc, rect_px.w, rect_px.h));
    if (!bmp.h) return Status::Error("win32: CreateCompatibleBitmap failed");

    HGDIOBJ old = SelectObject(mem.hdc, bmp.h);
    const BOOL blt_ok = BitBlt(mem.hdc, 0, 0, rect_px.w, rect_px.h, screen.hdc, rect_px.x, rect_px.y,
                                SRCCOPY | CAPTUREBLT);
    SelectObject(mem.hdc, old);
    if (!blt_ok) return Status::Error("win32: BitBlt failed");

    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(bih);
    bih.biWidth = rect_px.w;
    bih.biHeight = -rect_px.h;  // negative = top-down, matches our Image row order
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    std::vector<uint8_t> buf(static_cast<size_t>(rect_px.w) * static_cast<size_t>(rect_px.h) * 4);
    const int lines = GetDIBits(mem.hdc, bmp.h, 0, static_cast<UINT>(rect_px.h), buf.data(),
                                 reinterpret_cast<BITMAPINFO*>(&bih), DIB_RGB_COLORS);
    if (lines == 0) return Status::Error("win32: GetDIBits failed");

    out->resize(rect_px.w, rect_px.h);
    const size_t n = static_cast<size_t>(rect_px.w) * static_cast<size_t>(rect_px.h);
    for (size_t i = 0; i < n; ++i) {
        const uint8_t* s = &buf[i * 4];  // BGRA from GDI
        uint8_t* d = out->rgba.data() + i * 4;
        d[0] = s[2];
        d[1] = s[1];
        d[2] = s[0];
        d[3] = 255;
    }
    return Status::Ok();
}

} // namespace fsic::win32
