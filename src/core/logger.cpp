#include "logger.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <filesystem>

namespace Voidfall {

std::ofstream Logger::s_file_stream;
std::mutex Logger::s_mutex;
std::string Logger::s_filepath = "voidfall.log";
bool Logger::s_initialized = false;

std::string Logger::current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};
#if defined(_WIN32)
    localtime_s(&bt, &timer);
#else
    localtime_r(&timer, &bt);
#endif
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

const char* Logger::level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default: return "LOG";
    }
}

void Logger::init(const std::string& log_filepath) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_filepath = log_filepath;
    s_file_stream.open(s_filepath, std::ios::out | std::ios::trunc);
    s_initialized = true;

    std::string timestamp = current_timestamp();
    std::string sep(70, '=');
    s_file_stream << sep << "\n";
    s_file_stream << "  VOIDFALL: DREDGE - EXECUTION & ERROR LOG\n";
    s_file_stream << "  Session started: " << timestamp << "\n";
    s_file_stream << "  Working directory: " << std::filesystem::current_path().string() << "\n";
    s_file_stream << sep << "\n\n";
    s_file_stream.flush();
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_file_stream.is_open()) {
        s_file_stream << "\n[Logger] Session closed cleanly at " << current_timestamp() << "\n";
        s_file_stream.close();
    }
    s_initialized = false;
}

const std::string& Logger::log_file_path() {
    return s_filepath;
}

void Logger::log(LogLevel level, const std::string& tag, const std::string& message) {
    std::lock_guard<std::mutex> lock(s_mutex);
    std::string time_str = current_timestamp();
    const char* lvl_str = level_to_string(level);

    // Formatted line: [2026-09-30 12:30:00.123] [INFO] [Shader] Loading shader...
    std::ostringstream line;
    line << "[" << time_str << "] [" << lvl_str << "] [" << tag << "] " << message << "\n";
    std::string out = line.str();

    // Console output
    if (level == LogLevel::Error || level == LogLevel::Fatal) {
        std::cerr << out;
        std::cerr.flush();
    } else {
        std::cout << out;
        std::cout.flush();
    }

    // File output
    if (s_file_stream.is_open()) {
        s_file_stream << out;
        if (level == LogLevel::Error || level == LogLevel::Fatal || level == LogLevel::Warn) {
            s_file_stream.flush();
        }
    }
}

void Logger::debug(const std::string& tag, const std::string& message) {
    log(LogLevel::Debug, tag, message);
}

void Logger::info(const std::string& tag, const std::string& message) {
    log(LogLevel::Info, tag, message);
}

void Logger::warn(const std::string& tag, const std::string& message) {
    log(LogLevel::Warn, tag, message);
}

void Logger::error(const std::string& tag, const std::string& message) {
    log(LogLevel::Error, tag, message);
}

void Logger::fatal(const std::string& tag, const std::string& message) {
    log(LogLevel::Fatal, tag, message);
}

void Logger::setup_glfw_error_callback() {
    glfwSetErrorCallback([](int error_code, const char* description) {
        std::ostringstream oss;
        oss << "GLFW Error (0x" << std::hex << error_code << std::dec << "): " << (description ? description : "Unknown");
        Logger::error("GLFW", oss.str());
    });
}

#ifdef _WIN32
static LONG WINAPI unhandled_crash_filter(EXCEPTION_POINTERS* ep) {
    std::ostringstream oss;
    DWORD code = ep ? ep->ExceptionRecord->ExceptionCode : 0;
    void* addr = ep ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    oss << "CRASH: Unhandled Hardware/OS Exception Code 0x" << std::hex << code
        << " at address 0x" << addr;
    Logger::fatal("CrashHandler", oss.str());
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void Logger::setup_crash_handler() {
#ifdef _WIN32
    SetUnhandledExceptionFilter(unhandled_crash_filter);
#endif
}

typedef void (APIENTRY *GLDEBUGPROC)(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam
);

typedef void (APIENTRY *PFNGLDEBUGMESSAGECALLBACKPROC)(GLDEBUGPROC callback, const void* userParam);

static void APIENTRY gl_debug_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam
) {
    // Ignore benign notifications and redundant buffer memory warnings
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::ostringstream oss;
    oss << "OpenGL Debug [ID " << id << "]: " << (message ? message : "");

    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        Logger::error("OpenGL", oss.str());
    } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
        Logger::warn("OpenGL", oss.str());
    } else {
        Logger::debug("OpenGL", oss.str());
    }
}

void Logger::setup_gl_debug() {
    auto debug_func = reinterpret_cast<PFNGLDEBUGMESSAGECALLBACKPROC>(glfwGetProcAddress("glDebugMessageCallback"));
    if (debug_func) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        debug_func(gl_debug_callback, nullptr);
        Logger::info("OpenGL", "glDebugMessageCallback registered successfully");
    } else {
        Logger::info("OpenGL", "glDebugMessageCallback is not exposed by this driver");
    }
}

} // namespace Voidfall
