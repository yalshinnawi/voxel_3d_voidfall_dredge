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

    m_gpu_initialized = true;
    Logger::info("SurfaceClutter", "Instanced clutter system initialized (stalactites, stalagmites, voidite crystals)");
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
    glm::vec4 rock_mat(0.0f, 0.88f, 0.0f, 0.85f); // non-metallic, rough rock, zero emissive, 0.85 ao

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
    glm::vec4 rock_mat(0.04f, 0.84f, 0.0f, 0.90f);

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
    auto add_spire = [&](glm::vec3 base_pos, glm::vec3 dir, float length, float radius, float tilt_rad) {
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
            vertices.push_back({b0, norm, crystal_col, crystal_mat});
            vertices.push_back({b1, norm, crystal_col, crystal_mat});
            vertices.push_back({w1, norm, crystal_col, crystal_mat});
            vertices.push_back({w0, norm, crystal_col, crystal_mat});

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
            vertices.push_back({w0, norm, crystal_col, crystal_mat});
            vertices.push_back({w1, norm, crystal_col, crystal_mat});
            vertices.push_back({tip, norm, crystal_col, crystal_mat});

            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 2);
        }
    };

    // Central primary crystal spire jutting outward at 45°
    add_spire(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.72f, 0.09f, 0.785f /* 45° */);
    // Flanking crystal spire tilted at 35°
    add_spire(glm::vec3(-0.06f, 0.0f, 0.04f), glm::vec3(0.0f, 1.0f, 0.0f), 0.48f, 0.065f, -0.61f /* -35° */);
    // Flanking crystal spire tilted at 55°
    add_spire(glm::vec3(0.07f, 0.0f, -0.05f), glm::vec3(0.0f, 1.0f, 0.0f), 0.52f, 0.07f, 0.96f /* 55° */);

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

std::vector<ClutterInstance> SurfaceClutterSystem::generate_chunk_clutter(const Chunk& chunk, uint32_t seed) {
    std::vector<ClutterInstance> clutter;
    if (chunk.is_empty()) return clutter;

    ChunkPos cpos = chunk.get_pos();
    int world_base_x = cpos.x * CHUNK_SIZE;
    int world_base_y = cpos.y * CHUNK_SIZE;
    int world_base_z = cpos.z * CHUNK_SIZE;

    // Scan interior slice voxels
    for (int y = 1; y < CHUNK_SIZE - 1; ++y) {
        for (int z = 1; z < CHUNK_SIZE - 1; ++z) {
            for (int x = 1; x < CHUNK_SIZE - 1; ++x) {
                Voxel v = chunk.get_voxel(x, y, z);
                if (!v.is_solid() || v.is_liquid() || v.shape() != SHAPE_CUBE) {
                    continue;
                }

                int wx = world_base_x + x;
                int wy = world_base_y + y;
                int wz = world_base_z + z;

                // 1. Ceiling Stalactite Pass:
                // Solid rock voxel with open cavern air directly underneath
                if (chunk.get_voxel(x, y - 1, z).material_id == MAT_AIR) {
                    uint32_t h = spatial_hash(wx, wy, wz, seed ^ 0xCE11100u);
                    if ((h % 100) < 4) { // ~4% chance on exposed ceilings
                        float yaw = (h % 360) * glm::pi<float>() / 180.0f;
                        float scale_var = 0.85f + 0.35f * ((h >> 8) % 100) / 100.0f;

                        glm::vec3 anchor(wx + 0.5f, wy, wz + 0.5f); // Bottom face of ceiling voxel
                        glm::mat4 model = glm::translate(glm::mat4(1.0f), anchor);
                        model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                        model = glm::scale(model, glm::vec3(scale_var));

                        clutter.push_back({anchor, model, ClutterType::CeilingStalactite, glm::vec4(1.0f), 0.0f});
                    }
                }

                // 2. Floor Stalagmite & Rubble Pass:
                // Solid rock voxel with open air above
                if (chunk.get_voxel(x, y + 1, z).material_id == MAT_AIR) {
                    uint32_t h = spatial_hash(wx, wy, wz, seed ^ 0xF100888u);
                    if ((h % 100) < 4) { // ~4% chance on cavern floors
                        float yaw = (h % 360) * glm::pi<float>() / 180.0f;
                        float scale_var = 0.80f + 0.40f * ((h >> 8) % 100) / 100.0f;

                        glm::vec3 anchor(wx + 0.5f, wy + 1.0f, wz + 0.5f); // Top face of floor voxel
                        glm::mat4 model = glm::translate(glm::mat4(1.0f), anchor);
                        model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                        model = glm::scale(model, glm::vec3(scale_var));

                        clutter.push_back({anchor, model, ClutterType::FloorStalagmiteRubble, glm::vec4(1.0f), 0.0f});
                    }
                }

                // 3. Faceted Voidite & Prismatic Crystal Cluster Pass:
                // Mineral veins with adjacent air faces jutting outward at 30°–60° angles
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
                        if (chunk.get_voxel(nx, ny, nz).material_id == MAT_AIR) {
                            uint32_t h = spatial_hash(wx + dirs[d].x, wy + dirs[d].y, wz + dirs[d].z, seed ^ 0xC8899AAu);
                            if ((h % 100) < 35) { // 35% chance on exposed crystal vein boundaries
                                glm::vec3 norm = glm::vec3(dirs[d]);
                                glm::vec3 anchor = glm::vec3(wx + 0.5f, wy + 0.5f, wz + 0.5f) + norm * 0.5f;

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

                                float spin = (h % 360) * glm::pi<float>() / 180.0f;
                                float scale_var = 0.85f + 0.35f * ((h >> 6) % 100) / 100.0f;

                                glm::mat4 model = glm::translate(glm::mat4(1.0f), anchor);
                                model = model * rot;
                                model = glm::rotate(model, spin, glm::vec3(0.0f, 1.0f, 0.0f));
                                model = glm::scale(model, glm::vec3(scale_var));

                                glm::vec4 tint = (v.material_id == MAT_PRISMATIC_CRYSTAL)
                                    ? glm::vec4(0.85f, 0.45f, 0.98f, 1.0f)
                                    : glm::vec4(0.58f, 0.15f, 0.88f, 1.0f);

                                clutter.push_back({anchor, model, ClutterType::VoiditeCrystal, tint, 1.0f});
                            }
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
    float time
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
