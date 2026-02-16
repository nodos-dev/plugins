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
	float VideoTranslucencyDetail;
    float ReflectionShadowMask;
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

void main()
{	
	vec4 VideoColor = texture(Video, uv);
	vec4 RenderColor = texture(Render, uv);
	vec3 ShadowColor = texture(Shadow, uv).rgb;
	vec3 ReflectionColor = texture(Reflection, uv).rgb;
	vec3 BloomColor = texture(Bloom, uv).rgb;
    
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