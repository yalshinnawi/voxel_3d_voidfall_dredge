#version 430 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uFontTexture;
uniform vec4 uTextColor;

void main() {
    float alpha = texture(uFontTexture, vUV).r;
    if (alpha < 0.1) {
        discard;
    }
    FragColor = vec4(uTextColor.rgb, uTextColor.a * alpha);
}
