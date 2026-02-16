#version 450

layout(binding = 0) uniform sampler2D History[8];

layout(binding = 5) uniform Params
{
	int FramesCount;
};

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 rt;

void main()
{
	float mul = 1.0;
	float step = 1.0 / float(FramesCount);
	float total = 0.5 * float(FramesCount + 1);

	rt = vec4(0, 0, 0, 0);
	for(int i = 0; i < FramesCount; ++i)
	{
		rt += mul * texture(History[i], uv);
		mul -= step;
	}

	rt /= total;
}

