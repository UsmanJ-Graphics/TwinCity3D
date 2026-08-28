#version 330 core
in vec3 vWorldNormal;
in vec3 vWorldPos;
in float vDataValue;

uniform vec4 uBaseColor;
uniform int uUseDataColor;  // Phase 9: 1 = color from vDataValue via DataRamp, 0 = flat uBaseColor

out vec4 FragColor;

// Phase 9. One color ramp, shared by every data layer (HeatRisk,
// Population, GreenCoverage, BuildingDensity, Temperature — see
// twin/HeatLayers.h) — only the *value* baked into vDataValue changes per
// layer, never this function. Cool blue -> green -> yellow -> orange -> red.
// This is the single place a real legend/palette picker (Phase 15/16) would
// plug into — master spec Phase 9 explicitly says not to hard-code a
// particular palette into rendering logic per category, so it lives here
// once instead of being duplicated per layer.
vec3 DataRamp(float t) {
    t = clamp(t, 0.0, 1.0);
    vec3 c0 = vec3(0.20, 0.40, 0.85);  // low
    vec3 c1 = vec3(0.20, 0.70, 0.55);
    vec3 c2 = vec3(0.90, 0.85, 0.20);
    vec3 c3 = vec3(0.95, 0.55, 0.15);
    vec3 c4 = vec3(0.85, 0.15, 0.15);  // high

    float scaled = t * 4.0;
    if (scaled < 1.0) return mix(c0, c1, scaled);
    if (scaled < 2.0) return mix(c1, c2, scaled - 1.0);
    if (scaled < 3.0) return mix(c2, c3, scaled - 2.0);
    return mix(c3, c4, min(scaled - 3.0, 1.0));
}

void main() {
    vec3 normal = normalize(vWorldNormal);
    // Fixed overhead-ish sun direction; good enough for MVP spatial
    // readability, not meant to be physically accurate.
    vec3 lightDir = normalize(vec3(0.4, 0.85, 0.3));

    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.45;
    float lighting = ambient + (1.0 - ambient) * diffuse;

    vec3 baseColor;
    if (uUseDataColor == 1) {
        if (vDataValue < 0.0) {
            // No real data yet for this zone/layer (Phase 9 "no data"
            // sentinel, see kNoDataSentinel) — neutral gray so a
            // placeholder never masquerades as a real reading.
            baseColor = vec3(0.55, 0.55, 0.55);
        } else {
            baseColor = DataRamp(vDataValue);
        }
    } else {
        baseColor = uBaseColor.rgb;
    }

    vec3 color = baseColor * lighting;
    FragColor = vec4(color, uBaseColor.a);
}
