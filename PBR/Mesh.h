#pragma once

#include "Vertex.h"
#include "Material.h"

struct SubMesh
{
	uint32_t VAO;	// notez qu'il faut créer un VAO par SubMesh (VBO) quand bien meme on utilise le meme shader
	uint32_t VBO;	// ceci parceque l'identifiant du VBO est logiquement different a chaque fois
	uint32_t IBO;
	uint32_t verticesCount;
	uint32_t indicesCount;
	int32_t materialId;
};

// J'utilise volontairement des pointeurs plutôt que des std::vector afin d'insister 
// sur les problématiques d'allocation et surtout déallocation mémoire (cf. Destroy)
struct Mesh
{
	SubMesh* meshes;
	uint32_t meshCount;
	Material* materials;
	uint32_t materialCount;

	void Destroy();

	static bool ParseFBX(Mesh* obj, const char* filepath);
};


