#version 430 core

out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uSceneColor;
uniform sampler2D uBloomColor;
uniform sampler2D uVolumetricFog;
uniform sampler2D uSSAO;
uniform sampler2D uDepthTexture;

uniform float uExposure;
uniform float uBloomIntensity;
uniform float uRadiationGlitch;
uniform float uTime;
uniform float uNear;
uniform float uFar;

// ACES Filmic Tonemapping (Stephen Hill / Narkowicz Fit)
// Transforms sRGB primaries to ACEScg, applies ACES RRT + ODT curve,
// and maps back to sRGB. This eliminates harsh spotlight specular blowouts,
// smoothly compresses intense luminance into highlights, and avoids chrominance distortion.
const mat3 ACESInputMat = mat3(
    0.59719, 0.07600, 0.02840,
    0.35458, 0.90834, 0.13383,
    0.04823, 0.01566, 0.83777
);

const mat3 ACESOutputMat = mat3(
    1.60475, -0.10208, -0.00327,
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602
);

vec3 RRTAndODTFit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 aces_filmic(vec3 color) {
    vec3 v = ACESInputMat * max(color, vec3(0.0));
    vec3 a = RRTAndODTFit(v);
    vec3 result = ACESOutputMat * a;
    return clamp(result, 0.0, 1.0);
}

float linearize_depth(float d) {
    float near = (uNear > 0.0) ? uNear : 0.1;
    float far  = (uFar > 0.0)  ? uFar  : 250.0;
    float z_ndc = d * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z_ndc * (far - near));
}

// Bilateral upsampling for half-resolution volumetric fog
vec4 sample_bilateral_fog(vec2 uv) {
    ivec2 halfResI = textureSize(uVolumetricFog, 0);
    if (halfResI.x <= 0 || halfResI.y <= 0) {
        return texture(uVolumetricFog, uv);
    }
    vec2 halfRes = vec2(halfResI);
    vec2 halfTexel = 1.0 / halfRes;

    vec2 halfPos = uv * halfRes - 0.5;
    vec2 baseCoord = floor(halfPos);
    vec2 f = fract(halfPos);

    vec2 uv00 = (baseCoord + vec2(0.5, 0.5)) * halfTexel;
    vec2 uv10 = (baseCoord + vec2(1.5, 0.5)) * halfTexel;
    vec2 uv01 = (baseCoord + vec2(0.5, 1.5)) * halfTexel;
    vec2 uv11 = (baseCoord + vec2(1.5, 1.5)) * halfTexel;

    // Bilinear spatial weights
    float w00 = (1.0 - f.x) * (1.0 - f.y);
    float w10 = f.x * (1.0 - f.y);
    float w01 = (1.0 - f.x) * f.y;
    float w11 = f.x * f.y;

    float centerDepth = linearize_depth(texture(uDepthTexture, uv).r);

    float d00 = linearize_depth(texture(uDepthTexture, uv00).r);
    float d10 = linearize_depth(texture(uDepthTexture, uv10).r);
    float d01 = linearize_depth(texture(uDepthTexture, uv01).r);
    float d11 = linearize_depth(texture(uDepthTexture, uv11).r);

    // Cross-bilateral depth rejection weights
    float dw00 = exp(-abs(centerDepth - d00) * 1.5);
    float dw10 = exp(-abs(centerDepth - d10) * 1.5);
    float dw01 = exp(-abs(centerDepth - d01) * 1.5);
    float dw11 = exp(-abs(centerDepth - d11) * 1.5);

    float totalW00 = w00 * dw00;
    float totalW10 = w10 * dw10;
    float totalW01 = w01 * dw01;
    float totalW11 = w11 * dw11;

    float totalWeight = totalW00 + totalW10 + totalW01 + totalW11;

    vec4 fog00 = texture(uVolumetricFog, uv00);
    vec4 fog10 = texture(uVolumetricFog, uv10);
    vec4 fog01 = texture(uVolumetricFog, uv01);
    vec4 fog11 = texture(uVolumetricFog, uv11);

    if (totalWeight > 0.0001) {
        return (fog00 * totalW00 + fog10 * totalW10 + fog01 * totalW01 + fog11 * totalW11) / totalWeight;
    }

    // Fallback: choose the sample with closest depth to center
    float diff00 = abs(centerDepth - d00);
    float diff10 = abs(centerDepth - d10);
    float diff01 = abs(centerDepth - d01);
    float diff11 = abs(centerDepth - d11);

    float minDiff = min(min(diff00, diff10), min(diff01, diff11));
    if (minDiff == diff00) return fog00;
    if (minDiff == diff10) return fog10;
    if (minDiff == diff01) return fog01;
    return fog11;
}

void main() {
    vec2 uv = vUV;

    vec3 scene = texture(uSceneColor, uv).rgb;
    vec3 bloom = texture(uBloomColor, uv).rgb;
    vec4 fog   = sample_bilateral_fog(uv); // rgb = inscatter, a = fog factor (bilateral upsampled)

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
