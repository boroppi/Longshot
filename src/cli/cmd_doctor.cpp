#include "commands.h"

#include <cstdio>
#include <memory>
#include <string>

#include "args.h"
#include "fsic/backend_factory.h"
#include "fsic/image_io.h"

namespace fsic::cli {

namespace {

BackendKind parse_backend_kind(const std::string& s) {
    if (s == "win32") return BackendKind::Win32;
    if (s == "x11") return BackendKind::X11;
    if (s == "macos") return BackendKind::MacOS;
    if (s == "synthetic") return BackendKind::Synthetic;
    return BackendKind::Auto;
}

bool is_all_black(const fsic::Image& img) {
    for (size_t i = 0; i + 3 < img.rgba.size(); i += 4) {
        if (img.rgba[i] != 0 || img.rgba[i + 1] != 0 || img.rgba[i + 2] != 0) return false;
    }
    return true;
}

} // namespace

int cmd_doctor(const Args& args) {
    fsic::BackendOptions bopts;
    bopts.kind = parse_backend_kind(args.backend_kind);
    bopts.synthetic_dir = args.synthetic_dir;

    std::unique_ptr<fsic::IBackend> backend;
    fsic::Status st = fsic::create_backend(bopts, &backend);
    if (!st) {
        std::fprintf(stderr, "longshot: %s\n", st.message.c_str());
        return 1;
    }
    std::printf("backend: %s\n", backend->name());

    st = backend->init();
    if (!st) {
        std::printf("init: FAILED (%s)\n", st.message.c_str());
        return 1;
    }
    std::printf("init: ok\n");

    fsic::PermissionStatus perms;
    backend->check_permissions(&perms);
    std::printf("permissions: screen_capture=%s input_injection=%s\n",
                perms.screen_capture ? "granted" : "denied", perms.input_injection ? "granted" : "denied");
    if (!perms.detail.empty()) std::printf("  detail: %s\n", perms.detail.c_str());

    fsic::Rect desktop{};
    st = backend->get_virtual_desktop_bounds(&desktop);
    if (st) {
        std::printf("virtual desktop: %d,%d %dx%d\n", desktop.x, desktop.y, desktop.w, desktop.h);
        const double scale =
            backend->get_scale_for_point(fsic::Point{desktop.x + desktop.w / 2, desktop.y + desktop.h / 2});
        std::printf("scale at desktop center: %.2f\n", scale);
    } else {
        std::printf("virtual desktop: FAILED (%s)\n", st.message.c_str());
    }

#if FSIC_HAVE_STB
    std::printf("PNG output: compiled in\n");
#else
    std::printf("PNG output: NOT compiled in (BMP only)\n");
#endif

    bool capture_ok = false;
    fsic::Point cursor{};
    st = backend->get_cursor_position(&cursor);
    if (st) {
        const fsic::Rect test_rect{cursor.x - 32, cursor.y - 32, 64, 64};
        fsic::Image img;
        st = backend->capture_rect(test_rect, &img);
        if (st) {
            fsic::write_bmp("longshot-doctor.bmp", img);
            const bool black = is_all_black(img);
            std::printf("test capture: wrote longshot-doctor.bmp (%s)\n",
                        black ? "WARNING: all-black" : "ok");
            capture_ok = !black;
        } else {
            std::printf("test capture: FAILED (%s)\n", st.message.c_str());
        }
    } else {
        std::printf("cursor position: FAILED (%s)\n", st.message.c_str());
    }

    const bool ready = perms.screen_capture && perms.input_injection && capture_ok;
    std::printf("\n%s\n", ready ? "READY: capture should work." : "NOT READY: see warnings above.");
    return ready ? 0 : 1;
}

} // namespace fsic::cli
