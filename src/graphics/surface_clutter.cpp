#define GLM_ENABLE_EXPERIMENTAL
#include "surface_clutter.hpp"
#include "../core/logger.hpp"
#include <glad/glad.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

namespace Voidfall {

SurfaceClutterSystem::SurfaceClutterSystem() {
    m_staging_instances.reserve(1024);
}

SurfaceClutterSystem::~SurfaceClutterSystem() {
    cleanup();
}

void SurfaceClutterSystem::cleanup() {
    if (!m_gpu_initialized) return;

    for (auto& proto : m_prototypes) {
        if (proto.instance_vbo) { glDeleteBuffers(1, &proto.instance_vbo); proto.instance_vbo = 0; }
        if (proto.ebo)          { glDeleteBuffers(1, &proto.ebo); proto.ebo = 0; }
        if (proto.vbo)          { glDeleteBuffers(1, &proto.vbo); proto.vbo = 0; }
        if (proto.vao)          { glDeleteVertexArrays(1, &proto.vao); proto.vao = 0; }
        proto.index_count = 0;
    }
    m_gpu_initialized = false;
}

void SurfaceClutterSystem::init_gpu() {
    if (m_gpu_initialized) return;
    if (!glad_glGenVertexArrays || !glad_glGenBuffers || !glad_glDrawElementsInstanced) {
        return; // Headless / test environment fallback
    }

    // Load dedicated PBR instanced clutter shader
    bool loaded = m_shader.load_graphics("assets/shaders/clutter.vert", "assets/shaders/clutter.frag");
    if (!loaded) {
        Logger::warn("SurfaceClutter", "Failed to compile clutter shaders, attempting dist/ fallback");
        loaded = m_shader.load_graphics("dist/VoidfallDredge/assets/shaders/clutter.vert", "dist/VoidfallDredge/assets/shaders/clutter.frag");
    }

    build_stalactite_mesh(m_prototypes[static_cast<size_t>(ClutterType::CeilingStalactite)]);
    build_stalagmite_mesh(m_prototypes[static_cast<size_t>(ClutterType::FloorStalagmiteRubble)]);
    build_crystal_mesh(m_prototypes[static_cast<size_t>(ClutterType::VoiditeCrystal)]);
    build_prismatic_crystal_mesh(m_prototypes[static_cast<size_t>(ClutterType::PrismaticCrystal)]);
    build_spore_pod_mesh(m_prototypes[static_cast<size_t>(ClutterType::BioluminescentSporePod)]);
    build_wall_lamp_mesh(m_prototypes[static_cast<size_t>(ClutterType::WallIndustrialLamp)]);

    m_gpu_initialized = true;
    Logger::info("SurfaceClutter", "Instanced clutter system initialized (stalactites, stalagmites, voidite crystals, prismatic diamonds, spore pods, wall lamps)");
}

static void setup_proto_attributes(ClutterMeshPrototype& proto, const std::vector<ClutterVertex>& vertices, const std::vector<unsigned int>& indices) {
    proto.index_count = indices.size();

    glGenVertexArrays(1, &proto.vao);
    glGenBuffers(1, &proto.vbo);
    glGenBuffers(1, &proto.ebo);
    glGenBuffers(1, &proto.instance_vbo);

    glBindVertexArray(proto.vao);

    // 1. Static Prototype Geometry
    glBindBuffer(GL_ARRAY_BUFFER, proto.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(ClutterVertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, proto.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Attribute 0: Position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ClutterVertex), (void*)offsetof(ClutterVertex, position));

    // Attribute 1: Normal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ClutterVertex), (void*)offsetof(ClutterVertex, normal));

    // Attribute 2: Color (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ClutterVertex), (void*)offsetof(ClutterVertex, color));

    // Attribute 3: Material (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ClutterVertex), (void*)offsetof(ClutterVertex, material));

    // 2. Dynamic Per-Instance Buffer
    glBindBuffer(GL_ARRAY_BUFFER, proto.instance_vbo);
    // Attribute 4..7: Instance Model Matrix (mat4 takes 4 vec4 attribute locations)
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(4 + i);
        glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(ClutterInstanceGPUData), (void*)(sizeof(glm::vec4) * i));
        glVertexAttribDivisor(4 + i, 1);
    }

    // Attribute 8: Instance Tint & Emissive Boost (vec4)
    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(ClutterInstanceGPUData), (void*)offsetof(ClutterInstanceGPUData, tint));
    glVertexAttribDivisor(8, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// 1. Ceiling Stalactites: Tapered 6-sided conical rock spike hanging down from solid ceiling voxels
void SurfaceClutterSystem::build_stalactite_mesh(ClutterMeshPrototype& proto) {
    std::vector<ClutterVertex> vertices;
    std::vector<unsigned int> indices;

    const int SIDES = 6;
    const float radius = 0.22f;
    const float length = 1.15f;

    glm::vec3 tip(0.0f, -length, 0.0f);
    glm::vec4 rock_col(0.24f, 0.23f, 0.22f, 1.0f);
    // Non-interactive detritus: zero metallic, ultra-matte high roughness (0.96) to prevent specular distractions
    glm::vec4 rock_mat(0.0f, 0.96f, 0.0f, 0.85f);

    // Base ring vertices (y = 0.0)
    std::vector<glm::vec3> base_ring;
    for (int i = 0; i < SIDES; ++i) {
        float angle = i * glm::two_pi<float>() / SIDES;
        base_ring.push_back(glm::vec3(radius * std::cos(angle), 0.0f, radius * std::sin(angle)));
    }

    // 6 side triangular faces connecting base ring to the sharp downwards tip
    for (int i = 0; i < SIDES; ++i) {
        glm::vec3 p0 = base_ring[i];
        glm::vec3 p1 = base_ring[(i + 1) % SIDES];
        glm::vec3 norm = glm::normalize(glm::cross(tip - p0, p1 - p0));

        unsigned int base_idx = static_cast<unsigned int>(vertices.size());
        vertices.push_back({p0, norm, rock_col, rock_mat});
        vertices.push_back({p1, norm, rock_col, rock_mat});
        vertices.push_back({tip, norm, rock_col, rock_mat});

        indices.push_back(base_idx);
        indices.push_back(base_idx + 1);
        indices.push_back(base_idx + 2);
    }

    setup_proto_attributes(proto, vertices, indices);
}

// 2. Floor Stalagmites & Rubble: Angled rock mounds and jagged debris clusters placed on cavern floors
void SurfaceClutterSystem::build_stalagmite_mesh(ClutterMeshPrototype& proto) {
    std::vector<ClutterVertex> vertices;
    std::vector<unsigned int> indices;

    const int SIDES = 6;
    const float radii[6] = { 0.28f, 0.35f, 0.25f, 0.38f, 0.26f, 0.32f }; // Irregular perimeter
    const float height = 0.78f;

    glm::vec3 apex(0.04f, height, -0.05f); // Naturally tilted apex
    glm::vec4 rock_col(0.21f, 0.20f, 0.19f, 1.0f);
    // Non-interactive detritus: zero metallic, high roughness (0.95), zero emissive to prevent glare
    glm::vec4 rock_mat(0.0f, 0.95f, 0.0f, 0.90f);

    std::vector<glm::vec3> base_ring;
    for (int i = 0; i < SIDES; ++i) {
        float angle = i * glm::two_pi<float>() / SIDES;
        base_ring.push_back(glm::vec3(radii[i] * std::cos(angle), 0.0f, radii[i] * std::sin(angle)));
    }

    // Main rock mound
    for (int i = 0; i < SIDES; ++i) {
        glm::vec3 p0 = base_ring[i];
        glm::vec3 p1 = base_ring[(i + 1) % SIDES];
        glm::vec3 norm = glm::normalize(glm::cross(p1 - p0, apex - p0));

        unsigned int base_idx = static_cast<unsigned int>(vertices.size());
        vertices.push_back({p0, norm, rock_col, rock_mat});
        vertices.push_back({p1, norm, rock_col, rock_mat});
        vertices.push_back({apex, norm, rock_col, rock_mat});

        indices.push_back(base_idx);
        indices.push_back(base_idx + 1);
        indices.push_back(base_idx + 2);
    }

    // Secondary jagged rubble sub-cone at base (adds organic debris clutter feel)
    glm::vec3 sub_apex(0.18f, 0.32f, 0.14f);
    const float sub_r = 0.14f;
    std::vector<glm::vec3> sub_ring;
    for (int i = 0; i < 4; ++i) {
        float angle = i * glm::half_pi<float>();
        sub_ring.push_back(glm::vec3(0.18f + sub_r * std::cos(angle), 0.0f, 0.14f + sub_r * std::sin(angle)));
    }
    for (int i = 0; i < 4; ++i) {
        glm::vec3 p0 = sub_ring[i];
        glm::vec3 p1 = sub_ring[(i + 1) % 4];
        glm::vec3 norm = glm::normalize(glm::cross(p1 - p0, sub_apex - p0));

        unsigned int base_idx = static_cast<unsigned int>(vertices.size());
        vertices.push_back({p0, norm, rock_col * 0.92f, rock_mat});
        vertices.push_back({p1, norm, rock_col * 0.92f, rock_mat});
        vertices.push_back({sub_apex, norm, rock_col * 0.92f, rock_mat});

        indices.push_back(base_idx);
        indices.push_back(base_idx + 1);
        indices.push_back(base_idx + 2);
    }

    setup_proto_attributes(proto, vertices, indices);
}

// 3. Faceted Voidite Clusters: Angular, multi-faceted crystal prisms jutting outward at 30°–60° angles from mineral veins
void SurfaceClutterSystem::build_crystal_mesh(ClutterMeshPrototype& proto) {
    std::vector<ClutterVertex> vertices;
    std::vector<unsigned int> indices;

    glm::vec4 crystal_col(0.58f, 0.15f, 0.88f, 1.0f); // Radiant voidite amethyst
    glm::vec4 crystal_mat(0.22f, 0.12f, 3.8f, 1.0f);  // low roughness, high gloss, 3.8x emissive radiance

    // Helper to add a 6-sided faceted crystal spire with shaft and pointed cap
    auto add_spire = [&](glm::vec3 base_pos, glm::vec3 dir, float length, float radius, float tilt_rad, glm::vec4 col) {
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(dir, up)) > 0.95f) up = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 side = glm::normalize(glm::cross(dir, up));
        up = glm::normalize(glm::cross(side, dir));

        // Tilt axis
        dir = glm::normalize(dir + side * std::sin(tilt_rad));

        const int SIDES = 6;
        std::vector<glm::vec3> ring_base;
        std::vector<glm::vec3> ring_waist;

        float shaft_len = length * 0.72f;
        glm::vec3 tip = base_pos + dir * length;

        for (int i = 0; i < SIDES; ++i) {
            float angle = i * glm::two_pi<float>() / SIDES;
            glm::vec3 offset = (side * std::cos(angle) + up * std::sin(angle)) * radius;
            ring_base.push_back(base_pos + offset);
            ring_waist.push_back(base_pos + dir * shaft_len + offset * 0.92f);
        }

        // Shaft quad faces (2 triangles each)
        for (int i = 0; i < SIDES; ++i) {
            glm::vec3 b0 = ring_base[i];
            glm::vec3 b1 = ring_base[(i + 1) % SIDES];
            glm::vec3 w0 = ring_waist[i];
            glm::vec3 w1 = ring_waist[(i + 1) % SIDES];

            glm::vec3 norm = glm::normalize(glm::cross(b1 - b0, w0 - b0));

            unsigned int idx = static_cast<unsigned int>(vertices.size());
            vertices.push_back({b0, norm, col, crystal_mat});
            vertices.push_back({b1, norm, col, crystal_mat});
            vertices.push_back({w1, norm, col, crystal_mat});
            vertices.push_back({w0, norm, col, crystal_mat});

            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 2);

            indices.push_back(idx);
            indices.push_back(idx + 2);
            indices.push_back(idx + 3);
        }

        // Pointed apex cap (1 triangle per segment)
        for (int i = 0; i < SIDES; ++i) {
            glm::vec3 w0 = ring_waist[i];
            glm::vec3 w1 = ring_waist[(i + 1) % SIDES];
            glm::vec3 norm = glm::normalize(glm::cross(w1 - w0, tip - w0));

            unsigned int idx = static_cast<unsigned int>(vertices.size());
            vertices.push_back({w0, norm, col, crystal_mat});
            vertices.push_back({w1, norm, col, crystal_mat});
            vertices.push_back({tip, norm, col, crystal_mat});

            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 2);
        }
    };

    // Central primary crystal spire jutting outward at 42°
    add_spire(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.74f, 0.095f, 0.73f /* 42° */, glm::vec4(0.62f, 0.18f, 0.92f, 1.0f));
    // Flanking tall crystal spire tilted at -36°
    add_spire(glm::vec3(-0.08f, 0.0f, 0.05f), glm::vec3(0.0f, 1.0f, 0.0f), 0.58f, 0.075f, -0.63f /* -36° */, glm::vec4(0.55f, 0.12f, 0.85f, 1.0f));
    // Flanking crystal spire tilted at 56°
    add_spire(glm::vec3(0.09f, 0.0f, -0.06f), glm::vec3(0.0f, 1.0f, 0.0f), 0.54f, 0.070f, 0.98f /* 56° */, glm::vec4(0.65f, 0.22f, 0.95f, 1.0f));
    // Low forward cluster spire tilted at 22°
    add_spire(glm::vec3(0.04f, 0.0f, 0.09f), glm::vec3(0.0f, 1.0f, 0.0f), 0.44f, 0.062f, 0.38f /* 22° */, glm::vec4(0.52f, 0.14f, 0.82f, 1.0f));
    // Sharp needle spire tilted at -52°
    add_spire(glm::vec3(-0.06f, 0.0f, -0.07f), glm::vec3(0.0f, 1.0f, 0.0f), 0.36f, 0.048f, -0.91f /* -52° */, glm::vec4(0.70f, 0.25f, 0.98f, 1.0f));

    setup_proto_attributes(proto, vertices, indices);
}

