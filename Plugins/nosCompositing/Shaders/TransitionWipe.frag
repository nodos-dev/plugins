#version 450


layout(binding = 0) uniform sampler2D Input1;
layout(binding = 1) uniform sampler2D Input2;
layout(binding = 2) uniform TransitionParams
{
	float Amount;
	int UseX;
	int Reverse;
	float HalfWidth;
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;


void main()
{
	vec4 Color1 = texture(Input1, uv);
	vec4 Color2 = texture(Input2, uv);	
	
	float value = Params.UseX > 0
		? uv.x
		: uv.y;
	float reverseApplied = Params.Reverse > 0
		? 1.0 - value
		: value;

	// for the beginning and the end of the frame
	float correction = mix(Params.HalfWidth, -Params.HalfWidth, Params.Amount);

	// clamped ramp
	float transition = smoothstep(Params.Amount - Params.HalfWidth, Params.Amount + Params.HalfWidth, reverseApplied + correction);

	vec4 Color = mix(Color2, Color1, transition);
	rt = Color;
}