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
in float vIsVerticalFlow;
in vec2 v_FlowDir;

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
uniform float uHeadlampRange;

// Headlamp Shadow Mapping
uniform sampler2D uHeadlampShadowMap;
uniform int uHeadlampShadowsEnabled;
uniform mat4 uHeadlampLightSpaceMatrix;

float calculate_headlamp_shadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    if (uHeadlampShadowsEnabled == 0) return 1.0;
    
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }
    
    float bias = max(0.0040 * (1.0 - dot(normal, lightDir)), 0.0015);
    float currentDepth = projCoords.z;
    
    const vec2 poissonDisk[4] = vec2[](
        vec2(-0.38,  0.35),
        vec2( 0.35,  0.32),
        vec2(-0.32, -0.38),
        vec2( 0.38, -0.35)
    );
    
    vec2 texelSize = 1.0 / vec2(1024.0, 1024.0);
    float shadow = 0.0;
    for (int i = 0; i < 4; ++i) {
        float pcfDepth = texture(uHeadlampShadowMap, projCoords.xy + poissonDisk[i] * texelSize * 1.5).r;
        shadow += (currentDepth - bias > pcfDepth) ? 0.0 : 1.0;
    }
    return shadow * 0.25;
}

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
uniform float uDarknessFactor;

// Standard Material Tiers
const uint MAT_AIR                 = 0u;
const uint MAT_GRANITE             = 1u;
const uint MAT_FRACTURED_GRANITE   = 1u;
const uint MAT_BASALT              = 2u;
const uint MAT_VOLCANIC_BASALT     = 2u;
const uint MAT_VOIDITE             = 3u;
const uint MAT_VOIDITE_CRYSTAL     = 3u;
const uint MAT_BULKHEAD            = 4u;
const uint MAT_INDUSTRIAL_BULKHEAD = 4u;
const uint MAT_VAULT_DOOR          = 5u;
const uint MAT_REINFORCED_VAULT_DOOR = 5u;
const uint MAT_THERMITE_SLAG       = 6u;
const uint MAT_RADIOACTIVE         = 7u;
const uint MAT_RADIOACTIVE_ORE     = 7u;
const uint MAT_STONE               = 8u; // Dredge Bedrock / Precursor Stone
const uint MAT_DREDGE_BEDROCK      = 8u;
const uint MAT_WATER               = 11u;
const uint MAT_CRYSTAL_AQUIFER     = 11u;
const uint MAT_COOLANT             = 16u;
const uint MAT_ACID                = 15u;
const uint MAT_LAVA                = 6u;
const uint MAT_MOLTEN_MAGMA        = 6u;
const uint MAT_FLORA               = 12u;
const uint MAT_BIOLUMINESCENT_FLORA = 12u;
const uint MAT_PRISMATIC_CRYSTAL   = 13u;
const uint MAT_PRECURSOR_GLASS     = 13u;
const uint MAT_OBSIDIAN            = 14u;
const uint MAT_OBSIDIAN_SPIKES     = 14u;

// Material tier base colors fallback
vec3 get_material_albedo(uint layer) {
    switch (layer) {
        case MAT_GRANITE: return vec3(0.24, 0.22, 0.21); // Fractured Granite
        case MAT_BASALT: return vec3(0.12, 0.12, 0.14); // Volcanic Basalt
        case MAT_VOIDITE: return vec3(0.48, 0.12, 0.72); // Voidite Crystal
        case MAT_BULKHEAD: return vec3(0.26, 0.28, 0.32); // Industrial Bulkhead (charcoal gunmetal)
        case MAT_VAULT_DOOR: return vec3(0.55, 0.45, 0.20); // Reinforced Vault Door
        case MAT_THERMITE_SLAG: return vec3(0.85, 0.35, 0.05); // Thermite Slag
        case MAT_RADIOACTIVE: return vec3(0.15, 0.65, 0.25); // Radioactive Ore
        case MAT_STONE: return vec3(0.06, 0.06, 0.08); // Dredge Bedrock
        case MAT_WATER: return vec3(0.12, 0.60, 0.85); // Crystal Aquifer Water
        case MAT_FLORA: return vec3(0.20, 0.85, 0.35); // Bioluminescent Flora
        case MAT_PRISMATIC_CRYSTAL: return vec3(0.25, 0.92, 0.96); // Prismatic Crystal (Radiant Diamond Cyan, distinctly non-objective)
        case MAT_OBSIDIAN: return vec3(0.22, 0.05, 0.08); // Crystalline Obsidian Spikes
        case MAT_ACID: return vec3(0.18, 0.75, 0.10); // Bubbling Neon Green
        case MAT_COOLANT: return vec3(0.70, 0.85, 0.95); // Frosted Cryo White/Pale Blue
        default: return vec3(0.30, 0.30, 0.30);
    }
}

