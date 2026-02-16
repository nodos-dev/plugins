#version 450

layout (binding = 0) uniform UBO 
{
	mat4 MVP;
} ubo;

layout(binding = 1) uniform sampler2D InputTexture;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 rt;

void main()
{   
	rt = texture(InputTexture, inUV).rgba;
}