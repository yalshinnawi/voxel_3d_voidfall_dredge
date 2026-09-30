#version 430 core

out float FragColor;
in vec2 vUV;

uniform sampler2D uDepthTexture;
uniform mat4 uProjection;
uniform mat4 uInverseProj;

uniform float uRadius;
uniform float uBias;
uniform vec2 uScreenSize;

// Reconstruct view position from depth
vec3 get_view_pos(vec2 uv) {
    float depth = texture(uDepthTexture, uv).r;
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = uInverseProj * clip;
    return view.xyz / view.w;
}

void main() {
    vec3 fragPos = get_view_pos(vUV);
    if (fragPos.z < -100.0) {
        FragColor = 1.0;
        return;
    }

    vec2 texelSize = 1.0 / uScreenSize;
    float occlusion = 0.0;
    int samples = 8;

    // Fast 8-tap Poisson disk SSAO
    const vec2 kernel[8] = vec2[8](
        vec2( 0.35,  0.22), vec2(-0.45,  0.30),
        vec2( 0.20, -0.60), vec2(-0.25, -0.40),
        vec2( 0.70,  0.60), vec2(-0.80,  0.50),
        vec2( 0.60, -0.70), vec2(-0.75, -0.65)
    );

    for (int i = 0; i < samples; ++i) {
        vec2 sampleUV = vUV + kernel[i] * texelSize * (uRadius / -fragPos.z);
        vec3 samplePos = get_view_pos(sampleUV);

        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - samplePos.z));
        if (samplePos.z >= fragPos.z + uBias) {
            occlusion += rangeCheck;
        }
    }

    occlusion = 1.0 - (occlusion / float(samples));
    FragColor = clamp(occlusion, 0.0, 1.0);
}
