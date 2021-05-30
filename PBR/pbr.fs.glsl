#version 430

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoords;
in mat3 v_TBN; 

layout(location = 0) out vec4 o_FragColor;
layout(location = 1) out vec4 o_BrightnessColor;

struct Material {
	vec3 AmbientColor;
	vec3 DiffuseColor;
	float emissiveIntensity;
	vec3 SpecularColor;
	float Shininess;
};

layout(binding = 1) uniform Materials
{
	Material u_Material;
};

uniform vec3 u_CameraPosition;

//From UI
uniform vec3 u_albedo;
uniform float u_roughness;
uniform float u_metallic;
uniform bool u_displaySphere;
uniform bool u_displayAnisotropic;
uniform float u_lightIntensity;
uniform vec3 u_lightColor;
uniform float anisotropyIntensity;

//From FBX
layout(binding = 0) uniform sampler2D u_DiffuseTexture;
layout(binding = 1) uniform sampler2D u_NormalTexture;
layout(binding = 2) uniform sampler2D u_ORMTexture;
layout(binding = 6) uniform sampler2D u_EmissiveTexture;

//IBL
layout(binding = 3) uniform samplerCube u_cubeMap;
layout(binding = 4) uniform samplerCube u_radianceCubeMap;
layout(binding = 5) uniform samplerCube u_irradianceCubeMap;
layout(binding = 7) uniform samplerCube u_prefilteredmap;
layout(binding = 8) uniform sampler2D u_brdfLUT;


float PI = 3.141592;

vec3 TBNnormal()
{
    vec3 tangentNormal = texture(u_NormalTexture, v_TexCoords).xyz * 2.0 - 1.0;

    return normalize(v_TBN * tangentNormal);
}


//------------------------------------------------------------------>PBR
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

//-------------------------------------------------> Fake IBL : NOT USE (example below) 
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

//-------------------------------------------------> Anisotropy
//https://google.github.io/filament/Filament.md.html#materialsystem/anisotropicmodel
float D_GGX_Anisotropic(float at, float ab, float ToH, float BoH, float NoH) {
    // Burley 2012, "Physically-Based Shading at Disney"

    float a2 = at * ab;
    highp vec3 d = vec3(ab * ToH, at * BoH, a2 * NoH);
    highp float d2 = dot(d, d);
    float b2 = a2 / d2;
    return a2 * b2 * b2 * (1.0 / PI);
}


float V_SmithGGXCorrelated_Anisotropic(float at, float ab, float ToV, float BoV,
        float ToL, float BoL, float NoV, float NoL) {
    float lambdaV = NoL * length(vec3(at * ToV, ab * BoV, NoV));
    float lambdaL = NoV * length(vec3(at * ToL, ab * BoL, NoL));
    float v = 0.5 / (lambdaV + lambdaL);
    return v;
}


