// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(binding = 1)  uniform sampler2D Input;

layout(binding = 0, std430) uniform TextureGridParams
{
	vec2 Offset;
	vec2 Size;
};

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

void main()
{
    rt = texture(Input, uv);
}