// 4. Faceted Prismatic Diamond Clusters: Soft celestine/aquamarine crystal clusters and delicate mineral needles
void SurfaceClutterSystem::build_prismatic_crystal_mesh(ClutterMeshPrototype& proto) {
    std::vector<ClutterVertex> vertices;
    std::vector<unsigned int> indices;

    // Natural mineral gemstone: subtle 0.35x ambient luminescence, balanced matte roughness to avoid glare
    glm::vec4 diamond_mat(0.18f, 0.42f, 0.35f, 0.90f);

    auto add_spire = [&](glm::vec3 base_pos, glm::vec3 dir, float length, float radius, float tilt_rad, glm::vec4 col) {
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(dir, up)) > 0.95f) up = glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 side = glm::normalize(glm::cross(dir, up));
        up = glm::normalize(glm::cross(side, dir));

        dir = glm::normalize(dir + side * std::sin(tilt_rad));

        const int SIDES = 6;
        std::vector<glm::vec3> ring_base;
        std::vector<glm::vec3> ring_waist;

        float shaft_len = length * 0.70f;
        glm::vec3 tip = base_pos + dir * length;

        for (int i = 0; i < SIDES; ++i) {
            float angle = i * glm::two_pi<float>() / SIDES;
            glm::vec3 offset = (side * std::cos(angle) + up * std::sin(angle)) * radius;
            ring_base.push_back(base_pos + offset);
            ring_waist.push_back(base_pos + dir * shaft_len + offset * 0.90f);
        }

        for (int i = 0; i < SIDES; ++i) {
            glm::vec3 b0 = ring_base[i];
            glm::vec3 b1 = ring_base[(i + 1) % SIDES];
            glm::vec3 w0 = ring_waist[i];
            glm::vec3 w1 = ring_waist[(i + 1) % SIDES];

            glm::vec3 norm = glm::normalize(glm::cross(b1 - b0, w0 - b0));

            unsigned int idx = static_cast<unsigned int>(vertices.size());
            vertices.push_back({b0, norm, col, diamond_mat});
            vertices.push_back({b1, norm, col, diamond_mat});
            vertices.push_back({w1, norm, col, diamond_mat});
            vertices.push_back({w0, norm, col, diamond_mat});

            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 2);

            indices.push_back(idx);
            indices.push_back(idx + 2);
            indices.push_back(idx + 3);
        }

        // Apex cap with subtle translucent quartz crest
        glm::vec4 tip_col = glm::mix(col, glm::vec4(0.70f, 0.88f, 0.94f, 1.0f), 0.35f);
        for (int i = 0; i < SIDES; ++i) {
            glm::vec3 w0 = ring_waist[i];
            glm::vec3 w1 = ring_waist[(i + 1) % SIDES];
            glm::vec3 norm = glm::normalize(glm::cross(w1 - w0, tip - w0));

            unsigned int idx = static_cast<unsigned int>(vertices.size());
            vertices.push_back({w0, norm, col, diamond_mat});
            vertices.push_back({w1, norm, col, diamond_mat});
            vertices.push_back({tip, norm, tip_col, diamond_mat});

            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 2);
        }
    };

    // Delicate natural mineral cluster: nestled spires with gentle celestine tones that blend seamlessly into cavern rock
    add_spire(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.46f, 0.068f, 0.60f, glm::vec4(0.32f, 0.82f, 0.90f, 1.0f));
    add_spire(glm::vec3(-0.05f, 0.0f, 0.04f), glm::vec3(0.0f, 1.0f, 0.0f), 0.36f, 0.055f, -0.50f, glm::vec4(0.28f, 0.80f, 0.88f, 1.0f));
    add_spire(glm::vec3(0.06f, 0.0f, -0.04f), glm::vec3(0.0f, 1.0f, 0.0f), 0.34f, 0.052f, 0.78f, glm::vec4(0.35f, 0.85f, 0.92f, 1.0f));
    add_spire(glm::vec3(0.02f, 0.0f, 0.06f), glm::vec3(0.0f, 1.0f, 0.0f), 0.28f, 0.044f, 0.28f, glm::vec4(0.26f, 0.78f, 0.86f, 1.0f));
    add_spire(glm::vec3(-0.04f, 0.0f, -0.04f), glm::vec3(0.0f, 1.0f, 0.0f), 0.22f, 0.038f, -0.72f, glm::vec4(0.36f, 0.86f, 0.94f, 1.0f));

    setup_proto_attributes(proto, vertices, indices);
}

