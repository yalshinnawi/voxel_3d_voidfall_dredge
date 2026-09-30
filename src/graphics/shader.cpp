#include "shader.hpp"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace Voidfall {

Shader::~Shader() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

std::string Shader::read_file_to_string(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Shader] Error opening shader file: " << filepath << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

unsigned int Shader::compile_stage(unsigned int type, const std::string& source, const std::string& path) {
    if (source.empty()) return 0;

    unsigned int shader = glCreateShader(type);
    const char* src_ptr = source.c_str();
    glShaderSource(shader, 1, &src_ptr, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[1024];
        glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
        std::cerr << "[Shader] Compilation error in " << path << ":\n" << info_log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::load_graphics(const std::string& vert_path, const std::string& frag_path) {
    std::string vert_src = read_file_to_string(vert_path);
    std::string frag_src = read_file_to_string(frag_path);

    unsigned int vert_shader = compile_stage(GL_VERTEX_SHADER, vert_src, vert_path);
    unsigned int frag_shader = compile_stage(GL_FRAGMENT_SHADER, frag_src, frag_path);

    if (!vert_shader || !frag_shader) {
        if (vert_shader) glDeleteShader(vert_shader);
        if (frag_shader) glDeleteShader(frag_shader);
        return false;
    }

    if (m_program != 0) {
        glDeleteProgram(m_program);
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vert_shader);
    glAttachShader(m_program, frag_shader);
    glLinkProgram(m_program);

    int success = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[1024];
        glGetProgramInfoLog(m_program, sizeof(info_log), nullptr, info_log);
        std::cerr << "[Shader] Linking error (" << vert_path << ", " << frag_path << "):\n" << info_log << std::endl;
        glDeleteProgram(m_program);
        m_program = 0;
        glDeleteShader(vert_shader);
        glDeleteShader(frag_shader);
        return false;
    }

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    return true;
}

bool Shader::load_compute(const std::string& comp_path) {
    std::string comp_src = read_file_to_string(comp_path);
    unsigned int comp_shader = compile_stage(GL_COMPUTE_SHADER, comp_src, comp_path);
    if (!comp_shader) return false;

    if (m_program != 0) {
        glDeleteProgram(m_program);
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, comp_shader);
    glLinkProgram(m_program);

    int success = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[1024];
        glGetProgramInfoLog(m_program, sizeof(info_log), nullptr, info_log);
        std::cerr << "[Shader] Compute linking error in " << comp_path << ":\n" << info_log << std::endl;
        glDeleteProgram(m_program);
        m_program = 0;
        glDeleteShader(comp_shader);
        return false;
    }

    glDeleteShader(comp_shader);
    return true;
}

void Shader::use() const {
    if (m_program != 0) {
        glUseProgram(m_program);
    }
}

int Shader::get_uniform_location(const std::string& name) const {
    auto it = m_uniform_cache.find(name);
    if (it != m_uniform_cache.end()) {
        return it->second;
    }
    int loc = glGetUniformLocation(m_program, name.c_str());
    m_uniform_cache[name] = loc;
    return loc;
}

void Shader::set_mat4(const std::string& name, const glm::mat4& mat) const {
    int loc = get_uniform_location(name);
    if (loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, &mat[0][0]);
    }
}

void Shader::set_vec4(const std::string& name, const glm::vec4& vec) const {
    int loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform4f(loc, vec.x, vec.y, vec.z, vec.w);
    }
}

void Shader::set_vec3(const std::string& name, const glm::vec3& vec) const {
    int loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform3f(loc, vec.x, vec.y, vec.z);
    }
}

void Shader::set_vec2(const std::string& name, const glm::vec2& vec) const {
    int loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform2f(loc, vec.x, vec.y);
    }
}

void Shader::set_float(const std::string& name, float val) const {
    int loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform1f(loc, val);
    }
}

void Shader::set_int(const std::string& name, int val) const {
    int loc = get_uniform_location(name);
    if (loc != -1) {
        glUniform1i(loc, val);
    }
}

} // namespace Voidfall
