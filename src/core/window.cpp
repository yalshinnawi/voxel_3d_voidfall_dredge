#include "window.hpp"
#include "logger.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

namespace Voidfall {

Window::Window(const WindowConfig& config)
    : m_width(config.width)
    , m_height(config.height)
{
    Logger::setup_glfw_error_callback();

    if (!glfwInit()) {
        VF_LOG_FATAL("Window", "Failed to initialize GLFW subsystem");
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    m_window = glfwCreateWindow(m_width, m_height, config.title.c_str(), nullptr, nullptr);
    if (!m_window) {
        VF_LOG_WARN("Window", "OpenGL 4.5 window creation failed, attempting fallback to OpenGL 4.3...");
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        m_window = glfwCreateWindow(m_width, m_height, config.title.c_str(), nullptr, nullptr);
        if (!m_window) {
            glfwTerminate();
            VF_LOG_FATAL("Window", "Failed to create GLFW OpenGL 4.3+ window context");
            throw std::runtime_error("Failed to create GLFW OpenGL 4.3+ window");
        }
    }

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

    glfwSwapInterval(config.vsync ? 1 : 0);

    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);
    glfwSetCursorPosCallback(m_window, mouse_callback);
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

void Window::swap_buffers() {
    glfwSwapBuffers(m_window);
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

void Window::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && key >= 0 && key < 512) {
        if (action == GLFW_PRESS) {
            self->m_keys_down.set(key, true);
        } else if (action == GLFW_RELEASE) {
            self->m_keys_down.set(key, false);
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
