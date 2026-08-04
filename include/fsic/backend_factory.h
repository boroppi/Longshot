#pragma once
#include <memory>
#include <string>
#include "fsic/backend.h"
#include "fsic/status.h"

namespace fsic {

enum class BackendKind { Auto, Win32, X11, MacOS, Synthetic };

struct BackendOptions {
    BackendKind kind = BackendKind::Auto;
    std::string synthetic_dir;
    bool allow_xwayland = false;
};

Status create_backend(const BackendOptions& opts, std::unique_ptr<IBackend>* out);

} // namespace fsic
