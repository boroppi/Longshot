#pragma once
#include "fsic/status.h"

namespace fsic::cli {
struct Args;
int cmd_stitch(const Args& args);
int cmd_list(const Args& args);
int cmd_capture(const Args& args);
int cmd_doctor(const Args& args);
} // namespace fsic::cli
