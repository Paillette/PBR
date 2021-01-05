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
uniform float u_Time;

layout(binding = 0) uniform sampler2D u_DiffuseTexture;
layout(binding = 1) uniform sampler2D u_NormalTexture;
layout(binding = 2) uniform sampler2D u_ORMTexture;

float PI = 3.1416;

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

float SchlickIORFresnelFunction(float ior ,float LdotH)
{
    float f0 = pow(ior-1,2)/pow(ior+1, 2);
    return f0 + (1-f0) * SchlickFresnel(LdotH);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main(void)
{
	float ior = 1.3;

	//vec3 ORMLinear = pow(texture(u_ORMTexture, v_TexCoords).rgb, vec3(2.2));
	vec3 ORMLinear = texture(u_ORMTexture, v_TexCoords).rgb;
	float AO = ORMLinear.r;
	float roughness = ORMLinear.g;
	float metallic = ORMLinear.b;

	//lights
	//const vec3 L[2] = vec3[2](normalize(vec3(0.3, 0., 0.8)), normalize(vec3(0.0, 0.0, -1.0)));
	//const vec3 lightColor[2] = vec3[2](vec3(1.0, 1.0, 1.0), vec3(0.5, 0.5, 0.5));
	
	vec3 lightDir = normalize(vec3(.3, .0, .8));
	vec3 lightColor = vec3(1.);
	const float attenuation = 1.0;

	vec3 N = normalize(v_Normal);
	vec3 viewDir = normalize(u_CameraPosition - v_Position);

	//NORMALE
	vec3 T = normalize(v_Tangent.xyz);
	vec3 B = normalize(cross(N, T) * v_Tangent.w);
	// TBN
	mat3 TBN = mat3(T, B, N);
	// N
    N = texture(u_NormalTexture, v_TexCoords).rgb * 2.0 - 1.0;
	N = normalize(TBN * N);

	//---------------------------------->BASE COLOR
	vec3 baseColor = texture2D(u_DiffuseTexture, v_TexCoords).rgb;

	//whitout texture
	//vec3 baseColor = pow(vec3(0.6, 0.6, 0.6), vec3(2.2));

	vec3 halfVec = normalize(viewDir + lightDir);
	float NdotH = clamp(dot(N, halfVec), 0., 1.);
	float NdotL = clamp(dot(N, lightDir), 0., 1.);
	float NdotV = clamp(dot(N, viewDir), 0., 1.);
	float VdotH = clamp(dot(viewDir, halfVec), 0., 1.);
	float LdotH = clamp(dot(lightDir, halfVec), 0., 1.0);

	float R = pow(roughness, 2.0);

	vec3 f0 = vec3(0.04);
	f0 = mix(f0, baseColor, metallic);

	vec3 ambientColor = baseColor * vec3(.01);
	vec3 indirectColor = ambientColor;

	//---------------------------------------------------Diffuse Color
	 //if metallic == 1 : baseColor = 0.
     vec3 diffuseColor = baseColor * (1.0 - metallic);
	 vec3 diffuse = vec3(0.);
	 diffuse += diffuseColor * lightColor;
	 diffuse *= AO;

	 diffuse += indirectColor;

	 //------------------------------------------------Specular
	 vec3 specular = mix(u_Material.SpecularColor, baseColor, metallic * 0.5);
	 //GGX NDF
	 vec3 SpecularDistribution = specular;
	 SpecularDistribution *= DistributionGGX(R, NdotH);
	 
	 //Geometric Shadow
	 vec3 GeometricShadow = vec3(1.0);
	 GeometricShadow *= CookTorrenceGeometricShadowingFunction(NdotL, NdotV, VdotH, NdotH);
	 
	 //Schlick Fresnel
	 vec3 FresnelFunction = specular;
	 //FresnelFunction *=  SchlickIORFresnelFunction(ior, LdotH);
	 FresnelFunction = fresnelSchlick(NdotH, f0);

	 vec3 specularity = lightColor * FresnelFunction * (SpecularDistribution * GeometricShadow * PI * NdotL);
	 specularity *= clamp(pow(NdotV + AO, R * R) - 1. + AO, 0., 1.);

	 //Light ending
	 vec3 lightingModel = (diffuse + specularity);
	 
	 o_FragColor = vec4(lightingModel, 1.0);
}