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

layout(binding = 0) uniform sampler2D u_DiffuseTexture;
layout(binding = 1) uniform sampler2D u_NormalTexture;
layout(binding = 2) uniform sampler2D u_ORMTexture;

float PI = 3.1416;

float Lambert(vec3 N, vec3 L)
{
	return max(0.0, dot(N, L));
}

float Phong(vec3 N, vec3 L, vec3 V, float shininess)
{
	vec3 R = reflect(-L, N);
	return pow(max(0.0, dot(R, V)), shininess);
}

float BlinnPhong(vec3 N, vec3 L, vec3 V, float shininess)
{
	vec3 H = normalize(L + V);
	return pow(max(0.0, dot(N, H)), shininess);
}

//PBR
//NDF
float DistributionGGX(float roughness, float NdotH)
{
    float roughnessSqr = roughness*roughness;
    float NdotHSqr = NdotH*NdotH;
    float TanNdotHSqr = (1-NdotHSqr)/NdotHSqr;

    return (1.0 / PI) * sqrt(roughness/(NdotHSqr * (roughnessSqr + TanNdotHSqr)));

}

//GSF
float CookTorrenceGeometricShadowingFunction (float NdotL, float NdotV, float VdotH, float NdotH)
{
	float Gs = min(1.0, min(2*NdotH*NdotV / VdotH, 2*NdotH*NdotL / VdotH));
	return Gs;
}

// Fresnel
float MixFunction(float i, float j, float x) {
	 return  j * x + i * (1.0 - x);
}

float SchlickFresnel(float i){
    float x = clamp(1.0-i, 0.0, 1.0);
    float x2 = x*x;
    return x2*x2*x;
}

float calculateF0 (float NdotL, float NdotV, float LdotH, float roughness)
{
    float FresnelLight = SchlickFresnel(NdotL); 
    float FresnelView = SchlickFresnel(NdotV);
    float FresnelDiffuse90 = 0.5 + 2.0 * LdotH*LdotH * roughness;
    return  MixFunction(1, FresnelDiffuse90, FresnelLight) * MixFunction(1, FresnelDiffuse90, FresnelView);
}

vec3 SchlickFresnelFunction(vec3 SpecularColor,float LdotH)
{
    return SpecularColor + (1 - SpecularColor)* SchlickFresnel(LdotH);
}

float SchlickIORFresnelFunction(float ior ,float LdotH)
{
    float f0 = pow(ior-1,2)/pow(ior+1, 2);
    return f0 + (1-f0) * SchlickFresnel(LdotH);
}
void main(void)
{
	float ior = 1.3;
	float AO = texture(u_ORMTexture, v_TexCoords).r;
	float roughness = texture(u_ORMTexture, v_TexCoords).g;
	float metallic = texture(u_ORMTexture, v_TexCoords).b;

	//lights
	const vec3 L[2] = vec3[2](normalize(vec3(0.0, 0.0, 1.0)), normalize(vec3(0.0, 0.0, -1.0)));
	const vec3 lightColor[2] = vec3[2](vec3(1.0, 1.0, 1.0), vec3(0.5, 0.5, 0.5));
	const float attenuation = 1.0;

	vec3 N = normalize(v_Normal);
	vec3 V = normalize(u_CameraPosition - v_Position);

	vec4 baseTexel = texture2D(u_DiffuseTexture, v_TexCoords);
	//Gamma
	baseTexel.rgb = pow(baseTexel.rgb, vec3(2.2));
	vec3 baseColor = baseTexel.rgb;

	vec3 H = normalize(V + L[0]);
	float NdotH = max(dot(N, H), 0.);

	float NdotL = max(dot(N, L[0]), 0.);
	float NdotV = max(dot(N, V), 0.);
	float VdotH = max(dot(V, H), 0.);
	float LdotH = max(dot(L[0], H), 0.);

	//NORMALE
	vec3 T = normalize(v_Tangent.xyz);
	vec3 B = normalize(cross(N, T) * v_Tangent.w);
	// TBN
	mat3 TBN = mat3(T, B, N);
	// N
	N = texture(u_NormalTexture, v_TexCoords).rgb * 2.0 - 1.0;
	N = normalize(TBN * N);

	//-------------------------------------------------------BlinnPhong
	vec3 directColor = vec3(0.0);
	for (int i = 0; i < 2; i++)
	{
		vec3 diffuseColor = baseColor * u_Material.DiffuseColor * Lambert(N, L[i]);
		vec3 specularColor = u_Material.SpecularColor * BlinnPhong(N, L[i], V, 90);//u_Material.Shininess

		directColor += (diffuseColor + specularColor)* lightColor[i] * attenuation;
	}

	vec3 ambientColor = baseColor * u_Material.AmbientColor;
	vec3 indirectColor = ambientColor;

	vec3 color = directColor + indirectColor;
	
	//---------------------------------------------------------PBR
	//Diffuse Color
	 float R = 1 - (roughness * roughness);
	 R = R * R;
     vec3 diffuseColor = baseColor * (1.0 - metallic);
 	 float f0 = calculateF0(NdotL, NdotV, LdotH, R);
	 diffuseColor *= f0;
	 diffuseColor += indirectColor;

	//GGX NDF
	vec3 SpecularDistribution = u_Material.SpecularColor;
	SpecularDistribution *= DistributionGGX(R, NdotH);

	//Geometric Shadow
	float GeometricShadow = 1;
	GeometricShadow *= CookTorrenceGeometricShadowingFunction(NdotL, NdotV, VdotH, NdotH);

	//Schlick Fresnel
	vec3 FresnelFunction = u_Material.SpecularColor;
	FresnelFunction *=  SchlickIORFresnelFunction(ior, LdotH);

	vec3 specularity = (SpecularDistribution * FresnelFunction * GeometricShadow) / (4 * (  NdotL * NdotV));

	//Light ending
	vec3 lightingModel = (diffuseColor + specularity);
	//lightingModel *= NdotL;
	lightingModel *= attenuation;

	o_FragColor = vec4(lightingModel, 1.0);
}