// 5. Bioluminescent Fungal Spore Pods: Clustered subterranean mushrooms & glowing bulbous caps
void SurfaceClutterSystem::build_spore_pod_mesh(ClutterMeshPrototype& proto) {
    std::vector<ClutterVertex> vertices;
    std::vector<unsigned int> indices;

    // Organic bioluminescent material: moderate roughness (0.28), high emissive radiance (2.8f)
    glm::vec4 spore_mat(0.08f, 0.28f, 2.8f, 1.0f);
    glm::vec4 stalk_mat(0.02f, 0.65f, 0.45f, 0.90f);

    auto add_mushroom = [&](glm::vec3 base_pos, glm::vec3 lean_dir, float height, float stalk_r, float cap_r, glm::vec4 cap_col) {
        const int SIDES = 8;
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 dir = glm::normalize(up + lean_dir);
        glm::vec3 side = glm::normalize(glm::cross(dir, glm::vec3(0.0f, 0.0f, 1.0f)));
        glm::vec3 fwd = glm::normalize(glm::cross(side, dir));

        glm::vec3 stalk_top = base_pos + dir * height;
        glm::vec4 stalk_col = glm::vec4(0.65f, 0.88f, 0.82f, 1.0f);

        // Stalk cylinder
        std::vector<glm::vec3> ring_bot, ring_top;
        for (int i = 0; i < SIDES; ++i) {
            float angle = i * glm::two_pi<float>() / SIDES;
            glm::vec3 off = (side * std::cos(angle) + fwd * std::sin(angle)) * stalk_r;
            ring_bot.push_back(base_pos + off);
            ring_top.push_back(stalk_top + off * 0.85f);
        }

        for (int i = 0; i < SIDES; ++i) {
            glm::vec3 b0 = ring_bot[i];
            glm::vec3 b1 = ring_bot[(i + 1) % SIDES];
            glm::vec3 t0 = ring_top[i];
            glm::vec3 t1 = ring_top[(i + 1) % SIDES];

            glm::vec3 norm = glm::normalize(glm::cross(b1 - b0, t0 - b0));
            unsigned int idx = static_cast<unsigned int>(vertices.size());
            vertices.push_back({b0, norm, stalk_col, stalk_mat});
            vertices.push_back({b1, norm, stalk_col, stalk_mat});
            vertices.push_back({t1, norm, stalk_col, stalk_mat});
            vertices.push_back({t0, norm, stalk_col, stalk_mat});

            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 2);

            indices.push_back(idx);
            indices.push_back(idx + 2);
            indices.push_back(idx + 3);
        }

        // Bulbous spore cap (dome + undercut gills)
        glm::vec3 cap_apex = stalk_top + dir * (cap_r * 0.85f);
        std::vector<glm::vec3> rim;
        for (int i = 0; i < SIDES; ++i) {
            float angle = i * glm::two_pi<float>() / SIDES;
            glm::vec3 off = (side * std::cos(angle) + fwd * std::sin(angle)) * cap_r;
            rim.push_back(stalk_top + off);
        }

        // Cap upper dome
        for (int i = 0; i < SIDES; ++i) {
            glm::vec3 r0 = rim[i];
            glm::vec3 r1 = rim[(i + 1) % SIDES];
            glm::vec3 norm = glm::normalize(glm::cross(r1 - r0, cap_apex - r0));

            unsigned int idx = static_cast<unsigned int>(vertices.size());
            vertices.push_back({r0, norm, cap_col, spore_mat});
            vertices.push_back({r1, norm, cap_col, spore_mat});
            vertices.push_back({cap_apex, norm, cap_col, spore_mat});

            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 2);
        }

        // Cap underside / glowing gills
        glm::vec4 gill_col = glm::mix(cap_col, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.40f);
        for (int i = 0; i < SIDES; ++i) {
            glm::vec3 r0 = rim[i];
            glm::vec3 r1 = rim[(i + 1) % SIDES];
            glm::vec3 norm = -dir;

            unsigned int idx = static_cast<unsigned int>(vertices.size());
            vertices.push_back({stalk_top, norm, gill_col, spore_mat});
            vertices.push_back({r1, norm, gill_col, spore_mat});
            vertices.push_back({r0, norm, gill_col, spore_mat});

            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 2);
        }
    };

    // Central primary fungal stalk with wide glowing spore dome
    add_mushroom(glm::vec3(0.0f), glm::vec3(0.04f, 0.0f, -0.02f), 0.52f, 0.048f, 0.16f, glm::vec4(0.20f, 0.95f, 0.72f, 1.0f));
    // Flanking medium spore stalk tilted left
    add_mushroom(glm::vec3(-0.10f, 0.0f, 0.06f), glm::vec3(-0.18f, 0.0f, 0.12f), 0.40f, 0.038f, 0.13f, glm::vec4(0.28f, 0.98f, 0.65f, 1.0f));
    // Flanking spore stalk tilted right
    add_mushroom(glm::vec3(0.11f, 0.0f, -0.05f), glm::vec3(0.16f, 0.0f, -0.10f), 0.36f, 0.035f, 0.12f, glm::vec4(0.15f, 0.92f, 0.78f, 1.0f));
    // Low bulbous spore pod in front
    add_mushroom(glm::vec3(0.05f, 0.0f, 0.11f), glm::vec3(0.08f, 0.0f, 0.18f), 0.26f, 0.032f, 0.10f, glm::vec4(0.35f, 1.0f, 0.85f, 1.0f));
    // Tiny satellite spore node
    add_mushroom(glm::vec3(-0.07f, 0.0f, -0.09f), glm::vec3(-0.12f, 0.0f, -0.15f), 0.19f, 0.025f, 0.08f, glm::vec4(0.22f, 0.95f, 0.70f, 1.0f));

    setup_proto_attributes(proto, vertices, indices);
}

