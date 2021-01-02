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
    float roughnessSqr = roughness * roughness;
    float NdotHSqr = (NdotH * roughnessSqr - NdotH) * NdotH + 1.0;
    return roughnessSqr / (NdotHSqr * NdotHSqr * PI);

}

//GSF
float CookTorrenceGeometricShadowingFunction (float NdotL, float NdotV, float VdotH, float NdotH)
{
	float Gs = min(1.0, min(2*NdotH*NdotV / VdotH, 2*NdotH*NdotL / VdotH));
	return Gs;
}

// Fresnel
vec3 FresnelTerm(vec3 specularColor, float vdoth)
{
	vec3 fresnel = specularColor + (1. - specularColor) * pow((1. - vdoth), 5.);
	return fresnel;
}

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
	//const vec3 L[2] = vec3[2](normalize(vec3(0.3, 0., 0.8)), normalize(vec3(0.0, 0.0, -1.0)));
	//const vec3 lightColor[2] = vec3[2](vec3(1.0, 1.0, 1.0), vec3(0.5, 0.5, 0.5));
	
	vec3 lightDir = normalize(vec3(.3, .0, .8));
	vec3 lightColor = vec3(2.);
	const float attenuation = 1.0;

	vec3 N = normalize(v_Normal);
	vec3 viewDir = normalize(u_CameraPosition - v_Position);

	//---------------------------------->BASE COLOR
	vec4 baseTexel = texture2D(u_DiffuseTexture, v_TexCoords);
	//Gamma
	baseTexel.rgb = pow(baseTexel.rgb, vec3(2.2));
	vec3 baseColor = baseTexel.rgb;
	//whitout texture
	//vec3 baseColor = pow(vec3(0.6, 0.6, 0.6), vec3(2.2));

	vec3 halfVec = normalize(viewDir + lightDir);
	float NdotH = clamp(dot(N, halfVec), 0., 1.);
	float NdotL = clamp(dot(N, lightDir), 0., 1.);
	float NdotV = clamp(dot(N, viewDir), 0., 1.);
	float VdotH = clamp(dot(viewDir, halfVec), 0., 1.);
	float LdotH = clamp(dot(lightDir, halfVec), 0., 1.0);

	 float R = roughness * roughness;
	 R = max(.01, R);
 	 float f0 = calculateF0(NdotL, NdotV, LdotH, R);

	//NORMALE
	vec3 T = normalize(v_Tangent.xyz);
	vec3 B = normalize(cross(N, T) * v_Tangent.w);
	// TBN
	mat3 TBN = mat3(T, B, N);
	// N
	N = texture(u_NormalTexture, v_TexCoords).rgb * 2.0 - 1.0;
	N = normalize(TBN * N);

	vec3 ambientColor = baseColor * u_Material.AmbientColor;
	vec3 indirectColor = ambientColor;


	//---------------------------------------------------Diffuse Color
	 //if metallic == 1 : baseColor = 0.
     vec3 diffuseColor = baseColor * (1.0 - metallic);
	 vec3 diffuse = vec3(0);
	 diffuse += diffuseColor * lightColor * NdotL;
	 diffuse *= AO;

	 diffuse *= f0;

	 //------------------------------------------------Specular
	 vec3 specular = vec3(0.0);
	 //GGX NDF
	 vec3 SpecularDistribution = vec3(1.0); //u_Material.SpecularColor;
	 SpecularDistribution *= DistributionGGX(R, NdotH);
	 
	 //Geometric Shadow
	 vec3 GeometricShadow = vec3(1.0);
	 GeometricShadow *= CookTorrenceGeometricShadowingFunction(NdotL, NdotV, VdotH, NdotH);
	 
	 //Schlick Fresnel
	 vec3 FresnelFunction = vec3(1.0);//u_Material.SpecularColor;
	 FresnelFunction *=  SchlickIORFresnelFunction(ior, LdotH);

	 vec3 specularity = lightColor * FresnelFunction * (SpecularDistribution * GeometricShadow * PI * NdotL);
	 specularity *= clamp(pow(NdotV + AO, roughness * roughness) - 1. + AO, 0., 1.);

	 //Light ending
	 vec3 lightingModel = (diffuse + specularity);
	 
	 o_FragColor = vec4(lightingModel , 1.0);
}