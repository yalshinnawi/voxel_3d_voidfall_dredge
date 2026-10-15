#version 430 core

layout (location = 0) in uvec2 aPackedData;

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
uniform vec3 uChunkWorldPos;

void main() {
    uint d0 = aPackedData.x;
    uint d1 = aPackedData.y;

    float localX = float(d0 & 0x3Fu);
    uint fluidLvl = (d1 >> 28u) & 0x7u;
    float localY;
    if (fluidLvl == 6u) {
        float cornerH = float((d1 >> 14u) & 0x7Fu) / 127.0;
        localY = float((d0 >> 6u) & 0x3Fu) + cornerH;
    } else {
        uint subYHalf = (d1 >> 26u) & 0x1u;
        uint slopeWaterOffset = (d1 >> 27u) & 0x1u;
        localY = float((d0 >> 6u) & 0x3Fu) + (subYHalf == 1u ? 0.5 : 0.0) - (slopeWaterOffset == 1u ? 0.12 : 0.0);
    }
    float localZ = float((d0 >> 12u) & 0x3Fu);

    vec3 localPos = vec3(localX, localY, localZ);
    vec4 worldPos4 = uModel * vec4(localPos + uChunkWorldPos, 1.0);
    gl_Position = uLightSpaceMatrix * worldPos4;
}
