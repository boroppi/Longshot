#pragma once
#include <string>

namespace fsic {

struct Status {
    bool ok = true;
    std::string message;

    static Status Ok() { return Status{true, {}}; }
    static Status Error(std::string msg) { return Status{false, std::move(msg)}; }
    explicit operator bool() const { return ok; }
};

} // namespace fsic

#define FSIC_TRY(expr)                     \
    do {                                    \
        ::fsic::Status _st = (expr);        \
        if (!_st) return _st;               \
    } while (0)
