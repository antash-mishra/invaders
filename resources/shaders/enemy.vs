#version 330 core
layout (location = 0) in vec2 aPos;   
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec2 aInstanceOffset;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    vec2 instancePos = aPos + aInstanceOffset;
    gl_Position = projection * view * model * vec4(instancePos, 0.0, 1.0);
}  