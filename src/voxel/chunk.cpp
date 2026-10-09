#include "chunk.hpp"
#include <glad/glad.h>
#include <algorithm>

namespace Voidfall {

Chunk::Chunk(const ChunkPos& pos)
    : m_pos(pos)
    , m_voxels(CHUNK_VOLUME)
{
}

Chunk::~Chunk() {
    if (m_gpu_initialized) {
        if (m_vbo != 0) {
            glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }
        if (m_vao != 0) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
    }
}

void Chunk::set_voxel(int x, int y, int z, Voxel v) {
    if (!in_bounds(x, y, z)) return;
    size_t idx = to_index(x, y, z);
    bool was_renderable = m_voxels[idx].is_renderable();
    bool now_renderable = v.is_renderable();

    if (!was_renderable && now_renderable) {
        m_solid_count++;
    } else if (was_renderable && !now_renderable && m_solid_count > 0) {
        m_solid_count--;
    }

    m_voxels[idx] = v;
    mark_mesh_dirty();
    mark_structural_dirty();
}

void Chunk::set_voxel_idx(size_t idx, Voxel v) {
    if (idx >= m_voxels.size()) return;
    bool was_renderable = m_voxels[idx].is_renderable();
    bool now_renderable = v.is_renderable();

    if (!was_renderable && now_renderable) {
        m_solid_count++;
    } else if (was_renderable && !now_renderable && m_solid_count > 0) {
        m_solid_count--;
    }

    m_voxels[idx] = v;
    mark_mesh_dirty();
    mark_structural_dirty();
}

void Chunk::stage_mesh(std::vector<PackedVoxelVertex>&& vertices) {
    std::lock_guard<std::mutex> lock(m_stage_mutex);
    m_staged_vertices = std::move(vertices);
    m_has_staged_mesh.store(true, std::memory_order_release);
    clear_mesh_dirty();
}

void Chunk::upload_mesh() {
    std::vector<PackedVoxelVertex> local_mesh;
    {
        std::lock_guard<std::mutex> lock(m_stage_mutex);
        if (!m_has_staged_mesh.load(std::memory_order_acquire)) return;
        local_mesh = std::move(m_staged_vertices);
        m_has_staged_mesh.store(false, std::memory_order_release);
    }

    auto it = std::stable_partition(local_mesh.begin(), local_mesh.end(), [](const PackedVoxelVertex& v) {
        uint32_t layer = (v.data0 >> 23u) & 0xFFu;
        return !Voidfall::IsLiquid(static_cast<uint8_t>(layer));
    });
    m_solid_vertex_count = static_cast<size_t>(std::distance(local_mesh.begin(), it));
    m_liquid_vertex_count = local_mesh.size() - m_solid_vertex_count;
    m_uploaded_vertex_count = local_mesh.size();

    if (!glad_glGenVertexArrays || !glad_glGenBuffers) {
        return;
    }

    if (!m_gpu_initialized) {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        m_gpu_initialized = true;
    }

    if (m_uploaded_vertex_count == 0) {
        return;
    }

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(local_mesh.size() * sizeof(PackedVoxelVertex)),
                 local_mesh.data(),
                 GL_STATIC_DRAW);

    // Attribute 0: uvec2 (packed data0, data1) -> 8 bytes per vertex
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(0, 2, GL_UNSIGNED_INT, sizeof(PackedVoxelVertex), reinterpret_cast<const void*>(0));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Chunk::render_solid() const {
    if (!m_gpu_initialized || m_solid_vertex_count == 0) return;
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_solid_vertex_count));
    glBindVertexArray(0);
}

void Chunk::render_liquid() const {
    if (!m_gpu_initialized || m_liquid_vertex_count == 0) return;
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(m_solid_vertex_count), static_cast<GLsizei>(m_liquid_vertex_count));
    glBindVertexArray(0);
}

void Chunk::render() const {
    render_solid();
    render_liquid();
}

} // namespace Voidfall
