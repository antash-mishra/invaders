#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoords;

uniform float offsetX;  // Horizontal scroll offset for parallax

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    
    // Apply horizontal scrolling with wrapping
    TexCoords = vec2(aTexCoord.x + offsetX, aTexCoord.y);
} 