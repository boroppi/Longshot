#include "fsic/stitch.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "fsic/log.h"
#include "fsic/rowsig.h"
#include "fsic/tuning.h"

namespace fsic {

OffsetResult find_vertical_offset(const Image& a, const Image& b, const StitchOptions& opts) {
    const int H = a.h;
    RowSignatures sa = compute_row_signatures(a, ROWSIG_COLUMNS);
    RowSignatures sb = compute_row_signatures(b, ROWSIG_COLUMNS);

    const int min_ov = std::max(opts.min_overlap_rows,
                                 static_cast<int>(opts.min_overlap_fraction * static_cast<float>(H)));
    const int d_max = H - min_ov;
    if (d_max < 0) {
        return OffsetResult{0, 0.0f, 0.0f, false};
    }

    int best_offset = -1;
    float best_score = std::numeric_limits<float>::infinity();
    float runner_up_score = std::numeric_limits<float>::infinity();

    for (int d = 0; d <= d_max; ++d) {
        const int overlap = H - d;
        const int step = std::max(1, overlap / 256);
        double acc = 0.0;
        int cnt = 0;
        for (int y = 0; y < overlap; y += step) {
            acc += row_distance(sa, y + d, sb, y);
            ++cnt;
        }
        const float score = static_cast<float>(acc / cnt);
        if (score < best_score) {
            runner_up_score = best_score;
            best_score = score;
            best_offset = d;
        } else if (score < runner_up_score) {
            runner_up_score = score;
        }
    }

    // Refinement pass: exact (non-subsampled) scoring around the coarse winner.
    const int lo = std::max(0, best_offset - 2);
    const int hi = std::min(d_max, best_offset + 2);
    for (int d = lo; d <= hi; ++d) {
        const int overlap = H - d;
        double acc = 0.0;
        for (int y = 0; y < overlap; ++y) {
            acc += row_distance(sa, y + d, sb, y);
        }
        const float score = static_cast<float>(acc / overlap);
        if (score < best_score) {
            best_score = score;
            best_offset = d;
        }
    }

    bool confident = best_score < 2.0f;
    if (!confident && std::isfinite(runner_up_score)) {
        confident = best_score < MATCH_AMBIGUITY_RATIO * runner_up_score;
    }

    return OffsetResult{best_offset, best_score, runner_up_score, confident};
}

void detect_static_bands(const std::vector<Image>& frames, Band* out_top, Band* out_bottom) {
    const size_t n = frames.size();
    if (n == 0) {
        *out_top = Band{0, 0};
        *out_bottom = Band{0, 0};
        return;
    }
    const int H = frames[0].h;
    const size_t m = std::min<size_t>(n, 3);
    if (m < 2) {
        *out_top = Band{0, 0};
        *out_bottom = Band{0, 0};
        return;
    }

    std::vector<RowSignatures> sigs(m);
    for (size_t i = 0; i < m; ++i) sigs[i] = compute_row_signatures(frames[i], ROWSIG_COLUMNS);

    std::vector<bool> is_static(static_cast<size_t>(H), false);
    for (int y = 0; y < H; ++y) {
        float maxd = 0.0f;
        for (size_t i = 1; i < m; ++i) {
            maxd = std::max(maxd, row_distance(sigs[i - 1], y, sigs[i], y));
        }
        is_static[static_cast<size_t>(y)] = maxd < STATIC_ROW_EPS;
    }

    int top_h = 0;
    while (top_h < H && is_static[static_cast<size_t>(top_h)]) ++top_h;
    int bot_h = 0;
    while (bot_h < H && is_static[static_cast<size_t>(H - 1 - bot_h)]) ++bot_h;

    if (top_h < MIN_STATIC_BAND_PX) top_h = 0;
    if (bot_h < MIN_STATIC_BAND_PX) bot_h = 0;
    if (top_h + bot_h > H / 2) {
        FSIC_LOGW("suspicious static bands ignored (top=%d bottom=%d height=%d)", top_h, bot_h, H);
        top_h = 0;
        bot_h = 0;
    }

    *out_top = Band{0, top_h};
    *out_bottom = Band{H - bot_h, H};
}

Status stitch_frames(const std::vector<Image>& frames, const StitchOptions& opts, Image* out,
                      StitchReport* report) {
    if (frames.empty()) return Status::Error("stitch_frames: no frames provided");
    const int H = frames[0].h;
    const int W = frames[0].w;
    for (const auto& f : frames) {
        if (f.w != W || f.h != H) return Status::Error("stitch_frames: frame size mismatch");
    }

    Band top_band{};
    Band bottom_band{};
    detect_static_bands(frames, &top_band, &bottom_band);
    const int top_h = top_band.y1;
    const int bot_h = H - bottom_band.y0;
    const int CH = H - top_h - bot_h;
    if (CH <= 0) return Status::Error("stitch_frames: static bands consume entire frame height");

    std::vector<Image> content;
    content.reserve(frames.size());
    for (const auto& f : frames) {
        content.push_back(crop(f, Rect{0, top_h, W, CH}));
    }

    std::vector<int> offsets;
    int total = CH;
    int used_count = 1;
    size_t last_used_index = 0;

    for (size_t i = 1; i < frames.size(); ++i) {
        OffsetResult r = find_vertical_offset(content[i - 1], content[i], opts);
        if (r.offset <= STOP_OFFSET_PX) {
            break;
        }
        if (!r.confident) {
            report->warning += "low-confidence match at frame " + std::to_string(i) +
                                " (score=" + std::to_string(r.score) + "); ";
        }
        offsets.push_back(r.offset);
        total += r.offset;
        used_count += 1;
        last_used_index = i;
        if (top_h + total + bot_h > opts.max_canvas_height) {
            return Status::Error("stitch_frames: stitched height would exceed max_canvas_height (" +
                                  std::to_string(opts.max_canvas_height) +
                                  "); increase it or capture a smaller region");
        }
    }

    out->resize(W, top_h + total + bot_h);
    int y = 0;
    if (top_h > 0) {
        blit(*out, 0, 0, frames[0], Rect{0, 0, W, top_h});
        y = top_h;
    }
    blit(*out, 0, y, content[0], Rect{0, 0, W, CH});
    y += CH;
    for (size_t k = 0; k < offsets.size(); ++k) {
        const int d = offsets[k];
        const Image& src_content = content[k + 1];
        blit(*out, 0, y, src_content, Rect{0, CH - d, W, d});
        y += d;
    }
    if (bot_h > 0) {
        blit(*out, 0, y, frames[last_used_index], Rect{0, H - bot_h, W, bot_h});
    }

    report->frames_used = used_count;
    report->total_height = out->h;
    report->top_band_h = top_h;
    report->bottom_band_h = bot_h;
    report->offsets = offsets;
    return Status::Ok();
}

} // namespace fsic