void main(void)
{
//--------------------------------------> Light Pos 
    //(could be passed to shader)
	vec3 lightPos = normalize(vec3(50, 50., 50.));
	vec3 lightColor = u_lightColor * u_lightIntensity;
	vec3 L = normalize(lightPos - v_Position);

//---------------------------------------> AO / Roughness / Metallic Texture
	float AO;
	float roughness;
	float metallic;

	if(u_displaySphere && !u_displayAnisotropic) //Sphere
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
	if(u_displaySphere && !u_displayAnisotropic) //Sphere
	{
		N = normalize(v_Normal);
	}
	else //Other object
	{
		N = TBNnormal();
	}

	vec3 R = reflect(-viewDir, N); 

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
	vec3 specularity = vec3(0);
	//per light (only one here)
	for(int i = 0; i < 1; i++)
	{
		vec3 halfVec = normalize(viewDir + L);
		float dist = length(lightPos - v_Position);

		float attenuation = 1.0 /  pow(dist, 2.0);
		vec3 radiance = lightColor * attenuation;

		//Lambert
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

		specularity = ( Fresnel * SpecularDistribution * GeometricFunction )
		                   / (4.0 * NdotV * NdotL + 0.001);
		//coefficient
		vec3 ks = specularity;
		vec3 kD = vec3(1.0) - ks;
		kD *= 1.0 - metallic;

		reflectance += ( kD * baseColor / PI + specularity) * radiance * NdotL;
		//------------------------------------------------------------------------> anisotropy
		if(u_displayAnisotropic)
		{
			//vec3 anisotropicDirection = vec3(1.0, 0.0, 0.0);
			//vec3 t = normalize(v_TBN * anisotropicDirection);
			//vec3 b = normalize(cross(v_Normal, t));
			
			vec3 t = normalize(v_TBN[0]);
			vec3 b = normalize(v_TBN[1]);

			float TdotV = dot(t, viewDir);
			float BdotH = dot(b, halfVec);
			float TdotH = dot(t, halfVec);
			float BdotV = dot(b, viewDir);
			float TdotL = dot(t, L);
			float BdotL = dot(b, L);

			float at = max(roughness * (1.0 + anisotropyIntensity), 0.001);
			float ab = max(roughness * (1.0 - anisotropyIntensity), 0.001);

			SpecularDistribution = D_GGX_Anisotropic(at, ab, TdotH, BdotH, NdotH);
			float Visibility = V_SmithGGXCorrelated_Anisotropic(at, ab, TdotV, BdotV, TdotL, BdotL, NdotV, NdotL);
			Fresnel = fresnelSchlick(LdotH, f0);

			reflectance = SpecularDistribution * Visibility * Fresnel;
		}
	}

//--------------------------------------->specular indirect : IBL
	vec3 Fresnel = fresnelSchlickRoughness(NdotV, f0, roughness);
	const float MAX_REFLECTION_LOD = 4.0;

	vec3 cubeMap = texture(u_cubeMap, normalize(v_Normal)).rgb;
    //radiance texture created in a software (cmftStudio) pass to shader : NOT USED HERE
	vec3 radiance = texture(u_radianceCubeMap, normalize(v_Normal)).rgb;
	vec3 irradiance = texture(u_irradianceCubeMap, normalize(v_Normal)).rgb;

	//Mix between radiances textures with roughness : can be used for simulating non accurate IBL
	/*vec3 env = mix(cubeMap, radiance, clamp(pow(roughness, 2.0) * 4.5, 0., 1.));
	env = mix(env, irradiance, clamp((pow(roughness, 2.0)), 0., 1.));
	vec3 envBRDF = EnvBRDFApprox(vec3(0.01), pow(roughness, 2.0), NdotV);*/

    vec3 prefilteredColor = textureLod(u_prefilteredmap, R,  roughness * MAX_REFLECTION_LOD).rgb;    
    vec2 brdf  = texture(u_brdfLUT, vec2(max(dot(N, viewDir), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (Fresnel * brdf.x + brdf.y);

//--------------------------------------->indirect diffuse
	vec3 diffuse = baseColor;
	vec3 indirectDiffuse = irradiance;
	diffuse *= indirectDiffuse;

	vec3 ks = specular;
	vec3 kD = vec3(1.0) - ks;
	kD *= 1.0 - metallic;

	vec3 ambient = (kD * diffuse + specular) * AO;
	vec3 color = ambient + ( reflectance * PI );

//-------------------------------------->emissive
	vec3 emissive = texture(u_EmissiveTexture, v_TexCoords).rgb;
	//Need to be multiply by emissiveIntensity
	emissive *= 10.;
	color += emissive;
	
	//tonemapping Reinhard - filmic
	color /= color + vec3(1.0);

	//gamma correction 
	color = pow(color, vec3(1.0 / 2.2));

	//Bloom : NOT USED YET
	float brightness = dot(color, vec3(0.21, 0.71, 0.072));
	if(brightness > 1.0)
		o_BrightnessColor = vec4(color, 1.0);
	else
		o_BrightnessColor = vec4(1., 0., 0., 1.);
	
	
	o_FragColor = vec4(color, 1.0);
}