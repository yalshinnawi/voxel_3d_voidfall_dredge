#include "texture_array.hpp"
#include <glad/glad.h>
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

namespace Voidfall {

namespace {

constexpr float TWO_PI = 6.283185307179586f;

inline float fractf(float x) { return x - std::floor(x); }

inline float hash2d(float x, float y) {
    float n = std::sin(x * 127.1f + y * 311.7f) * 43758.5453123f;
    return fractf(n);
}

inline float periodic_hash2d(int ix, int iy, int px, int py) {
    int wx = (ix % px + px) % px;
    int wy = (iy % py + py) % py;
    float n = std::sin(static_cast<float>(wx) * 127.1f + static_cast<float>(wy) * 311.7f) * 43758.5453123f;
    return fractf(n);
}

inline float noise2d(float x, float y) {
    float ix = std::floor(x);
    float iy = std::floor(y);
    float fx = fractf(x);
    float fy = fractf(y);

    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);

    float a = hash2d(ix, iy);
    float b = hash2d(ix + 1.0f, iy);
    float c = hash2d(ix, iy + 1.0f);
    float d = hash2d(ix + 1.0f, iy + 1.0f);

    return a * (1.0f - ux) * (1.0f - uy) +
           b * ux * (1.0f - uy) +
           c * (1.0f - ux) * uy +
           d * ux * uy;
}

inline float periodic_noise2d(float fx, float fy, int period) {
    float x = fx * static_cast<float>(period);
    float y = fy * static_cast<float>(period);
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float lx = fractf(x);
    float ly = fractf(y);

    float ux = lx * lx * (3.0f - 2.0f * lx);
    float uy = ly * ly * (3.0f - 2.0f * ly);

    float a = periodic_hash2d(ix, iy, period, period);
    float b = periodic_hash2d(ix + 1, iy, period, period);
    float c = periodic_hash2d(ix, iy + 1, period, period);
    float d = periodic_hash2d(ix + 1, iy + 1, period, period);

    return a * (1.0f - ux) * (1.0f - uy) +
           b * ux * (1.0f - uy) +
           c * (1.0f - ux) * uy +
           d * ux * uy;
}

inline float fbm2d(float x, float y, int octaves = 4) {
    float val = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        val += amp * noise2d(x * freq, y * freq);
        freq *= 2.02f;
        amp *= 0.5f;
    }
    return val;
}

inline float periodic_fbm2d(float fx, float fy, int base_freq, int octaves = 4) {
    float val = 0.0f;
    float amp = 0.5f;
    int freq = base_freq;
    for (int i = 0; i < octaves; ++i) {
        val += amp * periodic_noise2d(fx, fy, freq);
        freq *= 2;
        amp *= 0.5f;
    }
    return val;
}

struct VoronoiResult {
    float f1;       // Distance to closest seed
    float f2;       // Distance to second closest seed
    float seed_x;   // Position of closest seed
    float seed_y;
    int cell_id;
};

inline VoronoiResult voronoi2d(float x, float y) {
    float ix = std::floor(x);
    float iy = std::floor(y);
    float fx = fractf(x);
    float fy = fractf(y);

    float f1 = 8.0f;
    float f2 = 8.0f;
    float sx = 0.0f, sy = 0.0f;
    int cid = 0;

    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            float rx = hash2d(ix + static_cast<float>(i) * 13.1f, iy + static_cast<float>(j) * 17.7f);
            float ry = hash2d(ix + static_cast<float>(i) * 31.4f + 5.1f, iy + static_cast<float>(j) * 23.9f + 9.3f);
            float px = static_cast<float>(i) + rx;
            float py = static_cast<float>(j) + ry;
            float d = std::sqrt((px - fx) * (px - fx) + (py - fy) * (py - fy));
            if (d < f1) {
                f2 = f1;
                f1 = d;
                sx = ix + static_cast<float>(i) + rx;
                sy = iy + static_cast<float>(j) + ry;
                cid = static_cast<int>(rx * 1000.0f) ^ (static_cast<int>(ry * 1000.0f) << 4);
            } else if (d < f2) {
                f2 = d;
            }
        }
    }
    return {f1, f2, sx, sy, cid};
}

inline VoronoiResult periodic_voronoi2d(float fx, float fy, int num_cells) {
    float x = fx * static_cast<float>(num_cells);
    float y = fy * static_cast<float>(num_cells);
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float lx = fractf(x);
    float ly = fractf(y);

    float f1 = 8.0f;
    float f2 = 8.0f;
    float sx = 0.0f, sy = 0.0f;
    int cid = 0;

    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            int cell_x = (ix + i) % num_cells;
            if (cell_x < 0) cell_x += num_cells;
            int cell_y = (iy + j) % num_cells;
            if (cell_y < 0) cell_y += num_cells;

            float rx = hash2d(static_cast<float>(cell_x) * 13.1f, static_cast<float>(cell_y) * 17.7f);
            float ry = hash2d(static_cast<float>(cell_x) * 31.4f + 5.1f, static_cast<float>(cell_y) * 23.9f + 9.3f);
            float px = static_cast<float>(i) + rx;
            float py = static_cast<float>(j) + ry;
            float d = std::sqrt((px - lx) * (px - lx) + (py - ly) * (py - ly));
            if (d < f1) {
                f2 = f1;
                f1 = d;
                sx = static_cast<float>(cell_x) + rx;
                sy = static_cast<float>(cell_y) + ry;
                cid = static_cast<int>(rx * 1000.0f) ^ (static_cast<int>(ry * 1000.0f) << 4);
            } else if (d < f2) {
                f2 = d;
            }
        }
    }
    return {f1, f2, sx, sy, cid};
}

