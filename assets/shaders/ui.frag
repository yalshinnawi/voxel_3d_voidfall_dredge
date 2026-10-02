#version 430 core

in vec4 vColor;
out vec4 FragColor;

// Optional font & drop shadow support for unified UI pipelines
uniform sampler2D uFontTexture;
uniform vec2 uShadowOffset;
uniform bool uHasTexture;

void main() {
    if (uHasTexture) {
        float shadowAlpha = (length(uShadowOffset) > 0.0) ? texture(uFontTexture, gl_FragCoord.xy + uShadowOffset).r : 0.0;
        float textAlpha = texture(uFontTexture, gl_FragCoord.xy).r;
        vec4 shadowColor = vec4(0.0, 0.0, 0.0, 0.95 * shadowAlpha);
        vec4 textColor = vec4(vColor.rgb, vColor.a * textAlpha);
        vec4 finalColor = mix(shadowColor, textColor, textAlpha);
        if (finalColor.a < 0.05) discard;
        FragColor = finalColor;
    } else {
        FragColor = vColor;
    }
}
