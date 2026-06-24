// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(binding = 0, std430) uniform UBO
{
    vec2 Offset;
    vec2 Size;
    vec4 BoxColor;
} ubo;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 rt;

void main()
{
    rt = ubo.BoxColor;
}
