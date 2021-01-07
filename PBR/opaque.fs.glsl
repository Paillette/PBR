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
layout(binding = 3) uniform samplerCube u_cubeMap;

float PI = 3.1416;

//PBR
//https://www.jordanstevenstechart.com/physically-based-rendering
//Real Shading in Unreal Engine 4 by Brian Karis, Epic Games
//Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max( denom, 0.00001f );
}

//Geometric Function
float GeometrySchlickGGX(float NdotV, float k)
{
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return nom / denom;
}
  
float GeometrySmith(vec3 N, vec3 V, vec3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);
	
    return ggx1 * ggx2;
}

// Fresnel
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

void main(void)
{
//--------------------------------------> Light Pos 
    //(could be passed to shader)
	vec3 lightPos = normalize(vec3(10., 10., 10.));
	vec3 lightColor = vec3(1.);
	vec3 L = normalize(lightPos - v_Position);

//---------------------------------------> AO / Roughness / Metallic Texture
	//vec3 ORMLinear = pow(texture(u_ORMTexture, v_TexCoords).rgb, vec3(2.2));
	vec3 ORMLinear = texture(u_ORMTexture, v_TexCoords).rgb;
	float AO = ORMLinear.r;
	float roughness = ORMLinear.g;
	float metallic = ORMLinear.b;

	//without texture
	AO = 1.0f;
	roughness = 1.0f;
	metallic = 0.0f;
//---------------------------------------> View direction (V)
	vec3 viewDir = normalize(u_CameraPosition - v_Position);

//----------------------------------------> Normale (N)
	vec3 N = normalize(v_Normal);
	vec3 R = reflect(viewDir, N); 
	//Tangente
	vec3 T = normalize(v_Tangent.xyz);
	vec3 B = normalize(cross(N, T) * v_Tangent.w);
	// TBN
	mat3 TBN = mat3(T, B, N);
	//with texture
    //N = texture(u_NormalTexture, v_TexCoords).rgb * 2.0 - 1.0;
	//N = normalize(TBN * N);

//-----------------------------------------> Lambertian Diffuse BRDF
	//vec3 baseColor = texture2D(u_DiffuseTexture, v_TexCoords).rgb;
	//whitout texture
	vec3 baseColor = vec3(0.5, 0.0, 0.0);
	//baseColor = baseColor * 1.0 / PI;

//----------------------------------------> Specular reflectance at normal incidence
	vec3 f0 = vec3(0.04);
	f0 = mix(f0, baseColor, metallic);

	float NdotV = clamp(dot(N, viewDir), 0., 1.);

//---------------------------------------> Cook-Torrance reflectance equation
	vec3 reflectance = vec3(0.);
	//per light (only one here)
	for(int i = 0; i < 1; i++)
	{
		vec3 halfVec = normalize(viewDir + L);
		float dist = length(lightPos - v_Position);

		float attenuation = 1.0 / pow(dist, 2.0);
		vec3 radiance = lightColor * attenuation;

		float NdotH = clamp(dot(N, halfVec), 0., 1.);
		float NdotL = clamp(dot(N, L), 0., 1.);
		float VdotH = clamp(dot(viewDir, halfVec), 0., 1.);
		float LdotH = clamp(dot(L, halfVec), 0., 1.0);
		float HdotV = clamp(dot(halfVec, viewDir), 0., 1.0);

		//GGX NDF
		float SpecularDistribution = DistributionGGX(N, halfVec, roughness);
		//Geometric Shadow
		float GeometricFunction = GeometrySmith(N, viewDir, L, roughness);
		//fresnel
		vec3 Fresnel = fresnelSchlick(HdotV, f0);

		vec3 specularity = ( Fresnel * SpecularDistribution * GeometricFunction )
		                   / (4.0 * NdotV * NdotL + 0.001);
		
		//coefficient
		vec3 ks = Fresnel;
		vec3 kD = vec3(1.0) - ks;
		kD *= 1.0 - metallic;

		//reflectance += ( kD * baseColor / PI + specularity) * radiance * NdotL;
		reflectance = vec3(SpecularDistribution);
	}

//---------------------------------------> Cubemap
	vec3 irradiance = texture(u_cubeMap, R).rgb;

//--------------------------------------->DiffuseColor
    vec3 ambient = vec3(0.03) * baseColor * AO;
    vec3 color = ambient + reflectance;
	
	color = color / ( color + vec3(1.0));
	//gamma correction 
	color = pow(color, vec3(1.0 / 2.2));

	o_FragColor = vec4(reflectance, 1.0);
}