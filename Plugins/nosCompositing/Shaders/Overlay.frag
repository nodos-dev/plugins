#version 450


layout(binding = 0) uniform sampler2D InBase;
layout(binding = 1) uniform sampler2D InOverlay;
layout(binding = 2) uniform OverlayParams
{
	vec4 UV;
	float Opacity;
	int BlendingMode; // normal blend = 1, additive = 0
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;


void main()
{
	vec4 Base = texture(InBase, uv);
	vec4 Overlay = texture(InOverlay, uv * Params.UV.zw + Params.UV.xy);
	vec4 Color = Base;
	Overlay.a *= Params.Opacity;
	Color.rgb = Base.rgb * (1 - Overlay.a) + Overlay.rgb * (Overlay.a * Params.BlendingMode + (1 - Params.BlendingMode) * Params.Opacity);
	Color.a = max(Base.a, Overlay.a);

	rt = Color;
}