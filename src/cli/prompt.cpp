#include "prompt.h"

#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <io.h>
#define fsic_isatty _isatty
#define fsic_fileno _fileno
#else
#include <unistd.h>
#define fsic_isatty isatty
#define fsic_fileno fileno
#endif

namespace fsic::cli {

bool stdin_is_tty() { return fsic_isatty(fsic_fileno(stdin)) != 0; }

bool wait_for_enter(const char* message) {
    if (!stdin_is_tty()) return false;
    std::fprintf(stdout, "%s", message);
    std::fflush(stdout);
    std::string line;
    return static_cast<bool>(std::getline(std::cin, line));
}

bool confirm_yes_no(const char* message, bool default_yes) {
    if (!stdin_is_tty()) return default_yes;
    std::fprintf(stdout, "%s [%s]: ", message, default_yes ? "Y/n" : "y/N");
    std::fflush(stdout);
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    if (line.empty()) return default_yes;
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(line[0])));
    return c == 'y';
}

} // namespace fsic::cli
