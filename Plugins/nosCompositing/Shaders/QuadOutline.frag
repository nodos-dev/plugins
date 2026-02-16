// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(binding = 0, std430) uniform Params
{
	vec2 Offset;
	vec2 Size;
	float AspectRatio;
	float OutlineWidth;
	vec4 Color;
};

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

void main()
{
	vec2 uvSize = Size * vec2(AspectRatio, 1.0);
	float uvOutline = OutlineWidth;

	if (min(uv.x, abs(1.0 - uv.x)) * uvSize.x > OutlineWidth && min(uv.y, abs(1.0 - uv.y)) * uvSize.y > OutlineWidth)
	{
		discard;
	}

	rt = Color;
}