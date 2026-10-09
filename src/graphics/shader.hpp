#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace Voidfall {

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool load_graphics(const std::string& vert_path, const std::string& frag_path);
    bool load_compute(const std::string& comp_path);

    void use() const;
    unsigned int id() const { return m_program; }

    void set_mat4(const std::string& name, const glm::mat4& mat) const;
    void set_vec4(const std::string& name, const glm::vec4& vec) const;
    void set_vec3(const std::string& name, const glm::vec3& vec) const;
    void set_vec2(const std::string& name, const glm::vec2& vec) const;
    void set_float(const std::string& name, float val) const;
    void set_int(const std::string& name, int val) const;

    void Bind() const { use(); }
    void SetMat4(const std::string& name, const glm::mat4& mat) const { set_mat4(name, mat); }
    void SetVec4(const std::string& name, const glm::vec4& vec) const { set_vec4(name, vec); }
    void SetVec3(const std::string& name, const glm::vec3& vec) const { set_vec3(name, vec); }
    void SetVec2(const std::string& name, const glm::vec2& vec) const { set_vec2(name, vec); }
    void SetFloat(const std::string& name, float val) const { set_float(name, val); }
    void SetInt(const std::string& name, int val) const { set_int(name, val); }

private:
    static std::string read_file_to_string(const std::string& filepath);
    static unsigned int compile_stage(unsigned int type, const std::string& source, const std::string& path);
    int get_uniform_location(const std::string& name) const;

    unsigned int m_program{0};
    mutable std::unordered_map<std::string, int> m_uniform_cache;
};

} // namespace Voidfall
