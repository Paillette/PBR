#version 120

uniform sampler2D u_Texture;
uniform sampler2D u_Normal;

varying vec2 v_UV;

void main(void)
{
	//gl_FragColor = texture2D(u_Texture, v_UV);
	gl_FragColor = texture2D(u_Normal, v_UV);
	/*
	// La normale originelle du vertex ne nous sert plus que pour creer le repere tangent
	// v_Tangent.w contient la valeur de correction de repère main-droite (handedness)
	vec3 N = normalize(v_Normal);
	vec3 T = normalize(v_Tangent.xyz);
	vec3 B = normalize(cross(N, T) * v_Tangent.w);
	// TBN transforme de Tangent Space vers World Space
	// (comme v_Tangent et v_Normal ont ete prealablement multipliees par la world matrix
	// TBN est donc la combinaison des transformations tangent-to-local et local-to-world)
	mat3 TBN = mat3(T, B, N);
	// N en tangent space que l'on transform en World Space
	N = texture(u_SamplerNormal, v_TexCoords).rgb * 2.0 - 1.0;
	N = normalize(TBN * N);
	*/


}