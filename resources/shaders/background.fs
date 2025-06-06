#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform float alpha;

void main()
{
    FragColor = vec4(0.0, 0.0, 0.0, alpha); // Darker for space feel
}
