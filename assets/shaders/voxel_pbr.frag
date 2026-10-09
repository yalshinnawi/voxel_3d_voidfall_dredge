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
const uint MAT_COOLANT             = 11u;
const uint MAT_ACID                = 6u;
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
        case MAT_BULKHEAD: return vec3(0.35, 0.38, 0.42); // Industrial Bulkhead
        case MAT_VAULT_DOOR: return vec3(0.55, 0.45, 0.20); // Reinforced Vault Door
        case MAT_THERMITE_SLAG: return vec3(0.85, 0.35, 0.05); // Thermite Slag
        case MAT_RADIOACTIVE: return vec3(0.15, 0.65, 0.25); // Radioactive Ore
        case MAT_STONE: return vec3(0.06, 0.06, 0.08); // Dredge Bedrock
        case MAT_WATER: return vec3(0.12, 0.60, 0.85); // Crystal Aquifer Water
        case MAT_FLORA: return vec3(0.20, 0.85, 0.35); // Bioluminescent Flora
        case MAT_PRISMATIC_CRYSTAL: return vec3(0.82, 0.38, 0.95); // Prismatic Crystal
        case MAT_OBSIDIAN: return vec3(0.22, 0.05, 0.08); // Crystalline Obsidian Spikes
        default: return vec3(0.30, 0.30, 0.30);
    }
}

vec2 get_material_rough_metal(uint layer) {
    switch (layer) {
        case MAT_VOIDITE: return vec2(0.15, 0.20); // Voidite crystal (polished gemstone)
        case MAT_BULKHEAD: return vec2(0.45, 0.88); // Industrial bulkhead (metallic satin)
        case MAT_VAULT_DOOR: return vec2(0.25, 0.92); // Vault door (polished metal)
        case MAT_THERMITE_SLAG: return vec2(0.65, 0.30); // Molten slag
        case MAT_WATER: return vec2(0.06, 0.15); // Water / puddle (fluid polish)
        case MAT_FLORA: return vec2(0.65, 0.05); // Flora (soft moss)
        case MAT_PRISMATIC_CRYSTAL: return vec2(0.15, 0.35); // Prismatic crystal (faceted diamond)
        case MAT_OBSIDIAN: return vec2(0.15, 0.45); // Obsidian spikes (glossy mineral glass)
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

    vec2 baseUV = fract(vUV);
    // World-Space UV Mapping for Liquids:
    // For liquid top surfaces, calculate texture UVs using world-space horizontal coordinates rather than quad-local [0, 1] UVs
    if (isLiquid && geoN.y > 0.4) {
        vec2 fluidUV = v_FragPos.xz * 0.25; // Continuous world scale across all blocks
        baseUV = fluidUV;
    }

    // Dynamic material UV animations
    if (vTexLayer == MAT_ACID || vTexLayer == MAT_THERMITE_SLAG) { // Thermite Slag molten magma / acid convection flow
        vec2 flowOffset = vec2(sin(uTime * 0.35 + vWorldPos.x * 0.15), cos(uTime * 0.28 + vWorldPos.z * 0.15)) * 0.05;
        baseUV = baseUV + flowOffset;
    } else if (vTexLayer == MAT_WATER || vTexLayer == MAT_COOLANT) { // Crystal Aquifer water ripples
        vec2 waveOffset = vec2(sin(uTime * 0.60 + vWorldPos.x * 0.35), cos(uTime * 0.50 + vWorldPos.z * 0.35)) * 0.03;
        baseUV = baseUV + waveOffset;
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
        float currentHeight = texture(uNormalArray, vec3(fract(currUV), float(vTexLayer))).a;
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
        // Completely bypass raymarching loops. Sample base UVs directly for normal and roughness calculations to protect GPU fillrate across open caverns.
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

    // Apply drilling / impact fracture overlay
    if (vDamage > 0.05) {
        albedo = mix(albedo, vec3(0.08, 0.07, 0.06), vDamage * 0.7);
    }

    // Zero-Cost Procedural Edge Chamfering:
    // To eliminate the razor-sharp digital 90° look on cubic faces without adding extra geometry:
    vec3 normal = N;
    vec2 v_TexCoords = fract(vUV);
    vec2 edgeDist = min(v_TexCoords, 1.0 - v_TexCoords);
    float minEdge = min(edgeDist.x, edgeDist.y);
    vec3 v_Normal = vNormal;
    if (isLiquid && abs(geoN.y) > 0.4) {
        v_Normal = geoN;
    }

    // Only bevel solid rock/metal edges; leave liquid surfaces perfectly smooth
    if (!isLiquid && minEdge < 0.045) {
        float bevel = smoothstep(0.0, 0.045, minEdge);
        vec3 bevelNormal = normalize(v_Normal + dFdx(v_FragPos) * 0.15 + dFdy(v_FragPos) * 0.15);
        normal = normalize(mix(bevelNormal, normal, bevel));
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

    // World-Space Continuous Flow-Aligned UV Projection & Translucent Specular for Liquids:
    if (isLiquid) {
        vec2 flowVector = (length(v_FlowDir) > 0.01) ? v_FlowDir : vec2(0.1, 0.05);
        vec2 flowOffset = flowVector * (uTime * 0.22);
        vec2 fluidUV = v_FragPos.xz * 0.35;

        if (uUseTextureArray == 1) {
            vec4 n1 = texture(uNormalArray, vec3(fluidUV - flowOffset, float(vTexLayer)));
            vec4 n2 = texture(uNormalArray, vec3(fluidUV * 1.25 - flowOffset * 1.35, float(vTexLayer)));
            normal = normalize(v_Normal + (n1.rgb + n2.rgb - 1.0) * 0.25);
        } else {
            float wave = sin((fluidUV.x - flowOffset.x) * 6.28 + uTime * 2.0) * cos((fluidUV.y - flowOffset.y) * 6.28 + uTime * 1.5) * 0.15;
            normal = normalize(v_Normal + vec3(wave, 0.0, wave));
        }

        if (v_MaterialID == MAT_ACID) {
            albedo = vec3(0.2, 0.8, 0.1);
        } else if (v_MaterialID == MAT_THERMITE_SLAG || v_MaterialID == MAT_LAVA) {
            albedo = vec3(0.85, 0.35, 0.05);
        } else {
            albedo = vec3(0.04, 0.55, 0.70); // High-visibility subterranean cyan
        }
        alpha = 0.75;
        roughness = 0.04;
        normal = normalize(v_Normal);
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
        roughness = max(roughness, 0.75); // Natural rock should be diffuse and matte
    } else if (matType == MAT_BULKHEAD) {
        roughness = max(roughness, 0.45); // Industrial plating: dull satin sheen, not chrome
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
                vec3 specularColor = (vec3(NDF_R, NDF_G, NDF_B) * G * F) / denom;

                vec3 kS = F;
                vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

                // Attenuate specular intensity specifically for the suit headlamp
                vec3 headlampSpecular = specularColor * 0.25; // Scale down headlamp specular contribution
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

                // Attenuate specular intensity specifically for the suit headlamp
                vec3 headlampSpecular = specularColor * 0.25; // Scale down headlamp specular contribution
                Lo += (kD * albedo / PI + headlampSpecular) * radiance * NdotL;
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
