#pragma once
#include <string>
#include <vector>
#include "fsic/image.h"
#include "fsic/status.h"
#include "fsic/types.h"

namespace fsic {

struct OffsetResult {
    int offset = -1;
    float score = 0.0f;
    float runner_up_score = 0.0f;
    bool confident = false;
};

struct StitchOptions {
    int min_overlap_rows = 24;          // default MIN_OVERLAP_ROWS
    float min_overlap_fraction = 0.15f; // default MIN_OVERLAP_FRACTION
    int max_canvas_height = 30000;      // default MAX_CANVAS_HEIGHT_DEFAULT
};

struct StitchReport {
    int frames_used = 0;
    int total_height = 0;
    int top_band_h = 0;
    int bottom_band_h = 0;
    std::vector<int> offsets;
    std::string warning;
};

OffsetResult find_vertical_offset(const Image& a, const Image& b, const StitchOptions& opts);
void detect_static_bands(const std::vector<Image>& frames, Band* out_top, Band* out_bottom);
Status stitch_frames(const std::vector<Image>& frames, const StitchOptions& opts,
                      Image* out, StitchReport* report);

} // namespace fsic
