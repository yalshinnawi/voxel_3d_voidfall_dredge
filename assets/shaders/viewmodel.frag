#version 430 core

in vec3 vNormal;
in vec4 vColor;
in vec3 vFragPos;

out vec4 FragColor;

uniform float uEmissive;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L1 = normalize(vec3(0.5, 0.8, 0.6));
    vec3 L2 = normalize(vec3(-0.4, 0.2, 0.8));
    
    float diff1 = max(dot(N, L1), 0.0);
    float diff2 = max(dot(N, L2), 0.0);
    
    vec3 ambient = vec3(0.35, 0.38, 0.45) * vColor.rgb;
    vec3 diffuse = (diff1 * 0.65 + diff2 * 0.35) * vColor.rgb;
    
    // View specular shine
    vec3 V = normalize(-vFragPos);
    vec3 H = normalize(L1 + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0) * 0.35;
    
    vec3 finalCol = ambient + diffuse + vec3(spec) + vColor.rgb * uEmissive;
    FragColor = vec4(finalCol, vColor.a);
}
