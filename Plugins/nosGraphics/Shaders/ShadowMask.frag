#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(binding = 1) uniform sampler2D DepthTexture;

layout (binding = 0, std430) uniform UBO 
{
    mat4 MVP;
    mat4 InvViewProj;
    float GroundLevel;
} ubo;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

void main()
{   
    ivec2 depthTextureSize = textureSize(DepthTexture, 0);
    vec2 screenUv = gl_FragCoord.xy / vec2(depthTextureSize);
    float sceneDepth = texture(DepthTexture, screenUv).r;
    if (sceneDepth >= 1.0)
        discard;

    vec2 ndc = vec2(screenUv.x * 2.0 - 1.0, screenUv.y * 2.0 - 1.0);
    vec4 worldPos = ubo.InvViewProj * vec4(ndc, sceneDepth, 1.0);
    worldPos /= worldPos.w;
    if (worldPos.z >= ubo.GroundLevel)
        discard;

    rt = vec4(uv.y, 0.0, 0.0, 1.0);
}
