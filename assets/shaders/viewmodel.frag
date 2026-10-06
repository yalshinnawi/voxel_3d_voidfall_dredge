#version 430 core

in vec3 vNormal;
in vec4 vColor;
in vec4 vMaterial; // x=metallic, y=roughness, z=emissive, w=ao
in vec3 vFragPos;

out vec4 FragColor;

uniform float uEmissive;

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(-vFragPos); // Camera is at origin in view space
    
    // Key directional light (upper right subterranean glow)
    vec3 L1 = normalize(vec3(0.4, 0.75, 0.55));
    // Soft bounce/fill light (underneath left)
    vec3 L2 = normalize(vec3(-0.45, -0.35, 0.4));
    // Delver Helmet Headlamp (forward along view axis with slight elevation)
    vec3 L_headlamp = normalize(vec3(0.0, 0.15, 0.95));
    
    float metallic = clamp(vMaterial.x, 0.0, 1.0);
    float roughness = clamp(vMaterial.y, 0.04, 1.0);
    float baseEmissive = vMaterial.z;
    float ao = clamp(vMaterial.w, 0.25, 1.0);
    
    // Diffuse lighting components
    float diff1 = max(dot(N, L1), 0.0);
    float diff2 = max(dot(N, L2), 0.0) * 0.30;
    float diff_head = max(dot(N, L_headlamp), 0.0) * 0.45;
    
    vec3 albedo = vColor.rgb;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // Ambient with subtle suit-visor reflectance
    vec3 ambient = vec3(0.05, 0.06, 0.08) * albedo * ao;
    vec3 diffuse = (diff1 * 0.65 + diff2 + diff_head) * albedo * (1.0 - metallic) * ao;
    
    // Specular highlight: GGX / Blinn-Phong microfacet response
    vec3 H1 = normalize(L1 + V);
    float specPower = mix(160.0, 6.0, roughness);
    float spec1 = pow(max(dot(N, H1), 0.0), specPower);
    
    vec3 H_head = normalize(L_headlamp + V);
    float spec_head = pow(max(dot(N, H_head), 0.0), specPower * 0.8);
    
    // Fresnel rim effect (Schlick's approximation)
    float NdotV = max(dot(N, V), 0.0);
    vec3 fresnel = F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 4.5);
    vec3 specular = (spec1 * 0.75 + spec_head * 0.35) * fresnel * mix(1.0, 2.8, metallic);
    
    // Emissive term: HUD display, status LEDs, and incandescent drill bit friction
    float emissiveBoost = 1.0 + uEmissive * 1.5;
    vec3 emissive = albedo * baseEmissive * emissiveBoost;
    
    // Thermal incandescence on drill bit when actively drilling
    if (baseEmissive > 0.15 && uEmissive > 0.4) {
        // Glowing hot thermal core (cherry red / molten tungsten)
        vec3 thermalGlow = vec3(1.0, 0.55, 0.15) * (uEmissive - 0.35) * 1.8;
        emissive += thermalGlow;
    }
    
    vec3 finalCol = ambient + diffuse + specular + emissive;
    FragColor = vec4(finalCol, vColor.a);
}
