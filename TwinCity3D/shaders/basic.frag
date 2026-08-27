#version 330 core
in vec3 vWorldNormal;
in vec3 vWorldPos;
in float vDataValue;

uniform vec4 uBaseColor;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(vWorldNormal);
    // Fixed overhead-ish sun direction; good enough for MVP spatial
    // readability, not meant to be physically accurate.
    vec3 lightDir = normalize(vec3(0.4, 0.85, 0.3));

    float diffuse = max(dot(normal, lightDir), 0.0);
    float ambient = 0.45;
    float lighting = ambient + (1.0 - ambient) * diffuse;

    vec3 color = uBaseColor.rgb * lighting;
    FragColor = vec4(color, uBaseColor.a);
}
