#version 430
//#version 150 => version openGl 3.2
//si on est dans une version avant opengl4, il faut activer #extension GL_ARB_explicit_attrib_location : enable

//le layout est un mot clé pour customiser la façon dont la variable est interprétée
///indique au compilateur qu'il doit, pour cette variable là, la mettre à 0
//variable que dans le shader utilisé
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;
layout(location = 3) in vec4 a_Tangent;

//pour que ça fonctionne avec les uniform en version en dessous de openGl 4.0
//activer l'extension #extension GL_ARB_explicit_uniform_location : enable 
//layout(location = 0) uniform mat4 u_WorldMatrix;
//layout(location = 1) uniform mat4 u_ViewMatrix;
//layout(location = 2) uniform mat4 u_ProjectionMatrix;
uniform Matrices
{
	mat4 u_WorldMatrix;
	mat4 u_ViewMatrix;
	mat4 u_ProjectionMatrix;
};

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoords;
out vec4 v_Tangent;

void main(void)
{
	v_TexCoords = vec2(a_TexCoords.x, 1.0 - a_TexCoords.y);

	v_Position = vec3(u_WorldMatrix * vec4(a_Position, 1.0));
	// note: techniquement il faudrait passer une normal matrix du C++ vers le GLSL
	// pour les raisons que l'on a vu en cours. A defaut on pourrait la calculer ici
	// mais les fonctions inverse() et transpose() n'existe pas dans toutes les versions d'OpenGL
	// on suppose ici que la matrice monde -celle appliquee a v_Position- est orthogonale (sans deformation des axes)
	v_Normal = mat3(u_WorldMatrix) * a_Normal;
	v_Tangent = u_WorldMatrix * a_Tangent;

	gl_Position = (u_ProjectionMatrix * u_ViewMatrix * u_WorldMatrix) * vec4(a_Position, 1.0);
}