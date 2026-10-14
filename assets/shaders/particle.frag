#version 430 core

in vec4 vColor;
in vec2 vUV;
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

void main() {
    // Soft circular / rounded bioluminescent spore & debris particle
    vec2 centerDist = vUV - vec2(0.5);
    float d = length(centerDist);
    if (d > 0.5) {
        discard;
    }

    // Soft radial luminous halo with concentrated glowing core
    float core = smoothstep(0.5, 0.08, d);
    float halo = smoothstep(0.5, 0.22, d) * 0.70;
    float alpha = (core * 0.75 + halo * 0.25) * vColor.a;

    vec3 rgb = vColor.rgb * (1.0 + core * 1.6);
    FragColor = vec4(rgb, alpha);

    // Write bright luminous spores and embers to HDR bloom buffer
    float luminance = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    if (luminance > 0.9) {
        BrightColor = vec4(rgb * 0.85, alpha);
    } else {
        BrightColor = vec4(0.0, 0.0, 0.0, alpha);
    }
}
