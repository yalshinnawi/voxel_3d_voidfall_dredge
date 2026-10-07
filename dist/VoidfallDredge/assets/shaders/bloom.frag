#version 430 core

out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uImage;
uniform float uOffset;

// 4-pass down/up Kawase blur filter
// Samples diagonally offset texels to provide smooth, artifact-free diffusion
// of the emissive bright extraction buffer.
void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(uImage, 0));
    vec2 offset = texelSize * (uOffset + 0.5);

    vec3 sum = vec3(0.0);
    sum += texture(uImage, vUV + vec2(-offset.x, -offset.y)).rgb;
    sum += texture(uImage, vUV + vec2( offset.x, -offset.y)).rgb;
    sum += texture(uImage, vUV + vec2(-offset.x,  offset.y)).rgb;
    sum += texture(uImage, vUV + vec2( offset.x,  offset.y)).rgb;

    FragColor = vec4(sum * 0.25, 1.0);
}
