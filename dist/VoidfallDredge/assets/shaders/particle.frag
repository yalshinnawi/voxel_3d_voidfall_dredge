#version 430 core

in vec4 vColor;
in vec2 vUV;
out vec4 FragColor;

void main() {
    // Soft circular / rounded debris particle
    vec2 centerDist = vUV - vec2(0.5);
    float d = length(centerDist);
    if (d > 0.5) {
        discard;
    }
    float alpha = smoothstep(0.5, 0.25, d) * vColor.a;
    FragColor = vec4(vColor.rgb, alpha);
}