// 6. Wall Industrial Bulkhead Worklight: Rugged steel junction plate, stanchion bracket, protective wire cage, and glowing halogen amber emitter
void SurfaceClutterSystem::build_wall_lamp_mesh(ClutterMeshPrototype& proto) {
    std::vector<ClutterVertex> vertices;
    std::vector<unsigned int> indices;

    // Materials:
    // Dark steel casing: metallic 0.85, roughness 0.45, zero emissive
    glm::vec4 metal_mat(0.85f, 0.45f, 0.0f, 0.70f);
    glm::vec4 metal_col(0.24f, 0.25f, 0.28f, 1.0f);
    // Glowing incandescent/halogen bulb: metallic 0.05, roughness 0.12, emissive 2.8f
    glm::vec4 bulb_mat(0.05f, 0.12f, 2.8f, 1.0f);
    glm::vec4 bulb_col(1.0f, 0.78f, 0.32f, 1.0f);

    auto add_box = [&](glm::vec3 min_p, glm::vec3 max_p, glm::vec4 col, glm::vec4 mat) {
        glm::vec3 corners[8] = {
            {min_p.x, min_p.y, min_p.z}, {max_p.x, min_p.y, min_p.z},
            {max_p.x, max_p.y, min_p.z}, {min_p.x, max_p.y, min_p.z},
            {min_p.x, min_p.y, max_p.z}, {max_p.x, min_p.y, max_p.z},
            {max_p.x, max_p.y, max_p.z}, {min_p.x, max_p.y, max_p.z}
        };
        const int face_indices[6][4] = {
            {0, 3, 2, 1}, // -Z
            {4, 5, 6, 7}, // +Z
            {0, 1, 5, 4}, // -Y
            {3, 7, 6, 2}, // +Y
            {0, 4, 7, 3}, // -X
            {1, 2, 6, 5}  // +X
        };
        const glm::vec3 face_normals[6] = {
            {0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {0, 1, 0}, {-1, 0, 0}, {1, 0, 0}
        };

        for (int f = 0; f < 6; ++f) {
            unsigned int idx = static_cast<unsigned int>(vertices.size());
            glm::vec3 n = face_normals[f];
            vertices.push_back({corners[face_indices[f][0]], n, col, mat});
            vertices.push_back({corners[face_indices[f][1]], n, col, mat});
            vertices.push_back({corners[face_indices[f][2]], n, col, mat});
            vertices.push_back({corners[face_indices[f][3]], n, col, mat});

            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 2);
            indices.push_back(idx);
            indices.push_back(idx + 2);
            indices.push_back(idx + 3);
        }
    };

    // 1. Wall mounting backplate (sitting directly on wall surface y=0.0 to 0.025)
    add_box(glm::vec3(-0.14f, 0.0f, -0.14f), glm::vec3(0.14f, 0.025f, 0.14f), metal_col, metal_mat);

    // 2. Angled bracket neck / stanchion projecting outward
    add_box(glm::vec3(-0.035f, 0.025f, -0.035f), glm::vec3(0.035f, 0.16f, 0.035f), metal_col, metal_mat);

    // 3. Housing base collar
    add_box(glm::vec3(-0.08f, 0.16f, -0.08f), glm::vec3(0.08f, 0.19f, 0.08f), metal_col, metal_mat);

    // 4. Glowing bulb capsule inside cage
    add_box(glm::vec3(-0.05f, 0.19f, -0.05f), glm::vec3(0.05f, 0.32f, 0.05f), bulb_col, bulb_mat);

    // 5. Protective outer cage struts (4 corner bars)
    float c_min = 0.07f;
    float c_w = 0.018f;
    add_box(glm::vec3(-c_min, 0.19f, -c_min), glm::vec3(-c_min + c_w, 0.34f, -c_min + c_w), metal_col, metal_mat);
    add_box(glm::vec3(c_min - c_w, 0.19f, -c_min), glm::vec3(c_min, 0.34f, -c_min + c_w), metal_col, metal_mat);
    add_box(glm::vec3(-c_min, 0.19f, c_min - c_w), glm::vec3(-c_min + c_w, 0.34f, c_min), metal_col, metal_mat);
    add_box(glm::vec3(c_min - c_w, 0.19f, c_min - c_w), glm::vec3(c_min, 0.34f, c_min), metal_col, metal_mat);

    // 6. Top angled protective reflector hood
    add_box(glm::vec3(-0.11f, 0.34f, -0.11f), glm::vec3(0.11f, 0.37f, 0.11f), glm::vec4(0.35f, 0.36f, 0.40f, 1.0f), metal_mat);

    setup_proto_attributes(proto, vertices, indices);
}

