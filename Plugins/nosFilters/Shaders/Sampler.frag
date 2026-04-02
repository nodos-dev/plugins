// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450

layout(binding = 0) uniform sampler2D Input;

layout(binding = 1) uniform SamplerParams
{
    // Homography stored as mat4 due to std140 padding (3x3 embedded in upper-left)
    mat4 Homography;
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

void main()
{
    // Lift the 2D UV into homogeneous coordinates (the projective plane)
    vec4 p = vec4(uv, 1.0, 0.0);

    // Apply the padded homography: project output plane onto input plane
    vec4 q = Params.Homography * p;

    // Perspective divide to return to 2D (this is what makes it projection-correct)
    vec2 sampleUV = q.xy / q.z;

    rt = texture(Input, sampleUV);
}
