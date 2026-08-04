#include "args.h"
#include "commands.h"
#include "fsic/status.h"

#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
    try {
        fsic::cli::Args args;
        fsic::Status status = fsic::cli::parse_args(argc, argv, &args);
        if (!status) {
            std::fprintf(stderr, "longshot: %s\n", status.message.c_str());
            fsic::cli::print_usage(stderr);
            return 2;
        }
        switch (args.command) {
            case fsic::cli::Command::Version:
                std::printf("longshot 0.1.0 (backend: none)\n");
                return 0;
            case fsic::cli::Command::Help:
                fsic::cli::print_usage(stdout);
                return 0;
            case fsic::cli::Command::Stitch:
                return fsic::cli::cmd_stitch(args);
            case fsic::cli::Command::List:
                return fsic::cli::cmd_list(args);
            case fsic::cli::Command::Capture:
                return fsic::cli::cmd_capture(args);
            case fsic::cli::Command::Doctor:
                return fsic::cli::cmd_doctor(args);
            default:
                fsic::cli::print_usage(stderr);
                return 2;
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "longshot: fatal: %s\n", e.what());
        return 1;
    }
}

