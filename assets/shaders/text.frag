#version 430 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uFontTexture;
uniform vec4 uTextColor;
uniform vec2 uShadowOffset;

void main() {
    // Drop shadow pass: sample font glyph at (vUV + uShadowOffset) with color rgba(0, 0, 0, 0.95)
    float shadowAlpha = (length(uShadowOffset) > 0.0) ? texture(uFontTexture, vUV + uShadowOffset).r : 0.0;
    float textAlpha = texture(uFontTexture, vUV).r;

    vec4 shadowColor = vec4(0.0, 0.0, 0.0, 0.95 * shadowAlpha);
    vec4 textColor = vec4(uTextColor.rgb, uTextColor.a * textAlpha);

    vec4 finalColor = mix(shadowColor, textColor, textAlpha);
    if (finalColor.a < 0.05) {
        discard;
    }
    FragColor = finalColor;
}
