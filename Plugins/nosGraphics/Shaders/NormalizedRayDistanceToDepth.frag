// Copyright MediaZ Teknoloji A.S. All Rights Reserved.

#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(binding = 0) uniform sampler2D Input;
layout (binding = 1, std430) uniform UBO 
{
    mat4 InverseProjection;
    float Scale;
    float ClipNear;
    float ClipFar;
} ubo;


layout(location = 0) out float FragColor;
layout(location = 0) in vec2 uv;

float RayDistanceToDepth(
    float normalizedRayDistance,
    float scale,
    vec2 uv,
    mat4 invProj,
    float zNear,
    float zFar)
{
    float zView = normalizedRayDistance * scale;

    float depth =
        zFar / (zFar - zNear) -
        (zFar * zNear) / ((zFar - zNear) * zView);

    return clamp(depth, 0.0, 1.0);
}

void main()
{
    float normalizedRayDistance = texture(Input, uv).r;
    float finDepth = RayDistanceToDepth(
        normalizedRayDistance,
        ubo.Scale,
        uv,
        ubo.InverseProjection,
        ubo.ClipNear,
        ubo.ClipFar);

    gl_FragDepth = finDepth;
    FragColor = finDepth;
}
