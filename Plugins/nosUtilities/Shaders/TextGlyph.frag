// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(binding = 1) uniform sampler2D Atlas;

layout(binding = 0, std430) uniform UBO
{
    vec2 Offset;
    vec2 Size;
    vec4 AtlasRect;   // xy = atlas uv min, zw = atlas uv extent
    vec4 FillColor;
    vec4 StrokeColor;
    float StrokeWidth; // outline thickness in output pixels (0 = none)
    float Softness;    // extra edge blur in output pixels (drop shadow)
    float PxRange;     // output pixels spanned by one full signed-distance unit
} ubo;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 rt;

void main()
{
    // uv (0,0) is the glyph top-left, matching the top-down atlas bitmaps.
    vec2 atlasUv = ubo.AtlasRect.xy + uv * ubo.AtlasRect.zw;
    float sd = texture(Atlas, atlasUv).r; // signed distance, 0.5 = glyph edge

    float distPx = (sd - 0.5) * ubo.PxRange; // signed distance, output pixels
    float aa = 0.75 + ubo.Softness;

    float fillA = smoothstep(-aa, aa, distPx);
    float outerA = smoothstep(-aa, aa, distPx + ubo.StrokeWidth);

    // Composite the fill over the stroke (stroke spans the whole silhouette).
    float fa = ubo.FillColor.a * fillA;
    float sa = ubo.StrokeColor.a * outerA;
    float outA = fa + sa * (1.0 - fa);
    vec3 outRGB = (ubo.FillColor.rgb * fa + ubo.StrokeColor.rgb * sa * (1.0 - fa)) / max(outA, 1e-5);

    rt = vec4(outRGB, outA);
}
