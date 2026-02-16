#version 450

layout (binding = 0) uniform UBO 
{
	mat4 MVP;
	vec4 GridColor;
	float Step;
} ubo;

layout (location = 0) in vec2 Pos2d;

layout(location = 0) out vec4 rt;

void main()
{
	vec2 px = 1.2*dFdx(Pos2d);
	vec2 py = 1.2*dFdy(Pos2d);

	vec2 v0 = fract(Pos2d / ubo.Step.xx);
		  
	vec2 v1 = fract((Pos2d+px) / ubo.Step);
	vec2 v2 = fract((Pos2d+py) / ubo.Step);
	vec2 v3 = fract((Pos2d-px) / ubo.Step);
	vec2 v4 = fract((Pos2d-py) / ubo.Step);

	vec2 d1 = abs(v1-v0);
	vec2 d2 = abs(v2-v0);
	vec2 d3 = abs(v3-v0);
	vec2 d4 = abs(v4-v0);

	float m1 = max(d1.x, d1.y);
	float m2 = max(d2.x, d2.y);
	float m3 = max(d3.x, d3.y);
	float m4 = max(d4.x, d4.y);

	vec4 col = ubo.GridColor;

	float q = min(length(px), length(py));
	if (abs(Pos2d.x) < q)
		col = vec4(1,0,0,1);
	else
		if (abs(Pos2d.y) < q)
			col = vec4(0,1,0,1);

	float v = 1-smoothstep(0, .1, q / ubo.Step);

	float m = max(max(m1, m2), max(m3, m4));
	rt = (col * m) * v;

	//rt = ubo.GridColor * (m + vec4(c.y, c.x, 0, .3));
}