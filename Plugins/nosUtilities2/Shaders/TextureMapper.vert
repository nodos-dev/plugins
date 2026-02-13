#version 450

layout (location = 0) in vec3 pos;

layout (binding = 0) uniform UBO 
{
	mat4 MVP;
} ubo;

layout (location = 0) out vec2 outUV;

void main() 
{
    outUV = pos.xy;
	gl_Position = ubo.MVP * vec4(pos, 1.0);
}