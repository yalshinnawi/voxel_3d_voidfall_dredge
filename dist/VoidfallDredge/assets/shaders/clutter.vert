#version 430 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aColor;
layout (location = 3) in vec4 aMaterial; // x=metallic, y=roughness, z=emissive, w=ao

// Instanced attributes
layout (location = 4) in mat4 aInstanceModel; // locations 4, 5, 6, 7
layout (location = 8) in vec4 aInstanceTint;  // rgb = tint, a = emissive multiplier

uniform mat4 uProjection;
uniform mat4 uView;

out vec3 vFragPos;
out vec3 vNormal;
out vec4 vColor;
out vec4 vMaterial;

void main() {
    vec4 worldPos = aInstanceModel * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;
    mat3 normalMat = mat3(aInstanceModel);
    vNormal = normalize(normalMat * aNormal);
    vColor = vec4(aColor.rgb * aInstanceTint.rgb, aColor.a);
    vMaterial = vec4(aMaterial.xy, aMaterial.z * aInstanceTint.a, aMaterial.w);
    gl_Position = uProjection * uView * worldPos;
}
