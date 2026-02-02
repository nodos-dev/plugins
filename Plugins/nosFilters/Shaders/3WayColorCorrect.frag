// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450

layout(binding = 0) uniform sampler2D Source;
layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

layout(binding=1) uniform CorrectParams 
{
    vec4  Saturation;
    vec4  Contrast;
    vec4  Gamma;
    vec4  Gain;
    vec4  Offset;
    vec4  SaturationShadows;
    vec4  ContrastShadows;
    vec4  GammaShadows;
    vec4  GainShadows;
    vec4  OffsetShadows;
    vec4  SaturationMidtones;
    vec4  ContrastMidtones;
    vec4  GammaMidtones;
    vec4  GainMidtones;
    vec4  OffsetMidtones;
    vec4  SaturationHighlights;
    vec4  ContrastHighlights;
    vec4  GammaHighlights;
    vec4  GainHighlights;
    vec4  OffsetHighlights;
    float ShadowsMax;
    float HighlightsMin;
};


const vec3 RGB2Y =
    vec3(
        0.2722287168, //AP1_2_XYZ_MAT[0][1],
        0.6740817658, //AP1_2_XYZ_MAT[1][1],
        0.0536895174  //AP1_2_XYZ_MAT[2][1]
    );


vec3 Correct(vec3 WorkingColor, vec4 Saturation, vec4 Contrast, vec4 Gamma, vec4 Gain, vec4 Offset, vec3 ContrastCenter)
{
	float Luma = dot(WorkingColor, RGB2Y);
	WorkingColor = max(vec3(0), mix(Luma.xxx, WorkingColor, Saturation.xyz * Saturation.w));
	WorkingColor = pow(WorkingColor * (1.0 / ContrastCenter), Contrast.xyz * Contrast.w) * ContrastCenter;
	WorkingColor = pow(WorkingColor, 1.0 / (Gamma.xyz * Gamma.w));
	WorkingColor = WorkingColor * (Gain.xyz * Gain.w) + Offset.xyz + Offset.w;
	return WorkingColor;
}

vec3 Correct(vec3 WorkingColor, vec4 Saturation, vec4 Contrast, vec4 Gamma, vec4 Gain, vec4 Offset)
{
	return Correct(WorkingColor, Saturation, Contrast, Gamma, Gain, Offset, vec3(0.18));
}

vec3 CorrectAll(vec3 WorkingColor)
{
	float Luma = dot(WorkingColor, RGB2Y);
	vec3 CCColorShadows = Correct(WorkingColor,
		SaturationShadows * Saturation,
		ContrastShadows * Contrast,
		GammaShadows * Gamma,
		GainShadows * Gain,
		OffsetShadows + Offset
	);
	float CCWeightShadows = 1 - smoothstep(0, ShadowsMax, Luma);
	vec3 CCColorHighlights = Correct(WorkingColor,
		SaturationHighlights * Saturation,
		ContrastHighlights * Contrast,
		GammaHighlights * Gamma,
		GainHighlights * Gain,
		OffsetHighlights + Offset
	);
	float CCWeightHighlights = smoothstep(HighlightsMin, 1, Luma);
	vec3 CCColorMidtones = Correct(WorkingColor,
		SaturationMidtones * Saturation,
		ContrastMidtones * Contrast,
		GammaMidtones * Gamma,
		GainMidtones * Gain,
		OffsetMidtones + Offset
	);
	float CCWeightMidtones = 1 - CCWeightShadows - CCWeightHighlights;
	vec3 WorkingColorSMH = CCColorShadows * CCWeightShadows + CCColorMidtones * CCWeightMidtones + CCColorHighlights * CCWeightHighlights;
	return WorkingColorSMH;
}


void main()
{
    vec4 src = texture(Source, uv);
	rt = vec4(CorrectAll(src.rgb), src.a);
}