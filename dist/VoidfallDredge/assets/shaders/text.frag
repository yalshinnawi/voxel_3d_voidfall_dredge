#version 430 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uFontTexture;
uniform vec4 uTextColor;
uniform vec2 uShadowOffset;

void main() {
    float textSample = texture(uFontTexture, vUV).r;
    if (textSample < 0.05) {
        discard;
    }

    // High-contrast smooth antialiased font edge
    float alpha = smoothstep(0.12, 0.65, textSample) * uTextColor.a;
    if (alpha < 0.02) {
        discard;
    }

    // Output pure non-premultiplied color for GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
    FragColor = vec4(uTextColor.rgb, alpha);
}
