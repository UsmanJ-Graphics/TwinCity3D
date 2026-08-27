#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in float aDataValue;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vWorldNormal;
out vec3 vWorldPos;
out float vDataValue;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vWorldNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vDataValue = aDataValue;
    gl_Position = uProjection * uView * worldPos;
}
