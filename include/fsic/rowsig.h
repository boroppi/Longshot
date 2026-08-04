#pragma once
#include <cstdint>
#include <vector>
#include "fsic/image.h"

namespace fsic {

struct RowSignatures {
    int rows = 0;
    int cols = 0;
    std::vector<uint8_t> data;  // size rows*cols, row-major
    const uint8_t* row(int y) const { return data.data() + static_cast<size_t>(y) * cols; }
};

RowSignatures compute_row_signatures(const Image& img, int sample_columns);
float row_distance(const RowSignatures& a, int ya, const RowSignatures& b, int yb);

} // namespace fsic
