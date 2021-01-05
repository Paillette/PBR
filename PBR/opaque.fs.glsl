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

//PBR implementation gltf
//https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#complete-model

//Specular NDF : GGX microfacet distribution 
//TrowbridgeReitzDistribution
float DistributionGGX(float roughness, float NdotH)
{
    float roughnessSqr = roughness * roughness;
    float NdotHSqr = (NdotH * NdotH) * (roughnessSqr - 1.0) + 1.0;
    return roughnessSqr / (NdotHSqr * NdotHSqr * PI);
}

//Geometric shadow
//Smith joint 
float V_GGX(float NdotL, float NdotV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}

// Fresnel Schlick
vec3 fresnel(vec3 f0, vec3 f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

void main(void)
{
	float ior = 1.3;

	vec3 ORMLinear = pow(texture(u_ORMTexture, v_TexCoords).rgb, vec3(2.2));
	//vec3 ORMLinear = texture(u_ORMTexture, v_TexCoords).rgb;
	float AO = ORMLinear.r;
	float roughness = ORMLinear.g;
	float metallic = ORMLinear.b * 10;

	vec3 lightDir = normalize(vec3(.3, .0, .8));
	vec3 lightColor = vec3(1.);
	const float attenuation = 1.0;

	vec3 N = normalize(v_Normal);
	vec3 viewDir = normalize(u_CameraPosition - v_Position);

	//---------------------------------->NORMALE
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
	float VdotN = clamp(dot(viewDir, N), 0., 1.);
	float LdotN = clamp(dot(lightDir, N), 0., 1.0);

	//Diffuse
	const vec3 dielectricSpecular = vec3(0.04);
	//if metallic == 1 : baseColor = 0.
    vec3 cDiff = mix(baseColor * (1.0 - dielectricSpecular), vec3(0.0), metallic);
	//F0
	vec3 f0 = mix(dielectricSpecular, baseColor, metallic);
	//Roughness
	float alpha = pow(roughness, 2.0);
	float alphaSq = pow(alpha, 20.);

	vec3 F = f0 + (vec3(1.0) - f0) * pow((1.0 - abs(VdotH)), 5.0);

	vec3 ambientColor = baseColor * vec3(.01);
	vec3 indirectColor = ambientColor;

	//---------------------------------------------------Diffuse Color
	 vec3 diffuse = ( vec3(1.) - F ) * ( 1 / PI ) * cDiff;
	 //diffuse += diffuseColor * lightColor;
	 //diffuse *= AO;

	 //diffuse += indirectColor;

	 //------------------------------------------------Specular
	 float G = V_GGX(NdotL, NdotV, alphaSq);
	 float D = DistributionGGX(roughness, NdotH);

	 vec3 specular = ( F * G * D ) / ( 4.0 * NdotL * NdotV );

	 //Light ending
	 vec3 lightingModel = diffuse;// + specular;
	 
	 o_FragColor = vec4(lightingModel, 1.0);
}