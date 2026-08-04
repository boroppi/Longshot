#pragma once

namespace fsic::cli {

bool wait_for_enter(const char* message);
bool confirm_yes_no(const char* message, bool default_yes);
bool stdin_is_tty();

} // namespace fsic::cli
