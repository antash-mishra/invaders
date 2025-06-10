#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D backgroundTexture;
uniform float alpha;

void main()
{
    vec4 texColor = texture(backgroundTexture, TexCoords);
    
    // Apply alpha for layer blending
    FragColor = vec4(texColor.rgb, texColor.a * alpha);
} 