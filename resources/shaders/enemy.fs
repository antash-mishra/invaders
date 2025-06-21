#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec2 TexCoords;

uniform sampler2D texture_diffuse0;

void main()
{
    FragColor = texture(texture_diffuse0, TexCoords);
    BrightColor = vec4(0.0, 0.0, 0.0, 1.0); // No bloom for enemies
}