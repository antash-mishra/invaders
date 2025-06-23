#version 300 es
precision highp float;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec2 TexCoords;

uniform sampler2D backgroundTexture;
uniform float alpha;

void main()
{
    vec4 texColor = texture(backgroundTexture, TexCoords);
    
    // Apply alpha for layer blending
    FragColor = vec4(texColor.rgb, texColor.a * alpha);
    BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
} 