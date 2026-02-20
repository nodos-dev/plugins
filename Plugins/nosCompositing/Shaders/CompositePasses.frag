#version 450

layout(binding = 0) uniform sampler2D Video;
layout(binding = 1) uniform sampler2D Render;
layout(binding = 2) uniform sampler2D Shadow;
layout(binding = 3) uniform sampler2D Reflection;
layout(binding = 4) uniform sampler2D Bloom;


layout(binding = 5) uniform KeyPass1Params
{
	float VideoOpacity;
	float TranslucencyGamma;
	float ShadowGamma;
	float Roughness;
	float SizeX;
	float SizeY;
	float K1;
	float K2;
	float DistortionScale;
	float VideoTranslucencyDetail;
    float ReflectionShadowMask;
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

vec2 P_Uniform(vec2 P)
{
	return (P - 0.5) * 2;
}

float CalculateR_Four(float R)
{
	float R2 = R * R;
	return 1 + Params.K1 * R2 + Params.K2 * R2 * R2;
}

vec2 RealityDistort(vec2 uv)
{
	vec2 AspectRatio = normalize(vec2(Params.SizeX / Params.SizeY, 1.0));
	
	vec2 P_Uniform_Aspect = P_Uniform(uv) * AspectRatio;
	float R = length(P_Uniform_Aspect);

	float R_Ratio = CalculateR_Four(R);
	vec2 P_Uniform_Aspect_Final = P_Uniform_Aspect * R_Ratio / Params.DistortionScale;
	return clamp((P_Uniform_Aspect_Final / AspectRatio)*0.5 + 0.5, 0.0, 1.0);
}

void main()
{
    vec2 uvDist = RealityDistort(uv);
	
	vec4 VideoColor = texture(Video, uv);
	vec4 RenderColor = texture(Render, uvDist);
	vec3 ShadowColor = texture(Shadow, uvDist).rgb;
	vec3 ReflectionColor = texture(Reflection, uvDist).rgb;
	vec3 BloomColor = texture(Bloom, uvDist).rgb;
    
    ReflectionColor *= mix(1-ShadowColor, vec3(1), 1-Params.ReflectionShadowMask);

	// adjust shadow
	ShadowColor = pow(ShadowColor, vec3(Params.ShadowGamma));

	// apply shadow + reflection
	vec3 OutColor = VideoColor.rgb * ShadowColor + ReflectionColor;   

	// Add Blend Video over the Render
	OutColor = RenderColor.rgb * (1-VideoColor.a) + OutColor;

	// Blend Foreground Graphics using Videomask
	RenderColor.a = pow(RenderColor.a, Params.TranslucencyGamma);

	// Value between the old implementation of composite passes and the new one
	float MixAlpha = mix(VideoColor.a * RenderColor.a * Params.VideoOpacity, RenderColor.a * Params.VideoOpacity, Params.VideoTranslucencyDetail);

	OutColor = mix(RenderColor.rgb, OutColor, MixAlpha);


	// Add bloom
	OutColor += BloomColor;

	// overwrite output alpha
	rt = vec4(OutColor, 1);
}