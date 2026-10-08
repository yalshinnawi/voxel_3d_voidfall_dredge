#version 430 core

in vec3 vFragPos;
in vec3 vNormal;
in vec4 vColor;
in vec4 vMaterial; // x=metallic, y=roughness, z=emissive, w=ao

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

uniform vec3 uCamPos;
uniform vec3 uHeadlampPos;
uniform vec3 uHeadlampDir;
uniform vec3 uHeadlampColor;
uniform float uHeadlampEnabled;
uniform float uTime;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vFragPos);

    float metallic = clamp(vMaterial.x, 0.0, 1.0);
    float roughness = clamp(vMaterial.y, 0.04, 1.0);
    float emissive = vMaterial.z;
    float ao = clamp(vMaterial.w, 0.15, 1.0);

    vec3 albedo = vColor.rgb;

    // Ambient cavern illumination
    vec3 ambient = albedo * 0.04 * ao;

    // Headlamp spotlight illumination
    vec3 Lo = vec3(0.0);
    if (uHeadlampEnabled > 0.5) {
        vec3 L = uHeadlampPos - vFragPos;
        float dist = length(L);
        L = normalize(L);

        float theta = dot(L, normalize(-uHeadlampDir));
        float innerCutoff = 0.951; // cos(18 deg)
        float outerCutoff = 0.848; // cos(32 deg)
        float epsilon = innerCutoff - outerCutoff;
        float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);

        if (spotIntensity > 0.0) {
            float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
            vec3 radiance = uHeadlampColor * 4.5 * attenuation * spotIntensity;

            // Cook-Torrance Specular + Lambertian Diffuse
            vec3 H = normalize(V + L);
            float NdotL = max(dot(N, L), 0.0);
            float NdotV = max(dot(N, V), 0.0);

            vec3 F0 = mix(vec3(0.04), albedo, metallic);
            vec3 F = F0 + (1.0 - F0) * pow(clamp(1.0 - max(dot(H, V), 0.0), 0.0, 1.0), 5.0);

            float alpha = roughness * roughness;
            float alpha2 = alpha * alpha;
            float NdotH = max(dot(N, H), 0.0);
            float denomD = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
            float D = alpha2 / (3.14159265 * denomD * denomD + 0.0001);

            float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
            float g1 = NdotV / (NdotV * (1.0 - k) + k);
            float g2 = NdotL / (NdotL * (1.0 - k) + k);
            float G = g1 * g2;

            vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
            vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
            vec3 diffuse = kD * albedo / 3.14159265;

            Lo += (diffuse + specular) * radiance * NdotL * ao;
        }
    }

    // Dynamic emissive glow (crystal resonance pulse)
    vec3 emissiveColor = vec3(0.0);
    if (emissive > 0.01) {
        float pulse = 0.85 + 0.30 * sin(uTime * 3.0 + dot(vFragPos, vec3(0.8, 1.2, 0.6)));
        emissiveColor = albedo * emissive * pulse;
    }

    vec3 finalColor = ambient + Lo + emissiveColor;

    FragColor = vec4(finalColor, 1.0);

    // Bloom bright pass
    float brightness = dot(finalColor, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > 1.0 || emissive > 1.0) {
        BrightColor = vec4(finalColor, 1.0);
    } else {
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
