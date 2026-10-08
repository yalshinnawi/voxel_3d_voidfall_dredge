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

    vec3 blurred = sum * 0.25;

    // Pass 0 (initial downsample extraction pass) safety cutoff:
    // Enforce 1.8-2.2 threshold so standard specular reflections and ambient
    // clear colors never enter the bloom blur chain.
    if (uOffset == 0.0) {
        float lum = dot(blurred, vec3(0.2126, 0.7152, 0.0722));
        float peak = max(blurred.r, max(blurred.g, blurred.b));
        if (lum < 1.8 && peak < 1.8) {
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    FragColor = vec4(blurred, 1.0);
}
