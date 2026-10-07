#include "window.hpp"
#include "logger.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#include <iostream>
#include <stdexcept>

namespace Voidfall {

Window::Window(const WindowConfig& config)
    : m_width(config.width)
    , m_height(config.height)
    , m_visible_requested(config.visible)
    , m_auto_screen_size(config.auto_screen_size)
    , m_fullscreen(config.fullscreen)
{
    Logger::setup_glfw_error_callback();

    if (!glfwInit()) {
        VF_LOG_FATAL("Window", "Failed to initialize GLFW subsystem");
        throw std::runtime_error("Failed to initialize GLFW");
    }

    GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* video_mode = primary_monitor ? glfwGetVideoMode(primary_monitor) : nullptr;

    if (config.auto_screen_size && primary_monitor && video_mode) {
        int work_x = 0, work_y = 0, work_w = 0, work_h = 0;
        glfwGetMonitorWorkarea(primary_monitor, &work_x, &work_y, &work_w, &work_h);
        if (work_w > 0 && work_h > 0) {
            m_width = work_w;
            m_height = work_h;
        } else if (video_mode->width > 0 && video_mode->height > 0) {
            m_width = video_mode->width;
            m_height = video_mode->height;
        }
        VF_LOG_INFO("Window", "Auto-detected primary monitor resolution: " << video_mode->width << "x" << video_mode->height
                    << " (Desktop workarea: " << m_width << "x" << m_height << ")");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    // Keep window strictly hidden initially during creation and initialization
    // to prevent any white canvas flash or uninitialized surface exposure.
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    if (config.auto_screen_size && !config.fullscreen) {
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    }

    // Always create windowed initially with GLFW_VISIBLE=FALSE so the window is never shown
    // with uninitialized buffers or default white OS background.
    m_window = glfwCreateWindow(m_width, m_height, config.title.c_str(), nullptr, nullptr);
    if (!m_window) {
        VF_LOG_WARN("Window", "OpenGL 4.5 window creation failed, attempting fallback to OpenGL 4.3...");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        m_window = glfwCreateWindow(m_width, m_height, config.title.c_str(), nullptr, nullptr);
        if (!m_window) {
            glfwTerminate();
            VF_LOG_FATAL("Window", "Failed to create GLFW OpenGL 4.3+ window context");
            throw std::runtime_error("Failed to create GLFW OpenGL 4.3+ window");
        }
    }

    if (config.auto_screen_size && !config.fullscreen) {
        glfwMaximizeWindow(m_window);
    }

    int fb_w = 0, fb_h = 0;
    glfwGetFramebufferSize(m_window, &fb_w, &fb_h);
    if (fb_w > 0 && fb_h > 0) {
        m_width = fb_w;
        m_height = fb_h;
    }
    VF_LOG_INFO("Window", "Window initialized with framebuffer resolution: " << m_width << "x" << m_height);

    glfwGetWindowPos(m_window, &m_windowed_x, &m_windowed_y);
    glfwGetWindowSize(m_window, &m_windowed_w, &m_windowed_h);

    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        VF_LOG_FATAL("Window", "Failed to initialize GLAD OpenGL loader");
        throw std::runtime_error("Failed to initialize GLAD OpenGL loader");
    }

    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);

    VF_LOG_INFO("Hardware", "GPU Vendor: " << (vendor ? reinterpret_cast<const char*>(vendor) : "Unknown"));
    VF_LOG_INFO("Hardware", "GPU Renderer: " << (renderer ? reinterpret_cast<const char*>(renderer) : "Unknown"));
    VF_LOG_INFO("Hardware", "OpenGL Version: " << (version ? reinterpret_cast<const char*>(version) : "Unknown"));
    VF_LOG_INFO("Hardware", "GLSL Version: " << (glsl_version ? reinterpret_cast<const char*>(glsl_version) : "Unknown"));

