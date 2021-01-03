#pragma once

#include "mat4.h"

struct Material
{
	vec3 ambientColor;
	vec3 diffuseColor;
	vec3 specularColor;
	float shininess;
	uint32_t ambientTexture;
	uint32_t diffuseTexture;
	uint32_t specularTexture;
	uint32_t metallicTexture;
	uint32_t normalTexture;

	static Material defaultMaterial;
};

