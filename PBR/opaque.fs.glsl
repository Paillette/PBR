#version 430

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoords;
in vec4 v_Tangent;

out vec4 o_FragColor;

struct Material {
	vec3 AmbientColor;
	vec3 DiffuseColor;
	vec3 SpecularColor;
	float Shininess;
};

layout(binding = 1) uniform Materials
{
	Material u_Material;
};


uniform vec3 u_CameraPosition;

//pour les textures, il faut utiliser 
layout(binding = 0) uniform sampler2D u_DiffuseTexture;
layout(binding = 1) uniform sampler2D u_NormalTexture;

// calcul du facteur diffus, suivant la loi du cosinus de Lambert
float Lambert(vec3 N, vec3 L)
{
	return max(0.0, dot(N, L));
}

// calcul du facteur speculaire, methode de Phong
float Phong(vec3 N, vec3 L, vec3 V, float shininess)
{
	// reflexion du vecteur incident I (I = -L)
	// suivant la loi de ibn Sahl / Snell / Descartes
	vec3 R = reflect(-L, N);
	return pow(max(0.0, dot(R, V)), shininess);
}

// calcul du facteur speculaire, methode Blinn-Phong
float BlinnPhong(vec3 N, vec3 L, vec3 V, float shininess)
{
	// reflexion inspire du modele micro-facette (H approxime la normale de la micro-facette)
	vec3 H = normalize(L + V);
	return pow(max(0.0, dot(N, H)), shininess);
}

void main(void)
{
	// directions des deux lumieres (fixes)
	const vec3 L[2] = vec3[2](normalize(vec3(0.0, 0.0, 1.0)), normalize(vec3(0.0, 0.0, -1.0)));
	const vec3 lightColor[2] = vec3[2](vec3(1.0, 1.0, 1.0), vec3(0.5, 0.5, 0.5));
	const float attenuation = 1.0; // on suppose une attenuation faible ici
	// theoriquement, l'attenuation naturelle est proche de 1 / distance²

	vec3 N = normalize(v_Normal);
	vec3 V = normalize(u_CameraPosition - v_Position);

	vec4 baseTexel = texture2D(u_DiffuseTexture, v_TexCoords);
	// decompression gamma, les couleurs des texels ont ete specifies dans l'espace colorimetrique
	// du moniteur (en sRGB) il faut donc convertir en RGB lineaire pour que les maths soient corrects
	// il faut de preference utiliser le hardware pour cette conversion 
	// ce qui peut se faire pour chaque texture en specifiantc le(s) format(s) interne(s) GL_SRGB8(_ALPHA8)
	baseTexel.rgb = pow(baseTexel.rgb, vec3(2.2));
	vec3 baseColor = baseTexel.rgb;

	//NORMALE
	// La normale originelle du vertex ne nous sert plus que pour creer le repere tangent
	// v_Tangent.w contient la valeur de correction de repère main-droite (handedness)
	vec3 T = normalize(v_Tangent.xyz);
	vec3 B = normalize(cross(N, T) * v_Tangent.w);
	// TBN transforme de Tangent Space vers World Space
	// (comme v_Tangent et v_Normal ont ete prealablement multipliees par la world matrix
	// TBN est donc la combinaison des transformations tangent-to-local et local-to-world)
	mat3 TBN = mat3(T, B, N);
	// N en tangent space que l'on transform en World Space
	N = texture(u_NormalTexture, v_TexCoords).rgb * 2.0 - 1.0;
	N = normalize(TBN * N);

	vec3 directColor = vec3(0.0);
	for (int i = 0; i < 2; i++)
	{
		// les couleurs diffuse et speculaire traduisent l'illumination directe de l'objet
		vec3 diffuseColor = baseColor * u_Material.DiffuseColor * Lambert(N, L[i]);
		vec3 specularColor = u_Material.SpecularColor * BlinnPhong(N, L[i], V, 90);//u_Material.Shininess

		directColor += (diffuseColor + specularColor)* lightColor[i] * attenuation;
	}

	// la couleur ambiante traduit une approximation de l'illumination indirecte de l'objet
	vec3 ambientColor = baseColor * u_Material.AmbientColor;
	vec3 indirectColor = ambientColor;

	vec3 color = directColor + indirectColor;
	
	// correction gamma (pas necessaire ici si glEnable(GL_FRAMEBUFFER_SRGB))
	//color = pow(color, vec3(1.0 / 2.2));

	o_FragColor = vec4(directColor, 1.0);
}