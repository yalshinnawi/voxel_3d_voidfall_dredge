#include "block_break_fx.hpp"
#include <algorithm>
#include <cmath>

namespace Voidfall {

glm::vec4 BlockBreakFX::get_material_crack_color(uint8_t mat_id, int stage) {
    stage = std::clamp(stage, 0, 5);
    switch (mat_id) {
        case MAT_FRACTURED_GRANITE: {
            // Authentic igneous granite: quartz/feldspar mineral stone gray-tan to bright quartz cleavage white
            if (stage < 2) return glm::vec4(0.80f, 0.76f, 0.70f, 0.88f);
            if (stage < 4) return glm::vec4(0.92f, 0.88f, 0.80f, 0.94f);
            return glm::vec4(1.0f, 0.96f, 0.90f, 1.0f);
        }
        case MAT_VOLCANIC_BASALT: {
            // Porous vesicular basalt: dark charcoal cooling joints to intense incandescent volcanic magma heat
            if (stage < 2) return glm::vec4(0.55f, 0.52f, 0.50f, 0.88f);
            if (stage < 4) return glm::vec4(0.98f, 0.52f, 0.12f, 0.95f);
            return glm::vec4(1.0f, 0.82f, 0.35f, 1.0f);
        }
        case MAT_VOIDITE_CRYSTAL: {
            // Authentic voidite geode crystal: deep resonant void purple to incandescent electric magenta/lilac core
            if (stage < 2) return glm::vec4(0.72f, 0.22f, 0.98f, 0.90f);
            if (stage < 4) return glm::vec4(0.92f, 0.38f, 1.0f, 0.96f);
            return glm::vec4(1.0f, 0.78f, 1.0f, 1.0f);
        }
        case MAT_INDUSTRIAL_BULKHEAD: {
            // Cold-rolled alloy bulkhead: dark gunmetal shear stress to high-voltage electric arcing & amber sparks
            if (stage < 2) return glm::vec4(0.60f, 0.70f, 0.82f, 0.90f);
            if (stage < 4) return glm::vec4(0.35f, 0.88f, 1.0f, 0.95f);
            return glm::vec4(1.0f, 0.85f, 0.30f, 1.0f);
        }
        case MAT_REINFORCED_VAULT_DOOR: {
            // Heavy gold-titanium alloy: burnished brass cleavages to radiant golden-yellow core
            if (stage < 2) return glm::vec4(0.85f, 0.72f, 0.28f, 0.90f);
            if (stage < 4) return glm::vec4(1.0f, 0.86f, 0.38f, 0.96f);
            return glm::vec4(1.0f, 0.96f, 0.72f, 1.0f);
        }
        case MAT_RADIOACTIVE_ORE: {
            // Toxic radioactive ore: deep toxic emerald to brilliant ionizing neon supergreen
            if (stage < 2) return glm::vec4(0.20f, 0.85f, 0.32f, 0.90f);
            if (stage < 4) return glm::vec4(0.32f, 1.0f, 0.45f, 0.96f);
            return glm::vec4(0.75f, 1.0f, 0.65f, 1.0f);
        }
        case MAT_THERMITE_SLAG: {
            // Molten thermite slag & magma: blazing magma red-orange to white-hot molten incandescent core
            if (stage < 2) return glm::vec4(1.0f, 0.38f, 0.06f, 0.92f);
            if (stage < 4) return glm::vec4(1.0f, 0.68f, 0.15f, 0.96f);
            return glm::vec4(1.0f, 0.95f, 0.75f, 1.0f);
        }
        case MAT_CRYSTAL_AQUIFER: {
            // Subterranean clear aquifer water: aquatic azure to crystalline ice-white flare
            if (stage < 2) return glm::vec4(0.25f, 0.80f, 0.95f, 0.88f);
            if (stage < 4) return glm::vec4(0.45f, 0.92f, 1.0f, 0.94f);
            return glm::vec4(0.90f, 0.98f, 1.0f, 1.0f);
        }
        case MAT_BIOLUMINESCENT_FLORA: {
            // Subterranean oasis flora & moss: spore emerald to luminous mint bioluminescence
            if (stage < 2) return glm::vec4(0.22f, 0.88f, 0.42f, 0.90f);
            if (stage < 4) return glm::vec4(0.38f, 0.98f, 0.62f, 0.96f);
            return glm::vec4(0.80f, 1.0f, 0.85f, 1.0f);
        }
        case MAT_PRISMATIC_CRYSTAL: {
            // Translucent radiant prismatic crystal: chromatic lilac to iridescent cyan-magenta diamond prism
            if (stage < 2) return glm::vec4(0.85f, 0.42f, 0.96f, 0.90f);
            if (stage < 4) return glm::vec4(0.65f, 0.80f, 1.0f, 0.96f);
            return glm::vec4(0.98f, 0.92f, 1.0f, 1.0f);
        }
        case MAT_ASTRAL_ORE: {
            // Mythic astral ore: incandescent stellar amber/gold to radiant cosmic stellar flare
            if (stage < 2) return glm::vec4(1.0f, 0.70f, 0.15f, 0.92f);
            if (stage < 4) return glm::vec4(1.0f, 0.88f, 0.35f, 0.96f);
            return glm::vec4(1.0f, 0.98f, 0.75f, 1.0f);
        }
        case MAT_OBSIDIAN_SPIKES: {
            // Vitreous needle obsidian: deep dark glass violet to razor reflective obsidian gleam
            if (stage < 2) return glm::vec4(0.48f, 0.22f, 0.68f, 0.90f);
            if (stage < 4) return glm::vec4(0.78f, 0.45f, 0.92f, 0.95f);
            return glm::vec4(0.95f, 0.82f, 1.0f, 1.0f);
        }
        case MAT_DREDGE_BEDROCK: {
            // Dense planetary crust: deep tectonic slate gray to high-pressure cleavage white
            if (stage < 2) return glm::vec4(0.48f, 0.50f, 0.55f, 0.90f);
            if (stage < 4) return glm::vec4(0.68f, 0.72f, 0.78f, 0.95f);
            return glm::vec4(0.92f, 0.94f, 0.98f, 1.0f);
        }
        default: {
            if (stage < 2) return glm::vec4(0.0f, 0.95f, 1.0f, 0.85f);
            if (stage < 4) return glm::vec4(1.0f, 0.75f, 0.15f, 0.92f);
            return glm::vec4(1.0f, 0.92f, 0.70f, 1.0f);
        }
    }
}

void BlockBreakFX::generate_block_crack_geometry(
    const glm::ivec3& voxel_pos,
    float progress,
    const glm::ivec3& face_norm,
    uint8_t mat_id,
    VoxelShape shape,
    std::vector<float>& out_lines,
    glm::vec4& out_color
) {
    if (progress <= 0.05f) return;

    progress = glm::clamp(progress, 0.0f, 1.0f);
    int stage = std::min(5, static_cast<int>(progress * 6.0f));
    out_color = get_material_crack_color(mat_id, stage);
    out_lines.reserve(512);

    auto add_segment = [&](const glm::vec3& p1, const glm::vec3& p2) {
        out_lines.insert(out_lines.end(), {p1.x, p1.y, p1.z, p2.x, p2.y, p2.z});
    };

    glm::vec3 n = glm::vec3(face_norm);
    if (glm::length(n) < 0.1f) n = glm::vec3(0.0f, 1.0f, 0.0f);
    else n = glm::normalize(n);

    glm::vec3 center = glm::vec3(voxel_pos) + glm::vec3(0.5f) + n * 0.501f;
    glm::vec3 u_dir(1.0f, 0.0f, 0.0f);
    glm::vec3 v_dir(0.0f, 0.0f, 1.0f);

    bool is_ramp = is_ramp_shape(shape);
    bool is_slab = is_slab_shape(shape);

    if (is_ramp) {
        bool is_slope_targeted = false;
        glm::vec3 n_slope(0.0f);
        glm::vec3 slope_u(0.0f);
        glm::vec3 slope_v(0.0f);

        if (shape == SHAPE_RAMP_EAST) { // Rises toward +X (y = x)
            if (face_norm.x > 0) {
                // High vertical back quad at X = 1
                center = glm::vec3(voxel_pos) + glm::vec3(1.002f, 0.5f, 0.5f);
                u_dir = glm::vec3(0.0f, 1.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            } else if (face_norm.y < 0) {
                // Floor base quad at Y = 0
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, -0.002f, 0.5f);
                u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            } else if (face_norm.z != 0) {
                // Triangular side wall
                float z_off = (face_norm.z > 0) ? 1.002f : -0.002f;
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, 0.333f, z_off);
                u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 1.0f, 0.0f);
            } else {
                // Hypotenuse 45° slope (facing -X and +Y)
                is_slope_targeted = true;
                n_slope = glm::normalize(glm::vec3(-1.0f, 1.0f, 0.0f));
                slope_u = glm::vec3(1.0f, 1.0f, 0.0f);
                slope_v = glm::vec3(0.0f, 0.0f, 1.0f);
            }
        } else if (shape == SHAPE_RAMP_WEST) { // Rises toward -X (y = 1 - x)
            if (face_norm.x < 0) {
                center = glm::vec3(voxel_pos) + glm::vec3(-0.002f, 0.5f, 0.5f);
                u_dir = glm::vec3(0.0f, 1.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            } else if (face_norm.y < 0) {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, -0.002f, 0.5f);
                u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            } else if (face_norm.z != 0) {
                float z_off = (face_norm.z > 0) ? 1.002f : -0.002f;
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, 0.333f, z_off);
                u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 1.0f, 0.0f);
            } else {
                is_slope_targeted = true;
                n_slope = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
                slope_u = glm::vec3(-1.0f, 1.0f, 0.0f);
                slope_v = glm::vec3(0.0f, 0.0f, 1.0f);
            }
        } else if (shape == SHAPE_RAMP_SOUTH) { // Rises toward +Z (y = z)
            if (face_norm.z > 0) {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, 0.5f, 1.002f);
                u_dir = glm::vec3(0.0f, 1.0f, 0.0f);
                v_dir = glm::vec3(1.0f, 0.0f, 0.0f);
            } else if (face_norm.y < 0) {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, -0.002f, 0.5f);
                u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            } else if (face_norm.x != 0) {
                float x_off = (face_norm.x > 0) ? 1.002f : -0.002f;
                center = glm::vec3(voxel_pos) + glm::vec3(x_off, 0.333f, 0.5f);
                u_dir = glm::vec3(0.0f, 0.0f, 1.0f);
                v_dir = glm::vec3(0.0f, 1.0f, 0.0f);
            } else {
                is_slope_targeted = true;
                n_slope = glm::normalize(glm::vec3(0.0f, 1.0f, -1.0f));
                slope_u = glm::vec3(0.0f, 1.0f, 1.0f);
                slope_v = glm::vec3(1.0f, 0.0f, 0.0f);
            }
        } else if (shape == SHAPE_RAMP_NORTH) { // Rises toward -Z (y = 1 - z)
            if (face_norm.z < 0) {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, 0.5f, -0.002f);
                u_dir = glm::vec3(0.0f, 1.0f, 0.0f);
                v_dir = glm::vec3(1.0f, 0.0f, 0.0f);
            } else if (face_norm.y < 0) {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, -0.002f, 0.5f);
                u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            } else if (face_norm.x != 0) {
                float x_off = (face_norm.x > 0) ? 1.002f : -0.002f;
                center = glm::vec3(voxel_pos) + glm::vec3(x_off, 0.333f, 0.5f);
                u_dir = glm::vec3(0.0f, 0.0f, 1.0f);
                v_dir = glm::vec3(0.0f, 1.0f, 0.0f);
            } else {
                is_slope_targeted = true;
                n_slope = glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f));
                slope_u = glm::vec3(0.0f, 1.0f, -1.0f);
                slope_v = glm::vec3(1.0f, 0.0f, 0.0f);
            }
        }

        if (is_slope_targeted) {
            center = glm::vec3(voxel_pos) + glm::vec3(0.5f, 0.5f, 0.5f) + n_slope * 0.003f;
            u_dir = slope_u;
            v_dir = slope_v;
        }
    } else if (is_slab) {
        if (shape == SHAPE_SLAB_BOTTOM) {
            if (face_norm.y > 0) {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, 0.502f, 0.5f);
                u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            } else if (face_norm.y < 0) {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, -0.002f, 0.5f);
                u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            } else {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f) + n * 0.501f;
                center.y = static_cast<float>(voxel_pos.y) + 0.25f;
                if (std::abs(n.x) > 0.8f) {
                    u_dir = glm::vec3(0.0f, 0.0f, 1.0f);
                } else {
                    u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                }
                v_dir = glm::vec3(0.0f, 0.5f, 0.0f);
            }
        } else { // SHAPE_SLAB_TOP
            if (face_norm.y < 0) {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, 0.498f, 0.5f);
                u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            } else if (face_norm.y > 0) {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f, 1.002f, 0.5f);
                u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            } else {
                center = glm::vec3(voxel_pos) + glm::vec3(0.5f) + n * 0.501f;
                center.y = static_cast<float>(voxel_pos.y) + 0.75f;
                if (std::abs(n.x) > 0.8f) {
                    u_dir = glm::vec3(0.0f, 0.0f, 1.0f);
                } else {
                    u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
                }
                v_dir = glm::vec3(0.0f, 0.5f, 0.0f);
            }
        }
    } else {
        // Standard full cube face
        if (std::abs(n.y) > 0.8f) {
            u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
            v_dir = glm::vec3(0.0f, 0.0f, 1.0f);
        } else if (std::abs(n.x) > 0.8f) {
            u_dir = glm::vec3(0.0f, 0.0f, 1.0f);
            v_dir = glm::vec3(0.0f, 1.0f, 0.0f);
        } else {
            u_dir = glm::vec3(1.0f, 0.0f, 0.0f);
            v_dir = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    auto to_world = [&](float u, float v) -> glm::vec3 {
        return center + u * u_dir + v * v_dir;
    };

    auto add_line = [&](float u1, float v1, float u2, float v2) {
        glm::vec3 p1 = to_world(u1, v1);
        glm::vec3 p2 = to_world(u2, v2);
        out_lines.insert(out_lines.end(), {p1.x, p1.y, p1.z, p2.x, p2.y, p2.z});
    };

    // Stage 0: Hairline fracture core
    add_line( 0.00f,  0.00f,   0.12f,  0.15f);
    add_line( 0.00f,  0.00f,  -0.14f,  0.11f);
    add_line( 0.00f,  0.00f,  -0.11f, -0.15f);
    add_line( 0.00f,  0.00f,   0.15f, -0.12f);
    add_line( 0.00f,  0.00f,   0.02f,  0.18f);
    add_line( 0.00f,  0.00f,  -0.03f, -0.17f);

    // Stage 1: Primary radial fissures reaching edges
    if (stage >= 1) {
        add_line( 0.12f,  0.15f,   0.28f,  0.32f);
        add_line( 0.28f,  0.32f,   0.42f,  0.40f);
        add_line(-0.14f,  0.11f,  -0.31f,  0.22f);
        add_line(-0.31f,  0.22f,  -0.44f,  0.35f);
        add_line(-0.11f, -0.15f,  -0.25f, -0.30f);
        add_line(-0.25f, -0.30f,  -0.38f, -0.42f);
        add_line( 0.15f, -0.12f,   0.32f, -0.26f);
        add_line( 0.32f, -0.26f,   0.44f, -0.36f);
        add_line( 0.02f,  0.18f,   0.06f,  0.45f);
        add_line(-0.03f, -0.17f,  -0.08f, -0.44f);
    }

    // Stage 2: Lateral cross-cracks & fragment boundaries
    if (stage >= 2) {
        add_line( 0.28f,  0.32f,  -0.14f,  0.11f);
        add_line(-0.31f,  0.22f,  -0.25f, -0.30f);
        add_line(-0.25f, -0.30f,   0.15f, -0.12f);
        add_line( 0.32f, -0.26f,   0.28f,  0.32f);
        add_line( 0.12f,  0.15f,   0.35f,  0.08f);
        add_line(-0.14f,  0.11f,  -0.42f,  0.02f);
        add_line(-0.11f, -0.15f,  -0.05f, -0.38f);
        add_line( 0.15f, -0.12f,   0.40f, -0.10f);
    }

    // Material texture micro-structure adjustments
    if (mat_id == MAT_VOIDITE_CRYSTAL || mat_id == MAT_PRISMATIC_CRYSTAL) {
        // Crystalline 60-degree cleavage facet lines
        add_line( 0.00f,  0.00f,   0.20f,  0.346f);
        add_line( 0.00f,  0.00f,  -0.20f,  0.346f);
        add_line( 0.00f,  0.00f,  -0.38f,  0.00f);
        add_line( 0.00f,  0.00f,   0.38f,  0.00f);
        add_line( 0.00f,  0.00f,  -0.20f, -0.346f);
        add_line( 0.00f,  0.00f,   0.20f, -0.346f);
    } else if (mat_id == MAT_INDUSTRIAL_BULKHEAD || mat_id == MAT_REINFORCED_VAULT_DOOR) {
        // Cold-rolled plate diagonal shear bands
        add_line(-0.35f, -0.35f,  0.35f,  0.35f);
        add_line(-0.35f,  0.35f,  0.35f, -0.35f);
        // Bolt stress ring
        add_line(-0.38f, -0.38f,  0.38f, -0.38f);
        add_line( 0.38f, -0.38f,  0.38f,  0.38f);
        add_line( 0.38f,  0.38f, -0.38f,  0.38f);
        add_line(-0.38f,  0.38f, -0.38f, -0.38f);
    }

    // Stage 3: Spiderweb circumferential perimeter loop
    if (stage >= 3) {
        add_line( 0.42f,  0.40f,   0.06f,  0.45f);
        add_line( 0.06f,  0.45f,  -0.44f,  0.35f);
        add_line(-0.44f,  0.35f,  -0.42f,  0.02f);
        add_line(-0.42f,  0.02f,  -0.38f, -0.42f);
        add_line(-0.38f, -0.42f,  -0.08f, -0.44f);
        add_line(-0.08f, -0.44f,   0.44f, -0.36f);
        add_line( 0.44f, -0.36f,   0.40f, -0.10f);
        add_line( 0.40f, -0.10f,   0.42f,  0.40f);
    }

    // Stage 4: Corner cleavage and face detachment
    if (stage >= 4) {
        add_line( 0.42f,  0.40f,   0.48f,  0.48f);
        add_line(-0.44f,  0.35f,  -0.48f,  0.48f);
        add_line(-0.38f, -0.42f,  -0.48f, -0.48f);
        add_line( 0.44f, -0.36f,   0.48f, -0.48f);
        // Perimeter detachment boundary
        add_line(-0.48f, -0.48f,   0.48f, -0.48f);
        add_line( 0.48f, -0.48f,   0.48f,  0.48f);
        add_line( 0.48f,  0.48f,  -0.48f,  0.48f);
        add_line(-0.48f,  0.48f,  -0.48f, -0.48f);
    }

    // Stage 5: Total pulverization network with diagonal shears
    if (stage >= 5) {
        add_line( 0.00f,  0.00f,   0.20f,  0.00f);
        add_line( 0.00f,  0.00f,  -0.20f,  0.00f);
        add_line( 0.00f,  0.00f,   0.00f,  0.20f);
        add_line( 0.00f,  0.00f,   0.00f, -0.20f);
        add_line(-0.20f,  0.00f,   0.00f,  0.20f);
        add_line( 0.00f,  0.20f,   0.20f,  0.00f);
        add_line( 0.20f,  0.00f,   0.00f, -0.20f);
        add_line( 0.00f, -0.20f,  -0.20f,  0.00f);
        add_line( 0.28f,  0.32f,   0.48f, -0.48f);
        add_line(-0.31f,  0.22f,   0.48f,  0.48f);
        add_line(-0.25f, -0.30f,   0.42f,  0.40f);
        add_line( 0.15f, -0.12f,  -0.48f,  0.48f);
    }

    // 3D Shape-conforming structural silhouette stress fractures
    if (is_ramp && stage >= 2) {
        glm::vec3 vp = glm::vec3(voxel_pos);
        glm::vec3 p_toe0(0.0f), p_toe1(0.0f), p_heel0(0.0f), p_heel1(0.0f), p_crest0(0.0f), p_crest1(0.0f);
        if (shape == SHAPE_RAMP_EAST) {
            p_toe0   = vp + glm::vec3(0.0f, 0.003f, 0.0f);
            p_toe1   = vp + glm::vec3(0.0f, 0.003f, 1.0f);
            p_heel0  = vp + glm::vec3(1.0f, 0.003f, 0.0f);
            p_heel1  = vp + glm::vec3(1.0f, 0.003f, 1.0f);
            p_crest0 = vp + glm::vec3(1.0f, 1.003f, 0.0f);
            p_crest1 = vp + glm::vec3(1.0f, 1.003f, 1.0f);
        } else if (shape == SHAPE_RAMP_WEST) {
            p_toe0   = vp + glm::vec3(1.0f, 0.003f, 0.0f);
            p_toe1   = vp + glm::vec3(1.0f, 0.003f, 1.0f);
            p_heel0  = vp + glm::vec3(0.0f, 0.003f, 0.0f);
            p_heel1  = vp + glm::vec3(0.0f, 0.003f, 1.0f);
            p_crest0 = vp + glm::vec3(0.0f, 1.003f, 0.0f);
            p_crest1 = vp + glm::vec3(0.0f, 1.003f, 1.0f);
        } else if (shape == SHAPE_RAMP_SOUTH) {
            p_toe0   = vp + glm::vec3(0.0f, 0.003f, 0.0f);
            p_toe1   = vp + glm::vec3(1.0f, 0.003f, 0.0f);
            p_heel0  = vp + glm::vec3(0.0f, 0.003f, 1.0f);
            p_heel1  = vp + glm::vec3(1.0f, 0.003f, 1.0f);
            p_crest0 = vp + glm::vec3(0.0f, 1.003f, 1.0f);
            p_crest1 = vp + glm::vec3(1.0f, 1.003f, 1.0f);
        } else if (shape == SHAPE_RAMP_NORTH) {
            p_toe0   = vp + glm::vec3(0.0f, 0.003f, 1.0f);
            p_toe1   = vp + glm::vec3(1.0f, 0.003f, 1.0f);
            p_heel0  = vp + glm::vec3(0.0f, 0.003f, 0.0f);
            p_heel1  = vp + glm::vec3(1.0f, 0.003f, 0.0f);
            p_crest0 = vp + glm::vec3(0.0f, 1.003f, 0.0f);
            p_crest1 = vp + glm::vec3(1.0f, 1.003f, 0.0f);
        }

        add_segment(p_toe0, p_crest0);
        add_segment(p_toe1, p_crest1);
        add_segment(p_crest0, p_crest1);
        add_segment(p_toe0, p_toe1);

        if (stage >= 3) {
            add_segment(p_heel0, p_crest0);
            add_segment(p_heel1, p_crest1);
            add_segment(p_toe0, p_heel0);
            add_segment(p_toe1, p_heel1);
            add_segment(p_heel0, p_heel1);
        }
    } else if (is_slab && stage >= 3) {
        glm::vec3 vp = glm::vec3(voxel_pos);
        float y_bot = (shape == SHAPE_SLAB_BOTTOM) ? 0.002f : 0.502f;
        float y_top = (shape == SHAPE_SLAB_BOTTOM) ? 0.502f : 1.002f;
        glm::vec3 b00(vp.x, vp.y + y_bot, vp.z);
        glm::vec3 b10(vp.x + 1.0f, vp.y + y_bot, vp.z);
        glm::vec3 b11(vp.x + 1.0f, vp.y + y_bot, vp.z + 1.0f);
        glm::vec3 b01(vp.x, vp.y + y_bot, vp.z + 1.0f);
        glm::vec3 t00(vp.x, vp.y + y_top, vp.z);
        glm::vec3 t10(vp.x + 1.0f, vp.y + y_top, vp.z);
        glm::vec3 t11(vp.x + 1.0f, vp.y + y_top, vp.z + 1.0f);
        glm::vec3 t01(vp.x, vp.y + y_top, vp.z + 1.0f);

        add_segment(t00, t10); add_segment(t10, t11); add_segment(t11, t01); add_segment(t01, t00);
        add_segment(b00, b10); add_segment(b10, b11); add_segment(b11, b01); add_segment(b01, b00);
        add_segment(b00, t00); add_segment(b10, t10); add_segment(b11, t11); add_segment(b01, t01);
    }
}

} // namespace Voidfall
