#include "texture_array.hpp"
#include <glad/glad.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace Voidfall {

TextureArray::TextureArray(int width, int height, int layers)
    : m_width(width)
    , m_height(height)
    , m_layers(layers)
{
    initialize_procedural_materials();
}

TextureArray::~TextureArray() {
    if (m_albedo_tex != 0) glDeleteTextures(1, &m_albedo_tex);
    if (m_normal_tex != 0) glDeleteTextures(1, &m_normal_tex);
    if (m_rough_metal_tex != 0) glDeleteTextures(1, &m_rough_metal_tex);
    if (m_emissive_tex != 0) glDeleteTextures(1, &m_emissive_tex);
}

void TextureArray::initialize_procedural_materials() {
    size_t slice_size = m_width * m_height * 4;
    size_t total_size = slice_size * m_layers;

    std::vector<uint8_t> albedo_data(total_size, 0);
    std::vector<uint8_t> normal_data(total_size, 128); // default normal (0, 0, 1) -> (128, 128, 255)
    std::vector<uint8_t> rough_metal_data(total_size, 0);
    std::vector<uint8_t> emissive_data(total_size, 0);

    for (int l = 0; l < m_layers; ++l) {
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                size_t idx = (l * m_width * m_height + y * m_width + x) * 4;

                // Procedural noise factor
                float fx = static_cast<float>(x) / static_cast<float>(m_width);
                float fy = static_cast<float>(y) / static_cast<float>(m_height);
                float noise = (std::sin(fx * 35.0f) * std::cos(fy * 35.0f) +
                               std::sin(fx * 80.0f + fy * 60.0f) * 0.5f) * 0.5f + 0.5f;

                uint8_t r = 60, g = 60, b = 60, a = 255;
                uint8_t nr = 128, ng = 128, nb = 255;
                uint8_t rough = 200, metal = 10;
                uint8_t er = 0, eg = 0, eb = 0;

                switch (l) {
                    case 1: // Fractured Granite
                        r = static_cast<uint8_t>(65 + 40 * noise);
                        g = static_cast<uint8_t>(60 + 35 * noise);
                        b = static_cast<uint8_t>(58 + 30 * noise);
                        rough = 220; metal = 15;
                        break;
                    case 2: // Volcanic Basalt
                        r = static_cast<uint8_t>(30 + 20 * noise);
                        g = static_cast<uint8_t>(32 + 20 * noise);
                        b = static_cast<uint8_t>(38 + 25 * noise);
                        rough = 240; metal = 20;
                        break;
                    case 3: { // Voidite Crystal
                        float crystal_facets = std::abs(std::sin(fx * 12.0f) * std::cos(fy * 12.0f));
                        r = static_cast<uint8_t>(120 + 100 * crystal_facets);
                        g = static_cast<uint8_t>(30 + 40 * crystal_facets);
                        b = static_cast<uint8_t>(180 + 75 * crystal_facets);
                        rough = 40; metal = 25;
                        er = static_cast<uint8_t>(150 + 105 * crystal_facets);
                        eg = 30;
                        eb = 255;
                        break;
                    }
                    case 4: { // Industrial Bulkhead (Metallic Vault Plate)
                        bool is_border = (x < 2 || x >= m_width - 2 || y < 2 || y >= m_height - 2);
                        bool is_rivet = ((x % 16 <= 2) && (y % 16 <= 2));
                        if (is_border || is_rivet) {
                            r = 110; g = 115; b = 120;
                        } else {
                            r = static_cast<uint8_t>(80 + 15 * noise);
                            g = static_cast<uint8_t>(85 + 15 * noise);
                            b = static_cast<uint8_t>(92 + 15 * noise);
                        }
                        rough = 90; metal = 210;
                        break;
                    }
                    case 5: { // Reinforced Vault Door (Brass/Gold alloy)
                        r = static_cast<uint8_t>(150 + 30 * noise);
                        g = static_cast<uint8_t>(120 + 25 * noise);
                        b = static_cast<uint8_t>(50 + 20 * noise);
                        rough = 60; metal = 240;
                        break;
                    }
                    case 6: { // Thermite Slag (Molten Orange)
                        float crack = std::abs(std::sin(fx * 25.0f + noise) * std::cos(fy * 25.0f));
                        r = static_cast<uint8_t>(220 + 35 * crack);
                        g = static_cast<uint8_t>(90 + 80 * crack);
                        b = static_cast<uint8_t>(15);
                        rough = 160; metal = 80;
                        er = 255; eg = static_cast<uint8_t>(100 + 120 * crack); eb = 20;
                        break;
                    }
                    case 7: { // Radioactive Ore (Emerald glow)
                        float toxic_vein = std::pow(noise, 3.0f);
                        r = static_cast<uint8_t>(40 + 20 * noise);
                        g = static_cast<uint8_t>(60 + 180 * toxic_vein);
                        b = static_cast<uint8_t>(45 + 50 * toxic_vein);
                        rough = 140; metal = 40;
                        if (toxic_vein > 0.5f) {
                            er = 30; eg = 240; eb = 70;
                        }
                        break;
                    }
                    case 8: // Dredge Bedrock
                        r = 18; g = 18; b = 22;
                        rough = 250; metal = 40;
                        break;
                    default:
                        r = 70; g = 70; b = 70;
                        break;
                }

                // Perturb normal slightly with noise
                float dnx = (std::sin((fx + 0.01f) * 40.0f) - std::sin(fx * 40.0f)) * 30.0f;
                float dny = (std::cos((fy + 0.01f) * 40.0f) - std::cos(fy * 40.0f)) * 30.0f;
                nr = static_cast<uint8_t>(std::clamp(128.0f + dnx, 0.0f, 255.0f));
                ng = static_cast<uint8_t>(std::clamp(128.0f + dny, 0.0f, 255.0f));
                nb = 255;

                albedo_data[idx + 0] = r;
                albedo_data[idx + 1] = g;
                albedo_data[idx + 2] = b;
                albedo_data[idx + 3] = a;

                normal_data[idx + 0] = nr;
                normal_data[idx + 1] = ng;
                normal_data[idx + 2] = nb;
                normal_data[idx + 3] = 255;

                rough_metal_data[idx + 0] = rough;
                rough_metal_data[idx + 1] = metal;
                rough_metal_data[idx + 2] = 0;
                rough_metal_data[idx + 3] = 255;

                emissive_data[idx + 0] = er;
                emissive_data[idx + 1] = eg;
                emissive_data[idx + 2] = eb;
                emissive_data[idx + 3] = 255;
            }
        }
    }

    auto create_tex_array = [this](unsigned int& tex, const void* data) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, m_width, m_height, m_layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    };

    create_tex_array(m_albedo_tex, albedo_data.data());
    create_tex_array(m_normal_tex, normal_data.data());
    create_tex_array(m_rough_metal_tex, rough_metal_data.data());
    create_tex_array(m_emissive_tex, emissive_data.data());
}

void TextureArray::bind_albedo(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_albedo_tex);
}

void TextureArray::bind_normal(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_normal_tex);
}

void TextureArray::bind_rough_metal(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_rough_metal_tex);
}

void TextureArray::bind_emissive(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_emissive_tex);
}

} // namespace Voidfall
