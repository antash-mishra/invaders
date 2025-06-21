#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec2 TexCoords;

uniform sampler2D texture_diffuse1;
uniform vec3 glowColor;
uniform float glowIntensity;

void main()
{
    vec4 color = texture(texture_diffuse1, TexCoords);
    float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    BrightColor = vec4(glowColor * glowIntensity * brightness, 1.0);
    FragColor = color;
}