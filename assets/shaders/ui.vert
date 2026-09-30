#version 430 core

layout (location = 0) in vec2 aPos;

uniform mat4 uProjection;
uniform vec4 uColor;

out vec4 vColor;

void main() {
    vColor = uColor;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
