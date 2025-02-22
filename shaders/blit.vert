#version 450

vec2 positions[4] = vec2[](
    vec2(1.0f, 0.5f),
    vec2(0.5f, 0.0f),
    vec2(0.0f, 0.5f),
    vec2(0.0f, 0.0f)
);

layout(location = 0) out vec2 fragTexCoords;


void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	fragTexCoords = positions[gl_VertexIndex];
}