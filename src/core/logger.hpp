#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>

namespace Voidfall {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

class Logger {
public:
    static void init(const std::string& log_filepath = "voidfall.log");
    static void shutdown();

    static void log(LogLevel level, const std::string& tag, const std::string& message);
    static void debug(const std::string& tag, const std::string& message);
    static void info(const std::string& tag, const std::string& message);
    static void warn(const std::string& tag, const std::string& message);
    static void error(const std::string& tag, const std::string& message);
    static void fatal(const std::string& tag, const std::string& message);

    // Callbacks setup
    static void setup_glfw_error_callback();
    static void setup_gl_debug();
    static void setup_crash_handler();

    static const std::string& log_file_path();

private:
    static std::string current_timestamp();
    static const char* level_to_string(LogLevel level);

    static std::ofstream s_file_stream;
    static std::mutex s_mutex;
    static std::string s_filepath;
    static bool s_initialized;
};

} // namespace Voidfall

// Convenience stream macros
#define VF_LOG_DEBUG(tag, expr) do { std::ostringstream _oss; _oss << expr; ::Voidfall::Logger::debug(tag, _oss.str()); } while(0)
#define VF_LOG_INFO(tag, expr)  do { std::ostringstream _oss; _oss << expr; ::Voidfall::Logger::info(tag, _oss.str()); } while(0)
#define VF_LOG_WARN(tag, expr)  do { std::ostringstream _oss; _oss << expr; ::Voidfall::Logger::warn(tag, _oss.str()); } while(0)
#define VF_LOG_ERROR(tag, expr) do { std::ostringstream _oss; _oss << expr; ::Voidfall::Logger::error(tag, _oss.str()); } while(0)
#define VF_LOG_FATAL(tag, expr) do { std::ostringstream _oss; _oss << expr; ::Voidfall::Logger::fatal(tag, _oss.str()); } while(0)
