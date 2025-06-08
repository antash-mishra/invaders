#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform float time;
uniform float alpha;

// Simple hash function for pseudo-random numbers
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// Simple star generation
float stars(vec2 uv, float density) {
    vec2 grid = uv * density;
    vec2 id = floor(grid);
    vec2 gv = fract(grid) - 0.5;
    
    float starSeed = hash(id);
    
    // Create a star if random value is small enough
    if (starSeed < 0.05) { // 5% chance for a star
        float dist = length(gv);
        float starSize = 0.1;
        
        // Simple star with smooth falloff
        float star = smoothstep(starSize, 0.0, dist);
        
        // Very gentle twinkling
        float twinkle = 0.7 + 0.3 * sin(time + starSeed * 6.28);
        
        return star * twinkle;
    }
    
    return 0.0;
}

void main()
{
    vec2 uv = TexCoords;
    
    // Simple dark space background
    vec3 spaceColor = vec3(0.01, 0.01, 0.05);
    
    // Simple two-layer starfield
    float starField = 0.0;
    
    // Bright close stars
    starField += stars(uv + vec2(0.0, time * 0.02), 20.0) * 1.0;
    
    // Dimmer distant stars
    starField += stars(uv + vec2(50.0, time * 0.01), 40.0) * 0.6;
    
    // White star color
    vec3 starColor = vec3(1.0, 0.95, 0.9);
    
    // Combine background and stars
    vec3 finalColor = spaceColor + starColor * starField;
    
    FragColor = vec4(finalColor, alpha);
}
