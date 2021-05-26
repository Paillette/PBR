#version 430

in vec3 v_Position;
in vec3 v_Normal;
in vec2 v_TexCoords;
in vec4 v_Tangent;

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
uniform vec3 u_albedo;
uniform float u_roughness;
uniform float u_metallic;
uniform bool u_displaySphere;
uniform bool u_displayIBL;
uniform sampler2D brdfLUT;

layout(binding = 0) uniform sampler2D u_DiffuseTexture;
layout(binding = 1) uniform sampler2D u_NormalTexture;
layout(binding = 2) uniform sampler2D u_ORMTexture;
layout(binding = 3) uniform samplerCube u_cubeMap;
layout(binding = 4) uniform samplerCube u_radianceCubeMap;
layout(binding = 5) uniform samplerCube u_irradianceCubeMap;
layout(binding = 6) uniform sampler2D u_EmissiveTexture;
layout(binding = 7) uniform samplerCube u_prefilteredmap;
layout(binding = 8) uniform sampler2D u_brdfLUT;


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

//------------------------------------------------->IBL
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

vec2 hammersley(uint i, uint N) 
{
	// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
	uint bits = (i << 16u) | (i >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float rdi = float(bits) * 2.3283064365386963e-10;
	return vec2(float(i) /float(N), rdi);
}

//https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
vec3 ImportanceSampleGGX(vec2 Xi,float Roughness, vec3 N )
{
	float a = Roughness * Roughness;
	float Phi = 2 * PI * Xi.x;
	float CosTheta = sqrt( (1 - Xi.y) / ( 1 + (a*a - 1) * Xi.y ) );
	float SinTheta = sqrt( 1 - CosTheta * CosTheta );
	vec3 H;
	H.x = SinTheta *cos( Phi );
	H.y = SinTheta *sin( Phi );
	H.z = CosTheta;
	
	vec3 UpVector = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
	vec3 TangentX = normalize(cross( UpVector, N ) );
	vec3 TangentY = cross( N, TangentX );
	return TangentX * H.x + TangentY * H.y + N * H.z;

}

// From the filament docs. Geometric Shadowing function
// https://google.github.io/filament/Filament.html#toc4.4.2
float G_Smith(float NoV, float NoL, float roughness)
{
	float k = (roughness * roughness) / 2.0;
	float GGXL = NoL / (NoL * (1.0 - k) + k);
	float GGXV = NoV / (NoV * (1.0 - k) + k);
	return GGXL * GGXV;
}

vec2 IntegratedBRDF(float Roughness, float NdotV, vec3 N)
{
	vec3 V;
	V.x = sqrt( 1.0 - NdotV * NdotV);
	V.y = 0.;
	V.z = NdotV;

	float A = 0;
	float B = 0;

	const uint NumSamples = 20;
	
	for(uint i = 0; i < NumSamples; i++ )
	{
		vec2 Xi = hammersley( i, NumSamples );
		vec3 H = ImportanceSampleGGX( Xi, Roughness, N );
		vec3 L = 2.0 * dot( V, H ) * H - V;

		float NoL = clamp(dot( N, L ), 0., 1. );
		float NoH = clamp(dot( N, H ), 0., 1. );
		float VoH = clamp(dot( V, H ), 0., 1. );
		
		//https://www.shadertoy.com/view/3lXXDB
		if( NoL > 0 )
		{
			float G = G_Smith( Roughness, NdotV, NoL );
			float Fc = pow( 1 - VoH, 5.0 );
			A += (1.0 - Fc) * G;
			B += Fc = G;
		}
	}
		return 4.0 * vec2(A, B) / float(NumSamples);
}

// -------------------------------------------------->OLD BLINN PHONG SHADING
float BlinnPhong(vec3 N, vec3 L, vec3 V, float shininess)
{
	vec3 H = normalize(L + V);
	return pow(max(0.0, dot(N, H)), shininess);
}

float Lambert(vec3 N, vec3 L)
{
	return max(0.0, dot(N, L));
}

void main(void)
{
//--------------------------------------> Light Pos 
    //(could be passed to shader)
	vec3 lightPos = normalize(vec3(50, 50., 50.));
	vec3 lightColor = vec3(5.);
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
	baseColor = pow(baseColor, vec3(2.2));

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

		float attenuation = 1.0 /  pow(dist, 2.0);
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
	vec3 cubeMap = texture(u_cubeMap, normalize(v_Normal)).rgb;
	vec3 radiance = texture(u_radianceCubeMap, normalize(v_Normal)).rgb;
	vec3 irradiance = texture(u_irradianceCubeMap, normalize(v_Normal)).rgb;

	//Mix between radiances textures with roughness
	vec3 env = mix(cubeMap, radiance, clamp(pow(roughness, 2.0) * 4.5, 0., 1.));
	env = mix(env, irradiance, clamp((pow(roughness, 2.0)), 0., 1.));
	vec3 envBRDF = EnvBRDFApprox(vec3(0.01), pow(roughness, 2.0), NdotV);

	float amountIBL = 0.3;
	vec3 specular = vec3(0.0);
	//Fake IBL
	if(u_displayIBL)
		specular = (env * ( Fresnel * envBRDF.x + envBRDF.y)) * amountIBL;
//--------------------------------------->IBL
	const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(u_prefilteredmap, R,  roughness * MAX_REFLECTION_LOD).rgb;    
    vec2 brdf  = texture(u_brdfLUT, vec2(max(dot(N, viewDir), 0.0), roughness)).rg;
    specular = prefilteredColor * (Fresnel * brdf.x + brdf.y);


//--------------------------------------->DiffuseColor
	vec3 diffuse = baseColor;
	vec3 ambient = vec3(0.03) * diffuse;

	if(!u_displaySphere)
		amountIBL = 0.02;

	//Fake IBL
	/*if(u_displayIBL)
	{
		diffuse = irradiance * baseColor;
		ambient = (kD * diffuse + specular) * amountIBL;

	}*/

	ambient *= AO;
    vec3 color = ambient + reflectance;
	color = color / ( color + vec3(1.0));

	ambient = (kD * diffuse + specular) * AO;
	color = ambient + reflectance;

	//emissive
	vec3 emissive = texture(u_EmissiveTexture, v_TexCoords).rgb;
	//Need to be multiply by emissiveIntensity
	emissive *= 10.;
	color += emissive;

	//gamma correction 
	color = pow(color, vec3(1.0 / 2.2));

	//Bloom
	float brightness = dot(color, vec3(0.21, 0.71, 0.072));
	if(brightness > 1.0)
		o_BrightnessColor = vec4(color, 1.0);
	else
		o_BrightnessColor = vec4(1., 0., 0., 1.);
	
	
	o_FragColor = vec4(irradiance, 1.0);
}