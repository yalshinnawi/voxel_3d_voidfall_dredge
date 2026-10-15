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
uniform int uSector;
uniform float uDarknessFactor;
uniform float uSuffocationDarkness;
uniform float u_ShieldActive;
uniform float u_ShieldTimer;

// Camera matrices for view reconstruction, SSAO & Puddle SSR
uniform mat4 uInverseProj;
uniform mat4 uInverseView;
uniform mat4 uProj;
uniform mat4 uView;

// ACES Filmic Tonemapping (Stephen Hill / Narkowicz Fit)
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

vec3 get_view_pos(vec2 uv) {
    float depth = texture(uDepthTexture, uv).r;
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = uInverseProj * clip;
    return view.xyz / max(view.w, 0.0001);
}

// 8-sample depth-buffer SSAO kernel (radius = 0.45m)
const vec2 ssaoKernel[8] = vec2[](
    vec2( 0.35,  0.22), vec2(-0.45,  0.30),
    vec2( 0.20, -0.60), vec2(-0.25, -0.40),
    vec2( 0.70,  0.60), vec2(-0.80,  0.50),
    vec2( 0.60, -0.70), vec2(-0.75, -0.65)
);

float calculate_ssao(vec2 uv, vec3 viewPos) {
    if (viewPos.z < -45.0) return 1.0;
    float occlusion = 0.0;
    float radius = 0.45;
    float bias = 0.025;
    vec2 texelSize = 1.0 / max(vec2(textureSize(uDepthTexture, 0)), vec2(1.0));

    for (int i = 0; i < 8; ++i) {
        vec2 sampleUV = uv + ssaoKernel[i] * texelSize * (radius / max(-viewPos.z, 0.2)) * 36.0;
        sampleUV = clamp(sampleUV, vec2(0.001), vec2(0.999));
        vec3 samplePos = get_view_pos(sampleUV);

        float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(viewPos.z - samplePos.z), 0.001));
        if (samplePos.z >= viewPos.z + bias) {
            occlusion += rangeCheck;
        }
    }
    return clamp(1.0 - (occlusion / 8.0) * 0.55, 0.35, 1.0);
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

    float w00 = (1.0 - f.x) * (1.0 - f.y);
    float w10 = f.x * (1.0 - f.y);
    float w01 = (1.0 - f.x) * f.y;
    float w11 = f.x * f.y;

    float centerDepth = linearize_depth(texture(uDepthTexture, uv).r);

    float d00 = linearize_depth(texture(uDepthTexture, uv00).r);
    float d10 = linearize_depth(texture(uDepthTexture, uv10).r);
    float d01 = linearize_depth(texture(uDepthTexture, uv01).r);
    float d11 = linearize_depth(texture(uDepthTexture, uv11).r);

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

    if (uSuffocationDarkness > 0.0) {
        scene *= clamp(1.0 - uSuffocationDarkness, 0.0, 1.0);
        bloom *= clamp(1.0 - uSuffocationDarkness, 0.0, 1.0);
    }
    vec4 fog = sample_bilateral_fog(uv);

    // Reconstruct view space position
    vec3 viewPos = get_view_pos(uv);

    // 1. Evaluate lightweight 8-sample depth-buffer SSAO kernel (radius = 0.45m)
    // Grounds dynamic debris, monster feet, and surface clutter against the voxel floor
    float ssao = calculate_ssao(uv, viewPos);
    scene *= ssao;

    // 2. Fast 8-step screen-space depth raymarch (SSR) on wet aquifer floor puddles (N.y > 0.85)
    // Reflects glowing Voidite crystals, thrown flares, and monster eyes on wet stone
    if (viewPos.z > -30.0) {
        vec3 viewNormal = normalize(cross(dFdx(viewPos), dFdy(viewPos)));
        vec3 worldNormal = normalize(mat3(uInverseView) * viewNormal);

        if (worldNormal.y > 0.82) {
            vec3 R_view = reflect(normalize(viewPos), viewNormal);
            if (R_view.y > 0.05) {
                vec3 rayPos = viewPos;
                float stepLen = 0.22 + 0.04 * (-viewPos.z * 0.04);
                vec3 hitColor = vec3(0.0);
                float hitWeight = 0.0;

                for (int s = 1; s <= 8; ++s) {
                    rayPos += R_view * stepLen;
                    vec4 proj = uProj * vec4(rayPos, 1.0);
                    vec2 rayUV = (proj.xy / proj.w) * 0.5 + 0.5;
                    if (rayUV.x < 0.03 || rayUV.x > 0.97 || rayUV.y < 0.03 || rayUV.y > 0.97) break;

                    vec3 sampledPos = get_view_pos(rayUV);
                    float depthDiff = rayPos.z - sampledPos.z;
                    if (depthDiff <= 0.0 && abs(depthDiff) < 0.40) {
                        vec3 reflSample = texture(uSceneColor, rayUV).rgb;
                        reflSample += texture(uBloomColor, rayUV).rgb * 0.85;
                        hitColor = reflSample;
                        hitWeight = smoothstep(0.0, 0.15, R_view.y) * (1.0 - float(s) / 9.0);
                        break;
                    }
                }
                if (hitWeight > 0.0) {
                    float fresnel = pow(clamp(1.0 - max(dot(viewNormal, -normalize(viewPos)), 0.0), 0.0, 1.0), 3.0);
                    scene = mix(scene, hitColor, clamp(fresnel * 0.60 * hitWeight, 0.0, 0.50));
                }
            }
        }
    }

    // Cavern expanses beyond 40.0m smoothly blend into an ambient depth haze
    float linearDepth = linearize_depth(texture(uDepthTexture, uv).r);
    if (linearDepth > 40.0) {
        vec3 depthHazeColor = vec3(0.04, 0.05, 0.07);
        if (uSector == 2) {
            depthHazeColor = vec3(0.08, 0.045, 0.025);
        } else if (uSector >= 3) {
            depthHazeColor = vec3(0.025, 0.07, 0.04);
        }
        if (uRadiationGlitch > 0.0) {
            depthHazeColor = mix(depthHazeColor, vec3(0.05, 0.28, 0.10), uRadiationGlitch * 0.85);
        }
        depthHazeColor *= clamp(1.0 - uDarknessFactor * 0.92, 0.08, 1.0);
        float depthHazeFactor = clamp((linearDepth - 40.0) / 40.0, 0.0, 0.85);
        scene = mix(scene, depthHazeColor, depthHazeFactor);
    }

    // Composite volumetric fog light shafts
    if (fog.a > 0.001) {
        scene = scene * clamp(1.0 - fog.a * 0.4, 0.2, 1.0) + fog.rgb;
    }

    // Additive Bloom
    scene += bloom * uBloomIntensity;

    // Subterranean Exposure
    vec3 hdr = scene * uExposure;

    // ACES Filmic Tonemapping
    vec3 ldr = aces_filmic(hdr);

    // Visor Vignette
    vec2 d = uv - vec2(0.5);
    float vignette = 1.0 - dot(d, d) * 0.75;
    ldr *= clamp(vignette, 0.0, 1.0);

    // Gamma correction
    vec3 mapped = pow(ldr, vec3(1.0 / 2.2));
    vec4 color = vec4(mapped, 1.0);

    // 3x3 Contrast Adaptive Sharpening (CAS) Pass
    ivec2 screenTexSize = textureSize(uSceneColor, 0);
    vec2 texel = 1.0 / max(vec2(screenTexSize), vec2(1.0));

    vec3 col_m = color.rgb;
    vec3 col_n = pow(aces_filmic(texture(uSceneColor, uv + vec2(0.0, -texel.y)).rgb * uExposure), vec3(1.0 / 2.2));
    vec3 col_s = pow(aces_filmic(texture(uSceneColor, uv + vec2(0.0,  texel.y)).rgb * uExposure), vec3(1.0 / 2.2));
    vec3 col_w = pow(aces_filmic(texture(uSceneColor, uv + vec2(-texel.x, 0.0)).rgb * uExposure), vec3(1.0 / 2.2));
    vec3 col_e = pow(aces_filmic(texture(uSceneColor, uv + vec2( texel.x, 0.0)).rgb * uExposure), vec3(1.0 / 2.2));

    vec3 minCol = min(min(min(col_n, col_s), min(col_w, col_e)), col_m);
    vec3 maxCol = max(max(max(col_n, col_s), max(col_w, col_e)), col_m);

    vec3 amp = clamp(min(minCol, 2.0 - maxCol) / max(maxCol, vec3(0.0001)), 0.0, 1.0);
    vec3 w = -sqrt(amp) * 0.18;

    vec3 casColor = (col_m + (col_n + col_s + col_w + col_e) * w) / (1.0 + 4.0 * w);
    color.rgb = clamp(casColor, 0.0, 1.0);

    // Apply shield strictly as an edge vignette
    if (u_ShieldActive > 0.0) {
        vec2 uv_shield = vUV - 0.5;
        float vignette_edge = smoothstep(0.35, 0.75, length(uv_shield));
        float shieldAlpha = clamp(u_ShieldTimer / 1.0, 0.0, 1.0); 
        vec3 shieldCyan = vec3(0.0, 0.95, 1.0);
        color.rgb += shieldCyan * vignette_edge * shieldAlpha * 0.35;
    }

    FragColor = color;
}
