#version 450

layout (location = 0) in vec3 pos;

layout (binding = 0) uniform UBO 
{
	mat4 MVP;
	vec4 GridColor;
	float Step;
} ubo;

layout (location = 0) out vec2 Pos2d;

void main() 
{
	Pos2d = pos.xy;
	gl_Position = ubo.MVP * vec4(pos, 1.0);
}
