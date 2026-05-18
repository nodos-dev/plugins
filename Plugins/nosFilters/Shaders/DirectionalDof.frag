// Copyright MediaZ Teknoloji A.S. All Rights Reserved.
// Directional depth-of-field pass.
// Computes circle-of-confusion (CoC) per pixel from a linear view-space Z input,
// then does a 1D weighted gather along Direction. Chain two instances
// (Direction = (1,0) and Direction = (0,1)) for a separable approximation of
// disc bokeh; visually close to a gaussian bokeh and cheap.

#version 450

#define MASK_THRESHOLD 0.001

layout(binding = 0) uniform sampler2D Input;
layout(binding = 1) uniform sampler2D Depth;
layout(binding = 2) uniform DirectionalDofParams
{
    // Focus distance in the same units as the Depth input (linear view-space Z).
    float FocusDistance;
    // Distance from focus where CoC reaches MaxRadius.
    // Smaller value = sharper focus falloff; larger = gentler.
    float FocusRange;
    // Maximum CoC radius in pixels.
    float MaxRadius;
    // 0 = treat zero depth as "no info, keep sharp"; 1 = treat zero depth as far.
    float BackgroundIsFar;
    vec2 Direction;
    // Optional: clamp CoC near the focus plane to avoid noise; raise to skip tiny blurs.
    float MinRadius;
    // Sample count along the direction (one side; total taps = 2*N+1). Higher = smoother.
    float SampleCount;
}
Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

float CocFromDepth(float Z)
{
    // Treat Z<=0 (no depth signal) as either "near focus" (BackgroundIsFar=0)
    // or as far plane (BackgroundIsFar=1). Picking far avoids halos around empty regions.
    if (Z <= 0.0)
        Z = mix(Params.FocusDistance, Params.FocusDistance + Params.FocusRange * 4.0, Params.BackgroundIsFar);

    float D   = abs(Z - Params.FocusDistance);
    float Coc = D / max(Params.FocusRange, 1e-4);
    Coc       = clamp(Coc * Params.MaxRadius, 0.0, Params.MaxRadius);
    return Coc;
}

void main()
{
    vec2 TextureSize = textureSize(Input, 0);
    vec2 TexelSize   = 1.0 / TextureSize;

    vec4  CenterColor = texture(Input, uv);
    float CenterZ     = texture(Depth, uv).r;
    float CenterCoC   = CocFromDepth(CenterZ);

    if (CenterCoC <= Params.MinRadius || Params.MaxRadius < MASK_THRESHOLD)
    {
        rt = CenterColor;
        return;
    }

    vec2 Dir = normalize(Params.Direction);

    int   N        = int(max(1.0, Params.SampleCount));
    float RadiusPx = CenterCoC;
    float Step     = RadiusPx / float(N);

    // Box-weighted average; for separable-2D this gives a soft disc.
    // CoC-clamping per sample prevents fragments in focus from bleeding outward.
    vec4  Accum  = CenterColor;
    float Weight = 1.0;

    for (int i = 1; i <= N; ++i)
    {
        float T      = float(i) * Step;
        vec2  Ofs    = Dir * T * TexelSize;

        vec4  SPos   = texture(Input, uv + Ofs);
        float ZPos   = texture(Depth, uv + Ofs).r;
        float CocPos = CocFromDepth(ZPos);
        float WPos   = Step <= CocPos ? 1.0 : 0.0;

        vec4  SNeg   = texture(Input, uv - Ofs);
        float ZNeg   = texture(Depth, uv - Ofs).r;
        float CocNeg = CocFromDepth(ZNeg);
        float WNeg   = Step <= CocNeg ? 1.0 : 0.0;

        Accum  += SPos * WPos + SNeg * WNeg;
        Weight += WPos + WNeg;
    }

    rt = Accum / Weight;
}
