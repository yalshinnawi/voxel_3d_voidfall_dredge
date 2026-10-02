#pragma once
#include <string>
#include <functional>
#include <bitset>
#include <glm/glm.hpp>

struct GLFWwindow;

namespace Voidfall {

struct WindowConfig {
    std::string title{"Voidfall: Dredge (Multiplayer Edition)"};
    int width{1600};
    int height{900};
    bool vsync{true};
    bool fullscreen{false};
    bool visible{true};
};

class Window {
public:
    explicit Window(const WindowConfig& config = WindowConfig{});
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool should_close() const;
    void poll_events();
    void swap_buffers();

    GLFWwindow* handle() const { return m_window; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    float aspect_ratio() const { return static_cast<float>(m_width) / static_cast<float>(m_height > 0 ? m_height : 1); }

    void set_cursor_locked(bool locked);
    bool is_cursor_locked() const { return m_cursor_locked; }

    bool is_key_down(int key) const;
    bool is_mouse_button_down(int button) const;
    glm::dvec2 get_cursor_pos() const;
    glm::dvec2 get_cursor_delta();

    using ResizeCallback = std::function<void(int, int)>;
    void set_resize_callback(ResizeCallback cb) { m_resize_cb = std::move(cb); }

    using KeyCallback = std::function<void(int key, int action)>;
    void set_key_callback(KeyCallback cb) { m_key_cb = std::move(cb); }

private:
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

    GLFWwindow* m_window{nullptr};
    int m_width{1600};
    int m_height{900};
    bool m_cursor_locked{true};

    double m_last_mouse_x{0.0};
    double m_last_mouse_y{0.0};
    double m_mouse_delta_x{0.0};
    double m_mouse_delta_y{0.0};
    bool m_first_mouse{true};

    // Continuous key and button bitsets to prevent sticking
    std::bitset<512> m_keys_down;
    std::bitset<16> m_mouse_down;

    ResizeCallback m_resize_cb;
    KeyCallback m_key_cb;
};

} // namespace Voidfall
