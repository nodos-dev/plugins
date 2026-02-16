#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(binding = 1) uniform sampler2D Textures[16];

layout(binding = 0, std430) uniform Params
{
	vec2 OutputSize;
	vec4 BackgroundColor;
	vec2 Positions[16];
	vec2 Scales[16];
	vec2 Origins[16];
	float Rotations[16];
	float Opacities[16];
	uint BlendModes;
	int Texture_Count;
	bool RGSS;
};

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;


vec4 sampleTex(sampler2D tex, vec2 uv)
{
	if(uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
		return vec4(0,0,0,0);
	else
		return texture(tex, uv, -1.0);
}


vec4 rgss(sampler2D tex, vec2 uv)
{
	vec2 dx = dFdx(uv);
	vec2 dy = dFdy(uv);

	vec2 uvOffsets = vec2(0.125, 0.375);

	vec4 result = vec4(0);
	result += sampleTex(tex, uv + uvOffsets.x * dx + uvOffsets.y * dy);
	result += sampleTex(tex, uv - uvOffsets.x * dx - uvOffsets.y * dy);
	result += sampleTex(tex, uv + uvOffsets.y * dx - uvOffsets.x * dy);
	result += sampleTex(tex, uv - uvOffsets.y * dx + uvOffsets.x * dy);

	result *= 0.25;
	return result;
}

vec2 transform(int i)
{
	float c = cos(Rotations[i] * 0.01745329251);
	float s = sin(Rotations[i] * 0.01745329251);
	vec2 t = OutputSize * (uv - Origins[i]);
	t -= Positions[i];
	t = vec2(t.x*c - t.y*s, t.x*s + t.y*c);
	t /= Scales[i];
	return t/OutputSize + Origins[i];
}

void main()
{	
	vec4 result = BackgroundColor;
	for(int i = 0; i < Texture_Count; ++i)
	{
		vec2 uv =  transform(i);
		vec4 color;
		if (RGSS)
		{
			color = rgss(Textures[i], uv) * Opacities[i];
		}
		else
		{
			if(uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
				continue;
			color = texture(Textures[i], uv) * Opacities[i];
		}

		if ((BlendModes & (1<<i)) > 0)
			result += color;
		else
			result = result * (1.0 - color.a) + vec4(color.rgb, 1.0) * color.a;
	}
	rt = result;
}