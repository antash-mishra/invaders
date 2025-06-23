#version 300 es
precision mediump float;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec2 TexCoords;

uniform sampler2D texture_diffuse0;
uniform vec3 glowColor;
uniform float glowIntensity;

void main()
{
    vec4 color = texture(texture_diffuse0, TexCoords);
    FragColor = color;
    BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}