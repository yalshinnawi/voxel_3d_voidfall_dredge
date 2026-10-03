#version 430 core

out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uDepthTexture;
uniform sampler2D uColorTexture;
uniform mat4 uInverseProj;
uniform mat4 uInverseView;

uniform vec3 uSonarOrigin;
uniform float uSonarRadius;
uniform int uSonarActive;
uniform float uTime;

void main() {
    vec4 baseColor = texture(uColorTexture, vUV);
    // Plain rock is not wireframed or tinted by screen post-process;
    // Highlighted ores/doors are rendered directly via dedicated wireframes.
    FragColor = baseColor;
}
