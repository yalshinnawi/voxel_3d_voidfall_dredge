#version 430 core

in vec3 vFragPos;
in vec3 vNormal;
in vec4 vColor;
in vec4 vMaterial; // x=metallic, y=roughness, z=emissive, w=ao/sssMask
in vec3 vModelPos;
in vec4 vFragPosLightSpace;

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
uniform float uDissolveProgress;
uniform float uDarknessFactor;

// Headlamp Shadow Mapping
uniform sampler2D uHeadlampShadowMap;
uniform int uHeadlampShadowsEnabled;

// Hit-Wound Decals & Shock Impulses
uniform float u_HitFlashTimer;
uniform vec3 u_LastHitLocalPos;
uniform int uIsMonster;
uniform vec3 uCavernAtmosphereColor;

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

float calculate_headlamp_shadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    if (uHeadlampShadowsEnabled == 0) return 1.0;
    
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }
    
    float bias = max(0.0035 * (1.0 - dot(normal, lightDir)), 0.0012);
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

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vFragPos);

    float metallic = clamp(vMaterial.x, 0.0, 1.0);
    float roughness = clamp(vMaterial.y, 0.04, 1.0);
    if (uState != 10 && uIsMonster != 0) {
        roughness = min(roughness, 0.22);
    }
    float baseEmissive = vMaterial.z;
    float ao = clamp(vMaterial.w, 0.15, 1.0);

    // ── Dedicated Delver Contractor Character Mode (uState == 10) ──
    if (uState == 10) {
        vec3 keyDir = normalize(vec3(0.45, 0.80, 0.40));
        vec3 fillDir = normalize(vec3(-0.60, 0.25, -0.45));
        vec3 bounceDir = normalize(vec3(0.0, -1.0, 0.20));
        
        float diffKey = max(dot(N, keyDir), 0.0);
        float diffFill = max(dot(N, fillDir), 0.0) * 0.42;
        float diffBounce = max(dot(N, bounceDir), 0.0) * 0.25;
        
        vec3 lightEnv = vec3(1.0, 0.97, 0.92) * diffKey + vec3(0.40, 0.62, 0.95) * diffFill + vec3(0.16, 0.14, 0.12) * diffBounce;
        
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
                float shadow = calculate_headlamp_shadow(vFragPosLightSpace, N, L_h);
                headlampTerm = uHeadlampColor * diff_h * falloff * atten * 1.5 * shadow;
            }
        }
        
        vec3 delverAlbedo = vColor.rgb;
        bool isVisor = (baseEmissive > 0.6);
        
        if (isVisor) {
            float hudScan = sin(vModelPos.y * 380.0) * 0.5 + 0.5;
            float hudGrid = sin(vModelPos.x * 280.0) * 0.5 + 0.5;
            delverAlbedo = mix(delverAlbedo * 0.88, delverAlbedo * 1.15, hudScan * hudGrid * 0.20);
        } else {
            if (roughness > 0.55 && metallic < 0.25) {
                float weave = sin(vModelPos.x * 220.0) * cos(vModelPos.y * 220.0) * 0.5 + 0.5;
                delverAlbedo *= (0.90 + 0.16 * weave);
            }
            if (metallic > 0.35) {
                float metalGrain = sin(vModelPos.x * 280.0 + vModelPos.z * 160.0) * 0.5 + 0.5;
                delverAlbedo *= (0.92 + 0.15 * metalGrain);
            }
        }
        
        vec3 ambientDelver = vec3(0.07, 0.08, 0.10) * delverAlbedo * ao;
        vec3 diffuseDelver = (lightEnv + headlampTerm) * delverAlbedo * (1.0 - metallic * 0.65) * ao;
        
        vec3 HKey = normalize(keyDir + V);
        float specPower = mix(160.0, 8.0, roughness);
        float specKey = pow(max(dot(N, HKey), 0.0), specPower);
        vec3 F0 = mix(vec3(0.04), delverAlbedo, metallic);
        vec3 specColor = F0 * specKey * mix(1.0, 2.8, metallic);
        
        float NdotV = max(dot(N, V), 0.0);
        float fresnel = pow(clamp(1.0 - NdotV, 0.0, 1.0), 3.4);
        vec3 rimColor = mix(vec3(0.4, 0.5, 0.6), vColor.rgb, 0.45) * fresnel * 0.75;
        
        float effEmissive = baseEmissive;
        if (isVisor) {
            effEmissive = min(baseEmissive, 1.6);
        }
        vec3 emissiveTerm = delverAlbedo * effEmissive;
        
        vec3 finalColor = ambientDelver + diffuseDelver + specColor + rimColor + emissiveTerm;
        FragColor = vec4(finalColor, vColor.a);
        
        float delverLum = dot(finalColor, vec3(0.2126, 0.7152, 0.0722));
        if (delverLum > 2.0 || effEmissive > 1.35) {
            BrightColor = vec4(finalColor, 1.0);
        } else {
            BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
        }
        return;
    }

    // ── Noise-Dissolve Carcass Decay (smooth procedural 3D noise burn-away) ──
    float dissolve = max(max(uDissolveThreshold, u_dissolveThreshold), uDissolveProgress);
    if (vColor.a < 0.99) {
        dissolve = max(dissolve, 1.0 - vColor.a);
    }
    vec3 dissolveRim = vec3(0.0);
    if (dissolve > 0.001) {
        float noiseVal = noise3D(vFragPos * 2.2);
        if (noiseVal < dissolve) {
            discard;
        }
        if (noiseVal < dissolve + 0.06) {
            dissolveRim = vec3(1.0, 0.45, 0.08) * 4.5;
        }
    }

    float darknessMult = clamp(1.0 - uDarknessFactor, 0.0, 1.0);
    vec3 ambient = vec3(0.014, 0.017, 0.024) * vColor.rgb * ao * darknessMult;

    vec3 headlampLight = vec3(0.0);
    vec3 sssLight = vec3(0.0);

    if (uHeadlampEnabled > 0.5) {
        vec3 toLight = uHeadlampPos - vFragPos;
        float dist = length(toLight);
        vec3 L = normalize(toLight);
        float diff = max(dot(N, L), 0.0);
        float spotDot = dot(normalize(-uHeadlampDir), L);
        float spotAngle = cos(radians(28.0));

        const float MAX_HEADLAMP_RANGE = 24.0;
        if (spotDot > spotAngle && dist < MAX_HEADLAMP_RANGE) {
            float spotFalloff = smoothstep(spotAngle, spotAngle + 0.08, spotDot);
            float rangeFactor = clamp(1.0 - (dist / MAX_HEADLAMP_RANGE), 0.0, 1.0);
            float atten = (rangeFactor * rangeFactor) / (1.0 + 0.08 * dist + 0.015 * dist * dist);

            float shadow = calculate_headlamp_shadow(vFragPosLightSpace, N, L);

            vec3 H = normalize(L + V);
            float specPower = mix(120.0, 12.0, roughness);
            float spec = pow(max(dot(N, H), 0.0), specPower);

            vec3 specColor = mix(vec3(0.05), vColor.rgb, metallic);
            vec3 diffuseTerm = diff * vColor.rgb * (1.0 - metallic) * ao;
            vec3 specTerm = spec * specColor * mix(1.2, 3.0, metallic);

            headlampLight = (diffuseTerm + specTerm) * uHeadlampColor * atten * spotFalloff * 2.4 * shadow;

            // Fake Subsurface Scattering (SSS) for translucent bell domes and weak-spot sacs
            if (vColor.a < 0.95) {
                float sssWrap = max(0.0, dot(-L, V));
                float sssIntensity = pow(sssWrap, 2.5) * 1.8;
                vec3 sssTint = mix(vColor.rgb, vec3(1.0, 0.40, 0.10), 0.5);
                sssLight = sssTint * sssIntensity * uHeadlampColor * atten * spotFalloff * shadow;
            }
        }
    }

    float NdotV = max(dot(N, V), 0.0);
    float rimFresnel = pow(clamp(1.0 - NdotV, 0.0, 1.0), 4.0);

    vec3 cavernRim = length(uCavernAtmosphereColor) > 0.1 ? uCavernAtmosphereColor : vec3(0.15, 0.65, 0.45);
    // StalkerState: 0=Idle, 1=Investigating, 2=Stalking, 3=Circling, 4=Lunging, 5=Stunned, 6=Fleeing, 8=Dying, 9=Dead
    if (uState == 4) {
        cavernRim = vec3(0.85, 0.08, 0.12);   // Aggressive blood-crimson rim when lunging
    } else if (uState == 5) {
        cavernRim = vec3(0.2, 0.90, 1.0);    // Electric cyan shockwave rim when stunned
    } else if (uState == 6) {
        cavernRim = vec3(1.0, 0.65, 0.1);    // Amber warning rim when fleeing
    } else if (uState == 2 || uState == 3) {
        cavernRim = vec3(0.25, 0.05, 0.15);  // Deep subtle obsidian-crimson sheen when stalking/circling
    } else if (uState == 8 || uState == 9) {
        cavernRim = vec3(0.0);               // Extinguished in death
    }
    vec3 rimLight = cavernRim * rimFresnel * 0.45;

    float emissiveMultiplier = (uState == 8 || uState == 9) ? 0.0 : 1.0;
    vec3 emissive = vColor.rgb * baseEmissive * emissiveMultiplier;

    if (u_HitFlashTimer > 0.0) {
        float hitDist = length(vModelPos - u_LastHitLocalPos);
        if (hitDist < 0.32) {
            float scarFactor = smoothstep(0.32, 0.04, hitDist);
            emissive += vec3(0.05, 0.90, 1.0) * scarFactor * 4.0 * u_HitFlashTimer;
        }
        emissive += vec3(0.75, 0.95, 1.0) * u_HitFlashTimer * 1.8;
    }

    if (uState == 4 && baseEmissive > 0.1) {
        emissive *= (1.5 + 0.5 * sin(uStateGlow * 20.0));
    }

    vec3 finalColor = ambient + headlampLight + sssLight + rimLight + emissive + dissolveRim;
    float outAlpha = (uState != 8 && uState != 9 && uDissolveThreshold <= 0.01 && uDissolveProgress <= 0.01) ? 1.0 : vColor.a;
    FragColor = vec4(finalColor, outAlpha);

    float stalkerLum = dot(finalColor, vec3(0.2126, 0.7152, 0.0722));
    float stalkerEmissive = max(emissive.r, max(emissive.g, emissive.b));
    const float BLOOM_CUTOFF = 2.0;

    if (stalkerLum > BLOOM_CUTOFF || stalkerEmissive > 1.8 || length(dissolveRim) > 1.0) {
        BrightColor = vec4(finalColor, 1.0);
    } else {
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
