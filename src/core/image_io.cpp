#include "fsic/image_io.h"

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#if FSIC_HAVE_STB
#include "stb/stb_image_write.h"
#endif

namespace fsic {

namespace {

// BIMAG-dimensional
#pragma pack(push, 1)
struct BitmapFileHeader {
    uint16_t type = 0x4D42;  // 'BM'
    uint32_t size = 0;
    uint16_t reserved1 = 0;
    uint16_t reserved2 = 0;
    uint32_t off_bits = 0;
};
struct BitmapInfoHeader {
    uint32_t size = 40;
    int32_t width = 0;
    int32_t height = 0;
    uint16_t planes = 1;
    uint16_t bit_count = 24;
    uint32_t compression = 0;
    uint32_t size_image = 0;
    int32_t x_pels_per_meter = 0;
    int32_t y_pels_per_meter = 0;
    uint32_t clr_used = 0;
    uint32_t clr_important = 0;
};
#pragma pack(pop)

Status fail(const std::string& why) { return Status::Error(why); }

}  // namespace

Status write_bmp(const std::string& path, const Image& img) {
    if (!img.valid()) return fail("cannot write BMP: invalid image");
    const int row_bytes = ((img.w * 3 + 3) & ~3);
    const uint32_t data_size = static_cast<uint32_t>(row_bytes) * img.h;
    const uint32_t file_size = 14 + 40 + data_size;

    BitmapFileHeader fh;
    fh.size = file_size;
    fh.off_bits = 14 + 40;
    BitmapInfoHeader ih;
    ih.width = img.w;
    ih.height = img.h;
    ih.bit_count = 24;
    ih.size_image = data_size;

    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs) return fail("cannot open BMP file: " + path);

    ofs.write(reinterpret_cast<const char*>(&fh), sizeof(fh));
    ofs.write(reinterpret_cast<const char*>(&ih), sizeof(ih));

    std::vector<uint8_t> row_buf(static_cast<size_t>(row_bytes), 0);
    for (int y = img.h - 1; y >= 0; --y) {
        const uint8_t* src = img.row(y);
        size_t o = 0;
        for (int x = 0; x < img.w; ++x) {
            row_buf[o++] = src[x * 4 + 2];  // B
            row_buf[o++] = src[x * 4 + 1];  // G
            row_buf[o++] = src[x * 4 + 0];  // R
        }
        ofs.write(reinterpret_cast<const char*>(row_buf.data()), row_bytes);
    }
    ofs.close();
    if (!ofs) return fail("error writing BMP file: " + path);
    return Status::Ok();
}

Status read_bmp(const std::string& path, Image* out) {
    if (!out) return fail("read_bmp: null output");
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return fail("cannot open BMP file: " + path);

    BitmapFileHeader fh;
    ifs.read(reinterpret_cast<char*>(&fh), sizeof(fh));
    if (!ifs) return fail("truncated BMP file header");
    if (fh.type != 0x4D42) return fail("not a BMP file (bad signature)");

    BitmapInfoHeader ih;
    ifs.read(reinterpret_cast<char*>(&ih), sizeof(ih));
    if (!ifs) return fail("truncated BMP info header");
    if (ih.size < 40) return fail("unsupported BMP info header size");
    if (ih.compression != 0) return fail("compressed BMPs are not supported");
    if (ih.bit_count != 24 && ih.bit_count != 32)
        return fail("unsupported BMP bit depth: " + std::to_string(ih.bit_count));

    const int32_t aw = ih.width;
    const int32_t ah = ih.height;
    if (aw <= 0 || ah == 0) return fail("invalid BMP dimensions");
    const bool top_down = ah < 0;
    const int h = top_down ? -ah : ah;

    const size_t bpp = ih.bit_count / 8;
    const size_t row_bytes = (static_cast<size_t>(aw) * bpp + 3) & ~static_cast<size_t>(3);

    out->resize(aw, h);
    std::vector<uint8_t> row_buf(row_bytes);
    for (int r = 0; r < h; ++r) {
        int read_row = top_down ? r : (h - 1 - r);
        ifs.seekg(static_cast<std::streamoff>(fh.off_bits) +
                  static_cast<std::streamoff>(read_row) * static_cast<std::streamoff>(row_bytes));
        ifs.read(reinterpret_cast<char*>(row_buf.data()), static_cast<std::streamsize>(row_bytes));
        if (!ifs) return fail("truncated BMP pixel data");
        uint8_t* dst = out->row(r);
        for (int x = 0; x < aw; ++x) {
            const uint8_t* p = row_buf.data() + static_cast<size_t>(x) * bpp;
            dst[x * 4 + 0] = p[2];  // R
            dst[x * 4 + 1] = p[1];  // G
            dst[x * 4 + 2] = p[0];  // B
            dst[x * 4 + 3] = 255;   // A
        }
    }
    return Status::Ok();
}

Status write_png(const std::string& path, const Image& img) {
#if FSIC_HAVE_STB
    if (!img.valid()) return fail("cannot write: invalid image");
    int result = stbi_write_png(path.c_str(), img.w, img.h, 4, img.rgba.data(), img.w * 4);
    if (result) return Status::Ok();
    return fail("stbi_write_png failed");
#else
    (void)path;
    (void)img;
    return fail("PNG support not compiled in; use a .bmp output path");
#endif
}

Status write_image_auto(const std::string& path, const Image& img) {
    if (path.size() >= 4) {
        const size_t dot = path.find_last_of('.');
        std::string ext;
        if (dot != std::string::npos) {
            ext = path.substr(dot);
            for (auto& c : ext) {
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            }
        }
        if (ext == ".png") return write_png(path, img);
    }
    return write_bmp(path, img);
}

} // namespace fsic

