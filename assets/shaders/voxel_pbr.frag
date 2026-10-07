#version 430 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
flat in uint vTexLayer;
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

// Point Lights (Dropped flares, thermite burns, anomalies, beacon)
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

// Material tier base colors
vec3 get_material_albedo(uint layer) {
    switch (layer) {
        case 1u: return vec3(0.24, 0.22, 0.21); // Fractured Granite
        case 2u: return vec3(0.12, 0.12, 0.14); // Volcanic Basalt
        case 3u: return vec3(0.48, 0.12, 0.72); // Voidite Crystal
        case 4u: return vec3(0.35, 0.38, 0.42); // Industrial Bulkhead (Metallic alloy)
        case 5u: return vec3(0.55, 0.45, 0.20); // Reinforced Vault Door (Brass/Gold alloy)
        case 6u: return vec3(0.85, 0.35, 0.05); // Thermite Slag (Molten orange)
        case 7u: return vec3(0.15, 0.65, 0.25); // Radioactive Ore (Emerald toxic)
        case 8u: return vec3(0.06, 0.06, 0.08); // Dredge Bedrock
        case 11u: return vec3(0.12, 0.60, 0.85); // Crystal Aquifer Water (Cyan azure)
        case 12u: return vec3(0.20, 0.85, 0.35); // Bioluminescent Flora (Vibrant emerald)
        case 13u: return vec3(0.82, 0.38, 0.95); // Prismatic Crystal (Radiant diamond violet)
        case 14u: return vec3(0.22, 0.05, 0.08); // Crystalline Obsidian Spikes (Charcoal / Blood Crimson)
        default: return vec3(0.30, 0.30, 0.30);
    }
}

