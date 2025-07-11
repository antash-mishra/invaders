#version 300 es
precision mediump float;

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
    
    // Apply model scaling only to quad vertices, not to instance position
    vec4 scaledQuad = model * vec4(aPos, 0.0, 1.0);
    vec2 finalPos = scaledQuad.xy + aInstanceOffset;
    
    gl_Position = projection * view * vec4(finalPos, 0.0, 1.0);
}  