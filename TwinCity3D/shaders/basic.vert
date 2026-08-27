#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in float aDataValue;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormal;
out float vDataValue;

void main() {
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vDataValue = aDataValue;
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
