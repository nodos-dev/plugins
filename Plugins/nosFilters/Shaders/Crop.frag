#version 450


layout(binding = 0) uniform sampler2D In;
layout(binding = 1) uniform CropParams {
	float Left;
	float Top;
	float Right;
	float Bottom;
	float LeftSmooth;
	float TopSmooth;
	float RightSmooth;
	float BottomSmooth;
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

void main()
{
	vec4 Coordinates = vec4(Params.Left, Params.Top, 1.0 - Params.Right, 1.0 - Params.Bottom);
	vec4 Smoothness = vec4(Params.LeftSmooth, Params.TopSmooth, Params.RightSmooth, Params.BottomSmooth);
	vec2 Size = (Coordinates.zw - Coordinates.xy);
	vec2 UV = uv * Size + Coordinates.xy;
	vec2 UV1 = smoothstep(Coordinates.xy, Coordinates.xy + Smoothness.xy * Size, UV);
	vec2 UV2 = smoothstep(Coordinates.zw - Smoothness.zw * Size, Coordinates.zw, UV);
	float Alpha = UV1.x;
	Alpha = mix(0, Alpha, UV1.y);
	Alpha = mix(Alpha, 0, UV2.x);
	Alpha = mix(Alpha, 0, UV2.y);

	vec4 Color = texture(In, UV);

	rt = mix(vec4(0,0,0,0), Color, Alpha);
}