#version 430 core

layout (location = 0) in vec3 aPos;

uniform mat4 uProjection;
uniform mat4 uView;
uniform vec3 uVoxelPos;

void main() {
    // Expand slightly by 0.005 so lines are outside the voxel face
    vec3 worldPos = uVoxelPos + (aPos * 1.01 - 0.005);
    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
}
