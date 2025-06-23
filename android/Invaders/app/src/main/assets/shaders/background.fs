#version 300 es
precision highp float;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in vec2 TexCoords;

uniform float time;   // seconds elapsed
uniform float alpha;  // global fade
uniform int currentLevel;
uniform int maxLevel;

// ------------------------------------------------------------
// Very small pseudo-random number helper (2-D hash)
// ------------------------------------------------------------
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898,78.233))) * 43758.5453);
}

// Generate a star sprite in the current cell.
// `uv`     – coordinates already multiplied by the layer density
// `jitter` – extra random offset inside the cell so stars are not centred
float star(vec2 uv, float threshold, float size) {
    vec2 cell = floor(uv);
    // decide if this cell contains a star
    if (rand(cell) > threshold) return 0.0;   // empty cell

    // random offset inside cell (so stars are not on a strict lattice)
    vec2 offset = vec2(rand(cell + 0.37), rand(cell + 7.13)) - 0.5;

    // local coordinates inside cell (-0.5 .. +0.5)
    vec2 gv = fract(uv) - 0.5 - offset;

    float dist = length(gv);
    float intensity = smoothstep(size, 0.0, dist); // soft disc

    // twinkle – modulate brightness a little over time (per-star phase)
    float phase  = rand(cell + 3.71);
    intensity *= 0.7 + 0.3 * sin(time * 3.0 + phase * 6.2831);
    return intensity;
}

vec3 getDayNightColor(float progress) {
    // progress: 0.0 = night, 0.5 = dawn/dusk, 1.0 = day
    
    // Night colors (deep space)
    vec3 nightSky = vec3(0.002, 0.005, 0.020);  // Deep space blue-black
    
    // Day colors  
    vec3 daySky = vec3(0.05, 0.15, 0.40);    // Space with distant nebula
    
    // Dawn/dusk colors
    vec3 dawnSky = vec3(0.25, 0.08, 0.15);   // Purple-magenta nebula
    
    vec3 skyColor;
    
    if (progress < 0.33) {
        // Night to dawn transition
        float t = progress / 0.33;
        skyColor = mix(nightSky, dawnSky, t);
    } else if (progress < 0.66) {
        // Dawn to day transition  
        float t = (progress - 0.33) / 0.33;
        skyColor = mix(dawnSky, daySky, t);
    } else {
        // Day to night transition
        float t = (progress - 0.66) / 0.34;
        skyColor = mix(daySky, nightSky, t);
    }
    
    return skyColor;
}

float getStarVisibility(float progress) {
    if (progress < 0.33) {
        return mix(1.0, 0.5, progress / 0.33);
    } else if (progress < 0.66) {
        return mix(0.5, 0.0, (progress - 0.33) / 0.33);
    } else {
        return mix(0.0, 1.0, (progress - 0.66) / 0.34);
    }
}

void main() {
    vec2 uv = TexCoords;

    // Calculate level progress (0.0 to 1.0)
    float levelProgress = float(currentLevel - 1) / float(maxLevel - 1);
    levelProgress = clamp(levelProgress, 0.0, 1.0);
    
    // Get day/night colors
    vec3 spaceColor = getDayNightColor(levelProgress);
    float starVisibility = getStarVisibility(levelProgress);
    
    // Calculate stars (same as before but modulated by visibility)
    float brightness = 0.0;
    
    // ----- NEAR (bright, fewer, faster) -----
    vec2 uvNear = vec2(uv.x, uv.y + time * 0.06);
    brightness += star(uvNear * 25.0, 0.10, 0.08) * 1.2;

    // ----- MID (medium) -----
    vec2 uvMid = vec2(uv.x, uv.y + time * 0.04);
    brightness += star(uvMid * 40.0, 0.15, 0.06) * 0.8;

    // ----- FAR (dim, many, slow) -----
    vec2 uvFar = vec2(uv.x, uv.y + time * 0.02);
    brightness += star(uvFar * 60.0, 0.10, 0.04) * 0.6;

    // Apply star visibility
    brightness *= starVisibility;
    
    vec3 col = spaceColor + vec3(brightness);

    FragColor   = vec4(col, alpha);
    BrightColor = vec4(0.0); // background is never sent to bloom
}
