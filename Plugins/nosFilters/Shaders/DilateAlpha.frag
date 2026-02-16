#version 450

layout(binding = 0) uniform sampler2D In;
layout(binding = 1) uniform DilateParams
{
	vec2 TexelSize;  // 1/1920, 1/1080
	vec2 Direction; // H: (1.0, 0.0), V: (0.0, 1.0)
	vec2 Radius;
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;


void main()
{
	float Radius = length(Params.Radius * Params.Direction);
    int LoopCount = abs(int(Radius)) + 1;
	float Current, Previous;

	vec4 Color = texture(In, uv);
	Previous = Color.a;
	vec2 HalfSize = Params.TexelSize * 0.5;

	for (int t = 1; t <= LoopCount; t++)                      
	{
		vec2 Offset = vec2(t * Params.Direction.x, t * Params.Direction.y) * Params.TexelSize;
		vec2 UVPositive = clamp(uv + Offset, HalfSize, 1 - HalfSize);
		vec2 UVNegative = clamp(uv - Offset, HalfSize, 1 - HalfSize);
		vec4 PositiveSample = texture(In, UVPositive);
		vec4 NegativeSample = texture(In, UVNegative);
		Current = Radius > 0.0f
			? min(Previous, min(PositiveSample.a, NegativeSample.a))
			: max(Previous, max(PositiveSample.a, NegativeSample.a));
        if (t != LoopCount)
        {
            Previous = Current;
        }
    }
	
    float SubPixelBlendRatioA = fract(abs(Radius));
    float FinalDilateA = mix(Previous, Current, SubPixelBlendRatioA);
	rt = vec4(Color.rgb, FinalDilateA);
}