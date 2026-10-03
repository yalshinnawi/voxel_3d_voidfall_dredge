#version 430 core

in vec3 vFragPos;
in vec3 vNormal;
in vec4 vColor;
in vec4 vMaterial; // x=metallic, y=roughness, z=emissive, w=ao

out vec4 FragColor;

uniform vec3 uCamPos;
uniform vec3 uHeadlampPos;
uniform vec3 uHeadlampDir;
uniform vec3 uHeadlampColor;
uniform float uHeadlampEnabled;
uniform float uStateGlow;
uniform int uState; // 0=Idle, 1=Stalking, 2=Circling, 3=Lunging, 4=Stunned, 5=Fleeing

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vFragPos);

    float metallic = clamp(vMaterial.x, 0.0, 1.0);
    float roughness = clamp(vMaterial.y, 0.04, 1.0);
    float baseEmissive = vMaterial.z;
    float ao = clamp(vMaterial.w, 0.15, 1.0);

    // Cavern subterranean ambient light
    vec3 ambient = vec3(0.06, 0.07, 0.10) * vColor.rgb * ao;

    // Headlamp spotlight calculation
    vec3 headlampLight = vec3(0.0);
    if (uHeadlampEnabled > 0.5) {
        vec3 toLight = uHeadlampPos - vFragPos;
        float dist = length(toLight);
        vec3 L = normalize(toLight);
        float diff = max(dot(N, L), 0.0);
        float spotDot = dot(normalize(-uHeadlampDir), L);
        float spotAngle = cos(radians(24.0));

        if (spotDot > spotAngle) {
            float spotFalloff = smoothstep(spotAngle, spotAngle + 0.08, spotDot);
            float atten = 1.0 / (1.0 + 0.08 * dist + 0.015 * dist * dist);

            // Specular GGX / Blinn-Phong highlight on chitinous carapace
            vec3 H = normalize(L + V);
            float specPower = mix(90.0, 8.0, roughness);
            float spec = pow(max(dot(N, H), 0.0), specPower);

            vec3 specColor = mix(vec3(0.04), vColor.rgb, metallic);
            vec3 diffuseTerm = diff * vColor.rgb * (1.0 - metallic) * ao;
            vec3 specTerm = spec * specColor * mix(1.0, 2.5, metallic);

            headlampLight = (diffuseTerm + specTerm) * uHeadlampColor * atten * spotFalloff * 2.2;
        }
    }

    // Predatory Fresnel rim lighting (chitinous edges glint menacingly in darkness)
    float NdotV = max(dot(N, V), 0.0);
    float fresnel = pow(clamp(1.0 - NdotV, 0.0, 1.0), 3.2);

    vec3 rimColor = vec3(0.12, 0.75, 0.35); // Void emerald rim
    if (uState == 3) {
        rimColor = vec3(1.0, 0.10, 0.15);   // Aggressive blood-crimson rim when lunging
    } else if (uState == 4) {
        rimColor = vec3(0.2, 0.90, 1.0);    // Electric cyan shockwave rim when stunned
    } else if (uState == 5) {
        rimColor = vec3(1.0, 0.65, 0.1);    // Amber warning rim when fleeing
    } else if (uState == 1) {
        rimColor = vec3(0.85, 0.15, 0.95);  // Deep void purple rim when stalking in shadows
    }
    vec3 rimLight = rimColor * fresnel * 0.95;

    // Emissive component (piercing glowing compound eyes & pulsating void core!)
    float emissiveMultiplier = 1.0 + uStateGlow * 1.5;
    vec3 emissive = vColor.rgb * baseEmissive * emissiveMultiplier;

    // If stunned, add electrical spark flicker
    if (uState == 4 && baseEmissive > 0.1) {
        emissive *= (1.5 + 0.5 * sin(uStateGlow * 20.0));
    }

    vec3 finalColor = ambient + headlampLight + rimLight + emissive;
    FragColor = vec4(finalColor, vColor.a);
}
