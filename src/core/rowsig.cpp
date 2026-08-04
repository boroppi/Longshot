#include "fsic/rowsig.h"

#include <algorithm>
#include <cstdlib>

namespace fsic {

RowSignatures compute_row_signatures(const Image& img, int sample_columns) {
    RowSignatures result;
    if (img.w <= 0 || img.h <= 0) return result;

    const int cols = std::max(1, std::min(img.w, sample_columns));
    std::vector<int> xs(static_cast<size_t>(cols));
    for (int i = 0; i < cols; ++i) {
        xs[static_cast<size_t>(i)] = (i * img.w) / cols + img.w / (2 * cols);
        xs[static_cast<size_t>(i)] = std::min(xs[static_cast<size_t>(i)], img.w - 1);
    }

    result.rows = img.h;
    result.cols = cols;
    result.data.resize(static_cast<size_t>(img.h) * static_cast<size_t>(cols));

    for (int y = 0; y < img.h; ++y) {
        const uint8_t* rowp = img.row(y);
        for (int i = 0; i < cols; ++i) {
            const uint8_t* p = rowp + static_cast<size_t>(xs[static_cast<size_t>(i)]) * 4;
            const int luma = (77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8;
            result.data[static_cast<size_t>(y) * static_cast<size_t>(cols) + static_cast<size_t>(i)] =
                static_cast<uint8_t>(luma);
        }
    }
    return result;
}

float row_distance(const RowSignatures& a, int ya, const RowSignatures& b, int yb) {
    if (a.cols != b.cols || a.cols <= 0) return 255.0f;
    const uint8_t* pa = a.row(ya);
    const uint8_t* pb = b.row(yb);
    int acc = 0;
    for (int i = 0; i < a.cols; ++i) {
        acc += std::abs(static_cast<int>(pa[i]) - static_cast<int>(pb[i]));
    }
    return static_cast<float>(acc) / static_cast<float>(a.cols);
}

} // namespace fsic