// Deterministic spatial hash for pseudo-random prop scattering
static inline uint32_t spatial_hash(int x, int y, int z, uint32_t seed) {
    uint32_t h = seed ^ (static_cast<uint32_t>(x) * 73856093u) ^ (static_cast<uint32_t>(y) * 19349663u) ^ (static_cast<uint32_t>(z) * 83492791u);
    h ^= (h >> 13);
    h *= 0x85ebca6bu;
    h ^= (h >> 16);
    return h;
}

ClutterInstance SurfaceClutterSystem::create_crystal_instance(
    const glm::ivec3& host_block,
    const glm::ivec3& face_dir,
    uint8_t mat_id,
    uint32_t seed
) {
    glm::vec3 norm = glm::vec3(face_dir);
    glm::vec3 anchor = glm::vec3(host_block) + glm::vec3(0.5f) + norm * 0.5f;

    // Orient towards face normal
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 rot(1.0f);
    if (glm::abs(glm::dot(norm, up)) < 0.99f) {
        glm::vec3 axis = glm::normalize(glm::cross(up, norm));
        float angle = std::acos(glm::clamp(glm::dot(up, norm), -1.0f, 1.0f));
        rot = glm::rotate(glm::mat4(1.0f), angle, axis);
    } else if (norm.y < 0.0f) {
        rot = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    uint32_t h = spatial_hash(host_block.x + face_dir.x, host_block.y + face_dir.y, host_block.z + face_dir.z, seed ^ 0xC8899AAu);
    float spin = (h % 360) * glm::pi<float>() / 180.0f;
    // Decorative crystals are subtly scaled down to nestle naturally into room rock faces
    float scale_var = (mat_id == MAT_PRISMATIC_CRYSTAL)
        ? (0.68f + 0.16f * ((h >> 6) % 100) / 100.0f)
        : (0.90f + 0.25f * ((h >> 6) % 100) / 100.0f);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), anchor);
    model = model * rot;
    model = glm::rotate(model, spin, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scale_var));

    ClutterType ctype = (mat_id == MAT_PRISMATIC_CRYSTAL)
        ? ClutterType::PrismaticCrystal
        : ClutterType::VoiditeCrystal;

    glm::vec4 tint = (mat_id == MAT_PRISMATIC_CRYSTAL)
        ? glm::vec4(0.32f, 0.85f, 0.92f, 0.85f)   // Soft celestine/aquamarine mineral tone (decorative)
        : glm::vec4(0.72f, 0.20f, 0.96f, 1.0f);  // Voidite deep amethyst (objective)

    return ClutterInstance{anchor, model, ctype, tint, 1.0f, host_block, face_dir};
}