vec2 get_material_rough_metal(uint layer) {
    switch (layer) {
        case MAT_VOIDITE: return vec2(0.15, 0.20); // Voidite crystal (polished gemstone)
        case MAT_BULKHEAD: return vec2(0.70, 0.82); // Industrial bulkhead (matte alloy satin)
        case MAT_VAULT_DOOR: return vec2(0.25, 0.92); // Vault door (polished metal)
        case MAT_THERMITE_SLAG: return vec2(0.65, 0.30); // Molten slag
        case MAT_WATER: return vec2(0.06, 0.15); // Water / puddle (fluid polish)
        case MAT_FLORA: return vec2(0.65, 0.05); // Flora (soft moss)
        case MAT_PRISMATIC_CRYSTAL: return vec2(0.15, 0.35); // Prismatic crystal (faceted diamond)
        case MAT_OBSIDIAN: return vec2(0.15, 0.45); // Obsidian spikes (glossy mineral glass)
        case MAT_ACID: return vec2(0.05, 0.05); // Acid
        case MAT_COOLANT: return vec2(0.01, 0.05); // Coolant
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

    vec3 v_FragPos = vWorldPos;
    uint v_MaterialID = vTexLayer;
    bool isLiquid = (v_MaterialID == MAT_WATER || v_MaterialID == MAT_ACID || v_MaterialID == MAT_COOLANT || v_MaterialID == MAT_LAVA || v_MaterialID == MAT_THERMITE_SLAG);

    if (isLiquid && abs(geoN.y) > 0.4) {
        vec3 triN = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
        if (dot(triN, geoN) < 0.0) triN = -triN;
        geoN = triN;
    }

    vec2 baseUV = vUV;
    bool isStepCascade = (vIsVerticalFlow > 0.5) || (isLiquid && abs(geoN.y) <= 0.35);

    // World-Space UV Mapping & Streaming Flow Motion for Liquids:
    if (isLiquid) {
        vec2 fluidUV;
        if (isStepCascade) {
            float lateralCoord = (abs(vNormal.x) > 0.5) ? v_FragPos.z : v_FragPos.x;
            fluidUV = vec2(lateralCoord, v_FragPos.y) * 0.25;
        } else {
            fluidUV = v_FragPos.xz * 0.25;
        }

        bool isHorizontalFlow = (length(v_FlowDir) > 0.01);
        vec2 flowOffset = vec2(0.0);

        if (vTexLayer == MAT_WATER || vTexLayer == MAT_CRYSTAL_AQUIFER) {
            if (isStepCascade) {
                flowOffset = vec2(sin(uTime * 2.8 + v_FragPos.y * 1.8) * 0.02, -uTime * 0.70);
            } else if (isHorizontalFlow) {
                vec2 dir = normalize(v_FlowDir);
                vec2 crossDir = vec2(-dir.y, dir.x);
                flowOffset = -dir * (uTime * 0.38) + crossDir * sin(uTime * 2.5 + dot(v_FragPos.xz, vec2(1.4, 1.1))) * 0.025;
            } else {
                vec2 ripple = vec2(sin(uTime * 0.70 + v_FragPos.x * 0.38 + v_FragPos.z * 0.22), cos(uTime * 0.60 + v_FragPos.z * 0.38 - v_FragPos.x * 0.22)) * 0.035;
                flowOffset = ripple + vec2(0.02, 0.01) * (uTime * 0.05);
            }
        } else if (vTexLayer == MAT_LAVA || vTexLayer == MAT_THERMITE_SLAG) {
            if (isStepCascade) {
                flowOffset = vec2(sin(uTime * 1.5 + v_FragPos.y) * 0.015, -uTime * 0.18);
            } else if (isHorizontalFlow) {
                vec2 dir = normalize(v_FlowDir);
                vec2 crossDir = vec2(-dir.y, dir.x);
                flowOffset = -dir * (uTime * 0.10) + crossDir * sin(uTime * 1.8 + dot(v_FragPos.xz, vec2(0.8, 0.6))) * 0.015;
            } else {
                flowOffset = vec2(sin(uTime * 0.30 + v_FragPos.x * 0.15), cos(uTime * 0.25 + v_FragPos.z * 0.15)) * 0.03 + vec2(0.008, 0.004) * (uTime * 0.02);
            }
        } else if (vTexLayer == MAT_ACID) {
            if (isStepCascade) {
                flowOffset = vec2(sin(uTime * 2.2 + v_FragPos.y * 1.5) * 0.02, -uTime * 0.50);
            } else if (isHorizontalFlow) {
                vec2 dir = normalize(v_FlowDir);
                vec2 crossDir = vec2(-dir.y, dir.x);
                flowOffset = -dir * (uTime * 0.30) + crossDir * sin(uTime * 2.2 + dot(v_FragPos.xz, vec2(1.2, 0.8))) * 0.025;
            } else {
                flowOffset = vec2(sin(uTime * 0.55 + v_FragPos.x * 0.30), cos(uTime * 0.45 + v_FragPos.z * 0.30)) * 0.035 + vec2(0.015, 0.015) * (uTime * 0.04);
            }
        } else if (vTexLayer == MAT_COOLANT) {
            if (isStepCascade) {
                flowOffset = vec2(sin(uTime * 2.0 + v_FragPos.y * 1.4) * 0.018, -uTime * 0.40);
            } else if (isHorizontalFlow) {
                vec2 dir = normalize(v_FlowDir);
                vec2 crossDir = vec2(-dir.y, dir.x);
                flowOffset = -dir * (uTime * 0.22) + crossDir * sin(uTime * 2.0 + dot(v_FragPos.xz, vec2(1.0, 0.9))) * 0.02;
            } else {
                flowOffset = vec2(sin(uTime * 0.45 + v_FragPos.x * 0.22), cos(uTime * 0.38 + v_FragPos.z * 0.22)) * 0.025 + vec2(0.01, 0.01) * (uTime * 0.03);
            }
        }

        baseUV = fluidUV + flowOffset;
    }

    // LOD Distance-Gated Parallax Occlusion Mapping (POM)
    vec3 u_CameraPos = uCameraPos;
    float camDist = length(v_FragPos - u_CameraPos);

    vec2 quadUV;
    if (!isLiquid && camDist < 7.0f) {
        // Fade displacement scale linearly to zero between 5.0m and 7.0m
        float pomFade = clamp((7.0 - camDist) / 2.0, 0.0, 1.0);
        float depthScale = 0.04 * pomFade;

        // Execute a 6-step POM raymarch using the height channel from GL_TEXTURE_2D_ARRAY
        const float numSteps = 6.0;
        float stepSize = 1.0 / numSteps;
        vec2 p = (V_tangent.xy / max(abs(V_tangent.z), 0.25)) * depthScale;
        vec2 deltaUV = p / numSteps;

        float currentLayerDepth = 0.0;
        vec2 currUV = baseUV;
        float currentHeight = texture(uNormalArray, vec3(currUV, float(vTexLayer))).a;
        float currentDepthMapValue = 1.0 - currentHeight;

        vec2 prevUV = currUV;
        float prevLayerDepth = currentLayerDepth;
        float prevDepthMapValue = currentDepthMapValue;

        for (int step = 0; step < 6; ++step) {
            if (currentLayerDepth >= currentDepthMapValue) {
                break;
            }
            prevUV = currUV;
            prevLayerDepth = currentLayerDepth;
            prevDepthMapValue = currentDepthMapValue;

            currUV -= deltaUV;
            currentLayerDepth += stepSize;
            currentHeight = texture(uNormalArray, vec3(currUV, float(vTexLayer))).a;
            currentDepthMapValue = 1.0 - currentHeight;
        }

        // Parallax Occlusion interpolation
        float afterDepth = currentLayerDepth - currentDepthMapValue;
        float beforeDepth = prevDepthMapValue - prevLayerDepth;
        float denom = afterDepth + beforeDepth;
        float weight = denom > 0.0001 ? clamp(beforeDepth / denom, 0.0, 1.0) : 0.0;
        quadUV = mix(prevUV, currUV, weight);
    } else {
        // Completely bypass raymarching loops. Sample base UVs directly for normal and roughness calculations to protect GPU fillrate across open caverns.
        quadUV = baseUV;
    }

    vec3 texCoord = vec3(quadUV, float(vTexLayer));

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
            vec2 interiorUV = quadUV - V_tangent.xy * 0.045;
            vec3 interiorTexCoord = vec3(interiorUV, float(vTexLayer));

            vec3 surfaceEmissive = texture(uEmissiveArray, texCoord).rgb;
            vec3 interiorEmissive = texture(uEmissiveArray, interiorTexCoord).rgb;

            // Blend surface facets with interior crystal nucleus - calibrated for radiant gemstone glow
            emissive = (surfaceEmissive * 0.55 + interiorEmissive * 0.45) * (vEmissive * 3.5 + 0.8);
        } else if (vTexLayer == 6u) {
            // Thermite Slag molten core radiant boost for genuine volcanic bloom
            emissive = texture(uEmissiveArray, texCoord).rgb * (vEmissive * 5.0 + 1.5);
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

    // Apply drilling / impact fracture overlay (only on solid structures)
    if (!isLiquid && vDamage > 0.05) {
        albedo = mix(albedo, vec3(0.08, 0.07, 0.06), vDamage * 0.7);
    }

    // Seamless normal mapping: adjacent blocks of the same type mesh together smoothly without artificial block-edge grooves
    vec3 normal = N;
    vec3 v_Normal = (isLiquid && abs(geoN.y) > 0.4) ? geoN : vNormal;

    // Procedural Screen-Space Edge Chamfering: soften razor-sharp 90 deg block borders without adding geometry
    vec2 v_TexCoords = fract(quadUV);
    vec2 edgeDist = min(v_TexCoords, 1.0 - v_TexCoords);
    float minEdge = min(edgeDist.x, edgeDist.y);
    if (minEdge < 0.045 && vTexLayer != MAT_WATER && !isLiquid) {
        float bevel = smoothstep(0.0, 0.045, minEdge);
        vec3 bevelNormal = normalize(v_Normal + dFdx(v_FragPos) * 0.16 + dFdy(v_FragPos) * 0.16);
        normal = normalize(mix(bevelNormal, normal, bevel));
    }

    // World-space triplanar mapping with low-frequency 3D strata noise on natural stone (MAT_GRANITE, MAT_BASALT)
    if (uUseTextureArray == 1 && (vTexLayer == MAT_GRANITE || vTexLayer == MAT_BASALT)) {
        vec3 blendWeights = pow(abs(v_Normal), vec3(4.0));
        blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z + 0.0001);

        float strataNoise = sin(v_FragPos.y * 0.45 + v_FragPos.x * 0.15) * cos(v_FragPos.z * 0.35) * 0.06;
        vec2 uvX = v_FragPos.zy * 0.25 + strataNoise;
        vec2 uvY = v_FragPos.xz * 0.25 + strataNoise;
        vec2 uvZ = v_FragPos.xy * 0.25 + strataNoise;

        float layerF = float(vTexLayer);
        vec3 colX = texture(uAlbedoArray, vec3(uvX, layerF)).rgb;
        vec3 colY = texture(uAlbedoArray, vec3(uvY, layerF)).rgb;
        vec3 colZ = texture(uAlbedoArray, vec3(uvZ, layerF)).rgb;
        albedo = colX * blendWeights.x + colY * blendWeights.y + colZ * blendWeights.z;

        vec2 rmX = texture(uRoughMetalArray, vec3(uvX, layerF)).rg;
        vec2 rmY = texture(uRoughMetalArray, vec3(uvY, layerF)).rg;
        vec2 rmZ = texture(uRoughMetalArray, vec3(uvZ, layerF)).rg;
        roughMetal = rmX * blendWeights.x + rmY * blendWeights.y + rmZ * blendWeights.z;

        vec3 nX = texture(uNormalArray, vec3(uvX, layerF)).xyz * 2.0 - 1.0;
        vec3 nY = texture(uNormalArray, vec3(uvY, layerF)).xyz * 2.0 - 1.0;
        vec3 nZ = texture(uNormalArray, vec3(uvZ, layerF)).xyz * 2.0 - 1.0;
        vec3 triNormal = normalize(nX * blendWeights.x + nY * blendWeights.y + nZ * blendWeights.z);
        normal = normalize(mix(normal, normalize(TBN * triNormal), 0.75));
    }

    // Distance-Gated Surface Relief & Normal Micro-Detail:
    // For fragments close to the camera (< 7.0m), apply continuous 3D noise displacement to the surface normal
    if (!isLiquid && length(v_FragPos - uCameraPos) < 7.0) {
        float grain = sin(v_FragPos.x * 12.0) * cos(v_FragPos.y * 12.0) * sin(v_FragPos.z * 12.0);
        normal = normalize(normal + grain * 0.06);
    }

    float roughness = clamp(roughMetal.r, 0.04, 0.99);
    float metallic  = clamp(roughMetal.g, 0.0, 1.0);
    float alpha = 1.0;

    // World-Space Continuous Flow-Aligned Shading, Multi-Layer Ripple Normals & Specular for Liquids:
    if (isLiquid) {
        // Smooth rounded transition normal over step edges:
        vec3 baseNormal = v_Normal;
        if (isStepCascade) {
            // Blend vertical drop face normal with +Y upward normal to round over the edge
            baseNormal = normalize(mix(v_Normal, vec3(0.0, 1.0, 0.0), 0.50));
        } else if (length(v_FlowDir) > 0.01) {
            // Smoothly tilt horizontal surface normal towards the direction of flow
            baseNormal = normalize(v_Normal + vec3(v_FlowDir.x, 0.0, v_FlowDir.y) * 0.20);
        }

        // Multi-Layer Flowing Ripple Normal Perturbation
        if (uUseTextureArray == 1) {
            vec3 n1 = texture(uNormalArray, texCoord).rgb * 2.0 - 1.0;
            // Secondary ripple wave layer counter-drifting for realistic surface wave interference
            vec2 rippleLayer2 = quadUV * 1.6 + vec2(uTime * 0.08, -uTime * 0.06);
            vec3 n2 = texture(uNormalArray, vec3(fract(rippleLayer2), float(vTexLayer))).rgb * 2.0 - 1.0;
            vec3 waveN = normalize(n1 + n2 * 0.6);
            normal = normalize(baseNormal + vec3(waveN.x, 0.0, waveN.y) * 0.26);
        } else {
            float wave = sin(quadUV.x * 20.0 + uTime * 2.5) * cos(quadUV.y * 20.0 + uTime * 2.0) * 0.18;
            normal = normalize(baseNormal + vec3(wave, 0.0, wave));
        }

        if (v_MaterialID == MAT_WATER) {
            // Preserve sampled caustic texture array colors so still pools and flowing streams stay 100% consistent
            if (uUseTextureArray == 1) {
                vec3 sampledAlbedo = texture(uAlbedoArray, texCoord).rgb;
                // Subtle depth mix with translucent subterranean cyan while preserving caustic webs
                albedo = mix(sampledAlbedo, vec3(0.04, 0.44, 0.65), 0.22);
                emissive = texture(uEmissiveArray, texCoord).rgb * 0.40;
            } else {
                albedo = vec3(0.08, 0.48, 0.72);
                emissive = vec3(0.03, 0.20, 0.35);
            }
            alpha = 0.75;
            roughness = 0.12; // Wet aquifer floor puddles at R = 0.12
            metallic = 0.02;
        } 
        else if (v_MaterialID == MAT_LAVA || v_MaterialID == MAT_THERMITE_SLAG) {
            if (uUseTextureArray == 1) {
                vec3 sampledAlbedo = texture(uAlbedoArray, texCoord).rgb;
                albedo = sampledAlbedo;
                emissive = texture(uEmissiveArray, texCoord).rgb * 3.2 + sampledAlbedo * 1.0;
            } else {
                albedo = vec3(0.85, 0.32, 0.05);
                emissive = vec3(1.2, 0.45, 0.08);
            }
            alpha = 1.0; // Lava is completely opaque
            roughness = 0.62;
            metallic = 0.05;
            normal = baseNormal; // Viscous lava maintains smooth bulk surface
        }
        else if (v_MaterialID == MAT_ACID) {
            if (uUseTextureArray == 1) {
                vec3 sampledAlbedo = texture(uAlbedoArray, texCoord).rgb;
                albedo = mix(sampledAlbedo, vec3(0.16, 0.78, 0.12), 0.15);
                emissive = texture(uEmissiveArray, texCoord).rgb * 1.6 + vec3(0.04, 0.18, 0.03);
            } else {
                albedo = vec3(0.18, 0.80, 0.12);
                emissive = vec3(0.10, 0.35, 0.05);
            }
            alpha = 0.82;
            roughness = 0.04;
            metallic = 0.05;
        }
        else if (v_MaterialID == MAT_COOLANT) {
            if (uUseTextureArray == 1) {
                vec3 sampledAlbedo = texture(uAlbedoArray, texCoord).rgb;
                albedo = mix(sampledAlbedo, vec3(0.70, 0.86, 0.96), 0.18);
                emissive = texture(uEmissiveArray, texCoord).rgb * 0.90;
            } else {
                albedo = vec3(0.72, 0.88, 0.96);
                emissive = vec3(0.15, 0.30, 0.45);
            }
            alpha = 0.65;
            roughness = 0.02;
            metallic = 0.10;
        }
    }
    N = normal;

    uint matType = vTexLayer;

    // Reserve low roughness values (< 0.20) strictly for wet puddle pools and polished Precursor glass
    bool isPuddle = (matType == MAT_WATER || isLiquid);
    bool isPrecursorGlass = (matType == MAT_PRISMATIC_CRYSTAL || matType == MAT_PRECURSOR_GLASS || matType == MAT_VOIDITE || matType == MAT_OBSIDIAN);
    if (!isPuddle && !isPrecursorGlass) {
        roughness = max(roughness, 0.20);
    }

    // Clamp roughness so stone and unpolished metal remain matte
    if (matType == MAT_STONE || matType == MAT_BASALT || matType == MAT_GRANITE) {
        roughness = max(roughness, 0.65); // Natural rock minimum roughness clamped to 0.65 to eliminate plastic sheen
    } else if (matType == MAT_BULKHEAD) {
        roughness = max(roughness, 0.68); // Industrial plating: rugged matte industrial alloy
        metallic = max(metallic, 0.90);  // Titanium bulkheads at M = 0.90
    }

    // Baked 2-Bit Vertex AO & Specular Occlusion factors
    const float aoTable[4] = float[](1.0, 0.72, 0.45, 0.20);
    uint aoIndex = clamp(vAOIndex, 0u, 3u);
    float ao = aoTable[aoIndex];
    float specAO = ao * ao; // Specular AO to eliminate shiny glare inside dark crevices

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    // 2. Evaluate Headlamp Spotlight (PBR with normal mapping & crystal optical dispersion)
    {
        vec3 lightDir = normalize(uHeadlampPos - vWorldPos);
        float dist = distance(uHeadlampPos, vWorldPos);
        float maxRange = uHeadlampRange > 0.0 ? uHeadlampRange : 22.0;

        if (dist < maxRange) {
            // Tighter Beam: Quadratic distance attenuation clamped at maxRange (22m)
            float rangeFactor = clamp(1.0 - (dist / maxRange), 0.0, 1.0);
            float attenuation = (rangeFactor * rangeFactor) / (dist * dist + 1.0);

            // Smooth spotlight cone falloff between outer and inner cutoff
            float theta = dot(lightDir, normalize(-uHeadlampDir));
            float spotFactor = smoothstep(uHeadlampOuterCutoff, uHeadlampInnerCutoff, theta);

            if (spotFactor > 0.0) {
                // Projected optical lens ring pattern
                float lensAngle = acos(clamp(theta, -1.0, 1.0));
                float lensRings = 1.0 + 0.08 * sin(lensAngle * 95.0) * smoothstep(0.0, 0.25, lensAngle);
                spotFactor *= lensRings;

                vec3 radiance = uHeadlampColor * uHeadlampIntensity * attenuation * spotFactor;
                vec4 fragPosLightSpace = uHeadlampLightSpaceMatrix * vec4(vWorldPos, 1.0);
                float shadowFactor = calculate_headlamp_shadow(fragPosLightSpace, N, lightDir);
                radiance *= shadowFactor;
                float NdotL = max(dot(N, lightDir), 0.0);

                // Cavern Wall Glare Reduction:
                // Lower specular factor on cavern walls (granite, basalt, bedrock) and wet/flat rock surfaces
                // to eliminate blinding white hotspots from the collinear headlamp beam.
                float headlampSpecFactor = 0.04; // Drastically lower specular factor on cavern walls & flat rock
                if (isCrystal) {
                    headlampSpecFactor = 0.18; // Preserves glistening chromatic facets on precious gemstones
                } else if (matType == MAT_BULKHEAD) {
                    headlampSpecFactor = 0.05; // Controlled matte dispersion on heavy industrial bulkhead metal
                } else if (matType == MAT_VAULT_DOOR) {
                    headlampSpecFactor = 0.10; // Controlled satin sheen on reinforced vault door
                } else if (isPuddle || isLiquid) {
                    headlampSpecFactor = 0.05; // Dampened sheen on wet pools to stop blinding reflection
                }

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
                    vec3 specularColor = (vec3(NDF_R, NDF_G, NDF_B) * G * F) / denom;

                    vec3 kS = F;
                    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

                    // Attenuate specular intensity specifically for the suit headlamp and multiply by Specular AO
                    vec3 headlampSpecular = min(specularColor * headlampSpecFactor * specAO, vec3(0.40));
                    Lo += (kD * albedo / PI + headlampSpecular) * radiance * NdotL;

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
                    vec3 specularColor = numerator / denominator;

                    // Attenuate specular intensity specifically for the suit headlamp and multiply by Specular AO
                    vec3 headlampSpecular = min(specularColor * headlampSpecFactor * specAO, vec3(0.30));
                    Lo += (kD * albedo / PI + headlampSpecular) * radiance * NdotL;
                }
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

                vec3 specular = ((NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001)) * specAO;
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

                vec3 specular = ((NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001)) * specAO;
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

    // 4. Dynamic Subterranean Ambient Lighting modulated by Depth, Normal Curvature & Baked AO
    float depthFactor = clamp((vWorldPos.y - 1.0) / 24.0, 0.35, 1.0);

    // Hemispherical ambient normal modulation: upward-facing surfaces catch subtle cavern sky/spore diffuse,
    // side walls catch balanced fill, underhangs receive moody contact occlusion.
    float hemiFactor = clamp(dot(N, vec3(0.0, 1.0, 0.0)) * 0.35 + 0.65, 0.45, 1.0);

    // Sector-specific subterranean ambient palette (richer atmospheric floor reveals cave architecture)
    vec3 baseSubterraneanAmbient = vec3(0.042, 0.056, 0.072); // Sector 1: Cold slate with phosphorescent cyan undertone
    if (uSector == 2) {
        baseSubterraneanAmbient = vec3(0.065, 0.038, 0.024); // Sector 2: Volcanic magma core warmth
    } else if (uSector >= 3) {
        baseSubterraneanAmbient = vec3(0.035, 0.062, 0.052); // Sector 3+: Toxic vault phosphorescent viridian / void
    }

    // Apply Baked Vertex AO & lower cavern ambient floor to 0.08 in pitch-black dark zones
    float darknessMult = clamp(1.0 - uDarknessFactor * 0.92, 0.08, 1.0);
    vec3 ambient = baseSubterraneanAmbient * depthFactor * hemiFactor * albedo * ao * darknessMult;

    vec3 finalColor = ambient + Lo + emissive;

    // 5. Seismic Sonar Pulse Overlay (Surveying skill feedback)
    if (vSonarIntensity > 0.01) {
        vec3 sonarColor = vec3(0.1, 0.8, 1.0); // Holographic cyan scan
        if (vTexLayer == 3u) sonarColor = vec3(0.9, 0.2, 1.0);
        if (vTexLayer == 7u) sonarColor = vec3(0.2, 1.0, 0.4);

        finalColor += sonarColor * vSonarIntensity * 2.0;
    }

    FragColor = vec4(finalColor, alpha);

    // 6. Thresholded Bright Extraction for HDR Bloom
    // Bloom luminance cutoff threshold raised from 1.0 to 2.0 (1.8-2.2 range).
    // Prevents standard specular reflections from entering the bloom blur:
    // only genuine emissive elements (Voidite crystals, molten slag, flare cores) cast bloom.
    float luminance = dot(finalColor, vec3(0.2126, 0.7152, 0.0722));
    float emissivePeak = max(emissive.r, max(emissive.g, emissive.b));
    const float BLOOM_CUTOFF = 2.0;

    if (luminance > BLOOM_CUTOFF || emissivePeak > 1.8) {
        BrightColor = vec4(finalColor, alpha);
    } else {
        BrightColor = vec4(0.0, 0.0, 0.0, alpha);
    }
}
