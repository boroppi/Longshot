#include "args.h"

#include <cstdlib>

namespace fsic::cli {

namespace {

bool parse_point(const std::string& s, int* x, int* y) {
    const size_t comma = s.find(',');
    if (comma == std::string::npos) return false;
    *x = std::atoi(s.substr(0, comma).c_str());
    *y = std::atoi(s.substr(comma + 1).c_str());
    return true;
}

// Splits a comma-separated list of exactly `count` integers.
bool parse_int_list(const std::string& s, int* out, int count) {
    size_t start = 0;
    for (int i = 0; i < count; ++i) {
        const size_t comma = s.find(',', start);
        const bool last = (i == count - 1);
        if (last != (comma == std::string::npos)) return false;
        const std::string field = last ? s.substr(start) : s.substr(start, comma - start);
        if (field.empty()) return false;
        out[i] = std::atoi(field.c_str());
        start = comma + 1;
    }
    return true;
}

} // namespace

Status parse_args(int argc, char** argv, Args* out) {
    if (argc < 2) {
        out->command = Command::Help;
        return Status::Ok();
    }
    std::string cmd = argv[1];
    if (cmd == "version") {
        out->command = Command::Version;
    } else if (cmd == "help" || cmd == "-h" || cmd == "--help") {
        out->command = Command::Help;
    } else if (cmd == "stitch") {
        out->command = Command::Stitch;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--frames" && i + 1 < argc) {
                out->frames_dir = argv[++i];
            } else if (arg == "--out" && i + 1 < argc) {
                out->out_path = argv[++i];
            } else if (arg == "--max-height" && i + 1 < argc) {
                out->max_height = std::atoi(argv[++i]);
            } else {
                return Status::Error("unknown option: " + arg);
            }
        }
        if (out->frames_dir.empty() || out->out_path.empty()) {
            return Status::Error("stitch requires --frames DIR and --out PATH");
        }
    } else if (cmd == "list") {
        out->command = Command::List;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--backend" && i + 1 < argc) {
                out->backend_kind = argv[++i];
            } else if (arg == "--synthetic-dir" && i + 1 < argc) {
                out->synthetic_dir = argv[++i];
            } else if (arg == "--json") {
                out->json = true;
            } else {
                return Status::Error("unknown option: " + arg);
            }
        }
    } else if (cmd == "capture") {
        out->command = Command::Capture;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--backend" && i + 1 < argc) {
                out->backend_kind = argv[++i];
            } else if (arg == "--synthetic-dir" && i + 1 < argc) {
                out->synthetic_dir = argv[++i];
            } else if (arg == "--out" && i + 1 < argc) {
                out->out_path = argv[++i];
            } else if (arg == "--anchor" && i + 1 < argc) {
                if (!parse_point(argv[++i], &out->anchor_x, &out->anchor_y)) {
                    return Status::Error("invalid --anchor value, expected X,Y");
                }
                out->has_anchor = true;
            } else if (arg == "--region" && i + 1 < argc) {
                int vals[4];
                if (!parse_int_list(argv[++i], vals, 4)) {
                    return Status::Error("invalid --region value, expected X,Y,W,H");
                }
                out->region_x = vals[0];
                out->region_y = vals[1];
                out->region_w = vals[2];
                out->region_h = vals[3];
                out->has_region = true;
            } else if (arg == "--window-id" && i + 1 < argc) {
                out->window_id = std::strtoull(argv[++i], nullptr, 10);
                out->has_window_id = true;
            } else if (arg == "--inset" && i + 1 < argc) {
                int vals[4];
                if (!parse_int_list(argv[++i], vals, 4)) {
                    return Status::Error("invalid --inset value, expected L,T,R,B");
                }
                out->inset_l = vals[0];
                out->inset_t = vals[1];
                out->inset_r = vals[2];
                out->inset_b = vals[3];
                out->has_inset = true;
            } else if (arg == "--no-probe") {
                out->no_probe = true;
            } else if (arg == "--no-scroll-to-top") {
                out->no_scroll_to_top = true;
            } else if (arg == "--keep-frames" && i + 1 < argc) {
                out->keep_frames_dir = argv[++i];
            } else if (arg == "--max-height" && i + 1 < argc) {
                out->max_height = std::atoi(argv[++i]);
            } else if (arg == "--yes") {
                out->yes = true;
            } else if (arg == "-v" || arg == "--verbose") {
                out->verbose = true;
            } else if (arg == "-q" || arg == "--quiet") {
                out->quiet = true;
            } else {
                return Status::Error("unknown option: " + arg);
            }
        }
        if (out->out_path.empty()) {
            return Status::Error("capture requires --out PATH");
        }
        if (out->no_probe && !out->has_region) {
            return Status::Error("--no-probe requires --region");
        }
    } else if (cmd == "doctor") {
        out->command = Command::Doctor;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--backend" && i + 1 < argc) {
                out->backend_kind = argv[++i];
            } else if (arg == "--synthetic-dir" && i + 1 < argc) {
                out->synthetic_dir = argv[++i];
            } else {
                return Status::Error("unknown option: " + arg);
            }
        }
    } else {
        return Status::Error("unknown command: " + cmd);
    }
    return Status::Ok();
}

void print_usage(FILE* stream) {
    std::fprintf(stream, "longshot <command> [options]\n\n");
    std::fprintf(stream, "commands:\n");
    std::fprintf(stream, "  version    print version information\n");
    std::fprintf(stream, "  help       show this help message\n");
    std::fprintf(stream, "  stitch     stitch frame_*.bmp files from a directory into one image\n");
    std::fprintf(stream, "             --frames DIR --out PATH [--max-height N]\n");
    std::fprintf(stream, "  list       list windows visible to a backend\n");
    std::fprintf(stream, "             --backend KIND [--synthetic-dir DIR] [--json]\n");
    std::fprintf(stream, "  capture    capture a scrollable region top-to-bottom and stitch it\n");
    std::fprintf(stream, "             --out PATH [--backend KIND] [--synthetic-dir DIR]\n");
    std::fprintf(stream, "             [--anchor X,Y | --region X,Y,W,H [--window-id N]]\n");
    std::fprintf(stream,
                 "             [--inset L,T,R,B] [--no-probe] [--no-scroll-to-top]\n");
    std::fprintf(stream,
                 "             [--keep-frames DIR] [--max-height N] [--yes] [-v] [-q]\n");
    std::fprintf(stream, "  doctor     diagnose backend/permission/capture readiness\n");
    std::fprintf(stream, "             [--backend KIND] [--synthetic-dir DIR]\n");
}

} // namespace fsic::cli

