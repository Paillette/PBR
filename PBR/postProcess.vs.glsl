#version 120

attribute vec4 a_Position;

varying vec2 v_UV;

void main(void)
{
	// conversion des positions en coordonnees de textures normalisees
	// suppose que les positions des sommets sont normalisees NDC
	v_UV = a_Position.xy * 0.5 + 0.5;
	gl_Position = a_Position;
}