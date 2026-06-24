// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
// Procedural bokeh kernel generator.
//
// Produces a grayscale unit-disc mask shaped like a regular polygon aperture
// (number of blades configurable) with optional roundness, rotation, soft edge
// and brightened rim. Intended as input to a kernel-weighted DoF gather.
//
// Convention: image is treated as the [-1, 1] unit square; pixels outside the
// kernel shape return 0; pixels inside return ~1, with a smooth edge falloff
// over EdgeSoftness. The mask is normalized so that center stays at 1.

#version 450

#define PI 3.14159265358979323846

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

layout(binding = 1) uniform BokehShapeParams
{
    // Aperture blade count. 0 or 1 = perfect circle.
    float BladeCount;
    // 0 = sharp polygon, 1 = perfect circle. Interpolates polygon edge toward disc.
    float Roundness;
    // Rotation of the polygon (radians).
    float Rotation;
    // Soft falloff width at the edge, in [0, 1] of unit-disc radius.
    float EdgeSoftness;
    // Extra brightness boost near the rim, [0, 1]. Mimics cat's-eye / specular bokeh.
    float RimBoost;
    // Width of the rim brightening band, in [0, 1] of radius.
    float RimWidth;
}
Params;

void main()
{
    // Map uv [0,1] to centered coords [-1,1]
    vec2  Pos = uv * 2.0 - 1.0;
    float R   = length(Pos);

    if (R > 1.0)
    {
        rt = vec4(0.0);
        return;
    }

    float Blades = max(Params.BladeCount, 1.0);

    // Polygon edge radius along this angular direction.
    // sectorAngle = 2*pi / N; angle from sector center is a; edge distance = cos(pi/N) / cos(a).
    float PolygonR = 1.0;
    if (Blades >= 3.0)
    {
        float Theta       = atan(Pos.y, Pos.x) - Params.Rotation;
        float SectorAngle = 2.0 * PI / Blades;
        float HalfSector  = SectorAngle * 0.5;
        // Angle measured from the nearest sector centerline, in [-HalfSector, +HalfSector].
        float A = mod(Theta + HalfSector, SectorAngle) - HalfSector;
        PolygonR = cos(HalfSector) / max(cos(A), 1e-4);
    }

    // Roundness mixes polygon edge toward the circumscribed circle (radius 1).
    float EdgeR = mix(PolygonR, 1.0, clamp(Params.Roundness, 0.0, 1.0));

    // Soft edge: 1 inside, 0 past the edge, smooth across EdgeSoftness.
    float Soft = max(Params.EdgeSoftness, 1e-4);
    float Mask = 1.0 - smoothstep(EdgeR - Soft, EdgeR, R);

    // Rim brightening: a soft band just inside the edge.
    float RimW   = max(Params.RimWidth, 1e-4);
    float RimPos = (R - (EdgeR - RimW)) / RimW;            // 0 at inner edge of rim, 1 at outer
    float Rim    = clamp(1.0 - abs(RimPos * 2.0 - 1.0), 0.0, 1.0);
    Mask        += Rim * Params.RimBoost * Mask;

    rt = vec4(Mask, Mask, Mask, 1.0);
}
