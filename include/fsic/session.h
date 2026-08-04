#pragma once
#include <string>
#include "fsic/backend.h"
#include "fsic/image.h"
#include "fsic/status.h"
#include "fsic/stitch.h"
#include "fsic/types.h"

namespace fsic {

struct CaptureConfig {
    Rect region;
    Point wheel_point;
    uint64_t window_id = 0;
    int notches = 3;
    int max_frames = 200;
    int max_canvas_height = 30000;
    bool use_keys = false;
    bool scroll_to_top = true;
    std::string keep_frames_dir;
};

struct CaptureResult {
    Image stitched;
    StitchReport report;
    int frames_captured = 0;
};

Status settle_capture(IBackend& backend, const Rect& rect, Image* out);
Status run_capture_session(IBackend& backend, const CaptureConfig& cfg, CaptureResult* out);

} // namespace fsic
