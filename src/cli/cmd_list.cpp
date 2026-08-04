#include "commands.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "args.h"
#include "fsic/backend_factory.h"

namespace fsic::cli {

namespace {
BackendKind parse_backend_kind(const std::string& s) {
    if (s == "win32") return BackendKind::Win32;
    if (s == "x11") return BackendKind::X11;
    if (s == "macos") return BackendKind::MacOS;
    if (s == "synthetic") return BackendKind::Synthetic;
    return BackendKind::Auto;
}

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}
} // namespace

int cmd_list(const Args& args) {
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

    std::vector<fsic::WindowInfo> windows;
    st = backend->enumerate_windows(&windows);
    if (!st) {
        std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
        return 1;
    }

    if (args.json) {
        std::printf("[\n");
        for (size_t i = 0; i < windows.size(); ++i) {
            const auto& w = windows[i];
            std::printf(
                "  {\"id\": %llu, \"scale\": %.2f, \"frame\": {\"x\": %d, \"y\": %d, \"w\": %d, "
                "\"h\": %d}, \"app\": \"%s\", \"title\": \"%s\"}%s\n",
                static_cast<unsigned long long>(w.id), w.scale, w.frame_px.x, w.frame_px.y, w.frame_px.w,
                w.frame_px.h, json_escape(w.app_name).c_str(), json_escape(w.title).c_str(),
                (i + 1 < windows.size()) ? "," : "");
        }
        std::printf("]\n");
        return 0;
    }

    std::printf("%-6s %-6s %-20s %-20s %s\n", "ID", "SCALE", "FRAME(x,y,w,h)", "APP", "TITLE");
    for (const auto& w : windows) {
        char frame_buf[64];
        std::snprintf(frame_buf, sizeof(frame_buf), "%d,%d,%d,%d", w.frame_px.x, w.frame_px.y,
                      w.frame_px.w, w.frame_px.h);
        std::printf("%-6llu %-6.2f %-20s %-20s %s\n", static_cast<unsigned long long>(w.id), w.scale,
                    frame_buf, w.app_name.c_str(), w.title.c_str());
    }
    return 0;
}

} // namespace fsic::cli