std::vector<ClutterInstance> SurfaceClutterSystem::generate_chunk_clutter(
    const Chunk& chunk,
    uint32_t seed,
    const std::function<Voxel(int, int, int)>& get_world_voxel,
    const std::function<bool(int, int, int)>& is_dark_zone
) {
    std::vector<ClutterInstance> clutter;
    if (chunk.is_empty()) return clutter;

    ChunkPos cpos = chunk.get_pos();
    int world_base_x = cpos.x * CHUNK_SIZE;
    int world_base_y = cpos.y * CHUNK_SIZE;
    int world_base_z = cpos.z * CHUNK_SIZE;

    auto sample_neighbor = [&](int lx, int ly, int lz, int gx, int gy, int gz) -> Voxel {
        if (Chunk::in_bounds(lx, ly, lz)) {
            return chunk.get_voxel(lx, ly, lz);
        }
        if (get_world_voxel) {
            return get_world_voxel(gx, gy, gz);
        }
        return Voxel{MAT_AIR, 0};
    };

    // Scan all chunk voxels
    for (int y = 0; y < CHUNK_SIZE; ++y) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                Voxel v = chunk.get_voxel(x, y, z);
                if (!v.is_solid() || v.is_liquid() || v.shape() != SHAPE_CUBE) {
                    continue;
                }

                int wx = world_base_x + x;
                int wy = world_base_y + y;
                int wz = world_base_z + z;

                // 1. Ceiling Stalactite Pass:
                // Solid rock voxel with open cavern air directly underneath (non-mineral blocks)
                if (v.material_id != MAT_VOIDITE_CRYSTAL && v.material_id != MAT_PRISMATIC_CRYSTAL) {
                    if (wy > 1 && sample_neighbor(x, y - 1, z, wx, wy - 1, wz).material_id == MAT_AIR) {
                        uint32_t h = spatial_hash(wx, wy, wz, seed ^ 0xCE11100u);
                        if ((h % 100) < 5) { // ~5% chance on exposed ceilings
                            float yaw = (h % 360) * glm::pi<float>() / 180.0f;
                            // Multi-tier sizes: small drop (0.35x), medium spike (0.60x), colossal stalactite (1.05x)
                            int tier = (h >> 12) % 100;
                            float base_scale = (tier < 35) ? 0.35f : ((tier < 80) ? 0.60f : 1.05f);
                            float scale_var = base_scale * (0.85f + 0.30f * ((h >> 8) % 100) / 100.0f);

                            glm::vec3 anchor(wx + 0.5f, wy, wz + 0.5f); // Bottom face of ceiling voxel
                            glm::mat4 model = glm::translate(glm::mat4(1.0f), anchor);
                            model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                            model = glm::scale(model, glm::vec3(scale_var));

                            clutter.push_back({anchor, model, ClutterType::CeilingStalactite, glm::vec4(1.0f), 0.0f, glm::ivec3(wx, wy, wz), glm::ivec3(0, -1, 0)});
                        }
                    }

                    // 2. Floor Stalagmite, Scree & Selective Fungal Pass:
                    // Solid rock voxel with open air above
                    if (wy < 31 && sample_neighbor(x, y + 1, z, wx, wy + 1, wz).material_id == MAT_AIR) {
                        uint32_t h = spatial_hash(wx, wy, wz, seed ^ 0xF100888u);
                        bool in_dark = is_dark_zone && is_dark_zone(wx, wy + 1, wz);

                        if ((h % 100) < 5) { // ~5% chance on cavern floors for scree / stalagmites
                            float yaw = (h % 360) * glm::pi<float>() / 180.0f;
                            int tier = (h >> 12) % 100;
                            float base_scale = (tier < 40) ? 0.32f : ((tier < 82) ? 0.62f : 1.15f);
                            float scale_var = base_scale * (0.80f + 0.35f * ((h >> 8) % 100) / 100.0f);

                            glm::vec3 anchor(wx + 0.5f, wy + 1.0f, wz + 0.5f); // Top face of floor voxel
                            glm::mat4 model = glm::translate(glm::mat4(1.0f), anchor);
                            model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                            model = glm::scale(model, glm::vec3(scale_var));

                            clutter.push_back({anchor, model, ClutterType::FloorStalagmiteRubble, glm::vec4(1.0f), 0.0f, glm::ivec3(wx, wy, wz), glm::ivec3(0, 1, 0)});
                        } else if (!in_dark) {
                            // Reduced mushroom spawn: only ~1.2% on standard floors, or ~4% on organic flora/moss blocks
                            bool is_flora = (v.material_id == MAT_BIOLUMINESCENT_FLORA);
                            bool spawn_mushroom = is_flora ? ((h % 100) < 9) : ((h % 100) == 7);

                            if (spawn_mushroom) {
                                float yaw = (h % 360) * glm::pi<float>() / 180.0f;
                                float scale_var = 0.55f + 0.30f * ((h >> 8) % 100) / 100.0f;

                                glm::vec3 anchor(wx + 0.5f, wy + 1.0f, wz + 0.5f);
                                glm::mat4 model = glm::translate(glm::mat4(1.0f), anchor);
                                model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                                model = glm::scale(model, glm::vec3(scale_var));

                                glm::vec4 tint = (v.material_id == MAT_VOLCANIC_BASALT)
                                    ? glm::vec4(1.0f, 0.62f, 0.18f, 1.0f) // Thermoluminescent volcanic ember pod
                                    : ((wy < 10)
                                        ? glm::vec4(0.78f, 0.28f, 0.98f, 1.0f) // Deep void amethyst spore pod
                                        : glm::vec4(0.20f, 0.95f, 0.70f, 1.0f)); // Cavern phosphorescent mint/cyan

                                clutter.push_back({anchor, model, ClutterType::BioluminescentSporePod, tint, 2.2f, glm::ivec3(wx, wy, wz), glm::ivec3(0, 1, 0)});
                            }
                        }
                    }

                    // 3. Exposed Wall-Face Detritus, Rock Outcrops, Wall Lamps, Crevice Crystals & Fungal Spores:
                    // For vertical wall faces facing into open cavern air
                    if (v.material_id != MAT_REINFORCED_VAULT_DOOR && v.material_id != MAT_DREDGE_BEDROCK) {
                        const glm::ivec3 wall_dirs[4] = {
                            { 1, 0, 0}, {-1, 0, 0},
                            { 0, 0, 1}, { 0, 0,-1}
                        };
                        for (int d = 0; d < 4; ++d) {
                            int nx = x + wall_dirs[d].x;
                            int ny = y;
                            int nz = z + wall_dirs[d].z;
                            int gnx = wx + wall_dirs[d].x;
                            int gny = wy;
                            int gnz = wz + wall_dirs[d].z;
                            Voxel n_vox = sample_neighbor(nx, ny, nz, gnx, gny, gnz);
                            if (n_vox.material_id == MAT_AIR) {
                                uint32_t wh = spatial_hash(gnx, gny, gnz, seed ^ (0xDA7A000u + d * 0x1337u));
                                bool in_dark = is_dark_zone && is_dark_zone(gnx, gny, gnz);

                                if ((wh % 100) < 7) { // ~7% chance on exposed wall faces
                                    int type_roll = (wh >> 8) % 100;
                                    ClutterType ctype = ClutterType::FloorStalagmiteRubble;
                                    glm::vec4 tint(1.0f);
                                    float scale = 0.50f;
                                    float emissive_boost = 0.0f;

                                    bool is_industrial_wall = (v.material_id == MAT_INDUSTRIAL_BULKHEAD);

                                    if (in_dark) {
                                        // Dark zones: strictly unpowered, non-emissive natural or industrial detritus
                                        if (type_roll < 65) {
                                            ctype = ClutterType::FloorStalagmiteRubble;
                                            tint = is_industrial_wall ? glm::vec4(0.35f, 0.38f, 0.42f, 1.0f) : glm::vec4(0.92f, 0.90f, 0.88f, 1.0f);
                                        } else {
                                            ctype = ClutterType::CeilingStalactite;
                                            scale = 0.42f;
                                            tint = glm::vec4(0.90f, 0.90f, 0.90f, 1.0f);
                                        }
                                        emissive_boost = 0.0f;
                                    } else if (is_industrial_wall) {
                                        // Industrial bulkhead walls: wall lamps and metal struts
                                        if (type_roll < 58) {
                                            ctype = ClutterType::WallIndustrialLamp;
                                            scale = 0.55f;
                                            tint = glm::vec4(1.0f, 0.82f, 0.35f, 1.0f); // Warm halogen glow
                                            emissive_boost = 2.4f;
                                        } else {
                                            ctype = ClutterType::FloorStalagmiteRubble; // Steel strut / brace
                                            tint = glm::vec4(0.35f, 0.38f, 0.42f, 1.0f);
                                            emissive_boost = 0.0f;
                                        }
                                    } else {
                                        // Natural rock cavern walls: rock spurs, stalactites, crevice crystals, miner lamps, and rare spores
                                        if (type_roll < 48) {
                                            ctype = ClutterType::FloorStalagmiteRubble; // Rock spur
                                            int tier = (wh >> 14) % 100;
                                            float base_s = (tier < 35) ? 0.35f : ((tier < 80) ? 0.60f : 1.05f);
                                            scale = base_s * (0.85f + 0.30f * ((wh >> 18) % 100) / 100.0f);
                                            tint = (v.material_id == MAT_VOLCANIC_BASALT)
                                                ? glm::vec4(0.82f, 0.84f, 0.86f, 1.0f)
                                                : glm::vec4(1.04f, 0.98f, 0.92f, 1.0f);
                                        } else if (type_roll < 72) {
                                            ctype = ClutterType::CeilingStalactite; // Wall stalactite
                                            scale = (0.45f + 0.35f * ((wh >> 16) % 100) / 100.0f) * 0.70f;
                                            tint = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
                                        } else if (type_roll < 86) {
                                            ctype = ClutterType::PrismaticCrystal; // Wall crevice crystal cluster
                                            scale = (0.40f + 0.30f * ((wh >> 16) % 100) / 100.0f) * 0.60f;
                                            emissive_boost = 1.0f;
                                            tint = glm::vec4(0.38f, 0.85f, 0.95f, 0.85f);
                                        } else if (type_roll < 95) {
                                            ctype = ClutterType::WallIndustrialLamp; // Mining worklight staked into rock
                                            scale = 0.52f;
                                            tint = glm::vec4(1.0f, 0.75f, 0.30f, 1.0f);
                                            emissive_boost = 2.4f;
                                        } else {
                                            // Very rare wall fungal colony (only ~5% of wall props)
                                            ctype = ClutterType::BioluminescentSporePod;
                                            scale = (0.45f + 0.30f * ((wh >> 16) % 100) / 100.0f) * 0.70f;
                                            emissive_boost = 2.2f;
                                            tint = (v.material_id == MAT_VOLCANIC_BASALT)
                                                ? glm::vec4(1.0f, 0.58f, 0.18f, 1.0f)
                                                : glm::vec4(0.22f, 0.95f, 0.72f, 1.0f);
                                        }
                                    }

                                    glm::vec3 norm = glm::vec3(wall_dirs[d]);
                                    glm::vec3 anchor = glm::vec3(wx, wy, wz) + glm::vec3(0.5f) + norm * 0.48f;
                                    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                                    glm::mat4 rot(1.0f);
                                    if (glm::abs(glm::dot(norm, up)) < 0.99f) {
                                        glm::vec3 axis = glm::normalize(glm::cross(up, norm));
                                        float angle = std::acos(glm::clamp(glm::dot(up, norm), -1.0f, 1.0f));
                                        rot = glm::rotate(glm::mat4(1.0f), angle, axis);
                                    }
                                    float spin = ((wh >> 6) % 360) * glm::pi<float>() / 180.0f;
                                    glm::mat4 model = glm::translate(glm::mat4(1.0f), anchor);
                                    model = model * rot;
                                    model = glm::rotate(model, spin, glm::vec3(0.0f, 1.0f, 0.0f));
                                    model = glm::scale(model, glm::vec3(scale));

                                    clutter.push_back({anchor, model, ctype, tint, emissive_boost, glm::ivec3(wx, wy, wz), wall_dirs[d]});
                                }
                            }
                        }
                    }
                }

                // 3. Faceted Voidite & Prismatic Crystal Cluster Pass:
                // Organic Crystal Voxels: Exposed surfaces spawn the 3D faceted cluster mesh by default!
                if (v.material_id == MAT_VOIDITE_CRYSTAL || v.material_id == MAT_PRISMATIC_CRYSTAL) {
                    const glm::ivec3 dirs[6] = {
                        { 1, 0, 0}, {-1, 0, 0},
                        { 0, 1, 0}, { 0,-1, 0},
                        { 0, 0, 1}, { 0, 0,-1}
                    };

                    for (int d = 0; d < 6; ++d) {
                        int nx = x + dirs[d].x;
                        int ny = y + dirs[d].y;
                        int nz = z + dirs[d].z;
                        int gnx = wx + dirs[d].x;
                        int gny = wy + dirs[d].y;
                        int gnz = wz + dirs[d].z;
                        Voxel n_vox = sample_neighbor(nx, ny, nz, gnx, gny, gnz);
                        if (!n_vox.is_solid()) {
                            // Guaranteed 3D faceted cluster mesh by default on all exposed faces
                            ClutterInstance inst = create_crystal_instance(
                                glm::ivec3(wx, wy, wz), dirs[d], v.material_id, seed
                            );
                            clutter.push_back(inst);
                        }
                    }
                }
            }
        }
    }

    return clutter;
}

