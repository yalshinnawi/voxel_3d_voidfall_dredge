#version 430 core

in vec3 vNormal;
in vec4 vColor;
in vec4 vMaterial; // x=metallic, y=roughness, z=emissive, w=ao
in vec3 vFragPos;
in vec3 vModelPos;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

uniform float uEmissive;

// ── High-Efficiency Procedural Hash & Noise Functions ──
float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

vec3 hash33(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

float noise3(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(mix(hash13(i + vec3(0,0,0)), hash13(i + vec3(1,0,0)), f.x),
            mix(hash13(i + vec3(0,1,0)), hash13(i + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash13(i + vec3(0,0,1)), hash13(i + vec3(1,0,1)), f.x),
            mix(hash13(i + vec3(0,1,1)), hash13(i + vec3(1,1,1)), f.x), f.y), f.z
    );
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(-vFragPos); // Camera is at origin in view space
    
    // Key directional light (upper right subterranean illumination)
    vec3 L1 = normalize(vec3(0.40, 0.75, 0.55));
    // Soft bounce/fill light (underneath left subterranean bounce)
    vec3 L2 = normalize(vec3(-0.45, -0.35, 0.40));
    // Delver Helmet Headlamp (forward along view axis with slight elevation)
    vec3 L_headlamp = normalize(vec3(0.0, 0.15, 0.95));
    
    float metallic = clamp(vMaterial.x, 0.0, 1.0);
    float roughness = clamp(vMaterial.y, 0.04, 1.0);
    float baseEmissive = vMaterial.z;
    float ao = clamp(vMaterial.w, 0.20, 1.0);
    vec3 albedo = vColor.rgb;

    // =========================================================
    // 1. PROCEDURAL PBR TEXTURES & SURFACE WEAR ENGINES
    // =========================================================
    
    // A. Transparent Optical Sight Glass Lens (pass-through optics)
    bool isGlassLens = (vColor.a < 0.20);
    
    // B. Illuminated Digital OLED Screens & Reticles
    bool isDigitalDisplay = (baseEmissive > 0.25 && !isGlassLens);
    if (isDigitalDisplay) {
        // High-density horizontal raster scanlines
        float scanline = sin(vModelPos.y * 750.0) * 0.5 + 0.5;
        // Sub-pixel vertical phosphor pitch
        float subpixel = sin(vModelPos.x * 1200.0) * 0.5 + 0.5;
        albedo *= (0.82 + 0.18 * scanline) * (0.88 + 0.12 * subpixel);
    }
    
    // C. Tactical Rubber Grip Knurling & Crosshatch Stippling
    bool isRubberGrip = (roughness > 0.65 && metallic < 0.22 && !isDigitalDisplay && !isGlassLens);
    if (isRubberGrip) {
        // Diamond knurling crosshatch relief on handles and fore-end slides
        vec2 gripUV = (abs(N.x) > 0.5) ? vModelPos.yz : ((abs(N.y) > 0.5) ? vModelPos.xz : vModelPos.xy);
        float knurl1 = sin((gripUV.x + gripUV.y) * 420.0);
        float knurl2 = sin((gripUV.x - gripUV.y) * 420.0);
        float diamondRelief = clamp(knurl1 * knurl2 * 1.5, -1.0, 1.0);
        
        // Perturb normal for tactile rubber 3D grip depth
        vec3 tangentU = normalize(cross(N, vec3(0.0, 1.0, 0.001)));
        vec3 tangentV = cross(N, tangentU);
        N = normalize(N + (tangentU * knurl1 + tangentV * knurl2) * 0.15);
        
        // Tactile micro-shading: darken recessed knurling seams
        albedo *= (0.85 + 0.15 * smoothstep(-0.2, 0.5, diamondRelief));
        roughness = clamp(roughness + (1.0 - diamondRelief) * 0.08, 0.60, 0.98);
    }
    
    // D. Brushed Gunmetal & Machined Cold-Rolled Steel
    bool isMetallicArmor = (metallic > 0.35 && !isDigitalDisplay && !isGlassLens);
    if (isMetallicArmor) {
        // Longitudinal micro-milling grain along weapon barrel and receiver axis
        float brushNoise = noise3(vec3(vModelPos.xy * 280.0, vModelPos.z * 18.0));
        float brushStreak = sin(vModelPos.x * 350.0 + vModelPos.y * 350.0) * 0.5 + 0.5;
        
        // Fine anisotropic roughness variation
        roughness = clamp(roughness + (brushNoise - 0.5) * 0.14, 0.04, 0.95);
        
        // Subtle ordnance steel grain modulation
        albedo *= (0.90 + 0.20 * brushNoise);
        
        // Directional anisotropic highlight perturbation
        vec3 brushDir = vec3(0.0, 0.0, 1.0);
        vec3 brushTangent = cross(N, brushDir);
        if (length(brushTangent) > 0.01) {
            brushTangent = normalize(brushTangent);
            N = normalize(N + brushTangent * ((brushStreak - 0.5) * 0.10));
        }
    }
    
    // E. Matte Cerakote & Heavy Industrial Receiver Stippling
    bool isReceiverBody = (metallic <= 0.35 && roughness >= 0.30 && !isRubberGrip && !isDigitalDisplay && !isGlassLens);
    if (isReceiverBody) {
        // Fine ceramic/phosphate sandblast micro-stipple
        float stipple = noise3(vModelPos * 140.0);
        albedo *= (0.92 + 0.16 * stipple);
    }
    
    // F. Ballistic Fabric & Pressurized Spacesuit Sleeve Weave
    bool isSuitCloth = (roughness > 0.70 && metallic < 0.10 && !isRubberGrip && !isDigitalDisplay && !isGlassLens);
    if (isSuitCloth) {
        // 2x2 woven ballistic twill weave on Delver suit arms and gloves
        float weave1 = sin(vModelPos.x * 320.0 + vModelPos.z * 160.0);
        float weave2 = cos(vModelPos.y * 320.0 - vModelPos.z * 160.0);
        float weavePattern = weave1 * weave2 * 0.5 + 0.5;
        albedo *= (0.88 + 0.18 * weavePattern);
    }
    
    // G. Chipped Paint & Silver Steel Edge Wear
    // Highlights sharp corners and beveled receiver flanges with exposed metal
    if (!isGlassLens && !isDigitalDisplay) {
        vec3 dNdx = dFdx(N);
        vec3 dNdy = dFdy(N);
        float edgeCurvature = length(dNdx) + length(dNdy);
        float edgeNoise = noise3(vModelPos * 90.0);
        
        if (edgeCurvature > 0.35 && edgeNoise > 0.40) {
            float wearIntensity = clamp((edgeCurvature - 0.35) * 1.8 * edgeNoise, 0.0, 0.85);
            vec3 exposedSteel = vec3(0.72, 0.75, 0.80);
            albedo = mix(albedo, exposedSteel, wearIntensity);
            metallic = mix(metallic, 0.90, wearIntensity);
            roughness = mix(roughness, 0.18, wearIntensity);
        }
    }
    
    // H. Thermal Bluing / Muzzle Heat Oxidation Temper Gradient
    // Appears at hot muzzle ports, borer tips, and combustion exhaust ports (negative Z)
    if (!isGlassLens && !isDigitalDisplay && vModelPos.z < -0.32) {
        float heatDist = clamp((-vModelPos.z - 0.32) / 0.16, 0.0, 1.0);
        if (heatDist > 0.05) {
            // Gold -> Violet -> Titanium Cobalt Blue temper rainbow
            vec3 temperStraw = vec3(0.88, 0.68, 0.28);
            vec3 temperViolet = vec3(0.68, 0.22, 0.62);
            vec3 temperBlue = vec3(0.16, 0.42, 0.88);
            
            vec3 temperCol;
            if (heatDist < 0.5) {
                temperCol = mix(temperStraw, temperViolet, heatDist * 2.0);
            } else {
                temperCol = mix(temperViolet, temperBlue, (heatDist - 0.5) * 2.0);
            }
            albedo = mix(albedo, temperCol, heatDist * 0.65);
        }
    }

    // =========================================================
    // 2. PBR LIGHTING CALCULATIONS
    // =========================================================
    
    // Diffuse lighting components
    float diff1 = max(dot(N, L1), 0.0);
    float diff2 = max(dot(N, L2), 0.0) * 0.32;
    float diff_head = max(dot(N, L_headlamp), 0.0) * 0.48;
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // Subterranean ambient lighting with micro-AO
    vec3 ambient = vec3(0.06, 0.07, 0.09) * albedo * ao;
    vec3 diffuse = (diff1 * 0.68 + diff2 + diff_head) * albedo * (1.0 - metallic) * ao;
    
    // Specular highlight: GGX / Blinn-Phong microfacet response
    vec3 H1 = normalize(L1 + V);
    float specPower = mix(180.0, 6.0, roughness);
    float spec1 = pow(max(dot(N, H1), 0.0), specPower);
    
    vec3 H_head = normalize(L_headlamp + V);
    float spec_head = pow(max(dot(N, H_head), 0.0), specPower * 0.85);
    
    // Fresnel rim effect (Schlick's approximation)
    float NdotV = max(dot(N, V), 0.0);
    vec3 fresnel = F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 4.5);
    vec3 specular = (spec1 * 0.80 + spec_head * 0.40) * fresnel * mix(1.0, 2.8, metallic);
    
    // Emissive term: HUD display, status LEDs, and incandescent drill bit friction
    float emissiveBoost = 1.0 + uEmissive * 1.5;
    vec3 emissive = albedo * baseEmissive * emissiveBoost;
    
    // Thermal incandescence on drill bit when actively drilling
    if (baseEmissive > 0.15 && uEmissive > 0.4) {
        // Glowing hot thermal core (cherry red / molten tungsten)
        vec3 thermalGlow = vec3(1.0, 0.55, 0.15) * (uEmissive - 0.35) * 1.8;
        emissive += thermalGlow;
    }
    
    // Transparent optical glass lenses
    if (isGlassLens) {
        ambient *= 0.12;
        diffuse *= 0.12;
        // Subtle anti-reflective green-magenta polarization sheen on outer lens edge
        float glassFresnel = pow(clamp(1.0 - NdotV, 0.0, 1.0), 3.0);
        specular += vec3(0.12, 0.85, 0.65) * glassFresnel * 0.45;
    }

    vec3 finalCol = ambient + diffuse + specular + emissive;
    FragColor = vec4(finalCol, vColor.a);

    // Bloom extraction: genuine incandescent elements / thermal glow cast bloom
    float vmLum = dot(finalCol, vec3(0.2126, 0.7152, 0.0722));
    float vmEmissive = max(emissive.r, max(emissive.g, emissive.b));
    const float BLOOM_CUTOFF = 2.0;

    if (vmLum > BLOOM_CUTOFF || vmEmissive > 1.8) {
        BrightColor = vec4(finalCol, 1.0);
    } else {
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
