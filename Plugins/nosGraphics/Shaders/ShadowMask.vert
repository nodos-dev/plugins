#version 450
#extension GL_EXT_scalar_block_layout : enable

layout (binding = 0, std430) uniform UBO 
{
    mat4 MVP;
    mat4 InvViewProj;
    float GroundLevel;
} ubo;

layout(location = 0) out vec2 uv;

const vec2 pos[6] =
    vec2[6](
        vec2(+0.5, 0.0),
        vec2(-0.5, 0.0),
        vec2(+0.5, -1.0),
        vec2(+0.5, -1.0),
        vec2(-0.5, 0.0),
        vec2(-0.5, -1.0));

void main() 
{
    uv = vec2(pos[gl_VertexIndex].x + 0.5, pos[gl_VertexIndex].y + 1.0);
    vec4 vertPos = vec4(pos[gl_VertexIndex].x, 0.0, pos[gl_VertexIndex].y, 1.0);
    vec4 clipPos = ubo.MVP * vertPos;
    gl_Position = clipPos;
}
