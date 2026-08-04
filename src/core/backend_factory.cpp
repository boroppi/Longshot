#include "fsic/backend_factory.h"

#if defined(FSIC_HAVE_SYNTHETIC)
#include "platform/synthetic/synthetic_backend.h"
#endif
#if defined(FSIC_PLATFORM_WIN32)
#include "platform/win32/win32_backend.h"
#endif

namespace fsic {

Status create_backend(const BackendOptions& opts, std::unique_ptr<IBackend>* out) {
    BackendKind kind = opts.kind;
    if (kind == BackendKind::Auto) {
#if defined(FSIC_PLATFORM_WIN32)
        kind = BackendKind::Win32;
#elif defined(__APPLE__)
        kind = BackendKind::MacOS;
#elif defined(__linux__)
        kind = BackendKind::X11;
#else
        return Status::Error(
            "create_backend: no default backend available on this platform; pass --backend explicitly");
#endif
    }

    switch (kind) {
        case BackendKind::Synthetic:
#if defined(FSIC_HAVE_SYNTHETIC)
            if (opts.synthetic_dir.empty()) {
                return Status::Error("synthetic backend requires --synthetic-dir");
            }
            out->reset(new SyntheticBackend(opts.synthetic_dir));
            return Status::Ok();
#else
            return Status::Error("synthetic backend not compiled in");
#endif
        case BackendKind::Win32:
#if defined(FSIC_PLATFORM_WIN32)
            out->reset(new Win32Backend());
            return Status::Ok();
#else
            return Status::Error("win32 backend not compiled in this build");
#endif
        case BackendKind::X11:
            return Status::Error("x11 backend not compiled in this build");
        case BackendKind::MacOS:
            return Status::Error("macos backend not compiled in this build");
        case BackendKind::Auto:
        default:
            return Status::Error("create_backend: unresolved backend kind");
    }
}

} // namespace fsic
