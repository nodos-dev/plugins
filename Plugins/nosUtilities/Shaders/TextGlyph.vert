// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec2 uv;

layout(binding = 0, std430) uniform UBO
{
    vec2 Offset;     // glyph quad top-left in 0..1 output coords (y down)
    vec2 Size;       // glyph quad size in 0..1 output coords
    vec4 AtlasRect;  // xy = atlas uv min, zw = atlas uv extent
    vec4 FillColor;
    vec4 StrokeColor;
    float StrokeWidth;
    float Softness;
    float PxRange;
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