    Logger::setup_gl_debug();

#if defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(m_window);
    if (hwnd) {
        // Enforce dark mode titlebar and borders on Windows 10 & 11
        HMODULE dwmapi = LoadLibraryA("dwmapi.dll");
        if (dwmapi) {
            typedef HRESULT (WINAPI *FnDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
            auto fn = reinterpret_cast<FnDwmSetWindowAttribute>(GetProcAddress(dwmapi, "DwmSetWindowAttribute"));
            if (fn) {
                BOOL darkMode = TRUE;
                DWORD attr20 = 20; // DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 20H1+ and Windows 11)
                DWORD attr19 = 19; // Windows 10 1809 - 1909
                fn(hwnd, attr20, &darkMode, sizeof(darkMode));
                fn(hwnd, attr19, &darkMode, sizeof(darkMode));
            }
            FreeLibrary(dwmapi);
        }
    }
#endif

    glViewport(0, 0, m_width, m_height);

    // Prime both front and back buffers with deep voidfall clear color (0.015, 0.018, 0.024)
    // to ensure any display swap is completely dark and free of default OS white flash.
    glClearColor(0.015f, 0.018f, 0.024f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(m_window);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glfwSwapBuffers(m_window);

    glfwSwapInterval(config.vsync ? 1 : 0);

    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
    glfwSetCursorPosCallback(m_window, mouse_callback);
    glfwSetScrollCallback(m_window, scroll_callback);
    glfwSetKeyCallback(m_window, key_callback);
    glfwSetMouseButtonCallback(m_window, mouse_button_callback);

    set_cursor_locked(true);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

bool Window::should_close() const {
    return glfwWindowShouldClose(m_window);
}

void Window::poll_events() {
    m_mouse_delta_x = 0.0;
    m_mouse_delta_y = 0.0;
    glfwPollEvents();
}

void Window::show() {
    if (m_window && m_visible_requested && !m_is_visible) {
        if (m_fullscreen) {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            if (monitor) {
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                if (mode) {
                    glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                }
            }
        } else if (m_auto_screen_size) {
            glfwMaximizeWindow(m_window);
        }
        glfwShowWindow(m_window);
        glfwFocusWindow(m_window);
        m_is_visible = true;
    }
}

void Window::swap_buffers() {
    glfwSwapBuffers(m_window);
    if (m_visible_requested && !m_is_visible) {
        show();
    }
}

void Window::set_cursor_locked(bool locked) {
    m_cursor_locked = locked;
    m_first_mouse = true;
    m_mouse_delta_x = 0.0;
    m_mouse_delta_y = 0.0;

    if (locked) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

bool Window::is_fullscreen() const {
    return m_window && glfwGetWindowMonitor(m_window) != nullptr;
}

void Window::set_fullscreen(bool fullscreen) {
    if (!m_window) return;
    bool currently_fs = is_fullscreen();
    if (currently_fs == fullscreen) return;

    if (fullscreen) {
        glfwGetWindowPos(m_window, &m_windowed_x, &m_windowed_y);
        glfwGetWindowSize(m_window, &m_windowed_w, &m_windowed_h);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode) {
                glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            }
        }
    } else {
        glfwSetWindowMonitor(m_window, nullptr, m_windowed_x, m_windowed_y, m_windowed_w, m_windowed_h, 0);
        glfwMaximizeWindow(m_window);
    }

    int fb_w = 0, fb_h = 0;
    glfwGetFramebufferSize(m_window, &fb_w, &fb_h);
    if (fb_w > 0 && fb_h > 0) {
        m_width = fb_w;
        m_height = fb_h;
        glViewport(0, 0, m_width, m_height);
        if (m_resize_cb) {
            m_resize_cb(m_width, m_height);
        }
    }
}

void Window::toggle_fullscreen() {
    set_fullscreen(!is_fullscreen());
}

bool Window::is_key_down(int key) const {
    if (key >= 0 && key < 512 && m_keys_down.test(key)) {
        return true;
    }
    return glfwGetKey(m_window, key) == GLFW_PRESS;
}

bool Window::is_mouse_button_down(int button) const {
    if (button >= 0 && button < 16 && m_mouse_down.test(button)) {
        return true;
    }
    return glfwGetMouseButton(m_window, button) == GLFW_PRESS;
}

glm::dvec2 Window::get_cursor_pos() const {
    double xpos, ypos;
    glfwGetCursorPos(m_window, &xpos, &ypos);
    return glm::dvec2(xpos, ypos);
}

glm::dvec2 Window::get_cursor_delta() {
    if (!m_cursor_locked) {
        m_mouse_delta_x = 0.0;
        m_mouse_delta_y = 0.0;
        return glm::dvec2(0.0, 0.0);
    }
    glm::dvec2 delta(m_mouse_delta_x, m_mouse_delta_y);
    m_mouse_delta_x = 0.0;
    m_mouse_delta_y = 0.0;
    return delta;
}

double Window::get_scroll_delta_y() {
    double delta = m_scroll_delta_y;
    m_scroll_delta_y = 0.0;
    return delta;
}

void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_width = width;
        self->m_height = height;
        glViewport(0, 0, width, height);
        if (self->m_resize_cb) {
            self->m_resize_cb(width, height);
        }
    }
}

void Window::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_cursor_locked) {
        if (self->m_first_mouse) {
            self->m_last_mouse_x = xpos;
            self->m_last_mouse_y = ypos;
            self->m_first_mouse = false;
        }
        self->m_mouse_delta_x += (xpos - self->m_last_mouse_x);
        self->m_mouse_delta_y += (self->m_last_mouse_y - ypos); // Invert Y for natural look
        self->m_last_mouse_x = xpos;
        self->m_last_mouse_y = ypos;
    }
}

void Window::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_scroll_delta_y += yoffset;
    }
}

void Window::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        if (key >= 0 && key < 512) {
            if (action == GLFW_PRESS) {
                self->m_keys_down.set(key, true);
            } else if (action == GLFW_RELEASE) {
                self->m_keys_down.set(key, false);
            }
        }
        if (self->m_key_cb) {
            self->m_key_cb(key, action);
        }
    }
}

void Window::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && button >= 0 && button < 16) {
        if (action == GLFW_PRESS) {
            self->m_mouse_down.set(button, true);
        } else if (action == GLFW_RELEASE) {
            self->m_mouse_down.set(button, false);
        }
    }
}

} // namespace Voidfall
