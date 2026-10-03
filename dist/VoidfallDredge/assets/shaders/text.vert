#version 430 core

layout (location = 0) in vec4 aVertex; // pos.xy, uv.zw

uniform mat4 uProjection;
out vec2 vUV;

void main() {
    vUV = aVertex.zw;
    gl_Position = uProjection * vec4(aVertex.xy, 0.0, 1.0);
}
