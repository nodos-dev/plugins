#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec2 vertexScreenUv;
layout (location = 1) in vec2 quadUv;
layout(location = 0) out vec4 rt;

layout (binding = 0, std430) uniform UBO 
{
    mat4 MVP;
    float AlphaClip;
} ubo;

void main()
{   
    rt = vec4(1.0);
}