void SurfaceClutterSystem::render_chunk_clutter(
    const std::vector<ClutterInstance>& instances,
    const glm::vec3& player_pos,
    const glm::mat4& view,
    const glm::mat4& proj,
    const glm::vec3& cam_pos,
    const glm::vec3& headlamp_pos,
    const glm::vec3& headlamp_dir,
    const glm::vec3& headlamp_color,
    bool headlamp_enabled,
    float time,
    float darkness_factor
) {
    if (instances.empty() || !m_gpu_initialized) return;

    m_shader.use();
    m_shader.set_mat4("uView", view);
    m_shader.set_mat4("uProjection", proj);
    m_shader.set_vec3("uCamPos", cam_pos);
    m_shader.set_vec3("uHeadlampPos", headlamp_pos);
    m_shader.set_vec3("uHeadlampDir", headlamp_dir);
    m_shader.set_vec3("uHeadlampColor", headlamp_color);
    m_shader.set_float("uHeadlampEnabled", headlamp_enabled ? 1.0f : 0.0f);
    m_shader.set_float("uTime", time);
    m_shader.set_float("uDarknessFactor", darkness_factor);

    const float MAX_DISTANCE_SQ = 32.0f * 32.0f; // 32-meter distance gate

    for (size_t type_idx = 0; type_idx < static_cast<size_t>(ClutterType::Count); ++type_idx) {
        ClutterType type = static_cast<ClutterType>(type_idx);
        const auto& proto = m_prototypes[type_idx];
        if (proto.vao == 0 || proto.index_count == 0) continue;

        m_staging_instances.clear();

        for (const auto& inst : instances) {
            if (inst.type != type) continue;

            // Distance culling: strictly cull props beyond 32 meters from the player
            float dist_sq = glm::distance2(inst.position, player_pos);
            if (dist_sq <= MAX_DISTANCE_SQ) {
                m_staging_instances.push_back({inst.model_matrix, inst.color_tint});
            }
        }

        if (!m_staging_instances.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, proto.instance_vbo);
            glBufferData(GL_ARRAY_BUFFER, m_staging_instances.size() * sizeof(ClutterInstanceGPUData), m_staging_instances.data(), GL_STREAM_DRAW);

            glBindVertexArray(proto.vao);
            glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(proto.index_count), GL_UNSIGNED_INT, 0, static_cast<GLsizei>(m_staging_instances.size()));
            glBindVertexArray(0);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace Voidfall
