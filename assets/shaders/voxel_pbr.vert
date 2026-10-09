#version 430 core

layout (location = 0) in uvec2 aPackedData;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uChunkWorldPos;

// Seismic Sonar Pulse uniforms
uniform vec3 uSonarOrigin;
uniform float uSonarRadius;
uniform float uSonarExpansionSpeed;
uniform int uSonarActive;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vTangent;
out vec3 vBitangent;
out vec2 vUV;
flat out uint vTexLayer;
flat out uint vAOIndex;
out float vAO;
out float vEmissive;
out float vDamage;
out float vSonarIntensity;
out float vIsVerticalFlow;
out vec2 v_FlowDir;

const vec3 NORMALS[6] = vec3[6](
    vec3( 1.0,  0.0,  0.0), // +X
    vec3(-1.0,  0.0,  0.0), // -X
    vec3( 0.0,  1.0,  0.0), // +Y
    vec3( 0.0, -1.0,  0.0), // -Y
    vec3( 0.0,  0.0,  1.0), // +Z
    vec3( 0.0,  0.0, -1.0)  // -Z
);

const vec3 TANGENTS[6] = vec3[6](
    vec3( 0.0,  0.0, -1.0), // +X
    vec3( 0.0,  0.0,  1.0), // -X
    vec3( 1.0,  0.0,  0.0), // +Y
    vec3( 1.0,  0.0,  0.0), // -Y
    vec3( 1.0,  0.0,  0.0), // +Z
    vec3(-1.0,  0.0,  0.0)  // -Z
);

const vec3 BITANGENTS[6] = vec3[6](
    vec3( 0.0,  1.0,  0.0), // +X
    vec3( 0.0,  1.0,  0.0), // -X
    vec3( 0.0,  0.0, -1.0), // +Y
    vec3( 0.0,  0.0,  1.0), // -Y
    vec3( 0.0,  1.0,  0.0), // +Z
    vec3( 0.0,  1.0,  0.0)  // -Z
);

void main() {
    uint d0 = aPackedData.x;
    uint d1 = aPackedData.y;

    // Unpack data0: 8 bytes bitfield
    float localX = float(d0 & 0x3Fu);
    uint subYHalf = (d1 >> 26u) & 0x1u;
    uint slopeWaterOffset = (d1 >> 27u) & 0x1u;
    uint fluidLvl = (d1 >> 28u) & 0x7u;
    v_FlowDir = vec2(0.0);
    float localY;
    if (fluidLvl == 6u) {
        float cornerH = float((d1 >> 14u) & 0x7Fu) / 127.0;
        localY = float((d0 >> 6u) & 0x3Fu) + cornerH;
        uint flowPacked = (d1 >> 21u) & 0x1Fu;
        if (flowPacked > 0u) {
            float angle = (float(flowPacked - 1u) / 30.0) * 6.2831853 - 3.14159265;
            v_FlowDir = vec2(cos(angle), sin(angle));
        }
    } else {
        float recess = 0.0;
        if (fluidLvl > 0u) {
            float liquidHeight = (fluidLvl >= 5u) ? 0.88 : (0.15 + (float(fluidLvl) / 5.0) * 0.68);
            recess = 1.0 - liquidHeight;
        }
        localY = float((d0 >> 6u) & 0x3Fu) - float(subYHalf) * 0.5 + float(slopeWaterOffset) * 0.04 - recess;
    }
    float localZ = float((d0 >> 12u) & 0x3Fu);
    uint normIdx = (d0 >> 18u) & 0x7u;
    uint aoIdx   = (d0 >> 21u) & 0x3u;
    vTexLayer    = (d0 >> 23u) & 0xFFu;
    uint auxBits = (d0 >> 31u) & 0x1u;

    // Unpack data1
    float uDim       = float(d1 & 0x3Fu);
    float vDim       = float((d1 >> 6u) & 0x3Fu);
    uint cornerIdx   = (d1 >> 12u) & 0x3u;
    vDamage          = float((d1 >> 14u) & 0xFu) / 15.0;
    vEmissive        = float((d1 >> 18u) & 0xFFu) / 255.0;
    vIsVerticalFlow  = float((d1 >> 31u) & 0x1u);

    // Baked Ambient Occlusion multipliers: ao=0 -> 1.0, ao=1 -> 0.72, ao=2 -> 0.45, ao=3 -> 0.20
    const float aoTable[4] = float[](1.0, 0.72, 0.45, 0.20);
    vAOIndex = aoIdx;
    vAO = aoTable[aoIdx];

    vec3 localPos = vec3(localX, localY, localZ);
    vec4 worldPos4 = uModel * vec4(localPos + uChunkWorldPos, 1.0);
    vWorldPos = worldPos4.xyz;
    mat3 normalMat = mat3(uModel);
    vNormal = normalize(normalMat * NORMALS[normIdx]);
    vTangent = normalize(normalMat * TANGENTS[normIdx]);
    vBitangent = normalize(normalMat * BITANGENTS[normIdx]);

    // Texture UVs repeating across greedy meshed quad
    vec2 cornerUV = vec2(0.0);
    if (cornerIdx == 1u) cornerUV = vec2(uDim, 0.0);
    else if (cornerIdx == 2u) cornerUV = vec2(uDim, vDim);
    else if (cornerIdx == 3u) cornerUV = vec2(0.0, vDim);
    vUV = cornerUV;

    // Seismic Sonar pulse ring calculation
    vSonarIntensity = 0.0;
    if (uSonarActive == 1) {
        float distToSonar = distance(vWorldPos, uSonarOrigin);
        float ringDelta = abs(distToSonar - uSonarRadius);
        if (ringDelta < 2.5) {
            vSonarIntensity = smoothstep(2.5, 0.0, ringDelta);
            // Surveyed ore/crystal resonance boost
            if (vTexLayer == 3u || vTexLayer == 7u || auxBits == 1u) {
                vSonarIntensity *= 2.2;
            }
        }
    }

    gl_Position = uProjection * uView * worldPos4;
}
