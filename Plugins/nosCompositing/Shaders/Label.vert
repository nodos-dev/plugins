#version 450

layout(location = 0) in vec4 posAndUV;
layout(location = 1) in vec4 color;
layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;

void main() 
{
	gl_Position = vec4(2.0*posAndUV.xy-vec2(1.0), 0.0, 1.0);
	outUV = posAndUV.zw;
	outColor = color;
}