#version 450
 
layout(binding = 0) uniform sampler2D Font;
 
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 rt;
 
void main()
{
    float alpha = texture(Font, inUV).x;
	rt = inColor * vec4(alpha,alpha,alpha,1.0);
}