// Clamp to uint8 helper
inline uint8_t u8clamp(float v) {
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
}

// Convert 3D normal vector (-1..1) to tangent-space normal bytes (0..255)
inline void pack_normal(float nx, float ny, float nz, uint8_t& out_r, uint8_t& out_g, uint8_t& out_b) {
    float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 0.0001f) {
        nx /= len;
        ny /= len;
        nz /= len;
    } else {
        nx = 0.0f; ny = 0.0f; nz = 1.0f;
    }
    out_r = u8clamp(nx * 127.5f + 128.0f);
    out_g = u8clamp(ny * 127.5f + 128.0f);
    out_b = u8clamp(nz * 127.5f + 128.0f);
}

} // namespace


TextureArray::TextureArray(int width, int height, int layers, bool pixel_art)
    : m_width(width)
    , m_height(height)
    , m_layers(layers)
    , m_pixel_art(pixel_art || (width <= 32 || height <= 32))
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
    std::vector<uint8_t> normal_data(total_size, 128); // default (128, 128, 255)
    std::vector<uint8_t> rough_metal_data(total_size, 0);
    std::vector<uint8_t> emissive_data(total_size, 0);

    for (int l = 0; l < m_layers; ++l) {
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                size_t idx = (l * m_width * m_height + y * m_width + x) * 4;

                float fx = static_cast<float>(x) / static_cast<float>(m_width);
                float fy = static_cast<float>(y) / static_cast<float>(m_height);

                // Default outputs
                float r = 60.0f, g = 60.0f, b = 60.0f, a = 255.0f;
                float nx = 0.0f, ny = 0.0f, nz = 1.0f;
                float rough = 200.0f, metal = 10.0f;
                float er = 0.0f, eg = 0.0f, eb = 0.0f;
                float height_disp = 0.5f;

                switch (l) {
                    case 1: { // ── MAT_FRACTURED_GRANITE (Realistic igneous rock with mineral grains and fissures) ──
                        float stone_fbm = periodic_fbm2d(fx, fy, 12, 4);
                        VoronoiResult vor = periodic_voronoi2d(fx, fy, 14);
                        float crack_dist = vor.f2 - vor.f1;
                        float is_crack = (crack_dist < 0.12f) ? std::pow(1.0f - (crack_dist / 0.12f), 2.0f) : 0.0f;

                        // Quartz (light gray), Feldspar (warm tan/salmon), Biotite (dark specks)
                        float mineral_grain = periodic_noise2d(fx, fy, 48);
                        float feldspar_tint = periodic_noise2d(fx, fy, 6);

                        // Base stone color
                        r = 95.0f + 55.0f * stone_fbm + 25.0f * feldspar_tint;
                        g = 88.0f + 48.0f * stone_fbm + 12.0f * feldspar_tint;
                        b = 85.0f + 45.0f * stone_fbm;

                        // Dark mineral flecks & crack crevice darkening
                        if (mineral_grain > 0.72f) {
                            r *= 0.45f; g *= 0.45f; b *= 0.50f;
                        }
                        r = r * (1.0f - 0.75f * is_crack);
                        g = g * (1.0f - 0.75f * is_crack);
                        b = b * (1.0f - 0.75f * is_crack);

                        // Normal map from stone relief & cracks
                        float d_fbm_x = periodic_fbm2d(fx + 0.01f, fy, 12, 4) - periodic_fbm2d(fx - 0.01f, fy, 12, 4);
                        float d_fbm_y = periodic_fbm2d(fx, fy + 0.01f, 12, 4) - periodic_fbm2d(fx, fy - 0.01f, 12, 4);
                        nx = -d_fbm_x * 2.5f;
                        ny = -d_fbm_y * 2.5f;
                        if (is_crack > 0.1f) {
                            nx += (hash2d(vor.seed_x, vor.seed_y) - 0.5f) * is_crack * 2.0f;
                            ny += (hash2d(vor.seed_y, vor.seed_x) - 0.5f) * is_crack * 2.0f;
                            nz = 0.5f;
                        }
                        rough = 210.0f + (mineral_grain > 0.8f ? -120.0f : 20.0f); // Sparkling quartz mica flecks
                        metal = 12.0f;
                        height_disp = std::clamp(0.5f + stone_fbm * 0.35f - is_crack * 0.5f, 0.0f, 1.0f);
                        break;
                    }

                    case 2: { // ── MAT_VOLCANIC_BASALT (Porous vesicular lava rock with cooling joints) ──
                        float basalt_fbm = periodic_fbm2d(fx, fy, 10, 4);
                        VoronoiResult vor = periodic_voronoi2d(fx, fy, 16);
                        // Gas vesicles (porous bubbles frozen in lava)
                        float vesicle = (vor.f1 < 0.28f) ? std::pow(1.0f - (vor.f1 / 0.28f), 2.5f) : 0.0f;

                        // Deep charcoal/slate
                        r = 28.0f + 16.0f * basalt_fbm - 15.0f * vesicle;
                        g = 30.0f + 16.0f * basalt_fbm - 15.0f * vesicle;
                        b = 36.0f + 20.0f * basalt_fbm - 15.0f * vesicle;

                        // Pitted vesicular crater normal map
                        float v_local_x = fractf(fx * 16.0f);
                        float v_local_y = fractf(fy * 16.0f);
                        nx = (v_local_x - 0.5f) * vesicle * 8.0f;
                        ny = (v_local_y - 0.5f) * vesicle * 8.0f;
                        nz = 1.0f - vesicle * 0.7f;

                        // Glassy obsidian cleavage flecks
                        float obsidian_fleck = periodic_noise2d(fx, fy, 56);
                        rough = (obsidian_fleck > 0.82f) ? 35.0f : 235.0f;
                        metal = (obsidian_fleck > 0.82f) ? 45.0f : 20.0f;
                        height_disp = std::clamp(0.6f + basalt_fbm * 0.3f - vesicle * 0.6f, 0.0f, 1.0f);
                        break;
                    }

                    case 3: { // ── MAT_VOIDITE_CRYSTAL (Authentic Geode Bedrock Host Matrix with Crystalline Roots) ──
                        float rock_fbm = periodic_fbm2d(fx, fy, 10, 4);
                        VoronoiResult vor = periodic_voronoi2d(fx, fy, 14);
                        float fissure = (vor.f2 - vor.f1 < 0.10f) ? std::pow(1.0f - ((vor.f2 - vor.f1) / 0.10f), 2.0f) : 0.0f;
                        float edge_dist = std::min({fx, 1.0f - fx, fy, 1.0f - fy});
                        float edge_fade = std::clamp((edge_dist - 0.02f) / 0.10f, 0.0f, 1.0f);
                        fissure *= edge_fade;

                        // Deep dark void-basalt / obsidian matrix with subtle violet undertones
                        r = 24.0f + 14.0f * rock_fbm + 18.0f * fissure;
                        g = 20.0f + 10.0f * rock_fbm + 4.0f * fissure;
                        b = 34.0f + 18.0f * rock_fbm + 35.0f * fissure;

                        // Rugged geode crust normal map
                        float d_fbm_x = periodic_fbm2d(fx + 0.01f, fy, 10, 4) - periodic_fbm2d(fx - 0.01f, fy, 10, 4);
                        float d_fbm_y = periodic_fbm2d(fx, fy + 0.01f, 10, 4) - periodic_fbm2d(fx, fy - 0.01f, 10, 4);
                        nx = -d_fbm_x * 2.2f;
                        ny = -d_fbm_y * 2.2f;
                        if (fissure > 0.05f) {
                            nx += (hash2d(vor.seed_x, vor.seed_y) - 0.5f) * fissure * 2.0f;
                            ny += (hash2d(vor.seed_y, vor.seed_x) - 0.5f) * fissure * 2.0f;
                            nz = 0.6f;
                        }

                        // Rough mineral host matrix rock with fine sparkling crystalline flecks
                        float crystal_fleck = periodic_noise2d(fx, fy, 48);
                        rough = (crystal_fleck > 0.85f) ? 55.0f : 210.0f;
                        metal = (crystal_fleck > 0.85f) ? 35.0f : 12.0f;

                        // Subtle deep geode root luminescence in deep fissures (er, eg, eb)
                        er = u8clamp(18.0f + 45.0f * fissure);
                        eg = u8clamp(4.0f  + 12.0f * fissure);
                        eb = u8clamp(32.0f + 65.0f * fissure);
                        height_disp = std::clamp(0.55f + rock_fbm * 0.3f - fissure * 0.4f, 0.0f, 1.0f);
                        break;
                    }

                    case 4: { // ── MAT_INDUSTRIAL_BULKHEAD (Ballistic Cold-Rolled Alloy Plate with Tread Grips & Flush Rivets) ──
                        // 1. Alternating heavy industrial non-slip diamond tread bars (period 6 = seamless across blocks)
                        float gx = fx * 6.0f;
                        float gy = fy * 6.0f;
                        int cell_ix = static_cast<int>(std::floor(gx));
                        int cell_iy = static_cast<int>(std::floor(gy));
                        float lx = fractf(gx) - 0.5f;
                        float ly = fractf(gy) - 0.5f;
                        bool alt = ((cell_ix + cell_iy) % 2) != 0;

                        float u, v;
                        if (alt) {
                            u = (lx + ly) * 0.70710678f;
                            v = (ly - lx) * 0.70710678f;
                        } else {
                            u = (lx - ly) * 0.70710678f;
                            v = (lx + ly) * 0.70710678f;
                        }

                        float bar_half_len = 0.22f;
                        float bar_half_w = 0.08f;
                        float clamped_u = std::clamp(u, -bar_half_len, bar_half_len);
                        float du = u - clamped_u;
                        float dv = v;
                        float bar_dist = std::sqrt(du * du + dv * dv);
                        bool is_tread_rib = (bar_dist < bar_half_w);
                        float bar_profile = is_tread_rib ? std::cos((bar_dist / bar_half_w) * 1.5707963f) : 0.0f;

                        // Bidirectional rounded bevel normals on tread rib (eliminates flat plastic reflection)
                        float d_dist_dx = (alt ? (du * 0.70710678f - dv * 0.70710678f) : (du * 0.70710678f + dv * 0.70710678f)) / std::max(bar_dist, 0.0001f);
                        float d_dist_dy = (alt ? (du * 0.70710678f + dv * 0.70710678f) : (-du * 0.70710678f + dv * 0.70710678f)) / std::max(bar_dist, 0.0001f);
                        float rib_nx = d_dist_dx * 0.55f * (1.0f - bar_profile * 0.35f);
                        float rib_ny = d_dist_dy * 0.55f * (1.0f - bar_profile * 0.35f);

                        // 2. Periodic flush structural anchor bolts with counterbore washer rings (4 per block, period 2)
                        float fsub_x = std::fmod(fx * 2.0f, 1.0f) - 0.5f;
                        float fsub_y = std::fmod(fy * 2.0f, 1.0f) - 0.5f;
                        float bolt_dist = std::sqrt(fsub_x * fsub_x + fsub_y * fsub_y);
                        bool is_bolt_head = (bolt_dist < 0.065f);
                        bool is_washer_groove = (bolt_dist >= 0.065f && bolt_dist < 0.088f);

                        // 3. Brushed steel and cold-rolled micro-relief noise (organic light scattering, prevents plastic mirror glare)
                        float brushed = (periodic_noise2d(fx, fy, 32) - 0.5f) * 12.0f;
                        float steel_micro_x = (periodic_noise2d(fx + 0.12f, fy + 0.12f, 48) - 0.5f) * 0.20f;
                        float steel_micro_y = (periodic_noise2d(fx + 0.28f, fy + 0.28f, 48) - 0.5f) * 0.20f;

                        if (is_bolt_head) {
                            r = 108.0f + brushed * 0.5f;
                            g = 114.0f + brushed * 0.5f;
                            b = 126.0f + brushed * 0.5f;
                            rough = 145.0f; // PBR ~0.57 (hardened bolt steel)
                            metal = 238.0f; // PBR ~0.93 (high metallic)
                            nx = (fsub_x / 0.065f) * 0.40f;
                            ny = (fsub_y / 0.065f) * 0.40f;
                            nz = std::sqrt(std::max(0.1f, 1.0f - nx * nx - ny * ny));
                            height_disp = 0.82f;
                        } else if (is_washer_groove) {
                            r = 44.0f; g = 46.0f; b = 52.0f; // Dark recessed oxidized washer counterbore
                            rough = 215.0f; // PBR ~0.84 (matte oxide)
                            metal = 160.0f; // PBR ~0.63
                            nx = steel_micro_x * 0.5f;
                            ny = steel_micro_y * 0.5f;
                            nz = 0.98f;
                            height_disp = 0.34f;
                        } else if (is_tread_rib) {
                            r = 82.0f + 16.0f * bar_profile + brushed;
                            g = 88.0f + 18.0f * bar_profile + brushed;
                            b = 100.0f + 20.0f * bar_profile + brushed;
                            rough = 155.0f + 25.0f * (1.0f - bar_profile); // PBR ~0.61 - 0.70
                            metal = 225.0f; // PBR ~0.88
                            nx = rib_nx;
                            ny = rib_ny;
                            nz = std::sqrt(std::max(0.15f, 1.0f - nx * nx - ny * ny));
                            height_disp = 0.58f + 0.26f * bar_profile;
                        } else {
                            // Authentic heavy cold-rolled gunmetal titanium plate (matte industrial finish)
                            r = 62.0f + brushed;
                            g = 66.0f + brushed;
                            b = 76.0f + brushed;
                            rough = 185.0f; // PBR ~0.725 (matte industrial cold-rolled steel finish, stops plastic glare)
                            metal = 205.0f; // PBR ~0.804 (heavy structural alloy)
                            nx = steel_micro_x;
                            ny = steel_micro_y;
                            nz = std::sqrt(std::max(0.2f, 1.0f - nx * nx - ny * ny));
                            height_disp = 0.48f;
                        }
                        break;
                    }

                    case 5: { // ── MAT_REINFORCED_VAULT_DOOR (Heavy Gold-Titanium Alloy with Chevron Ribs) ──
                        // Interlocking heavy diagonal chevron ribs
                        float chev = std::abs(std::fmod(fx * 6.0f + std::abs(fy - 0.5f) * 4.0f, 1.0f) - 0.5f);
                        bool is_ridge = (chev < 0.16f);

                        // Scratched wear revealing dark steel under golden brass
                        float scratch = periodic_noise2d(fx, fy, 64);
                        bool is_worn = (scratch > 0.78f);

                        if (is_worn) {
                            r = 70.0f; g = 72.0f; b = 78.0f; // Hardened dark steel underlayer
                            rough = 110.0f; metal = 210.0f;
                        } else if (is_ridge) {
                            r = 215.0f; g = 175.0f; b = 60.0f; // Polished titanium-gold ridge
                            nx = 0.6f; ny = 0.4f; nz = 0.7f;
                            rough = 45.0f; metal = 245.0f;
                        } else {
                            r = 175.0f; g = 140.0f; b = 45.0f; // Recessed gold alloy panel
                            rough = 65.0f; metal = 240.0f;
                        }
                        height_disp = is_ridge ? 0.85f : (is_worn ? 0.35f : 0.5f);
                        break;
                    }

                    case 6: { // ── MAT_THERMITE_SLAG (Volcanic Magma & Incandescent Slag Fissures) ──
                        VoronoiResult vor = periodic_voronoi2d(fx, fy, 8);
                        float fissure_dist = vor.f2 - vor.f1;
                        float is_fissure = (fissure_dist < 0.18f) ? std::pow(1.0f - (fissure_dist / 0.18f), 1.8f) : 0.0f;
                        float edge_dist = std::min({fx, 1.0f - fx, fy, 1.0f - fy});
                        float edge_fade = std::clamp((edge_dist - 0.02f) / 0.10f, 0.0f, 1.0f);
                        is_fissure *= edge_fade;

                        if (is_fissure > 0.05f) {
                            // Blazing molten magma chasm
                            r = 255.0f;
                            g = u8clamp(110.0f + 130.0f * is_fissure);
                            b = u8clamp(20.0f + 40.0f * is_fissure);

                            // Molten magma emission
                            er = 255.0f;
                            eg = u8clamp(120.0f + 125.0f * is_fissure);
                            eb = 30.0f;

                            rough = 70.0f; metal = 30.0f;
                            height_disp = 0.2f - is_fissure * 0.2f;
                        } else {
                            // Black cooling volcanic crust islands
                            float crust_noise = periodic_fbm2d(fx, fy, 14, 3);
                            r = 22.0f + 15.0f * crust_noise;
                            g = 20.0f + 12.0f * crust_noise;
                            b = 22.0f + 14.0f * crust_noise;

                            nx = (periodic_noise2d(fx, fy, 20) - 0.5f) * 1.5f;
                            ny = (periodic_noise2d(fx + 0.1f, fy + 0.1f, 20) - 0.5f) * 1.5f;
                            nz = 0.9f;

                            rough = 240.0f; metal = 20.0f;
                            height_disp = 0.7f + crust_noise * 0.3f;
                        }
                        break;
                    }

                    case 7: { // ── MAT_RADIOACTIVE_ORE (Pitchblende Host Rock with Glowing Emerald Uraninite Needles) ──
                        VoronoiResult vor = periodic_voronoi2d(fx, fy, 12);
                        bool is_crystal_needle = (vor.f1 < 0.22f);
                        float edge_dist = std::min({fx, 1.0f - fx, fy, 1.0f - fy});
                        float edge_fade = std::clamp((edge_dist - 0.02f) / 0.10f, 0.0f, 1.0f);

                        if (is_crystal_needle && edge_fade > 0.05f) {
                            // Glowing toxic crystal needle
                            float needle_sharp = std::pow(1.0f - (vor.f1 / 0.22f), 1.6f) * edge_fade;
                            r = 45.0f + 40.0f * needle_sharp;
                            g = 220.0f + 35.0f * needle_sharp;
                            b = 75.0f + 50.0f * needle_sharp;

                            float v_local_x = fractf(fx * 12.0f);
                            float v_local_y = fractf(fy * 12.0f);
                            nx = (v_local_x - 0.5f) * 5.0f;
                            ny = (v_local_y - 0.5f) * 5.0f;
                            nz = 0.8f;

                            rough = 32.0f; metal = 50.0f;

                            // Cherenkov toxic radiation luminescence
                            er = 30.0f;
                            eg = u8clamp(200.0f + 55.0f * needle_sharp);
                            eb = 70.0f;
                            height_disp = 0.8f + needle_sharp * 0.2f;
                        } else {
                            // Dark rugged pitchblende matrix
                            float rock_noise = periodic_fbm2d(fx, fy, 16, 4);
                            r = 30.0f + 20.0f * rock_noise;
                            g = 35.0f + 22.0f * rock_noise;
                            b = 32.0f + 20.0f * rock_noise;

                            rough = 220.0f; metal = 25.0f;
                            height_disp = 0.4f + rock_noise * 0.2f;
                        }
                        break;
                    }

                    case 8: { // ── MAT_DREDGE_BEDROCK (Ultra-Dense Planetary Crust with Compressed Strata) ──
                        float strata = std::sin(fy * TWO_PI * 5.0f + periodic_fbm2d(fx, fy, 6, 3) * 6.0f);
                        float iron_fleck = periodic_noise2d(fx, fy, 60);

                        r = 18.0f + 10.0f * strata + (iron_fleck > 0.85f ? 45.0f : 0.0f);
                        g = 18.0f + 10.0f * strata + (iron_fleck > 0.85f ? 40.0f : 0.0f);
                        b = 24.0f + 12.0f * strata + (iron_fleck > 0.85f ? 35.0f : 0.0f);

                        nx = strata * 0.4f;
                        ny = (periodic_noise2d(fx, fy, 24) - 0.5f) * 0.8f;
                        nz = 0.95f;

                        rough = (iron_fleck > 0.85f) ? 60.0f : 245.0f;
                        metal = (iron_fleck > 0.85f) ? 180.0f : 35.0f;
                        height_disp = std::clamp(0.5f + strata * 0.3f, 0.0f, 1.0f);
                        break;
                    }

                    case 9: { // MAT_GAS
                        float gas_n = periodic_fbm2d(fx, fy, 8, 3);
                        r = 60.0f + 40.0f * gas_n;
                        g = 170.0f + 50.0f * gas_n;
                        b = 40.0f + 30.0f * gas_n;
                        er = 30.0f; eg = 150.0f; eb = 30.0f;
                        rough = 255.0f; metal = 0.0f;
                        break;
                    }

                    case 10: { // MAT_VOLATILE_SMOKE
                        float smoke_n = periodic_fbm2d(fx, fy, 8, 3);
                        r = 45.0f + 30.0f * smoke_n;
                        g = 42.0f + 25.0f * smoke_n;
                        b = 55.0f + 35.0f * smoke_n;
                        rough = 255.0f; metal = 0.0f;
                        break;
                    }

                    case 11: { // ── MAT_CRYSTAL_AQUIFER (Shimmering Subterranean Water with Caustic Cells) ──
                        float wave1 = std::sin(fx * TWO_PI * 5.0f + fy * TWO_PI * 3.0f);
                        float wave2 = std::cos(fx * TWO_PI * 3.0f - fy * TWO_PI * 5.0f);
                        float caustic = std::pow((wave1 * wave2) * 0.5f + 0.5f, 1.5f);

                        r = 20.0f + 35.0f * caustic;
                        g = 145.0f + 70.0f * caustic;
                        b = 225.0f + 30.0f * caustic;

                        nx = wave1 * 0.4f;
                        ny = wave2 * 0.4f;
                        nz = 0.9f;

                        rough = 14.0f; metal = 45.0f; // Pure fluid polish
                        er = 15.0f + 25.0f * caustic;
                        eg = 85.0f + 55.0f * caustic;
                        eb = 140.0f + 65.0f * caustic;
                        height_disp = std::clamp(0.5f + caustic * 0.4f, 0.0f, 1.0f);
                        break;
                    }

                    case 12: { // ── MAT_BIOLUMINESCENT_FLORA (Velvety Moss with Glowing Hyphae & Spore Pods) ──
                        float moss_fbm = periodic_fbm2d(fx, fy, 18, 4);
                        VoronoiResult vor = periodic_voronoi2d(fx, fy, 10);
                        bool is_spore_node = (vor.f1 < 0.20f);
                        float hyphae_vein = std::pow(periodic_fbm2d(fx, fy, 24, 3), 2.2f);

                        r = 22.0f + 30.0f * moss_fbm;
                        g = 85.0f + 95.0f * moss_fbm;
                        b = 35.0f + 35.0f * moss_fbm;

                        rough = 185.0f; metal = 15.0f;

                        if (is_spore_node) {
                            float spore_glow = std::pow(1.0f - (vor.f1 / 0.20f), 2.0f);
                            g = u8clamp(g + 120.0f * spore_glow);
                            b = u8clamp(b + 95.0f * spore_glow);
                            er = 20.0f;
                            eg = u8clamp(210.0f + 45.0f * spore_glow);
                            eb = u8clamp(110.0f + 90.0f * spore_glow);
                            rough = 45.0f;
                        } else if (hyphae_vein > 0.4f) {
                            er = 15.0f; eg = 160.0f; eb = 65.0f;
                        }
                        height_disp = is_spore_node ? 0.85f : (0.4f + moss_fbm * 0.3f);
                        break;
                    }

                    case 13: { // ── MAT_PRISMATIC_CRYSTAL (Seamless Toroidal Diamond Prismatic Jewel Cluster) ──
                        struct PrismSeed {
                            float x, y, nx, ny, height, shade;
                        };
                        const PrismSeed prism_seeds[8] = {
                            {0.22f, 0.28f, -0.60f,  0.40f, 0.98f, 1.05f},
                            {0.74f, 0.20f,  0.50f,  0.50f, 0.90f, 0.95f},
                            {0.42f, 0.48f, -0.40f, -0.60f, 1.00f, 1.10f},
                            {0.82f, 0.52f,  0.60f, -0.35f, 0.92f, 1.00f},
                            {0.18f, 0.82f, -0.50f, -0.50f, 0.88f, 0.92f},
                            {0.62f, 0.78f,  0.40f,  0.60f, 0.95f, 1.05f},
                            {0.90f, 0.85f, -0.45f,  0.55f, 0.82f, 0.88f},
                            {0.50f, 0.15f,  0.30f, -0.70f, 0.92f, 1.02f}
                        };

                        float best_d1 = 10.0f;
                        float best_d2 = 10.0f;
                        int best_seed = 0;
                        float best_dx = 0.0f, best_dy = 0.0f;

                        for (int si = 0; si < 8; ++si) {
                            for (int oj = -1; oj <= 1; ++oj) {
                                for (int oi = -1; oi <= 1; ++oi) {
                                    float sx = prism_seeds[si].x + static_cast<float>(oi);
                                    float sy = prism_seeds[si].y + static_cast<float>(oj);
                                    float dx = fx - sx;
                                    float dy = fy - sy;
                                    float d = std::sqrt(dx * dx + dy * dy);
                                    if (d < best_d1) {
                                        best_d2 = best_d1;
                                        best_d1 = d;
                                        best_seed = si;
                                        best_dx = dx;
                                        best_dy = dy;
                                    } else if (d < best_d2) {
                                        best_d2 = d;
                                    }
                                }
                            }
                        }

                        const PrismSeed& seed = prism_seeds[best_seed];
                        float facet_ridge = best_d2 - best_d1;
                        float is_ridge = (facet_ridge < 0.055f) ? std::pow(1.0f - (facet_ridge / 0.055f), 2.0f) : 0.0f;
                        float center_glow = std::pow(std::max(0.0f, 1.0f - best_d1 * 2.5f), 2.2f);

                        // Decorative mineral colors: Soft natural celestine / aquamarine with subtle crystalline depth
                        // Subdued, naturally integrated gemstone palette that avoids overpowering the player's view
                        float shade = seed.shade;
                        r = (38.0f + 22.0f * center_glow + 55.0f * is_ridge) * shade;
                        g = (142.0f + 25.0f * center_glow + 30.0f * is_ridge) * shade;
                        b = (182.0f + 20.0f * center_glow + 20.0f * is_ridge) * shade;

                        float slope = 0.55f;
                        nx = seed.nx * slope;
                        ny = seed.ny * slope;
                        nz = std::sqrt(std::max(0.1f, 1.0f - nx * nx - ny * ny));

                        if (is_ridge > 0.1f) {
                            float r_ang = std::atan2(best_dy, best_dx);
                            nx = nx * (1.0f - is_ridge) + std::cos(r_ang) * 0.45f * is_ridge;
                            ny = ny * (1.0f - is_ridge) + std::sin(r_ang) * 0.45f * is_ridge;
                            nz = nz * (1.0f - is_ridge) + 0.85f * is_ridge;
                        }

                        // Organic mineral roughness (prevents harsh specular glare under headlamp)
                        rough = 55.0f + 25.0f * (1.0f - is_ridge);
                        metal = 28.0f; // Subtle natural crystalline sheen

                        // Gentle ambient mineral luminescence (soft calcite/quartz glow, not an aggressive spotlight)
                        er = u8clamp(10.0f + 14.0f * center_glow + 28.0f * is_ridge);
                        eg = u8clamp(48.0f + 28.0f * center_glow + 24.0f * is_ridge);
                        eb = u8clamp(68.0f + 32.0f * center_glow + 18.0f * is_ridge);
                        height_disp = std::clamp(seed.height * 0.75f + is_ridge * 0.25f, 0.0f, 1.0f);
                        break;
                    }

                    case 14: { // ── MAT_OBSIDIAN_SPIKES (Razor-Sharp Conchoidal Glass with Crimson Hazard Veins) ──
                        float conchoid = std::sin(fx * TWO_PI * 4.0f + std::cos(fy * TWO_PI * 4.0f) * 3.0f);
                        VoronoiResult vor = periodic_voronoi2d(fx, fy, 10);
                        bool is_warning_vein = (vor.f1 < 0.16f);

                        // Jet-black mirror glass
                        r = 18.0f + 8.0f * conchoid;
                        g = 16.0f + 6.0f * conchoid;
                        b = 22.0f + 8.0f * conchoid;

                        nx = conchoid * 0.6f;
                        ny = (periodic_noise2d(fx, fy, 28) - 0.5f) * 0.8f;
                        nz = 0.85f;

                        rough = 22.0f; metal = 65.0f;

                        if (is_warning_vein) {
                            float vein_intensity = std::pow(1.0f - (vor.f1 / 0.16f), 1.8f);
                            r = u8clamp(180.0f + 75.0f * vein_intensity);
                            g = 15.0f;
                            b = 20.0f;
                            er = 255.0f;
                            eg = 15.0f;
                            eb = 25.0f;
                        }
                        height_disp = std::clamp(0.5f + conchoid * 0.3f + (is_warning_vein ? 0.2f : 0.0f), 0.0f, 1.0f);
                        break;
                    }

                    case 15: { // ── MAT_ACID (Bubbling Corrosive Neon Green Acid) ──
                        float wave1 = std::sin(fx * TWO_PI * 4.0f + fy * TWO_PI * 4.0f);
                        float wave2 = std::cos(fx * TWO_PI * 3.0f - fy * TWO_PI * 4.0f);
                        VoronoiResult vor = periodic_voronoi2d(fx, fy, 14);
                        bool is_bubble = (vor.f1 < 0.22f);
                        float bubble_rim = (vor.f1 >= 0.16f && vor.f1 < 0.22f) ? 1.0f : 0.0f;

                        r = 45.0f + 15.0f * wave1;
                        g = 190.0f + 35.0f * wave2;
                        b = 25.0f + 10.0f * wave1;

                        nx = wave1 * 0.4f;
                        ny = wave2 * 0.4f;
                        nz = 0.9f;

                        rough = 12.0f; metal = 30.0f;
                        if (is_bubble) {
                            float b_glow = std::pow(1.0f - (vor.f1 / 0.22f), 1.8f);
                            r = u8clamp(70.0f + 30.0f * b_glow);
                            g = u8clamp(220.0f + 35.0f * b_glow);
                            b = 40.0f;
                            er = 15.0f + 40.0f * b_glow;
                            eg = 180.0f + 75.0f * b_glow;
                            eb = 20.0f;
                            rough = 8.0f;
                            height_disp = 0.7f + b_glow * 0.3f;
                        } else {
                            er = 12.0f;
                            eg = 65.0f;
                            eb = 5.0f;
                            height_disp = 0.5f + bubble_rim * 0.2f;
                        }
                        break;
                    }

                    case 16: { // ── MAT_COOLANT (Frosted Cryogenic Sub-Zero Fluid) ──
                        float frost_fbm = periodic_fbm2d(fx, fy, 16, 3);
                        float wave = std::sin(fx * TWO_PI * 3.0f + fy * TWO_PI * 2.0f) * 0.5f + 0.5f;

                        r = 175.0f + 25.0f * wave + 15.0f * frost_fbm;
                        g = 215.0f + 20.0f * wave + 15.0f * frost_fbm;
                        b = 245.0f + 10.0f * wave;

                        nx = std::sin(fx * TWO_PI * 4.0f) * 0.3f;
                        ny = std::cos(fy * TWO_PI * 4.0f) * 0.3f;
                        nz = 0.95f;

                        rough = 8.0f; metal = 50.0f;
                        er = 30.0f + 20.0f * frost_fbm;
                        eg = 60.0f + 30.0f * frost_fbm;
                        eb = 110.0f + 40.0f * frost_fbm;
                        height_disp = 0.5f + frost_fbm * 0.2f;
                        break;
                    }

                    case 17: { // ── MAT_ASTRAL_ORE (Exotic Stellar Cosmic Gold Ore with Luminescent Filaments) ──
                        float wave1 = std::sin(fx * TWO_PI * 4.0f + fy * TWO_PI * 2.0f);
                        float wave2 = std::cos(fx * TWO_PI * 2.0f - fy * TWO_PI * 5.0f);
                        float astral_fbm = periodic_fbm2d(fx, fy, 18, 4);
                        VoronoiResult vor = periodic_voronoi2d(fx, fy, 12);
                        bool is_crystal_vein = (vor.f1 < 0.18f);

                        // Base metallic rich astral gold with deep celestial bronze undertones
                        r = 210.0f + 35.0f * wave1 + 10.0f * astral_fbm;
                        g = 165.0f + 25.0f * wave2 + 10.0f * astral_fbm;
                        b = 45.0f + 20.0f * wave1;

                        nx = wave1 * 0.45f;
                        ny = wave2 * 0.45f;
                        nz = 0.85f;

                        rough = 15.0f;
                        metal = 85.0f; // High metallic sheen

                        if (is_crystal_vein) {
                            float vein_glow = std::pow(1.0f - (vor.f1 / 0.18f), 2.0f);
                            // Radiant stellar white-gold crest
                            r = u8clamp(245.0f + 10.0f * vein_glow);
                            g = u8clamp(215.0f + 40.0f * vein_glow);
                            b = u8clamp(90.0f + 165.0f * vein_glow);
                            er = u8clamp(240.0f + 15.0f * vein_glow);
                            eg = u8clamp(200.0f + 55.0f * vein_glow);
                            eb = u8clamp(70.0f + 185.0f * vein_glow);
                            rough = 6.0f;
                            metal = 95.0f;
                            height_disp = 0.65f + vein_glow * 0.35f;
                        } else {
                            er = 45.0f + 25.0f * astral_fbm;
                            eg = 35.0f + 20.0f * astral_fbm;
                            eb = 10.0f;
                            height_disp = 0.45f + astral_fbm * 0.25f;
                        }
                        break;
                    }


                    default:
                        r = 70.0f; g = 70.0f; b = 70.0f;
                        break;
                }

                // Pack into texture data arrays
                albedo_data[idx + 0] = u8clamp(r);
                albedo_data[idx + 1] = u8clamp(g);
                albedo_data[idx + 2] = u8clamp(b);
                albedo_data[idx + 3] = u8clamp(a);

                pack_normal(nx, ny, nz, normal_data[idx + 0], normal_data[idx + 1], normal_data[idx + 2]);
                normal_data[idx + 3] = u8clamp(height_disp * 255.0f);

                rough_metal_data[idx + 0] = u8clamp(rough);
                rough_metal_data[idx + 1] = u8clamp(metal);
                rough_metal_data[idx + 2] = 0;
                rough_metal_data[idx + 3] = 255;

                emissive_data[idx + 0] = u8clamp(er);
                emissive_data[idx + 1] = u8clamp(eg);
                emissive_data[idx + 2] = u8clamp(eb);
                emissive_data[idx + 3] = 255;
            }
        }
    }

    auto create_tex_array = [this](unsigned int& tex, const void* data) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, m_width, m_height, m_layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

        if (m_pixel_art) {
            // Low-res / pixel-art styles: set min/mag filter to GL_NEAREST_MIPMAP_LINEAR to keep texel borders crisp without blur
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
        } else {
            // High-res textures: enable 16x Anisotropic Filtering (GL_TEXTURE_MAX_ANISOTROPY_EXT) and set mip filter to GL_LINEAR_MIPMAP_LINEAR
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            GLfloat max_aniso = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_aniso);
            GLint aniso = (max_aniso >= 16.0f) ? 16 : (max_aniso > 1.0f ? static_cast<GLint>(max_aniso) : 16);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
        }

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    };

    create_tex_array(m_albedo_tex, albedo_data.data());
    create_tex_array(m_normal_tex, normal_data.data());
    create_tex_array(m_rough_metal_tex, rough_metal_data.data());
    create_tex_array(m_emissive_tex, emissive_data.data());
}

void TextureArray::set_pixel_art(bool enabled) {
    m_pixel_art = enabled;
    auto configure_tex = [this](unsigned int tex) {
        if (tex == 0) return;
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        if (m_pixel_art) {
            // Low-res / pixel-art styles: crisp texel borders without blur
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
        } else {
            // High-res textures: enable 16x Anisotropic Filtering and linear mip filter
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            GLfloat max_aniso = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_aniso);
            GLint aniso = (max_aniso >= 16.0f) ? 16 : (max_aniso > 1.0f ? static_cast<GLint>(max_aniso) : 16);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
        }
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    };

    configure_tex(m_albedo_tex);
    configure_tex(m_normal_tex);
    configure_tex(m_rough_metal_tex);
    configure_tex(m_emissive_tex);
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
