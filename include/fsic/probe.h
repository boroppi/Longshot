#pragma once
#include "fsic/backend.h"
#include "fsic/image.h"
#include "fsic/status.h"
#include "fsic/types.h"

namespace fsic {

struct ProbeOptions {
    int steps = 3;
    int notches = 3;
    int pixel_eps = 8;
    float line_fraction = 0.12f;
    Rect search_bounds;
};

struct ProbeResult {
    Rect region;
    Image before;
    int steps_scrolled = 0;
};

Status probe_scroll_region(IBackend& backend, Point anchor, const ProbeOptions& opts, ProbeResult* out);

} // namespace fsic
