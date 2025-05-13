#version 450

layout (location = 0) out vec2 uv;

layout (binding = 0) uniform UBO 
{
    vec2 Offset;
    vec2 Size;
} ubo;

const vec2 pos[6] =
    vec2[6](
        vec2(0.0, +1.0),
        vec2(+1.0, +1.0),
        vec2(0.0, 0.0),
        vec2(0.0, 0.0),
        vec2(+1.0, +1.0),
        vec2(+1.0, 0.0));

void main() 
{
    gl_Position = vec4((pos[gl_VertexIndex] * ubo.Size * 2) + vec2(-1, -1) + ubo.Offset * 2, 0.0, 1.0);
	uv = vec2(pos[gl_VertexIndex].x, pos[gl_VertexIndex].y);
}