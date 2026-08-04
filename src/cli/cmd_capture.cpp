#include "commands.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>

#include "args.h"
#include "fsic/backend_factory.h"
#include "fsic/image_io.h"
#include "fsic/log.h"
#include "fsic/probe.h"
#include "fsic/session.h"
#include "fsic/tuning.h"
#include "prompt.h"

namespace fsic::cli {

namespace {

BackendKind parse_backend_kind(const std::string& s) {
    if (s == "win32") return BackendKind::Win32;
    if (s == "x11") return BackendKind::X11;
    if (s == "macos") return BackendKind::MacOS;
    if (s == "synthetic") return BackendKind::Synthetic;
    return BackendKind::Auto;
}

std::string probe_image_path(const std::string& out_path) {
    const size_t slash = out_path.find_last_of("/\\");
    const size_t dot = out_path.find_last_of('.');
    const std::string stem =
        (dot != std::string::npos && (slash == std::string::npos || dot > slash)) ? out_path.substr(0, dot)
                                                                                    : out_path;
    return stem + "-probe.bmp";
}

constexpr const char* kNonInteractiveMsg = "longshot: stdin is not interactive; pass --region and --yes\n";

} // namespace

int cmd_capture(const Args& args) {
    if (args.verbose) fsic::log_set_level(fsic::LogLevel::Debug);
    if (args.quiet) fsic::log_set_level(fsic::LogLevel::Error);

    fsic::BackendOptions bopts;
    bopts.kind = parse_backend_kind(args.backend_kind);
    bopts.synthetic_dir = args.synthetic_dir;

    std::unique_ptr<fsic::IBackend> backend;
    fsic::Status st = fsic::create_backend(bopts, &backend);
    if (!st) {
        std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
        return 1;
    }
    st = backend->init();
    if (!st) {
        std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
        return 1;
    }

    fsic::PermissionStatus perms;
    st = backend->check_permissions(&perms);
    if (st && !(perms.screen_capture && perms.input_injection)) {
        std::fprintf(stderr, "longshot: permission denied: %s\n", perms.detail.c_str());
        return 3;
    }

    fsic::Rect region{};
    uint64_t window_id = args.has_window_id ? args.window_id : 0;

    if (args.has_region) {
        // Fully non-interactive path: region is already known, no probe, no prompts.
        region = fsic::Rect{args.region_x, args.region_y, args.region_w, args.region_h};
    } else {
        fsic::Point anchor{};
        if (args.has_anchor) {
            anchor = fsic::Point{args.anchor_x, args.anchor_y};
        } else {
            if (!stdin_is_tty()) {
                std::fprintf(stderr, "%s", kNonInteractiveMsg);
                return 2;
            }
            if (!wait_for_enter(
                    "Hover the mouse over the scrollable region you want, then press ENTER here.")) {
                std::fprintf(stderr, "%s", kNonInteractiveMsg);
                return 2;
            }
            st = backend->get_cursor_position(&anchor);
            if (!st) {
                std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
                return 1;
            }
        }

        fsic::WindowInfo win{};
        fsic::Rect search_bounds{};
        st = backend->window_at_point(anchor, &win);
        if (st) {
            window_id = win.id;
            search_bounds = win.client_px;
            backend->activate_window(window_id);
        } else {
            st = backend->get_virtual_desktop_bounds(&search_bounds);
            if (!st) {
                std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
                return 1;
            }
        }

        fsic::ProbeOptions popts;
        popts.search_bounds = search_bounds;
        fsic::ProbeResult presult;
        st = fsic::probe_scroll_region(*backend, anchor, popts, &presult);
        if (!st) {
            std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
            return 1;
        }
        region = presult.region;

        std::printf("detected scrollable region: %d,%d %dx%d\n", region.x, region.y, region.w, region.h);

        fsic::Image outlined = presult.before;
        fsic::draw_rect_outline(
            outlined,
            fsic::Rect{region.x - search_bounds.x, region.y - search_bounds.y, region.w, region.h}, 255, 0,
            255, 2);
        const std::string probe_path = probe_image_path(args.out_path);
        st = fsic::write_bmp(probe_path, outlined);
        if (!st) {
            std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
            return 1;
        }
        std::printf("wrote probe image: %s\n", probe_path.c_str());
    }

    if (args.has_inset) {
        region = fsic::Rect{region.x + args.inset_l, region.y + args.inset_t,
                             region.w - args.inset_l - args.inset_r, region.h - args.inset_t - args.inset_b};
        if (region.w < fsic::MIN_REGION_W || region.h < fsic::MIN_REGION_H) {
            std::fprintf(stderr, "longshot: --inset leaves region too small (%dx%d)\n", region.w, region.h);
            return 2;
        }
    }

    if (!args.yes) {
        if (!stdin_is_tty()) {
            std::fprintf(stderr, "%s", kNonInteractiveMsg);
            return 2;
        }
        if (!confirm_yes_no("Capture this region?", false)) {
            std::printf("aborted\n");
            return 4;
        }
    }

    fsic::CaptureConfig cfg;
    cfg.region = region;
    cfg.wheel_point =
        fsic::Point{region.x + std::min(24, region.w / 8), region.y + region.h / 2};
    cfg.window_id = window_id;
    cfg.notches = fsic::PROBE_NOTCHES;
    cfg.max_frames = fsic::MAX_FRAMES_DEFAULT;
    cfg.max_canvas_height = args.max_height > 0 ? args.max_height : fsic::MAX_CANVAS_HEIGHT_DEFAULT;
    cfg.use_keys = false;
    cfg.scroll_to_top = !args.no_scroll_to_top;
    cfg.keep_frames_dir = args.keep_frames_dir;

    fsic::CaptureResult result;
    st = fsic::run_capture_session(*backend, cfg, &result);
    if (!st) {
        std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
        return 1;
    }

    st = fsic::write_image_auto(args.out_path, result.stitched);
    if (!st) {
        std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
        return 1;
    }

    std::printf("captured %d frames -> stitched %dx%d, wrote %s\n", result.frames_captured,
                result.stitched.w, result.stitched.h, args.out_path.c_str());
    if (!result.report.warning.empty()) {
        std::fprintf(stderr, "warning: %s\n", result.report.warning.c_str());
    }
    return 0;
}

} // namespace fsic::cli
