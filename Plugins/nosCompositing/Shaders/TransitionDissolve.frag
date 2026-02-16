#version 450


layout(binding = 0) uniform sampler2D Input1;
layout(binding = 1) uniform sampler2D Input2;
layout(binding = 2) uniform TransitionParams
{
	float Amount;
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;


void main()
{
	vec4 Color1 = texture(Input1, uv);
	vec4 Color2 = texture(Input2, uv);	
	vec4 Color = mix(Color1, Color2, Params.Amount);
	rt = Color;
}