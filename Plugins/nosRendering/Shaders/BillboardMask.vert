#version 450
#extension GL_EXT_scalar_block_layout : enable

layout (location = 0) out vec2 outScreenUv;
layout (location = 1) out vec2 outQuadUv;

layout (binding = 0, std430) uniform UBO 
{
    mat4 MVP;
    float AlphaClip;
} ubo;

const vec2 pos[6] =
    vec2[6](
        vec2(+1.0, +1.0),
        vec2(-1.0, +1.0),
        vec2(+1.0, -1.0),
        vec2(+1.0, -1.0),
        vec2(-1.0, +1.0),
        vec2(-1.0, -1.0));
const vec2 uv[6] =
    vec2[6](
        vec2(1.0, 0.0),
        vec2(0.0, 0.0),
        vec2(1.0, 1.0),
        vec2(1.0, 1.0),
        vec2(0.0, 0.0),
        vec2(0.0, 1.0));

void main() 
{
    vec4 vertPos = vec4(pos[gl_VertexIndex].x, 0.0, pos[gl_VertexIndex].y, 1.0);
    vec4 clipPos = ubo.MVP * vertPos;
    vec4 screenPos = clipPos / clipPos.w;
    gl_Position = screenPos;
    vec2 screenUv = screenPos.xy * 0.5 + 0.5;
	outScreenUv = screenUv;
    outQuadUv = uv[gl_VertexIndex];
}