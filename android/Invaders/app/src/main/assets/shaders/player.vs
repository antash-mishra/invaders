#version 300 es
precision mediump float;

layout (location = 0) in vec2 aPos;   
layout (location = 1) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    
    gl_Position = projection * view * model * vec4(aPos, 0.0, 1.0);
}