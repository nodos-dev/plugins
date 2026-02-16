#version 450


layout(binding = 0) uniform CropParams {
	vec4 BackgroundColor;
	vec4 ForegroundColor;
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
	vec2 UV1 = smoothstep(Coordinates.xy, Coordinates.xy + Smoothness.xy, uv);
	vec2 UV2 = smoothstep(Coordinates.zw - Smoothness.zw, Coordinates.zw, uv);
	
	vec4 Color = mix(Params.BackgroundColor, Params.ForegroundColor, UV1.x);
	Color = mix(Params.BackgroundColor, Color, UV1.y);
	Color = mix(Color, Params.BackgroundColor, UV2.x);
	Color = mix(Color, Params.BackgroundColor, UV2.y);
	rt = Color;
}