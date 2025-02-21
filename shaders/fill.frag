#version 450

layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outColor2;

void main() {
    outColor = vec4(1.0f, 1.0f, 0.0f, 1.0f);//fragColor, 1.0);
	outColor2 = vec4(0.0f, 1.0, 1.0, 1.0);//fragColor.zxy,1.0);
}