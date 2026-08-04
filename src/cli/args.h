#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include "fsic/status.h"

namespace fsic::cli {

enum class Command { None, Version, Help, Stitch, List, Capture, Doctor };

struct Args {
    Command command = Command::None;
    std::string frames_dir;
    std::string out_path;
    int max_height = 0;  // 0 means "use default"

    // list / capture (shared)
    std::string backend_kind = "auto";  // auto, win32, x11, macos, synthetic
    std::string synthetic_dir;
    bool json = false;  // list only

    // capture-specific
    bool has_anchor = false;
    int anchor_x = 0;
    int anchor_y = 0;

    bool has_region = false;
    int region_x = 0, region_y = 0, region_w = 0, region_h = 0;

    bool has_window_id = false;
    uint64_t window_id = 0;

    bool has_inset = false;
    int inset_l = 0, inset_t = 0, inset_r = 0, inset_b = 0;

    bool no_probe = false;
    bool no_scroll_to_top = false;
    std::string keep_frames_dir;

    bool yes = false;
    bool verbose = false;
    bool quiet = false;
};

Status parse_args(int argc, char** argv, Args* out);
void print_usage(FILE* stream);

} // namespace fsic::cli
