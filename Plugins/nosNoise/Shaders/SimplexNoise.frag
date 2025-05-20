// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450

layout(binding = 0) uniform sampler2D Input;
layout(binding = 1) uniform SimplexNoiseParams {
	vec2 Position;
	float Scale;
	uvec2 Resolution;
	float Minimum;
	float Maximum;
}
Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

#include "SimplexNoiseCommon.glsl"

void main() {
	float AspectRatio = float(Params.Resolution.x) / float(Params.Resolution.y);
	vec2 AdjustedUV = uv;
	AdjustedUV.x *= AspectRatio;

	vec2 NoisePos = Params.Position + AdjustedUV * Params.Scale;
	float NoiseValue = snoise(NoisePos);
	NoiseValue = mix(Params.Minimum, Params.Maximum, NoiseValue * 0.5 + 0.5);
	rt = vec4(NoiseValue, NoiseValue, NoiseValue, 1.0);
}
