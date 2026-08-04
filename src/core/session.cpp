#include "fsic/session.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <utility>

#include "fsic/image_io.h"
#include "fsic/log.h"
#include "fsic/tuning.h"

namespace fsic {

namespace fs = std::filesystem;

Status settle_capture(IBackend& backend, const Rect& rect, Image* out) {
    Image prev;
    Status st = backend.capture_rect(rect, &prev);
    if (!st) return st;
    for (int i = 0; i < SETTLE_MAX_POLLS; ++i) {
        backend.sleep_ms(SETTLE_POLL_MS);
        Image cur;
        st = backend.capture_rect(rect, &cur);
        if (!st) return st;
        if (mean_abs_diff(prev, cur) <= SETTLE_MAD_EPS) {
            *out = std::move(cur);
            return Status::Ok();
        }
        prev = std::move(cur);
    }
    FSIC_LOGW("region did not settle within %d ms (animated content?)",
              SETTLE_MAX_POLLS * SETTLE_POLL_MS);
    *out = std::move(prev);
    return Status::Ok();
}

namespace {

void scroll_to_top(IBackend& backend, const CaptureConfig& cfg) {
    Image a;
    if (!settle_capture(backend, cfg.region, &a)) return;
    const int notches = std::max(1, cfg.notches);
    const int iters = SCROLL_TO_TOP_MAX_ITERS / notches;
    StitchOptions opts;
    for (int i = 0; i < iters; ++i) {
        backend.inject_scroll(cfg.wheel_point, ScrollDir::Up, cfg.notches);
        Image b;
        if (!settle_capture(backend, cfg.region, &b)) return;
        OffsetResult r = find_vertical_offset(b, a, opts);
        if (r.offset <= STOP_OFFSET_PX) return;
        a = std::move(b);
    }
    FSIC_LOGW("could not confirm top of region after %d iterations", iters);
}

} // namespace

Status run_capture_session(IBackend& backend, const CaptureConfig& cfg, CaptureResult* out) {
    if (cfg.window_id != 0) {
        Status ast = backend.activate_window(cfg.window_id);
        if (!ast) FSIC_LOGW("activate_window failed: %s", ast.message.c_str());
    }
    backend.sleep_ms(150);
    backend.move_cursor(cfg.wheel_point);

    if (cfg.scroll_to_top) scroll_to_top(backend, cfg);

    std::vector<Image> frames;
    Image first;
    Status st = settle_capture(backend, cfg.region, &first);
    if (!st) return st;

    if (!cfg.keep_frames_dir.empty()) {
        std::error_code ec;
        fs::create_directories(cfg.keep_frames_dir, ec);
        write_bmp((fs::path(cfg.keep_frames_dir) / "frame_000.bmp").string(), first);
    }
    frames.push_back(std::move(first));

    int stop_hits = 0;
    for (int i = 1; i < cfg.max_frames; ++i) {
        if (cfg.use_keys) {
            backend.inject_key(cfg.window_id, Key::PageDown);
        } else {
            backend.inject_scroll(cfg.wheel_point, ScrollDir::Down, cfg.notches);
        }
        backend.sleep_ms(POST_SCROLL_DELAY_MS);

        Image f;
        st = settle_capture(backend, cfg.region, &f);
        if (!st) return st;

        if (!cfg.keep_frames_dir.empty()) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "frame_%03d.bmp", i);
            write_bmp((fs::path(cfg.keep_frames_dir) / buf).string(), f);
        }

        StitchOptions probe_opts;
        probe_opts.max_canvas_height = cfg.max_canvas_height;
        OffsetResult r = find_vertical_offset(frames.back(), f, probe_opts);
        frames.push_back(std::move(f));

        if (r.offset <= STOP_OFFSET_PX) {
            if (++stop_hits >= STOP_CONFIRM_FRAMES) break;
        } else {
            stop_hits = 0;
        }
    }

    StitchOptions opts;
    opts.max_canvas_height = cfg.max_canvas_height;
    Status sst = stitch_frames(frames, opts, &out->stitched, &out->report);
    if (!sst) return sst;
    out->frames_captured = static_cast<int>(frames.size());
    return Status::Ok();
}

} // namespace fsic
