#version 330 core
layout (location = 0) in vec2 aPos;   // Already in NDC (-1 to +1)
layout (location = 1) in vec2 aTexCoords;

uniform float time;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords + vec2(0.0, time * 0.05); // Scrolling effect
    gl_Position = vec4(aPos, 0.0, 1.0); // Direct to clip space - NO MATRICES!
}
