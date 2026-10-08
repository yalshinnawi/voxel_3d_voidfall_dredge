#version 430 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vTangent;
in vec3 vBitangent;
in vec2 vUV;
flat in uint vTexLayer;
flat in uint vAOIndex;
in float vAO;
in float vEmissive;
in float vDamage;
in float vSonarIntensity;

// PBR Texture Arrays
uniform sampler2DArray uAlbedoArray;
uniform sampler2DArray uNormalArray;
uniform sampler2DArray uRoughMetalArray;
uniform sampler2DArray uEmissiveArray;
uniform int uUseTextureArray;

// Camera / Headlamp
uniform vec3 uCameraPos;
uniform vec3 uHeadlampPos;
uniform vec3 uHeadlampDir;
uniform vec3 uHeadlampColor;
uniform float uHeadlampInnerCutoff;
uniform float uHeadlampOuterCutoff;
uniform float uHeadlampIntensity;

// Point Lights Data & Clustered SSBOs
struct PointLightData {
    vec4 position_radius; // xyz = position, w = radius
    vec4 color_intensity; // xyz = color, w = intensity
};

const uint CLUSTERS_X = 16u;
const uint CLUSTERS_Y = 9u;
const uint CLUSTERS_Z = 24u;
const uint TOTAL_CLUSTERS = 3456u;
const uint MAX_LIGHTS_PER_CLUSTER = 64u;

struct ClusterRecord {
    uint count;
    uint pad0;
    uint pad1;
    uint pad2;
    uint light_indices[MAX_LIGHTS_PER_CLUSTER];
};

layout (std430, binding = 0) readonly buffer PointLightBuffer {
    PointLightData bPointLights[];
};

layout (std430, binding = 1) readonly buffer ClusterLightBuffer {
    ClusterRecord bClusters[TOTAL_CLUSTERS];
};

uniform mat4 uView;
uniform vec2 uScreenSize;
uniform float uNear;
uniform float uFar;
uniform int uClusteredLightingEnabled;

// Fallback point lights (when clustered lighting disabled)
struct PointLight {
    vec3 position;
    vec3 color;
    float radius;
    float intensity;
};

const int MAX_POINT_LIGHTS = 16;
uniform int uNumPointLights;
uniform PointLight uPointLights[MAX_POINT_LIGHTS];

// Sector biomes & animation
uniform float uTime;
uniform int uSector;

// Material tier base colors fallback
vec3 get_material_albedo(uint layer) {
    switch (layer) {
        case 1u: return vec3(0.24, 0.22, 0.21); // Fractured Granite
        case 2u: return vec3(0.12, 0.12, 0.14); // Volcanic Basalt
        case 3u: return vec3(0.48, 0.12, 0.72); // Voidite Crystal
        case 4u: return vec3(0.35, 0.38, 0.42); // Industrial Bulkhead
        case 5u: return vec3(0.55, 0.45, 0.20); // Reinforced Vault Door
        case 6u: return vec3(0.85, 0.35, 0.05); // Thermite Slag
        case 7u: return vec3(0.15, 0.65, 0.25); // Radioactive Ore
        case 8u: return vec3(0.06, 0.06, 0.08); // Dredge Bedrock
        case 11u: return vec3(0.12, 0.60, 0.85); // Crystal Aquifer Water
        case 12u: return vec3(0.20, 0.85, 0.35); // Bioluminescent Flora
        case 13u: return vec3(0.82, 0.38, 0.95); // Prismatic Crystal
        case 14u: return vec3(0.22, 0.05, 0.08); // Crystalline Obsidian Spikes
        default: return vec3(0.30, 0.30, 0.30);
    }
}

vec2 get_material_rough_metal(uint layer) {
    switch (layer) {
        case 3u: return vec2(0.15, 0.20); // Voidite crystal (polished gemstone)
        case 4u: return vec2(0.35, 0.88); // Industrial bulkhead (metallic)
        case 5u: return vec2(0.25, 0.92); // Vault door (polished metal)
        case 6u: return vec2(0.65, 0.30); // Molten slag
        case 11u: return vec2(0.06, 0.15); // Water / puddle (fluid polish)
        case 12u: return vec2(0.65, 0.05); // Flora (soft moss)
        case 13u: return vec2(0.15, 0.35); // Prismatic crystal (faceted diamond)
        case 14u: return vec2(0.15, 0.45); // Obsidian spikes (glossy mineral glass)
        default: return vec2(0.85, 0.05); // Rock / granite / basalt
    }
}

