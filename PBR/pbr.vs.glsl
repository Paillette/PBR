#version 430

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec3 a_Tangent;

uniform Matrices
{
	mat4 u_WorldMatrix;
	mat4 u_ViewMatrix;
	mat4 u_ProjectionMatrix;
};

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoords;
out mat3 v_TBN;

void main(void)
{
	v_TexCoords = vec2(a_TexCoords.x, 1.0 - a_TexCoords.y);

	v_Position = vec3(u_WorldMatrix * vec4(a_Position, 1.0));
	v_Normal = mat3(u_WorldMatrix) * a_Normal;
	
	vec3 T = mat3(u_WorldMatrix) * a_Tangent;
	vec3 B = vec3(cross(v_Normal, T));
	
	v_TBN = mat3(T, B, v_Normal);

	gl_Position = (u_ProjectionMatrix * u_ViewMatrix * u_WorldMatrix) * vec4(a_Position, 1.0);
}