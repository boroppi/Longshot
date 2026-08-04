#pragma once

namespace fsic {

enum class LogLevel { Error, Warn, Info, Debug };

void log_set_level(LogLevel level);
#if defined(__GNUC__) || defined(__clang__)
void log_msg(LogLevel level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
#else
void log_msg(LogLevel level, const char* fmt, ...);
#endif

} // namespace fsic

#define FSIC_LOGE(...) ::fsic::log_msg(::fsic::LogLevel::Error, __VA_ARGS__)
#define FSIC_LOGW(...) ::fsic::log_msg(::fsic::LogLevel::Warn, __VA_ARGS__)
#define FSIC_LOGI(...) ::fsic::log_msg(::fsic::LogLevel::Info, __VA_ARGS__)
#define FSIC_LOGD(...) ::fsic::log_msg(::fsic::LogLevel::Debug, __VA_ARGS__)
