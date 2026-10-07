#include "texture_array.hpp"
#include <glad/glad.h>
#include <vector>
#include <cmath>
#include <algorithm>

namespace Voidfall {

namespace {

inline float fractf(float x) { return x - std::floor(x); }

inline float hash2d(float x, float y) {
    float n = std::sin(x * 127.1f + y * 311.7f) * 43758.5453123f;
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

                switch (l) {
                    case 1: { // ── MAT_FRACTURED_GRANITE (Realistic igneous rock with mineral grains and fissures) ──
                        float stone_fbm = fbm2d(fx * 12.0f, fy * 12.0f, 4);
                        VoronoiResult vor = voronoi2d(fx * 14.0f, fy * 14.0f);
                        float crack_dist = vor.f2 - vor.f1;
                        float is_crack = (crack_dist < 0.12f) ? std::pow(1.0f - (crack_dist / 0.12f), 2.0f) : 0.0f;

                        // Quartz (light gray), Feldspar (warm tan/salmon), Biotite (dark specks)
                        float mineral_grain = noise2d(fx * 45.0f, fy * 45.0f);
                        float feldspar_tint = noise2d(fx * 6.0f + 2.0f, fy * 6.0f + 7.0f);

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
                        float d_fbm_x = fbm2d((fx + 0.01f) * 12.0f, fy * 12.0f) - fbm2d((fx - 0.01f) * 12.0f, fy * 12.0f);
                        float d_fbm_y = fbm2d(fx * 12.0f, (fy + 0.01f) * 12.0f) - fbm2d(fx * 12.0f, (fy - 0.01f) * 12.0f);
                        nx = -d_fbm_x * 2.5f;
                        ny = -d_fbm_y * 2.5f;
                        if (is_crack > 0.1f) {
                            nx += (hash2d(vor.seed_x, vor.seed_y) - 0.5f) * is_crack * 2.0f;
                            ny += (hash2d(vor.seed_y, vor.seed_x) - 0.5f) * is_crack * 2.0f;
                            nz = 0.5f;
                        }
                        rough = 210.0f + (mineral_grain > 0.8f ? -120.0f : 20.0f); // Sparkling quartz mica flecks
                        metal = 12.0f;
                        break;
                    }

                    case 2: { // ── MAT_VOLCANIC_BASALT (Porous vesicular lava rock with cooling joints) ──
                        float basalt_fbm = fbm2d(fx * 10.0f, fy * 10.0f, 4);
                        VoronoiResult vor = voronoi2d(fx * 16.0f, fy * 16.0f);
                        // Gas vesicles (porous bubbles frozen in lava)
                        float vesicle = (vor.f1 < 0.28f) ? std::pow(1.0f - (vor.f1 / 0.28f), 2.5f) : 0.0f;

                        // Deep charcoal/slate
                        r = 28.0f + 16.0f * basalt_fbm - 15.0f * vesicle;
                        g = 30.0f + 16.0f * basalt_fbm - 15.0f * vesicle;
                        b = 36.0f + 20.0f * basalt_fbm - 15.0f * vesicle;

                        // Pitted vesicular crater normal map
                        nx = (fx - vor.seed_x / 16.0f) * vesicle * 8.0f;
                        ny = (fy - vor.seed_y / 16.0f) * vesicle * 8.0f;
                        nz = 1.0f - vesicle * 0.7f;

                        // Glassy obsidian cleavage flecks
                        float obsidian_fleck = noise2d(fx * 55.0f, fy * 55.0f);
                        rough = (obsidian_fleck > 0.82f) ? 35.0f : 235.0f;
                        metal = (obsidian_fleck > 0.82f) ? 45.0f : 20.0f;
                        break;
                    }

                    case 3: { // ── MAT_VOIDITE_CRYSTAL (Authentic Seamless Faceted Gemstone Cluster) ──
                        // Seamless toroidal crystal facets across the entire block (no artificial square borders)
                        struct GemSeed {
                            float x, y, nx, ny, height, shade;
                        };
                        const GemSeed gem_seeds[8] = {
                            {0.18f, 0.22f,  0.60f,  0.35f, 0.95f, 1.05f},
                            {0.68f, 0.16f, -0.55f,  0.45f, 0.88f, 0.92f},
                            {0.38f, 0.52f,  0.30f, -0.65f, 1.00f, 1.15f},
                            {0.84f, 0.48f, -0.65f, -0.30f, 0.92f, 0.98f},
                            {0.14f, 0.78f,  0.45f, -0.50f, 0.85f, 0.88f},
                            {0.58f, 0.82f, -0.35f,  0.60f, 0.96f, 1.08f},
                            {0.88f, 0.88f,  0.50f,  0.50f, 0.80f, 0.85f},
                            {0.48f, 0.18f, -0.20f, -0.70f, 0.90f, 1.02f}
                        };

                        float best_d1 = 10.0f;
                        float best_d2 = 10.0f;
                        int best_seed = 0;
                        float best_dx = 0.0f, best_dy = 0.0f;

                        for (int si = 0; si < 8; ++si) {
                            // Test all 9 toroidal neighbor shifts for perfect seamless wrapping
                            for (int oj = -1; oj <= 1; ++oj) {
                                for (int oi = -1; oi <= 1; ++oi) {
                                    float sx = gem_seeds[si].x + static_cast<float>(oi);
                                    float sy = gem_seeds[si].y + static_cast<float>(oj);
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

                        const GemSeed& seed = gem_seeds[best_seed];
                        float facet_ridge = best_d2 - best_d1;
                        float is_ridge = (facet_ridge < 0.055f) ? std::pow(1.0f - (facet_ridge / 0.055f), 2.0f) : 0.0f;
                        float center_glow = std::pow(std::max(0.0f, 1.0f - best_d1 * 2.5f), 2.2f);

                        // Subtle internal crystal growth striations / cleavage planes
                        float striation = std::sin((best_dx * seed.nx + best_dy * seed.ny) * 65.0f) * 0.06f;

                        // Rich gemstone color palette: Deep royal amethyst, vibrant electric purple, electric magenta facet ridges
                        float shade = seed.shade * (1.0f + striation);
                        r = (100.0f + 40.0f * center_glow + 95.0f * is_ridge) * shade;
                        g = (20.0f  + 25.0f * center_glow + 75.0f * is_ridge) * shade;
                        b = (180.0f + 35.0f * center_glow + 55.0f * is_ridge) * shade;

                        // Facet surface normal tilted at prismatic cleavage angles
                        float slope = 0.65f; // ~37 degree facet tilt
                        nx = seed.nx * slope;
                        ny = seed.ny * slope;
                        nz = std::sqrt(std::max(0.1f, 1.0f - nx * nx - ny * ny));

                        // Ridge bevel catches light from perpendicular directions
                        if (is_ridge > 0.1f) {
                            float r_ang = std::atan2(best_dy, best_dx);
                            nx = nx * (1.0f - is_ridge) + std::cos(r_ang) * 0.6f * is_ridge;
                            ny = ny * (1.0f - is_ridge) + std::sin(r_ang) * 0.6f * is_ridge;
                            nz = nz * (1.0f - is_ridge) + 0.8f * is_ridge;
                        }

                        // Ultra-smooth polished gemstone surface
                        rough = 18.0f + 14.0f * (1.0f - is_ridge);
                        metal = 52.0f; // High refractive gemstone brilliance

                        // Controlled luminous emission that glows vibrantly without washing out into flat white
                        er = u8clamp(75.0f + 55.0f * center_glow + 105.0f * is_ridge);
                        eg = u8clamp(15.0f + 20.0f * center_glow + 45.0f  * is_ridge);
                        eb = u8clamp(155.0f + 45.0f * center_glow + 85.0f * is_ridge);
                        break;
                    }

                    case 4: { // ── MAT_INDUSTRIAL_BULKHEAD (Ballistic Cold-Rolled Alloy Plate with Tread Grips & Hex Bolts) ──
                        // Perimeter border frame (10% border inset)
                        float border_w = 0.08f;
                        bool is_border = (fx < border_w || fx > (1.0f - border_w) || fy < border_w || fy > (1.0f - border_w));

                        // Diamond-plate / raised tread pattern in center
                        float tread_x = std::fmod(fx * 16.0f, 1.0f);
                        float tread_y = std::fmod(fy * 16.0f, 1.0f);
                        float tread_diag = std::abs((tread_x - 0.5f) + (tread_y - 0.5f));
                        bool is_tread_rib = (!is_border && tread_diag < 0.22f && (tread_x > 0.15f && tread_x < 0.85f));

                        // 4 Corner Hex Bolts (at 12% in from corners)
                        float bolt_dist = 10.0f;
                        float bolt_centers[4][2] = {{0.12f, 0.12f}, {0.88f, 0.12f}, {0.12f, 0.88f}, {0.88f, 0.88f}};
                        bool is_bolt = false;
                        for (int bi = 0; bi < 4; ++bi) {
                            float bdx = fx - bolt_centers[bi][0];
                            float bdy = fy - bolt_centers[bi][1];
                            float bd = std::sqrt(bdx * bdx + bdy * bdy);
                            if (bd < 0.045f) {
                                is_bolt = true;
                                bolt_dist = bd;
                                nx = bdx / 0.045f;
                                ny = bdy / 0.045f;
                                nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));
                                break;
                            }
                        }

                        // Brushed steel micro-scratch noise
                        float brushed = noise2d(fx * 90.0f, fy * 8.0f) * 15.0f;

                        if (is_bolt) {
                            r = 145.0f + brushed; g = 150.0f + brushed; b = 160.0f + brushed;
                            rough = 55.0f; metal = 245.0f;
                        } else if (is_border) {
                            // Chamfered frame normal
                            r = 110.0f + brushed; g = 115.0f + brushed; b = 125.0f + brushed;
                            if (fx < border_w) nx = -0.7f;
                            else if (fx > (1.0f - border_w)) nx = 0.7f;
                            if (fy < border_w) ny = -0.7f;
                            else if (fy > (1.0f - border_w)) ny = 0.7f;
                            rough = 75.0f; metal = 225.0f;
                        } else if (is_tread_rib) {
                            r = 105.0f + brushed; g = 110.0f + brushed; b = 120.0f + brushed;
                            nx = 0.5f; ny = 0.5f; nz = 0.7f;
                            rough = 70.0f; metal = 230.0f;
                        } else {
                            r = 75.0f + brushed; g = 80.0f + brushed; b = 88.0f + brushed;
                            rough = 85.0f; metal = 235.0f;
                        }
                        break;
                    }

                    case 5: { // ── MAT_REINFORCED_VAULT_DOOR (Heavy Gold-Titanium Alloy with Chevron Ribs) ──
                        // Interlocking heavy diagonal chevron ribs
                        float chev = std::abs(std::fmod(fx * 6.0f + std::abs(fy - 0.5f) * 4.0f, 1.0f) - 0.5f);
                        bool is_ridge = (chev < 0.16f);

                        // Scratched wear revealing dark steel under golden brass
                        float scratch = noise2d(fx * 75.0f, fy * 65.0f);
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
                        break;
                    }

                    case 6: { // ── MAT_THERMITE_SLAG (Volcanic Magma & Incandescent Slag Fissures) ──
                        VoronoiResult vor = voronoi2d(fx * 8.0f, fy * 8.0f);
                        float fissure_dist = vor.f2 - vor.f1;
                        float is_fissure = (fissure_dist < 0.18f) ? std::pow(1.0f - (fissure_dist / 0.18f), 1.8f) : 0.0f;

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
                        } else {
                            // Black cooling volcanic crust islands
                            float crust_noise = fbm2d(fx * 14.0f, fy * 14.0f, 3);
                            r = 22.0f + 15.0f * crust_noise;
                            g = 20.0f + 12.0f * crust_noise;
                            b = 22.0f + 14.0f * crust_noise;

                            nx = (noise2d(fx * 20.0f, fy * 20.0f) - 0.5f) * 1.5f;
                            ny = (noise2d(fx * 20.0f + 2.0f, fy * 20.0f + 2.0f) - 0.5f) * 1.5f;
                            nz = 0.9f;

                            rough = 240.0f; metal = 20.0f;
                        }
                        break;
                    }

                    case 7: { // ── MAT_RADIOACTIVE_ORE (Pitchblende Host Rock with Glowing Emerald Uraninite Needles) ──
                        VoronoiResult vor = voronoi2d(fx * 12.0f, fy * 12.0f);
                        bool is_crystal_needle = (vor.f1 < 0.22f);

                        if (is_crystal_needle) {
                            // Glowing toxic crystal needle
                            float needle_sharp = std::pow(1.0f - (vor.f1 / 0.22f), 1.6f);
                            r = 45.0f + 40.0f * needle_sharp;
                            g = 220.0f + 35.0f * needle_sharp;
                            b = 75.0f + 50.0f * needle_sharp;

                            nx = (fx - vor.seed_x / 12.0f) * 5.0f;
                            ny = (fy - vor.seed_y / 12.0f) * 5.0f;
                            nz = 0.8f;

                            rough = 32.0f; metal = 50.0f;

                            // Cherenkov toxic radiation luminescence
                            er = 30.0f;
                            eg = u8clamp(200.0f + 55.0f * needle_sharp);
                            eb = 70.0f;
                        } else {
                            // Dark rugged pitchblende matrix
                            float rock_noise = fbm2d(fx * 15.0f, fy * 15.0f, 4);
                            r = 30.0f + 20.0f * rock_noise;
                            g = 35.0f + 22.0f * rock_noise;
                            b = 32.0f + 20.0f * rock_noise;

                            rough = 220.0f; metal = 25.0f;
                        }
                        break;
                    }

                    case 8: { // ── MAT_DREDGE_BEDROCK (Ultra-Dense Planetary Crust with Compressed Strata) ──
                        float strata = std::sin(fy * 32.0f + fbm2d(fx * 6.0f, fy * 2.0f, 3) * 6.0f);
                        float iron_fleck = noise2d(fx * 60.0f, fy * 60.0f);

                        r = 18.0f + 10.0f * strata + (iron_fleck > 0.85f ? 45.0f : 0.0f);
                        g = 18.0f + 10.0f * strata + (iron_fleck > 0.85f ? 40.0f : 0.0f);
                        b = 24.0f + 12.0f * strata + (iron_fleck > 0.85f ? 35.0f : 0.0f);

                        nx = (strata) * 0.4f;
                        ny = (noise2d(fx * 25.0f, fy * 25.0f) - 0.5f) * 0.8f;
                        nz = 0.95f;

                        rough = (iron_fleck > 0.85f) ? 60.0f : 245.0f;
                        metal = (iron_fleck > 0.85f) ? 180.0f : 35.0f;
                        break;
                    }

                    case 9: { // MAT_GAS
                        float gas_n = fbm2d(fx * 8.0f, fy * 8.0f, 3);
                        r = 60.0f + 40.0f * gas_n;
                        g = 170.0f + 50.0f * gas_n;
                        b = 40.0f + 30.0f * gas_n;
                        er = 30.0f; eg = 150.0f; eb = 30.0f;
                        rough = 255.0f; metal = 0.0f;
                        break;
                    }

                    case 10: { // MAT_VOLATILE_SMOKE
                        float smoke_n = fbm2d(fx * 7.0f, fy * 7.0f, 3);
                        r = 45.0f + 30.0f * smoke_n;
                        g = 42.0f + 25.0f * smoke_n;
                        b = 55.0f + 35.0f * smoke_n;
                        rough = 255.0f; metal = 0.0f;
                        break;
                    }

                    case 11: { // ── MAT_CRYSTAL_AQUIFER (Shimmering Subterranean Water with Caustic Cells) ──
                        float wave1 = std::sin(fx * 30.0f + fy * 20.0f);
                        float wave2 = std::cos(fx * 22.0f - fy * 32.0f);
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
                        break;
                    }

                    case 12: { // ── MAT_BIOLUMINESCENT_FLORA (Velvety Moss with Glowing Hyphae & Spore Pods) ──
                        float moss_fbm = fbm2d(fx * 18.0f, fy * 18.0f, 4);
                        VoronoiResult vor = voronoi2d(fx * 10.0f, fy * 10.0f);
                        bool is_spore_node = (vor.f1 < 0.20f);
                        float hyphae_vein = std::pow(fbm2d(fx * 25.0f, fy * 25.0f, 3), 2.2f);

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

                        // Prismatic radiant colors (iridescent violet, radiant diamond cyan, white highlights)
                        float shade = seed.shade;
                        r = (140.0f + 50.0f * center_glow + 65.0f * is_ridge) * shade;
                        g = (185.0f + 40.0f * center_glow + 55.0f * is_ridge) * shade;
                        b = (245.0f + 10.0f * center_glow) * shade;

                        float slope = 0.65f;
                        nx = seed.nx * slope;
                        ny = seed.ny * slope;
                        nz = std::sqrt(std::max(0.1f, 1.0f - nx * nx - ny * ny));

                        if (is_ridge > 0.1f) {
                            float r_ang = std::atan2(best_dy, best_dx);
                            nx = nx * (1.0f - is_ridge) + std::cos(r_ang) * 0.6f * is_ridge;
                            ny = ny * (1.0f - is_ridge) + std::sin(r_ang) * 0.6f * is_ridge;
                            nz = nz * (1.0f - is_ridge) + 0.8f * is_ridge;
                        }

                        rough = 14.0f + 12.0f * (1.0f - is_ridge);
                        metal = 75.0f; // Flawless gemstone polish & high refraction

                        // Radiant diamond luminescence
                        er = u8clamp(90.0f + 55.0f * center_glow + 95.0f * is_ridge);
                        eg = u8clamp(145.0f + 50.0f * center_glow + 60.0f * is_ridge);
                        eb = u8clamp(215.0f + 40.0f * center_glow);
                        break;
                    }

                    case 14: { // ── MAT_OBSIDIAN_SPIKES (Razor-Sharp Conchoidal Glass with Crimson Hazard Veins) ──
                        float conchoid = std::sin(fx * 24.0f + std::cos(fy * 24.0f) * 3.0f);
                        VoronoiResult vor = voronoi2d(fx * 10.0f, fy * 10.0f);
                        bool is_warning_vein = (vor.f1 < 0.16f);

                        // Jet-black mirror glass
                        r = 18.0f + 8.0f * conchoid;
                        g = 16.0f + 6.0f * conchoid;
                        b = 22.0f + 8.0f * conchoid;

                        nx = conchoid * 0.6f;
                        ny = (noise2d(fx * 30.0f, fy * 30.0f) - 0.5f) * 0.8f;
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
                normal_data[idx + 3] = 255;

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
