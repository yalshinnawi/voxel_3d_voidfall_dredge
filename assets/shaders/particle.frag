#version 430 core

in vec4 vColor;
in vec2 vUV;
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

void main() {
    // Velocity-stretched ribbon sparks and soft particles with smooth radial falloff
    vec2 ndcUV = vUV * 2.0 - 1.0;
    float d = length(ndcUV);
    if (d > 1.0) {
        discard;
    }

    // Smooth radial falloff and intense glowing emissive core
    float falloff = smoothstep(1.0, 0.0, d);
    float core = smoothstep(0.65, 0.0, d);
    float alpha = falloff * vColor.a;

    vec3 rgb = vColor.rgb * (1.0 + core * 2.5);
    FragColor = vec4(rgb, alpha);

    // Write bright sparks, flares, and embers to HDR bloom buffer
    float luminance = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    if (luminance > 0.85) {
        BrightColor = vec4(rgb * 1.25, alpha);
    } else {
        BrightColor = vec4(0.0, 0.0, 0.0, alpha);
    }
}
