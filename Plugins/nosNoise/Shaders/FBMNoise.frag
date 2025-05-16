// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450

layout(binding = 0) uniform sampler2D Input;
layout(binding = 1) uniform FBMNoiseParams {
    vec2 Position;
    float Scale;
    uvec2 Resolution;
    float Minimum;
    float Maximum;
    int Octaves;
    float Persistence;
    float Lacunarity;
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

#include "SimplexNoiseCommon.glsl"

float fbm(vec2 pos) {
    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;
    for (int i = 0; i < max(Params.Octaves, 1); ++i) {
        value += snoise(pos * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= Params.Persistence;
        frequency *= Params.Lacunarity;
    }
    return value / maxValue;
}

void main() {
    float AspectRatio = float(Params.Resolution.x) / float(Params.Resolution.y);
    vec2 AdjustedUV = uv;
    AdjustedUV.x *= AspectRatio;

    vec2 NoisePos = Params.Position + AdjustedUV * Params.Scale;
    float NoiseValue = fbm(NoisePos);
    NoiseValue = mix(Params.Minimum, Params.Maximum, NoiseValue * 0.5 + 0.5); // Remap from [-1,1] to [0,1]
    rt = vec4(NoiseValue, NoiseValue, NoiseValue, 1.0);
} 