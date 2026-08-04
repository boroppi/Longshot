#include "fsic/probe.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "fsic/session.h"
#include "fsic/tuning.h"

namespace fsic {

namespace {

int luma(const uint8_t* p) { return (77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8; }

int abs_luma_diff(const uint8_t* a, const uint8_t* b) { return std::abs(luma(a) - luma(b)); }

// Longest contiguous run of `true` in v; returns [lo, hi) half-open, {0,0} if none.
void longest_run(const std::vector<bool>& v, int* lo_out, int* hi_out) {
    int best_lo = 0, best_len = 0;
    int cur_lo = -1;
    for (int i = 0; i < static_cast<int>(v.size()); ++i) {
        if (v[static_cast<size_t>(i)]) {
            if (cur_lo < 0) cur_lo = i;
            const int len = i - cur_lo + 1;
            if (len > best_len) {
                best_len = len;
                best_lo = cur_lo;
            }
        } else {
            cur_lo = -1;
        }
    }
    *lo_out = best_lo;
    *hi_out = best_lo + best_len;
}

// Bridges false-runs of length <= max_gap that are bounded by `true` on both
// sides (never extends past the outermost true). Real content has
// inter-line/inter-word whitespace that shows no motion between frames even
// inside a genuinely scrolling region; without this, the unanimous 3-step
// intersection fragments into short runs. See PROBE_MAX_GAP_PX in tuning.h.
void close_gaps(std::vector<bool>& v, int max_gap) {
    const int n = static_cast<int>(v.size());
    int i = 0;
    while (i < n) {
        if (v[static_cast<size_t>(i)]) {
            ++i;
            continue;
        }
        int j = i;
        while (j < n && !v[static_cast<size_t>(j)]) ++j;
        const bool bounded = (i > 0) && (j < n);
        if (bounded && (j - i) <= max_gap) {
            for (int k = i; k < j; ++k) v[static_cast<size_t>(k)] = true;
        }
        i = j;
    }
}

} // namespace

Status probe_scroll_region(IBackend& backend, Point anchor, const ProbeOptions& opts, ProbeResult* out) {
    const Rect search = opts.search_bounds;
    Image base;
    Status st = settle_capture(backend, search, &base);
    if (!st) return st;
    out->before = base;

    const int W = search.w;
    const int H = search.h;
    std::vector<int> row_hits(static_cast<size_t>(H), 0);
    std::vector<int> col_hits(static_cast<size_t>(W), 0);

    for (int step = 1; step <= opts.steps; ++step) {
        backend.inject_scroll(anchor, ScrollDir::Down, opts.notches);
        Image cur;
        st = settle_capture(backend, search, &cur);
        if (!st) return st;

        std::vector<int> col_changed(static_cast<size_t>(W), 0);
        for (int y = 0; y < H; ++y) {
            int changed_in_row = 0;
            const uint8_t* brow = base.row(y);
            const uint8_t* crow = cur.row(y);
            for (int x = 0; x < W; ++x) {
                if (abs_luma_diff(brow + static_cast<size_t>(x) * 4, crow + static_cast<size_t>(x) * 4) >
                    opts.pixel_eps) {
                    ++changed_in_row;
                    ++col_changed[static_cast<size_t>(x)];
                }
            }
            if (static_cast<float>(changed_in_row) >= opts.line_fraction * static_cast<float>(W)) {
                row_hits[static_cast<size_t>(y)]++;
            }
        }
        for (int x = 0; x < W; ++x) {
            if (static_cast<float>(col_changed[static_cast<size_t>(x)]) >=
                opts.line_fraction * static_cast<float>(H)) {
                col_hits[static_cast<size_t>(x)]++;
            }
        }

        base = std::move(cur);
        out->steps_scrolled = step;
    }

    std::vector<bool> row_ok(static_cast<size_t>(H));
    std::vector<bool> col_ok(static_cast<size_t>(W));
    for (int y = 0; y < H; ++y) row_ok[static_cast<size_t>(y)] = row_hits[static_cast<size_t>(y)] == opts.steps;
    for (int x = 0; x < W; ++x) col_ok[static_cast<size_t>(x)] = col_hits[static_cast<size_t>(x)] == opts.steps;

    close_gaps(row_ok, PROBE_MAX_GAP_PX);
    close_gaps(col_ok, PROBE_MAX_GAP_PX);

    int y0, y1, x0, x1;
    longest_run(row_ok, &y0, &y1);
    longest_run(col_ok, &x0, &x1);

    // Restore the scroll position we consumed while probing.
    backend.inject_scroll(anchor, ScrollDir::Up, opts.steps * opts.notches + 2);

    if ((y1 - y0) < MIN_REGION_H || (x1 - x0) < MIN_REGION_W) {
        return Status::Error(
            "No scrollable region detected under the cursor. Check that the pointer is over "
            "scrollable content, or pass --region.");
    }

    out->region = Rect{search.x + x0, search.y + y0, x1 - x0, y1 - y0};
    return Status::Ok();
}

} // namespace fsic
