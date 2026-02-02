#version 450

layout(binding = 0) uniform MultiviewerParams
{
	int ProgramChannel;
	int PreviewChannel;
	vec2 TexelSize;
} Params;
layout(binding = 1) uniform sampler2D Program;
layout(binding = 2) uniform sampler2D Preview;
layout(binding = 3) uniform sampler2D Channels[10];
layout(binding = 4) uniform sampler2D MultiviewerLabels;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

vec2 positions[10] = vec2[10](
	vec2(0.0, 0.533333),
	vec2(0.2, 0.533333),
	vec2(0.4, 0.533333),
	vec2(0.6, 0.533333),
	vec2(0.8, 0.533333),
	vec2(0.0, 0.766666),
	vec2(0.2, 0.766666),
	vec2(0.4, 0.766666),
	vec2(0.6, 0.766666),
	vec2(0.8, 0.766666)
);

vec2 sizes[10] = vec2[10](
	vec2(0.2, 0.2),
	vec2(0.2, 0.2),
	vec2(0.2, 0.2),
	vec2(0.2, 0.2),
	vec2(0.2, 0.2),
	vec2(0.2, 0.2),
	vec2(0.2, 0.2),
	vec2(0.2, 0.2),
	vec2(0.2, 0.2),
	vec2(0.2, 0.2)
);

void main()
{
	vec4 Color = vec4(0.0,0.0,0.0,0.0);

	if (uv.x >= 0.0 && uv.x < 0.5 && uv.y >= 0.0 && uv.y < 0.5) {
		Color = texture(Program, uv / vec2(0.5, 0.5));
	}
	else if (uv.x >= 0.5 && uv.x < 1.0 && uv.y >= 0.0 && uv.y < 0.5) {
		Color = texture(Preview, (uv - vec2(0.5, 0.0)) / vec2(0.5, 0.5));
	}
	else {
		int i = int(floor(uv.x / 0.2)) + int(floor((uv.y - 0.533333) / 0.233333)) * 5;

		if (uv.x >= positions[i].x && uv.x < positions[i].x + sizes[i].x &&
			uv.y >= positions[i].y && uv.y < positions[i].y + sizes[i].y
		) {
			float distanceToEdge = min(
				min(uv.x - positions[i].x, positions[i].x + sizes[i].x - uv.x) / Params.TexelSize.x,
				min(uv.y - positions[i].y, positions[i].y + sizes[i].y - uv.y) / Params.TexelSize.y
				);
			float drawBorder = float(distanceToEdge < 5 && (i == Params.ProgramChannel || i == Params.PreviewChannel));
			vec4 borderColor = mix(vec4(0.0, 1.0, 0.0, 1.0), vec4(1.0, 0.0, 0.0, 1.0), float(i == Params.ProgramChannel));
			Color = mix(texture(Channels[i], (uv - positions[i]) / sizes[i]), borderColor, drawBorder);
		}
	}


	vec4 labels = texture(MultiviewerLabels, uv);
	rt = mix(Color, labels, labels.a);
}