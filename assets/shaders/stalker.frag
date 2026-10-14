#version 430 core

in vec3 vFragPos;
in vec3 vNormal;
in vec4 vColor;
in vec4 vMaterial; // x=metallic, y=roughness, z=emissive, w=ao
in vec3 vModelPos;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

uniform vec3 uCamPos;
uniform vec3 uHeadlampPos;
uniform vec3 uHeadlampDir;
uniform vec3 uHeadlampColor;
uniform float uHeadlampEnabled;
uniform float uStateGlow;
uniform int uState; // 0=Idle, 1=Stalking, 2=Circling, 3=Lunging, 4=Stunned, 5=Fleeing, 10=Delver
uniform float uDissolveThreshold; // 0.0 = solid, 1.0 = fully dissolved
uniform float u_dissolveThreshold;
uniform float uDarknessFactor;

// 3D procedural noise for dissolve burn-away cutouts
float hash3D(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.1, 0.1, 0.1));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise3D(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash3D(p + vec3(0,0,0)), hash3D(p + vec3(1,0,0)), f.x),
                   mix(hash3D(p + vec3(0,1,0)), hash3D(p + vec3(1,1,0)), f.x), f.y),
               mix(mix(hash3D(p + vec3(0,0,1)), hash3D(p + vec3(1,0,1)), f.x),
                   mix(hash3D(p + vec3(0,1,1)), hash3D(p + vec3(1,1,1)), f.x), f.y), f.z);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vFragPos);

    float metallic = clamp(vMaterial.x, 0.0, 1.0);
    float roughness = clamp(vMaterial.y, 0.04, 1.0);
    float baseEmissive = vMaterial.z;
    float ao = clamp(vMaterial.w, 0.15, 1.0);

    // ── Dedicated Delver Contractor Character Mode (uState == 10) ──
    if (uState == 10) {
        // Multi-source studio and subterranean illumination
        vec3 keyDir = normalize(vec3(0.45, 0.80, 0.40));
        vec3 fillDir = normalize(vec3(-0.60, 0.25, -0.45));
        vec3 bounceDir = normalize(vec3(0.0, -1.0, 0.20));
        
        float diffKey = max(dot(N, keyDir), 0.0);
        float diffFill = max(dot(N, fillDir), 0.0) * 0.42;
        float diffBounce = max(dot(N, bounceDir), 0.0) * 0.25;
        
        vec3 lightEnv = vec3(1.0, 0.97, 0.92) * diffKey + vec3(0.40, 0.62, 0.95) * diffFill + vec3(0.16, 0.14, 0.12) * diffBounce;
        
        // Headlamp directional spotlight
        vec3 headlampTerm = vec3(0.0);
        if (uHeadlampEnabled > 0.5) {
            vec3 toL = uHeadlampPos - vFragPos;
            float dist = length(toL);
            vec3 L_h = normalize(toL);
            float diff_h = max(dot(N, L_h), 0.0);
            float spot = dot(normalize(-uHeadlampDir), L_h);
            float spotCut = cos(radians(30.0));
            if (spot > spotCut && dist < 25.0) {
                float falloff = smoothstep(spotCut, spotCut + 0.08, spot);
                float atten = 1.0 / (1.0 + 0.05 * dist + 0.01 * dist * dist);
                headlampTerm = uHeadlampColor * diff_h * falloff * atten * 1.5;
            }
        }
        
        vec3 delverAlbedo = vColor.rgb;
        bool isVisor = (baseEmissive > 0.6);
        
        if (isVisor) {
            // Subtle internal HUD horizontal scanning reticle grid
            float hudScan = sin(vModelPos.y * 380.0) * 0.5 + 0.5;
            float hudGrid = sin(vModelPos.x * 280.0) * 0.5 + 0.5;
            delverAlbedo = mix(delverAlbedo * 0.88, delverAlbedo * 1.15, hudScan * hudGrid * 0.20);
        } else {
            // Ballistic weave texture on suit fabric sleeves & legs
            if (roughness > 0.55 && metallic < 0.25) {
                float weave = sin(vModelPos.x * 220.0) * cos(vModelPos.y * 220.0) * 0.5 + 0.5;
                delverAlbedo *= (0.90 + 0.16 * weave);
            }
            // Fine machined metallic grain on armor plates and mechanical joints
            if (metallic > 0.35) {
                float metalGrain = sin(vModelPos.x * 280.0 + vModelPos.z * 160.0) * 0.5 + 0.5;
                delverAlbedo *= (0.92 + 0.15 * metalGrain);
            }
        }
        
        vec3 ambientDelver = vec3(0.07, 0.08, 0.10) * delverAlbedo * ao;
        vec3 diffuseDelver = (lightEnv + headlampTerm) * delverAlbedo * (1.0 - metallic * 0.65) * ao;
        
        // Specular microfacet reflection
        vec3 HKey = normalize(keyDir + V);
        float specPower = mix(160.0, 8.0, roughness);
        float specKey = pow(max(dot(N, HKey), 0.0), specPower);
        vec3 F0 = mix(vec3(0.04), delverAlbedo, metallic);
        vec3 specColor = F0 * specKey * mix(1.0, 2.8, metallic);
        
        // Crisp, refined class-tinted Fresnel rim highlight
        float NdotV = max(dot(N, V), 0.0);
        float fresnel = pow(clamp(1.0 - NdotV, 0.0, 1.0), 3.4);
        vec3 rimColor = mix(vec3(0.4, 0.5, 0.6), vColor.rgb, 0.45) * fresnel * 0.75;
        
        // Capped visor emissive to prevent blown-out pure white rectangles
        float effEmissive = baseEmissive;
        if (isVisor) {
            effEmissive = min(baseEmissive, 1.6);
        }
        vec3 emissiveTerm = delverAlbedo * effEmissive;
        
        vec3 finalColor = ambientDelver + diffuseDelver + specColor + rimColor + emissiveTerm;
        FragColor = vec4(finalColor, vColor.a);
        
        // Controlled bloom output
        float delverLum = dot(finalColor, vec3(0.2126, 0.7152, 0.0722));
        if (delverLum > 2.0 || effEmissive > 1.35) {
            BrightColor = vec4(finalColor, 1.0);
        } else {
            BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
        }
        return;
    }

    // Void Dissolution / Alpha Burn-Away cutoff
    float dissolve = max(max(uDissolveThreshold, u_dissolveThreshold), 1.0 - vColor.a);
    vec3 dissolveRim = vec3(0.0);
    if (dissolve > 0.001) {
        float noiseVal = noise3D(vFragPos * 2.0);
        if (noiseVal < dissolve) {
            discard;
        }
        // Glowing cyan/amber burn rim along edge boundaries
        if (noiseVal < dissolve + 0.05) {
            dissolveRim = vec3(0.0, 0.95, 1.0) * 3.0;
        }
    }


    // Cavern subterranean ambient light (pitch-black shadow until lit by headlamp/flares)
    float darknessMult = clamp(1.0 - uDarknessFactor, 0.0, 1.0);
    vec3 ambient = vec3(0.014, 0.017, 0.024) * vColor.rgb * ao * darknessMult;

    // Headlamp spotlight calculation
    vec3 headlampLight = vec3(0.0);
    if (uHeadlampEnabled > 0.5) {
        vec3 toLight = uHeadlampPos - vFragPos;
        float dist = length(toLight);
        vec3 L = normalize(toLight);
        float diff = max(dot(N, L), 0.0);
        float spotDot = dot(normalize(-uHeadlampDir), L);
        float spotAngle = cos(radians(28.0));

        const float MAX_HEADLAMP_RANGE = 22.0;
        if (spotDot > spotAngle && dist < MAX_HEADLAMP_RANGE) {
            float spotFalloff = smoothstep(spotAngle, spotAngle + 0.08, spotDot);
            float rangeFactor = clamp(1.0 - (dist / MAX_HEADLAMP_RANGE), 0.0, 1.0);
            float atten = (rangeFactor * rangeFactor) / (1.0 + 0.08 * dist + 0.015 * dist * dist);

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
    } else if (uState == 8 || uState == 9) {
        rimColor = vec3(0.0);               // Extinguished in death
    }
    vec3 rimLight = rimColor * fresnel * 0.95;

    // Emissive component (piercing glowing compound eyes & pulsating void core!)
    float emissiveMultiplier = (uState == 8 || uState == 9) ? 0.0 : 1.0;
    vec3 emissive = vColor.rgb * baseEmissive * emissiveMultiplier;

    // If stunned, add electrical spark flicker
    if (uState == 4 && baseEmissive > 0.1) {
        emissive *= (1.5 + 0.5 * sin(uStateGlow * 20.0));
    }

    vec3 finalColor = ambient + headlampLight + rimLight + emissive + dissolveRim;
    FragColor = vec4(finalColor, vColor.a);

    // Bloom extraction: only genuine emissive elements (eyes, void core, hit flash, dissolve rim) cast bloom.
    // Standard chitinous armor specular highlights are suppressed by the 2.0 cutoff.
    float stalkerLum = dot(finalColor, vec3(0.2126, 0.7152, 0.0722));
    float stalkerEmissive = max(emissive.r, max(emissive.g, emissive.b));
    const float BLOOM_CUTOFF = 2.0;

    if (stalkerLum > BLOOM_CUTOFF || stalkerEmissive > 1.8 || length(dissolveRim) > 1.0) {
        BrightColor = vec4(finalColor, 1.0);
    } else {
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
