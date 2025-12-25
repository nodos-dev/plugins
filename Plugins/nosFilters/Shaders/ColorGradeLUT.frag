// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450

precision highp float;
precision highp sampler3D;
precision highp sampler2D;
layout(binding = 0) uniform sampler2D Source;
layout(binding = 1) uniform sampler3D ColorLUT;
layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

void main()
{
	vec4 color = texture(Source, uv);
	
	vec3 lutSize = vec3(textureSize(ColorLUT, 0));
	// This is a workaround for the fact that texture sampling expects coordinates in texel center instead of corner.
	// https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-24-using-lookup-tables-accelerate-color
	vec3 sampleCoord = (color.rgb * (lutSize - 1.0) + 0.5) / lutSize;
	vec3 lutColor = texture(ColorLUT, sampleCoord).rgb;
	rt = vec4(lutColor, 1.0);
}