// GGX / Cook-Torrance PBR helper functions
const float PI = 3.14159265359;

float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return num / (PI * denom * denom + 0.0001);
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k + 0.0001);
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometry_schlick_ggx(NdotV, roughness) * geometry_schlick_ggx(NdotL, roughness);
}

vec3 fresnel_schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 geoN = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);

    // Orthonormal TBN frame from vertex attributes
    vec3 T = normalize(vTangent - dot(vTangent, geoN) * geoN);
    vec3 B = normalize(cross(geoN, T));
    mat3 TBN = mat3(T, B, geoN);

    // Tangent-space view ray (used for parallax crystal depth & internal refraction)
    vec3 V_tangent = transpose(TBN) * V;

    vec2 baseUV = fract(vUV);

    // Dynamic material UV animations
    if (vTexLayer == 6u) { // Thermite Slag molten magma convection flow
        vec2 flowOffset = vec2(sin(uTime * 0.35 + vWorldPos.x * 0.15), cos(uTime * 0.28 + vWorldPos.z * 0.15)) * 0.05;
        baseUV = fract(baseUV + flowOffset);
    } else if (vTexLayer == 11u) { // Crystal Aquifer water ripples
        vec2 waveOffset = vec2(sin(uTime * 0.60 + vWorldPos.x * 0.35), cos(uTime * 0.50 + vWorldPos.z * 0.35)) * 0.03;
        baseUV = fract(baseUV + waveOffset);
    }

    // LOD Distance-Gated Parallax Occlusion Mapping (POM)
    vec3 v_FragPos = vWorldPos;
    vec3 u_CameraPos = uCameraPos;
    float dist = length(v_FragPos - u_CameraPos);

    vec2 quadUV;
    if (dist < 7.0f) {
        // Attenuate POM displacement scale smoothly to zero between 5.0m and 7.0m
        float pomFade = smoothstep(7.0, 5.0, dist);
        float depthScale = 0.04 * pomFade;

        // Execute an 8-step POM raymarch using the height map layer from GL_TEXTURE_2D_ARRAY
        const float numSteps = 8.0;
        float stepSize = 1.0 / numSteps;
        vec2 p = (V_tangent.xy / max(abs(V_tangent.z), 0.25)) * depthScale;
        vec2 deltaUV = p / numSteps;

        float currentLayerDepth = 0.0;
        vec2 currUV = baseUV;
        float currentHeight = texture(uNormalArray, vec3(fract(currUV), float(vTexLayer))).a;
        float currentDepthMapValue = 1.0 - currentHeight;

        vec2 prevUV = currUV;
        float prevLayerDepth = currentLayerDepth;
        float prevDepthMapValue = currentDepthMapValue;

        for (int step = 0; step < 8; ++step) {
            if (currentLayerDepth >= currentDepthMapValue) {
                break;
            }
            prevUV = currUV;
            prevLayerDepth = currentLayerDepth;
            prevDepthMapValue = currentDepthMapValue;

            currUV -= deltaUV;
            currentLayerDepth += stepSize;
            currentHeight = texture(uNormalArray, vec3(fract(currUV), float(vTexLayer))).a;
            currentDepthMapValue = 1.0 - currentHeight;
        }

        // Parallax Occlusion interpolation
        float afterDepth = currentLayerDepth - currentDepthMapValue;
        float beforeDepth = prevDepthMapValue - prevLayerDepth;
        float denom = afterDepth + beforeDepth;
        float weight = denom > 0.0001 ? clamp(beforeDepth / denom, 0.0, 1.0) : 0.0;
        quadUV = mix(prevUV, currUV, weight);
    } else {
        // Completely bypass raymarching loops. Directly sample the base UVs for tangent normal and roughness calculations to protect GPU fillrate across open cavern expanses.
        quadUV = baseUV;
    }

    vec3 texCoord = vec3(fract(quadUV), float(vTexLayer));

    // 1. Sample Texture Arrays or procedural fallback
    vec3 albedo;
    vec2 roughMetal;
    vec3 emissive = vec3(0.0);
    vec3 N = geoN;

    bool isCrystal = (vTexLayer == 3u || vTexLayer == 13u);

    if (uUseTextureArray == 1) {
        // Sample Tangent Normal and Roughness from the active GL_TEXTURE_2D_ARRAY
        vec3 mapN = texture(uNormalArray, texCoord).xyz * 2.0 - 1.0;
        N = normalize(TBN * mapN);

        albedo = texture(uAlbedoArray, texCoord).rgb;
        roughMetal = texture(uRoughMetalArray, texCoord).rg;

        if (isCrystal) {
            // Interior Parallax Mapping: Look *into* the crystal volume!
            // As the camera moves, the deep glowing crystalline core shifts with true 3D depth
            vec2 interiorUV = fract(quadUV - V_tangent.xy * 0.045);
            vec3 interiorTexCoord = vec3(interiorUV, float(vTexLayer));

            vec3 surfaceEmissive = texture(uEmissiveArray, texCoord).rgb;
            vec3 interiorEmissive = texture(uEmissiveArray, interiorTexCoord).rgb;

            // Blend surface facets with interior crystal nucleus - calibrated for radiant gemstone glow
            emissive = (surfaceEmissive * 0.55 + interiorEmissive * 0.45) * (vEmissive * 1.1 + 0.35);
        } else {
            emissive = texture(uEmissiveArray, texCoord).rgb * (vEmissive * 2.2 + 0.3);
        }
    } else {
        albedo = get_material_albedo(vTexLayer);
        vec3 worldCoord = vWorldPos * 2.0;
        float hash = fract(sin(dot(floor(worldCoord), vec3(12.9898, 78.233, 37.719))) * 43758.5453);
        albedo *= (0.85 + 0.3 * hash);
        roughMetal = get_material_rough_metal(vTexLayer);

        if (vTexLayer == 3u) {
            float shimmer = 0.85 + 0.25 * sin(uTime * 3.5 + dot(vWorldPos, vec3(0.5, 0.7, 0.3)));
            emissive = vec3(0.45, 0.15, 0.85) * (vEmissive * 4.0 + 0.8) * shimmer;
        } else if (vTexLayer == 6u) {
            float pulse = 0.9 + 0.2 * sin(uTime * 6.0 + dot(vWorldPos, vec3(1.2, 0.9, 0.4)));
            emissive = vec3(1.0, 0.4, 0.05) * (vEmissive * 8.0 + 2.0) * pulse;
        } else if (vTexLayer == 7u) {
            float pulse = 0.8 + 0.3 * sin(uTime * 2.0 + dot(vWorldPos, vec3(0.3, 1.1, 0.7)));
            emissive = vec3(0.1, 0.95, 0.3) * (vEmissive * 4.5 + 1.0) * pulse;
        } else if (vTexLayer == 11u) {
            float caustic = 0.85 + 0.25 * sin(uTime * 2.5 + dot(vWorldPos, vec3(1.2, 0.5, 0.9)));
            emissive = vec3(0.10, 0.65, 0.92) * (vEmissive * 2.5 + 0.3) * caustic;
        } else if (vTexLayer == 12u) {
            float pulse = 0.8 + 0.3 * sin(uTime * 1.8 + dot(vWorldPos, vec3(0.6, 1.2, 0.8)));
            emissive = vec3(0.15, 0.95, 0.38) * (vEmissive * 4.0 + 0.8) * pulse;
        } else if (vTexLayer == 13u) {
            float sheen = 0.85 + 0.3 * sin(uTime * 2.8 + dot(vWorldPos, vec3(1.1, 1.1, 0.6)));
            emissive = vec3(0.85, 0.40, 0.98) * (vEmissive * 4.5 + 1.0) * sheen;
        } else if (vTexLayer == 14u) {
            float pulse = 0.85 + 0.25 * sin(uTime * 4.0 + dot(vWorldPos, vec3(1.0, 2.0, 1.0)));
            emissive = vec3(0.98, 0.12, 0.18) * (vEmissive * 3.5 + 0.8) * pulse;
        }
    }

    // Dynamic crystal pulsing resonance
    if (vTexLayer == 3u) { // Voidite Crystal hypnotic void breathing
        float shimmer = 0.85 + 0.30 * sin(uTime * 2.6 + dot(vWorldPos, vec3(0.6, 0.9, 0.4)));
        emissive *= shimmer;
    } else if (vTexLayer == 13u) { // Prismatic Crystal chromatic shimmer
        float sheen = 0.85 + 0.35 * sin(uTime * 2.2 + dot(vWorldPos, vec3(1.1, 0.8, 0.5)));
        emissive *= sheen;
    }

    // Apply drilling / impact fracture overlay
    if (vDamage > 0.05) {
        albedo = mix(albedo, vec3(0.08, 0.07, 0.06), vDamage * 0.7);
    }

    // Zero-Cost Procedural Edge Chamfering:
    // To eliminate the razor-sharp digital 90° look on cubic faces without adding extra geometry:
    vec3 normal = N;
    // Chamfer outer edges based on UV proximity
    vec2 v_TexCoords = fract(vUV);
    vec2 edgeDist = min(v_TexCoords, 1.0 - v_TexCoords);
    float minEdge = min(edgeDist.x, edgeDist.y);
    if (minEdge < 0.05) {
        float bevelFactor = smoothstep(0.0, 0.05, minEdge);
        // Soften normal away from face perpendicular
        normal = normalize(mix(normal + dFdx(v_FragPos) * 0.15 + dFdy(v_FragPos) * 0.15, normal, bevelFactor));
    }
    N = normal;

    float roughness = clamp(roughMetal.r, 0.04, 0.99);
    float metallic  = clamp(roughMetal.g, 0.0, 1.0);

    // Identify material categories for roughness scaling
    bool isPuddle = (vTexLayer == 11u);
    bool isPolishedMetal = (metallic > 0.5) && (vTexLayer == 4u || vTexLayer == 5u || metallic >= 0.7);
    bool isStone = (vTexLayer == 1u || vTexLayer == 2u || vTexLayer == 7u || vTexLayer == 8u) ||
                   (!isPuddle && !isPolishedMetal && vTexLayer != 3u && vTexLayer != 6u &&
                    vTexLayer != 12u && vTexLayer != 13u && vTexLayer != 14u);

    // Restrict low roughness (< 0.15) strictly to puddles and polished metals
    if (!isPuddle && !isPolishedMetal) {
        roughness = max(roughness, 0.15);
    }

    // Clamp stone minimum roughness to 0.45 to prevent cavern walls from looking like polished plastic
    if (isStone) {
        roughness = max(roughness, 0.45);
    }

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    // 2. Evaluate Headlamp Spotlight (PBR with normal mapping & crystal optical dispersion)
    {
        vec3 lightDir = normalize(uHeadlampPos - vWorldPos);
        float dist = distance(uHeadlampPos, vWorldPos);
        // Smooth inverse-square distance attenuation
        float attenuation = 1.0 / (dist * dist + 1.0);

        // Smooth spotlight cone falloff between outer and inner cutoff (replaces hard cutoff)
        float theta = dot(lightDir, normalize(-uHeadlampDir));
        float spotFactor = smoothstep(uHeadlampOuterCutoff, uHeadlampInnerCutoff, theta);

        if (spotFactor > 0.0) {
            vec3 radiance = uHeadlampColor * uHeadlampIntensity * attenuation * spotFactor;
            float NdotL = max(dot(N, lightDir), 0.0);

            if (isCrystal) {
                // Prismatic Spectral Dispersion: Crystals split white light into rainbow facets
                vec3 H_mid = normalize(V + lightDir);
                vec3 disp = TBN * vec3(0.025, 0.015, 0.0);
                vec3 H_R = normalize(V + lightDir + disp);
                vec3 H_B = normalize(V + lightDir - disp);

                float NDF_R = distribution_ggx(N, H_R, roughness);
                float NDF_G = distribution_ggx(N, H_mid, roughness);
                float NDF_B = distribution_ggx(N, H_B, roughness);

                float G = geometry_smith(N, V, lightDir, roughness);
                vec3 F = fresnel_schlick(max(dot(H_mid, V), 0.0), F0);

                float denom = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
                vec3 specular = (vec3(NDF_R, NDF_G, NDF_B) * G * F) / denom;

                vec3 kS = F;
                vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
                Lo += (kD * albedo / PI + specular) * radiance * NdotL;

                // Diamond Micro-Facet Sparkle: Pinpoint glints that twinkle as the camera shifts
                vec2 sparkleCoord = floor(vUV * 64.0);
                float sparkleRand = fract(sin(dot(sparkleCoord + floor(V.xy * 32.0), vec2(12.9898, 78.233))) * 43758.5453);
                float sparkleGlint = pow(sparkleRand, 28.0) * pow(max(dot(N, H_mid), 0.0), 12.0) * 8.0;
                vec3 sparkleColor = (vTexLayer == 3u) ? vec3(1.0, 0.7, 1.0) : vec3(0.9, 0.95, 1.0);
                Lo += sparkleColor * sparkleGlint * radiance * NdotL;

                // Subsurface Translucency Glow
                float sss = pow(clamp(dot(V, -lightDir) * 0.5 + 0.5, 0.0, 1.0), 3.5);
                vec3 translucencyColor = (vTexLayer == 3u) ? vec3(0.65, 0.20, 0.85) : vec3(0.40, 0.75, 0.95);
                Lo += translucencyColor * sss * radiance * 0.9;
            } else {
                vec3 H = normalize(V + lightDir);
                float NDF = distribution_ggx(N, H, roughness);
                float G = geometry_smith(N, V, lightDir, roughness);
                vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

                vec3 kS = F;
                vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

                vec3 numerator = NDF * G * F;
                float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
                vec3 specular = numerator / denominator;

                Lo += (kD * albedo / PI + specular) * radiance * NdotL;
            }
        }
    }

    // 3. Dynamic Point Lights (Clustered Forward Lighting / Fallback)
    if (uClusteredLightingEnabled != 0) {
        // Compute 3D Cluster coordinates (16x9x24)
        uint clusterX = uint(clamp(gl_FragCoord.x / max(uScreenSize.x, 1.0) * float(CLUSTERS_X), 0.0, float(CLUSTERS_X - 1u)));
        uint clusterY = uint(clamp(gl_FragCoord.y / max(uScreenSize.y, 1.0) * float(CLUSTERS_Y), 0.0, float(CLUSTERS_Y - 1u)));

        vec4 viewPos = uView * vec4(vWorldPos, 1.0);
        float viewDepth = -viewPos.z;

        uint clusterZ = 0u;
        float zNear = max(uNear, 0.01);
        float zFar = max(uFar, zNear + 1.0);
        if (viewDepth > zNear) {
            float depthRatio = clamp(viewDepth / zNear, 1.0, zFar / zNear);
            float depthNorm = log(depthRatio) / log(zFar / zNear);
            clusterZ = uint(clamp(depthNorm * float(CLUSTERS_Z), 0.0, float(CLUSTERS_Z - 1u)));
        }

        uint clusterIdx = clusterX + clusterY * CLUSTERS_X + clusterZ * (CLUSTERS_X * CLUSTERS_Y);
        clusterIdx = min(clusterIdx, TOTAL_CLUSTERS - 1u);
        uint clusterLightCount = min(bClusters[clusterIdx].count, MAX_LIGHTS_PER_CLUSTER);

        for (uint i = 0u; i < clusterLightCount; ++i) {
            uint lightIdx = bClusters[clusterIdx].light_indices[i];
            if (lightIdx >= 256u) continue;
            vec3 lightPos = bPointLights[lightIdx].position_radius.xyz;
            float radius = bPointLights[lightIdx].position_radius.w;
            vec3 lightColor = bPointLights[lightIdx].color_intensity.rgb;
            float intensity = bPointLights[lightIdx].color_intensity.w;

            vec3 lightDir = normalize(lightPos - vWorldPos);
            float dist = distance(lightPos, vWorldPos);

            if (dist < radius) {
                float attenuation = clamp(1.0 - (dist / radius), 0.0, 1.0);
                attenuation *= attenuation;

                vec3 radiance = lightColor * intensity * attenuation;
                float NdotL = max(dot(N, lightDir), 0.0);
                vec3 H = normalize(V + lightDir);

                float NDF = distribution_ggx(N, H, roughness);
                float G = geometry_smith(N, V, lightDir, roughness);
                vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

                vec3 kS = F;
                vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

                vec3 specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001);
                Lo += (kD * albedo / PI + specular) * radiance * NdotL;

                if (isCrystal) {
                    float sss = pow(clamp(dot(V, -lightDir) * 0.5 + 0.5, 0.0, 1.0), 3.0);
                    vec3 translucencyColor = (vTexLayer == 3u) ? vec3(0.55, 0.15, 0.75) : vec3(0.35, 0.65, 0.90);
                    Lo += translucencyColor * sss * radiance * 0.7;
                }
            }
        }
    } else {
        // Fallback unclustered loop
        for (int i = 0; i < uNumPointLights && i < MAX_POINT_LIGHTS; ++i) {
            vec3 lightPos = uPointLights[i].position;
            vec3 lightDir = normalize(lightPos - vWorldPos);
            float dist = distance(lightPos, vWorldPos);
            float radius = uPointLights[i].radius;

            if (dist < radius) {
                float attenuation = clamp(1.0 - (dist / radius), 0.0, 1.0);
                attenuation *= attenuation;

                vec3 radiance = uPointLights[i].color * uPointLights[i].intensity * attenuation;
                float NdotL = max(dot(N, lightDir), 0.0);
                vec3 H = normalize(V + lightDir);

                float NDF = distribution_ggx(N, H, roughness);
                float G = geometry_smith(N, V, lightDir, roughness);
                vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

                vec3 kS = F;
                vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

                vec3 specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001);
                Lo += (kD * albedo / PI + specular) * radiance * NdotL;

                if (isCrystal) {
                    // Point light subsurface glow through crystal
                    float sss = pow(clamp(dot(V, -lightDir) * 0.5 + 0.5, 0.0, 1.0), 3.0);
                    vec3 translucencyColor = (vTexLayer == 3u) ? vec3(0.55, 0.15, 0.75) : vec3(0.35, 0.65, 0.90);
                    Lo += translucencyColor * sss * radiance * 0.7;
                }
            }
        }
    }

    // 4. Dynamic Subterranean Ambient Lighting modulated by Depth & Baked AO
    float depthFactor = clamp((vWorldPos.y - 2.0) / 22.0, 0.12, 0.85);

    vec3 baseSubterraneanAmbient = vec3(0.012, 0.016, 0.022); // Cold Slate
    if (uSector == 2) {
        baseSubterraneanAmbient = vec3(0.022, 0.012, 0.007); // Magma Core
    } else if (uSector >= 3) {
        baseSubterraneanAmbient = vec3(0.007, 0.020, 0.011); // Toxic Vault
    }

    // Unpack Baked Vertex AO
    const float aoTable[4] = float[](1.0, 0.72, 0.45, 0.20);
    uint aoIndex = clamp(vAOIndex, 0u, 3u);
    vec3 ambient = baseSubterraneanAmbient * depthFactor * albedo * aoTable[aoIndex];

    vec3 finalColor = ambient + Lo + emissive;

    // 5. Seismic Sonar Pulse Overlay (Surveying skill feedback)
    if (vSonarIntensity > 0.01) {
        vec3 sonarColor = vec3(0.1, 0.8, 1.0); // Holographic cyan scan
        if (vTexLayer == 3u) sonarColor = vec3(0.9, 0.2, 1.0);
        if (vTexLayer == 7u) sonarColor = vec3(0.2, 1.0, 0.4);

        finalColor += sonarColor * vSonarIntensity * 2.0;
    }

    FragColor = vec4(finalColor, 1.0);

    // 6. Thresholded Bright Extraction for HDR Bloom
    float luminance = dot(finalColor, vec3(0.2126, 0.7152, 0.0722));
    if (luminance > 1.2 || length(emissive) > 0.5 || vSonarIntensity > 0.5) {
        BrightColor = vec4(finalColor, 1.0);
    } else {
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
