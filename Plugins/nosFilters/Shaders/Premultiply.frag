#version 450


layout(binding = 0) uniform sampler2D In;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

void main()
{
	vec4 Color = texture(In, uv);
	Color = vec4(Color.rgb * Color.a, Color.a);
	rt = Color;
}