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
uniform vec3 u_albedo;
uniform float u_roughness;
uniform float u_metallic;
uniform bool u_displaySphere;

layout(binding = 0) uniform sampler2D u_DiffuseTexture;
layout(binding = 1) uniform sampler2D u_NormalTexture;
layout(binding = 2) uniform sampler2D u_ORMTexture;
layout(binding = 3) uniform samplerCube u_cubeMap;
layout(binding = 4) uniform samplerCube u_radianceCubeMap;
layout(binding = 5) uniform samplerCube u_irradianceCubeMap;


float PI = 3.1416;

vec3 TBNnormal()
{
    vec3 tangentNormal = texture(u_NormalTexture, v_TexCoords).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(v_Position);
    vec3 Q2  = dFdy(v_Position);
    vec2 st1 = dFdx(v_TexCoords);
    vec2 st2 = dFdy(v_TexCoords);

    vec3 N   = normalize(v_Normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}


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

    return nom / max( denom, 0.000001f );
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

// Fresnel in reflectance 
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

//Fresnel for environnement
//https://learnopengl.com/PBR/IBL/Specular-IBL
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
} 

//IBL
// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
vec3 EnvBRDFApprox(vec3 specularColor, float roughness, float ndotv)
{
	const vec4 c0 = vec4(-1, -0.0275, -0.572, 0.022);
	const vec4 c1 = vec4(1, 0.0425, 1.04, -0.04);
	vec4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
	vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
	return specularColor * AB.x + AB.y;
}

vec3 EnvRemap(vec3 c)
{
	return pow(2. * c, vec3(2.2));
}

void main(void)
{
//--------------------------------------> Light Pos 
    //(could be passed to shader)
	vec3 lightPos = normalize(vec3(20, 50., 100.));
	vec3 lightColor = vec3(1.);
	vec3 L = normalize(lightPos - v_Position);

//---------------------------------------> AO / Roughness / Metallic Texture
	float AO;
	float roughness;
	float metallic;

	if(u_displaySphere) //Sphere
	{
		AO = 1.0f;
		roughness = u_roughness;
		metallic = u_metallic;
	}
	else //Other object
	{
		vec3 ORMLinear = texture(u_ORMTexture, v_TexCoords).rgb;
		AO = ORMLinear.r;
		roughness = ORMLinear.g;
		metallic = ORMLinear.b;
	}
//---------------------------------------> View direction (V)
	vec3 viewDir = normalize(u_CameraPosition - v_Position);

//----------------------------------------> Normale (N)
	vec3 N;
	if(u_displaySphere) //Sphere
	{
		N = normalize(v_Normal);
	}
	else //Other object
	{
		N = TBNnormal();
	}

	vec3 R = reflect(viewDir, N); 

//-----------------------------------------> Lambertian Diffuse BRDF
	vec3 baseColor;
	if(u_displaySphere) //Sphere
	{
		baseColor = u_albedo;
	}
	else //Other object
	{
		baseColor = texture2D(u_DiffuseTexture, v_TexCoords).rgb;
	}

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

		reflectance += ( kD * baseColor / PI + specularity) * radiance * NdotL;
	}

//---------------------------------------> Cubemap
	vec3 Fresnel = fresnelSchlickRoughness(NdotV, f0, roughness);

	//coefficient
	vec3 ks = Fresnel;
	vec3 kD = vec3(1.0) - ks;
	kD *= 1.0 - metallic;

//--------------------------------------> Cubemap
    //radiance and irradiance textures created in a software (cmftStudio) pass to shader 
	vec3 cubeMap = texture(u_cubeMap, N).rgb;
	vec3 radiance = texture(u_radianceCubeMap, N).rgb;
	vec3 irradiance = texture(u_irradianceCubeMap, N).rgb;

	//Mix between textures with roughness
	vec3 env = mix(cubeMap, radiance, clamp(pow(roughness, 2.0) * 4., 0., 1.));
	env = mix(env, irradiance, clamp((pow(roughness, 2.0) - 0.25 ) / 0.75, 0., 1.));

	vec3 envSpecularColor = EnvBRDFApprox(vec3(1.), pow(roughness, 2.0), NdotV);

	vec3 diffuse = irradiance * baseColor;
	vec3 specular = envSpecularColor * env * Fresnel;

//--------------------------------------->DiffuseColor
	vec3 ambient = (kD * diffuse + specular) * AO;
    vec3 color = ambient + reflectance;

	color = color / ( color + vec3(1.0));

	//gamma correction 
	color = pow(color, vec3(1.0 / 2.2));

	o_FragColor = vec4(color, 1.0);
}