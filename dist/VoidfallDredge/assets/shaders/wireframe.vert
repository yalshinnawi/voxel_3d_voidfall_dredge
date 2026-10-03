#version 430 core

layout (location = 0) in vec3 aPos;

uniform mat4 uProjection;
uniform mat4 uView;
uniform vec3 uVoxelPos;
uniform int uIsLineOnly;

void main() {
    vec3 worldPos;
    if (uIsLineOnly == 1) {
        worldPos = aPos;
    } else {
        worldPos = uVoxelPos + (aPos * 1.01 - 0.005);
    }
    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
}
