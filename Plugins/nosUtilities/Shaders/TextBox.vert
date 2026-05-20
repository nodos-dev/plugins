// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec2 uv;

layout(binding = 0, std430) uniform UBO
{
    vec2 Offset;   // box top-left in 0..1 output coords (y down)
    vec2 Size;     // box size in 0..1 output coords
    vec4 BoxColor;
} ubo;

const vec2 pos[6] =
    vec2[6](
        vec2(0.0, +1.0),
        vec2(+1.0, +1.0),
        vec2(0.0, 0.0),
        vec2(0.0, 0.0),
        vec2(+1.0, +1.0),
        vec2(+1.0, 0.0));

void main()
{
    vec2 p = pos[gl_VertexIndex];
    gl_Position = vec4((p * ubo.Size * 2) + vec2(-1, -1) + ubo.Offset * 2, 0.0, 1.0);
    uv = p;
}
