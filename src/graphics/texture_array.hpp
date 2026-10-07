#pragma once
#include <string>
#include <vector>

namespace Voidfall {

class TextureArray {
public:
    TextureArray(int width = 128, int height = 128, int layers = 16);
    ~TextureArray();

    TextureArray(const TextureArray&) = delete;
    TextureArray& operator=(const TextureArray&) = delete;

    void initialize_procedural_materials();

    void bind_albedo(unsigned int unit = 0) const;
    void bind_normal(unsigned int unit = 1) const;
    void bind_rough_metal(unsigned int unit = 2) const;
    void bind_emissive(unsigned int unit = 3) const;

    unsigned int albedo_id() const { return m_albedo_tex; }
    unsigned int normal_id() const { return m_normal_tex; }
    unsigned int rough_metal_id() const { return m_rough_metal_tex; }
    unsigned int emissive_id() const { return m_emissive_tex; }

private:
    int m_width{128};
    int m_height{128};
    int m_layers{16};

    unsigned int m_albedo_tex{0};
    unsigned int m_normal_tex{0};
    unsigned int m_rough_metal_tex{0};
    unsigned int m_emissive_tex{0};
};

} // namespace Voidfall
