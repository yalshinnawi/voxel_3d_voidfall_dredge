#version 430 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aColor;
layout (location = 3) in vec4 aMaterial; // x=metallic, y=roughness, z=emissive, w=ao/tendril_flag

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;
uniform float uTime;
uniform int uIsDrifter;

out vec3 vFragPos;
out vec3 vNormal;
out vec4 vColor;
out vec4 vMaterial;
out vec3 vModelPos;
out vec4 vFragPosLightSpace;

void main() {
    vec3 animatedPos = aPos;

    if (uIsDrifter == 1 || (aMaterial.w < 0.25 && aPos.y < -0.05)) {
        float wave = sin(uTime * 3.5 + aPos.y * 2.5) * 0.18;
        animatedPos.x += wave;
        animatedPos.z += wave * 0.5;
    }

    vec4 worldPos = uModel * vec4(animatedPos, 1.0);
    vFragPos = worldPos.xyz;
    vModelPos = aPos;
    vNormal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    vColor = aColor;
    vMaterial = aMaterial;
    vFragPosLightSpace = uLightSpaceMatrix * worldPos;

    gl_Position = uProjection * uView * worldPos;
}
