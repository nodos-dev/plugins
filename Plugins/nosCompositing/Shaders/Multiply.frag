#version 450
 
layout(binding = 0) uniform sampler2D Input1;
layout(binding = 1) uniform sampler2D Input2;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;
 
void main()
{
    rt = texture(Input1, uv) * texture(Input2, uv);
}

