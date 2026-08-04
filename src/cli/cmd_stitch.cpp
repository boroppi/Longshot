#include "commands.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <vector>

#include "args.h"
#include "fsic/image_io.h"
#include "fsic/stitch.h"
#include "fsic/tuning.h"

namespace fsic::cli {

namespace fs = std::filesystem;

int cmd_stitch(const Args& args) {
    std::vector<fs::path> frame_paths;
    std::error_code ec;
    fs::directory_iterator it(args.frames_dir, ec);
    if (ec) {
        std::fprintf(stderr, "longshot: cannot read directory: %s\n", args.frames_dir.c_str());
        return 1;
    }
    for (const auto& entry : it) {
        if (!entry.is_regular_file()) continue;
        const std::string name = entry.path().filename().string();
        if (name.rfind("frame_", 0) == 0 && entry.path().extension() == ".bmp") {
            frame_paths.push_back(entry.path());
        }
    }
    if (frame_paths.empty()) {
        std::fprintf(stderr, "longshot: no frame_*.bmp files found in %s\n", args.frames_dir.c_str());
        return 1;
    }
    std::sort(frame_paths.begin(), frame_paths.end());

    std::vector<fsic::Image> frames;
    frames.reserve(frame_paths.size());
    for (const auto& p : frame_paths) {
        fsic::Image img;
        fsic::Status st = fsic::read_bmp(p.string(), &img);
        if (!st) {
            std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
            return 1;
        }
        frames.push_back(std::move(img));
    }

    fsic::StitchOptions opts;
    opts.min_overlap_rows = fsic::MIN_OVERLAP_ROWS;
    opts.min_overlap_fraction = fsic::MIN_OVERLAP_FRACTION;
    opts.max_canvas_height = args.max_height > 0 ? args.max_height : fsic::MAX_CANVAS_HEIGHT_DEFAULT;

    fsic::Image result;
    fsic::StitchReport report;
    fsic::Status st = fsic::stitch_frames(frames, opts, &result, &report);
    if (!st) {
        std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
        return 1;
    }

    st = fsic::write_image_auto(args.out_path, result);
    if (!st) {
        std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
        return 1;
    }

    std::printf("stitched %d frames -> %dx%d (static bands: top=%d bottom=%d)\n", report.frames_used,
                result.w, result.h, report.top_band_h, report.bottom_band_h);
    if (!report.warning.empty()) {
        std::fprintf(stderr, "warning: %s\n", report.warning.c_str());
    }
    return 0;
}

} // namespace fsic::cli
