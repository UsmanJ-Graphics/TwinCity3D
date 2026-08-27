#version 330 core
in vec3 vNormal;
in float vDataValue;

uniform vec4 uBaseColor;

out vec4 FragColor;

void main() {
    vec3 lightDir = normalize(vec3(0.4, 1.0, 0.3));
    float diffuse = max(dot(normalize(vNormal), lightDir), 0.0);
    float ambient = 0.4;
    float lighting = ambient + diffuse * 0.6;
    FragColor = vec4(uBaseColor.rgb * lighting, uBaseColor.a);
}
