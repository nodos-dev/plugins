#version 450


layout(binding = 0) uniform sampler2D In;

layout(binding = 1) uniform ColorMatrixParams {
    uint Conversion;
	vec3 Red;
	vec3 Green;
	vec3 Blue;
} Params;

layout(location = 0) out vec4 rt;
layout(location = 0) in vec2 uv;

mat3 Matrices[3] =  mat3[3](
    mat3(Params.Red, Params.Green, Params.Blue),
    mat3(
		vec3(1.660490, -0.587641, -0.072850),
		vec3(-0.124550, 1.132900, -0.008349),	
		vec3(-0.018151, -0.100579, 1.118729)

    ),
    mat3(
		vec3(0.627404, 0.329283, 0.043313),
		vec3(0.069097, 0.919540, 0.011362),	
		vec3(0.016391, 0.088013, 0.895595)	
    ));

void main()
{
	vec3 Sample = texture(In, uv).rgb;
    // mat3 Matrix = mat3(Params.Red, Params.Green, Params.Blue);
    vec3 Color = Sample * Matrices[Params.Conversion];
    rt = vec4(Color, 1.0);
}