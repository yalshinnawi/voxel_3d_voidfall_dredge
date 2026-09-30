#version 430 core

out vec4 FragColor;
in vec2 vUV;

uniform sampler2D uDepthTexture;
uniform sampler2D uColorTexture;
uniform mat4 uInverseProj;
uniform mat4 uInverseView;

uniform vec3 uSonarOrigin;
uniform float uSonarRadius;
uniform int uSonarActive;
uniform float uTime;

void main() {
    vec4 baseColor = texture(uColorTexture, vUV);
    if (uSonarActive == 0) {
        FragColor = baseColor;
        return;
    }

    vec2 texel = 1.0 / textureSize(uDepthTexture, 0);

    // Sobel edge filter on depth to detect structural wireframes
    float dC = texture(uDepthTexture, vUV).r;
    float dL = texture(uDepthTexture, vUV - vec2(texel.x, 0.0)).r;
    float dR = texture(uDepthTexture, vUV + vec2(texel.x, 0.0)).r;
    float dU = texture(uDepthTexture, vUV + vec2(0.0, texel.y)).r;
    float dD = texture(uDepthTexture, vUV - vec2(0.0, texel.y)).r;

    float edge = abs(dL - dR) + abs(dU - dD);
    float edgeFactor = smoothstep(0.0005, 0.004, edge);

    // Reconstruct world position
    vec4 clipPos = vec4(vUV * 2.0 - 1.0, dC * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInverseProj * clipPos;
    viewPos /= viewPos.w;
    vec3 worldPos = (uInverseView * viewPos).xyz;

    float distToSonar = distance(worldPos, uSonarOrigin);
    float waveDist = abs(distToSonar - uSonarRadius);

    vec3 sonarColor = vec3(0.08, 0.75, 0.95); // Surveying holographic cyan

    // Holographic scanlines
    float scanline = sin(vUV.y * 800.0 + uTime * 15.0) * 0.5 + 0.5;

    vec3 finalColor = baseColor.rgb;

    if (waveDist < 3.0) {
        float waveIntensity = smoothstep(3.0, 0.0, waveDist);
        // Wireframe edges glow intensely
        finalColor += sonarColor * (edgeFactor * 3.0 + 0.4) * waveIntensity;
        // Subtle scanline grid
        finalColor += sonarColor * scanline * 0.15 * waveIntensity;
    }

    FragColor = vec4(finalColor, 1.0);
}
