#version 430 core

out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uSceneColor;
uniform sampler2D uBloomColor;
uniform sampler2D uVolumetricFog;
uniform sampler2D uSSAO;

uniform float uExposure;
uniform float uBloomIntensity;
uniform float uRadiationGlitch;
uniform float uTime;

// ACES Filmic Tonemapping Curve
vec3 aces_filmic(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec2 uv = vUV;

    // Radiation glitch distortion on high radiation tick
    if (uRadiationGlitch > 0.05) {
        float glitchNoise = sin(uv.y * 120.0 + uTime * 40.0) * cos(uTime * 25.0);
        if (abs(glitchNoise) > 0.8) {
            uv.x += glitchNoise * 0.008 * uRadiationGlitch;
        }
    }

    vec3 scene = texture(uSceneColor, uv).rgb;
    vec3 bloom = texture(uBloomColor, uv).rgb;
    vec4 fog   = texture(uVolumetricFog, uv); // rgb = inscatter, a = fog factor

    // Composite volumetric fog light shafts
    if (fog.a > 0.001) {
        scene = scene * clamp(1.0 - fog.a * 0.4, 0.2, 1.0) + fog.rgb;
    }

    // Additive Bloom
    scene += bloom * uBloomIntensity;

    // Subterranean Exposure
    vec3 hdr = scene * uExposure;

    // ACES tonemapping
    vec3 ldr = aces_filmic(hdr);

    // Visor Vignette
    vec2 d = uv - vec2(0.5);
    float vignette = 1.0 - dot(d, d) * 0.75;
    ldr *= clamp(vignette, 0.0, 1.0);

    // Gamma correction
    vec3 mapped = pow(ldr, vec3(1.0 / 2.2));

    FragColor = vec4(mapped, 1.0);
}
