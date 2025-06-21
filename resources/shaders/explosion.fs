#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec2 screenPos;

uniform float explosionTime;
uniform float explosionDuration;
uniform vec2 explosionCenter;
uniform float explosionProgress;
uniform float currentTime;

// Simple noise without textures
float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

void main()
{
    vec2 uv = screenPos;
    
    // Calculate distance from center of quad (0.5, 0.5) 
    float dist = distance(uv, vec2(0.5, 0.5));
    
    // === EXPANDING EXPLOSION RING ===
    float ringRadius = explosionProgress * 0.8;
    float ringThickness = 0.15;
    float ring = smoothstep(ringRadius + ringThickness, ringRadius, dist) - 
                 smoothstep(ringRadius, ringRadius - ringThickness, dist);
    ring *= (1.0 - explosionProgress * 0.7); // Fade slower for more intensity
    
    // === SECONDARY RING (for more boom) ===
    float ring2Radius = explosionProgress * 0.5;
    float ring2Thickness = 0.08;
    float ring2 = smoothstep(ring2Radius + ring2Thickness, ring2Radius, dist) - 
                  smoothstep(ring2Radius, ring2Radius - ring2Thickness, dist);
    ring2 *= (1.0 - explosionProgress * 0.8);
    
    // === CENTER FLASH (much brighter) ===
    float flash = exp(-dist * 3.0) * exp(-explosionProgress * 3.0);
    float innerFlash = exp(-dist * 8.0) * exp(-explosionProgress * 5.0); // Super bright center
    
    // === SHOCKWAVE (more dramatic) ===
    float wave = sin((dist - explosionProgress * 3.0) * 25.0);
    wave = max(0.0, wave) * (1.0 - smoothstep(0.0, 1.0, dist)) * (1.0 - explosionProgress);
    
    // === MULTIPLE SPARK LAYERS ===
    vec2 pixelGrid = floor(uv * 32.0);
    float sparkNoise = random(pixelGrid + floor(currentTime * 12.0));
    float sparks = step(0.75, sparkNoise);
    sparks *= (1.0 - smoothstep(0.0, 0.6, dist)) * (1.0 - explosionProgress * 0.3);
    
    // Big sparks
    vec2 bigPixelGrid = floor(uv * 16.0);
    float bigSparkNoise = random(bigPixelGrid + floor(currentTime * 8.0));
    float bigSparks = step(0.9, bigSparkNoise);
    bigSparks *= (1.0 - smoothstep(0.0, 0.7, dist)) * (1.0 - explosionProgress * 0.4);
    
    // === INTENSE BOOM COLORS ===
    vec3 innerFlashColor = vec3(1.5, 1.5, 1.5);     // Super bright white
    vec3 flashColor = vec3(1.2, 0.9, 0.4);          // Bright yellow-white
    vec3 ring1Color = vec3(1.3, 0.5, 0.1);          // Intense orange-red
    vec3 ring2Color = vec3(1.5, 0.8, 0.0);          // Bright orange-yellow
    vec3 waveColor = vec3(1.2, 0.2, 0.0);           // Deep red
    vec3 sparkColor = vec3(1.4, 1.2, 0.3);          // Bright yellow
    vec3 bigSparkColor = vec3(1.5, 0.9, 0.2);       // Orange-yellow
    
    // === COMBINE EFFECTS WITH MORE BOOM ===
    vec3 color = vec3(0.0); // Transparent background
    
    float intensity = 1.2 - explosionProgress * 0.8; // Stay brighter longer
    float earlyIntensity = 1.5 - explosionProgress * 1.2; // Extra bright at start
    
    // Layer the effects for maximum impact
    color += innerFlashColor * innerFlash * earlyIntensity * 2.0;  // Super bright center
    color += flashColor * flash * intensity * 1.5;                // Bright flash
    color += ring1Color * ring * intensity * 1.3;                 // Main ring
    color += ring2Color * ring2 * intensity * 1.4;                // Secondary ring
    color += waveColor * wave * intensity * 0.6;                  // Shockwave
    color += sparkColor * sparks * intensity * 1.2;               // Small sparks
    color += bigSparkColor * bigSparks * intensity * 1.8;         // Big sparks
    
    // Boost overall brightness but keep some limits
    color = min(color, vec3(2.0)); // Allow brighter than normal
    
    // Calculate alpha for transparency (more intense)
    float alpha = (innerFlash * 2.0 + flash * 1.5 + ring * 1.3 + ring2 * 1.4 + 
                   wave * 0.6 + sparks * 1.2 + bigSparks * 1.8) * intensity;
    alpha = clamp(alpha, 0.0, 1.0);
    
    FragColor = vec4(color, alpha);
    BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
