#version 450


layout(binding = 0) uniform sampler2D In;
layout(binding = 1) uniform ColorMatrixParams {
	vec2 BlurSize;
	float RedBlend;
	float GreenBlend;
	float BlueBlend;
	float AlphaBlend;
	vec2 Direction;
	vec2 TexelSize;
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;


void main()
{
	vec4 Color = texture(In, uv);

	float BlurSizeF = length(Params.BlurSize * Params.Direction);

	int Radius = int(abs(BlurSizeF))+1;
	vec4 Accumulator = texture(In, uv);

	for (int t = 1; t < Radius; t++)
	{
		vec2 Offset = Params.Direction * vec2(t, t);
		vec2 UVPositive = clamp(uv + Params.TexelSize * Offset, 0 + Params.TexelSize / 2, 1 - Params.TexelSize / 2);
		vec2 UVNegative = clamp(uv - Params.TexelSize * Offset, 0 + Params.TexelSize / 2, 1 - Params.TexelSize / 2);
		Accumulator += texture(In, UVPositive);
		Accumulator += texture(In, UVNegative);
	}
	vec2 Offset = Params.Direction * vec2(Radius, Radius);
	vec2 UVPositive = clamp(uv + Params.TexelSize * Offset, 0 + Params.TexelSize / 2, 1 - Params.TexelSize / 2);
	vec2 UVNegative = clamp(uv - Params.TexelSize * Offset, 0 + Params.TexelSize / 2, 1 - Params.TexelSize / 2);
	vec4 LastPixel = texture(In, UVPositive);
	vec4 FirstPixel = texture(In, UVNegative);

	float SubPixelBlendRatio = fract(abs(BlurSizeF));
	Accumulator += SubPixelBlendRatio * (FirstPixel +  LastPixel);
	vec4 Blurred = Accumulator / (2 * BlurSizeF + 1);

	vec4 Blend = vec4(Params.RedBlend, Params.GreenBlend, Params.BlueBlend, Params.AlphaBlend);
	rt = mix(Color, Blurred, Blend);
}