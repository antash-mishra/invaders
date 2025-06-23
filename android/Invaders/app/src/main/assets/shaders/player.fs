#version 300 es
precision mediump float;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec2 TexCoords;

uniform vec3 glowColor;
uniform float glowIntensity;

uniform sampler2D texture_diffuse0;

void main()
{
    vec4 color  = texture(texture_diffuse0, TexCoords);
    float alpha = color.a;

    // Detect edge of the sprite via alpha gradient
    float grad  = length(vec2(dFdx(alpha), dFdy(alpha)));
    // grad is ~0 inside/outside; peaks at the outline. Remap to 0..1
    float edge  = smoothstep(0.0, 0.03, grad); // 0-3% of texel size band

    // Glow only on the outline, not the fill
    vec3 glow   = glowColor * glowIntensity * edge;

    BrightColor = vec4(glow, edge); // send to bloom buffer
    FragColor   = color;
}