vec2 get_material_rough_metal(uint layer) {
    switch (layer) {
        case 3u: return vec2(0.15, 0.10); // Voidite crystal (smooth glass)
        case 4u: return vec2(0.35, 0.85); // Industrial bulkhead (metallic)
        case 5u: return vec2(0.25, 0.90); // Vault door (polished metal)
        case 6u: return vec2(0.70, 0.30); // Molten slag
        case 11u: return vec2(0.08, 0.10); // Water (ultra-smooth fluid)
        case 12u: return vec2(0.65, 0.05); // Flora (soft moss)
        case 13u: return vec2(0.12, 0.35); // Prismatic crystal (faceted gem)
        case 14u: return vec2(0.30, 0.45); // Obsidian spikes (glossy mineral needle)
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
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);

    // 1. Sample Texture Arrays or use procedural PBR defaults
    vec3 albedo;
    vec2 roughMetal;
    vec3 emissive = vec3(0.0);

    if (uUseTextureArray == 1) {
        vec3 texCoord = vec3(fract(vUV), float(vTexLayer));
        albedo = texture(uAlbedoArray, texCoord).rgb;
        roughMetal = texture(uRoughMetalArray, texCoord).rg;
        emissive = texture(uEmissiveArray, texCoord).rgb * vEmissive * 4.0;
    } else {
        albedo = get_material_albedo(vTexLayer);
        // Add subtle procedural rock noise
        vec3 worldCoord = vWorldPos * 2.0;
        float hash = fract(sin(dot(floor(worldCoord), vec3(12.9898, 78.233, 37.719))) * 43758.5453);
        albedo *= (0.85 + 0.3 * hash);

        roughMetal = get_material_rough_metal(vTexLayer);

        if (vTexLayer == 3u) { // Voidite crystal emissive glow
            float shimmer = 0.85 + 0.25 * sin(uTime * 3.5 + dot(vWorldPos, vec3(0.5, 0.7, 0.3)));
            emissive = vec3(0.38, 0.12, 0.65) * (vEmissive * 3.5 + 0.6) * shimmer;
        } else if (vTexLayer == 6u) { // Molten thermite burn
            float pulse = 0.9 + 0.2 * sin(uTime * 6.0 + dot(vWorldPos, vec3(1.2, 0.9, 0.4)));
            emissive = vec3(1.0, 0.4, 0.05) * (vEmissive * 8.0 + 2.0) * pulse;
        } else if (vTexLayer == 7u) { // Radioactive ore
            float pulse = 0.8 + 0.3 * sin(uTime * 2.0 + dot(vWorldPos, vec3(0.3, 1.1, 0.7)));
            emissive = vec3(0.1, 0.9, 0.3) * (vEmissive * 4.0 + 1.0) * pulse;
        } else if (vTexLayer == 11u) { // Crystal aquifer water
            float caustic = 0.85 + 0.25 * sin(uTime * 2.5 + dot(vWorldPos, vec3(1.2, 0.5, 0.9)));
            emissive = vec3(0.10, 0.65, 0.92) * (vEmissive * 2.5 + 0.3) * caustic;
        } else if (vTexLayer == 12u) { // Bioluminescent flora
            float pulse = 0.8 + 0.3 * sin(uTime * 1.8 + dot(vWorldPos, vec3(0.6, 1.2, 0.8)));
            emissive = vec3(0.15, 0.95, 0.38) * (vEmissive * 4.0 + 0.8) * pulse;
        } else if (vTexLayer == 13u) { // Prismatic crystal
            float sheen = 0.85 + 0.3 * sin(uTime * 2.8 + dot(vWorldPos, vec3(1.1, 1.1, 0.6)));
            emissive = vec3(0.85, 0.40, 0.98) * (vEmissive * 4.5 + 1.0) * sheen;
        } else if (vTexLayer == 14u) { // Crystalline obsidian spikes (Ominous crimson danger warning)
            float pulse = 0.85 + 0.25 * sin(uTime * 4.0 + dot(vWorldPos, vec3(1.0, 2.0, 1.0)));
            emissive = vec3(0.98, 0.12, 0.18) * (vEmissive * 3.5 + 0.8) * pulse;
        }
    }

    // Apply drilling / impact fracture overlay
    if (vDamage > 0.05) {
        albedo = mix(albedo, vec3(0.08, 0.07, 0.06), vDamage * 0.7);
    }

    float roughness = clamp(roughMetal.r, 0.05, 0.99);
    float metallic  = clamp(roughMetal.g, 0.0, 1.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    // 2. Evaluate Headlamp Spotlight (Forward Clustered Light)
    {
        vec3 lightDir = normalize(uHeadlampPos - vWorldPos);
        float dist = distance(uHeadlampPos, vWorldPos);
        float attenuation = 1.0 / (1.0 + 0.05 * dist + 0.01 * dist * dist);

        // Spotlight cone
        float theta = dot(lightDir, normalize(-uHeadlampDir));
        float epsilon = uHeadlampInnerCutoff - uHeadlampOuterCutoff;
        float spotFactor = clamp((theta - uHeadlampOuterCutoff) / max(epsilon, 0.001), 0.0, 1.0);

        if (spotFactor > 0.0) {
            vec3 H = normalize(V + lightDir);
            vec3 radiance = uHeadlampColor * uHeadlampIntensity * attenuation * spotFactor;

            float NDF = distribution_ggx(N, H, roughness);
            float G = geometry_smith(N, V, lightDir, roughness);
            vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, lightDir), 0.0) + 0.0001;
            vec3 specular = numerator / denominator;

            float NdotL = max(dot(N, lightDir), 0.0);
            Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        }
    }

    // 3. Dynamic Point Lights (Flares, Thermite, Anomalies, Beacon)
    for (int i = 0; i < uNumPointLights && i < MAX_POINT_LIGHTS; ++i) {
        vec3 lightPos = uPointLights[i].position;
        vec3 lightDir = normalize(lightPos - vWorldPos);
        float dist = distance(lightPos, vWorldPos);
        float radius = uPointLights[i].radius;

        if (dist < radius) {
            float attenuation = clamp(1.0 - (dist / radius), 0.0, 1.0);
            attenuation *= attenuation; // Smooth quadratic falloff

            vec3 radiance = uPointLights[i].color * uPointLights[i].intensity * attenuation;
            vec3 H = normalize(V + lightDir);

            float NDF = distribution_ggx(N, H, roughness);
            float G = geometry_smith(N, V, lightDir, roughness);
            vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

            vec3 specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, lightDir), 0.0) + 0.0001);
            float NdotL = max(dot(N, lightDir), 0.0);
            Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        }
    }

    // 4. Dynamic Subterranean Ambient Lighting modulated by Depth, Cavern Atmospheric Breathing & Baked AO
    // Vertical depth attenuation: deep bedrock fissures plunge into darkness, while high vaulted arches retain subtle subterranean silhouette
    float depthFactor = clamp((vWorldPos.y - 2.0) / 22.0, 0.12, 0.85);

    // Subterranean ambient lighting maintains stable, pleasant illumination without disorienting undulating waves
    float timePulse = 1.0;

    // High-contrast, dark subterranean cavern ambient lighting.
    // Unlit voxels plunge into pitch-black void, making flashlight beam and chemical flares indispensable.
    vec3 baseSubterraneanAmbient = vec3(0.012, 0.016, 0.022); // Sector 1: Cold Steel & Slate Deep Shadow
    if (uSector == 2) {
        baseSubterraneanAmbient = vec3(0.022, 0.012, 0.007); // Sector 2: Magma Core / Basalt (Warm Smoldering Ember)
    } else if (uSector >= 3) {
        baseSubterraneanAmbient = vec3(0.007, 0.020, 0.011); // Sector 3: Toxic Dredge Vaults (Faint Xenobiotic Emerald)
    }
    vec3 ambient = baseSubterraneanAmbient * depthFactor * timePulse * albedo * vAO;

    vec3 finalColor = ambient + Lo + emissive;

    // 5. Seismic Sonar Pulse Overlay (Surveying skill feedback)
    if (vSonarIntensity > 0.01) {
        vec3 sonarColor = vec3(0.1, 0.8, 1.0); // Holographic cyan scan
        // If high-value Voidite crystal or radioactive deposit, glow vibrant gold/magenta
        if (vTexLayer == 3u) sonarColor = vec3(0.9, 0.2, 1.0);
        if (vTexLayer == 7u) sonarColor = vec3(0.2, 1.0, 0.4);

        finalColor += sonarColor * vSonarIntensity * 2.0;
    }

    FragColor = vec4(finalColor, 1.0);

    // 6. Thresholded Bright Extraction for HDR Bloom
    // Emissive crystals, thermite burns, flares, and specular highlights
    float luminance = dot(finalColor, vec3(0.2126, 0.7152, 0.0722));
    if (luminance > 1.2 || length(emissive) > 0.5 || vSonarIntensity > 0.5) {
        BrightColor = vec4(finalColor, 1.0);
    } else {
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
