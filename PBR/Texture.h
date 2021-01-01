#pragma once

#include <string>
#include <vector>

struct Texture
{
	std::string name;
	uint32_t id;
	//uint8_t* albedo;
	//int width;
	//int height;
	//int bpp;

	static uint32_t LoadTexture(const char* path);
	
	// version tres basique d'un texture manager
	static std::vector<Texture> textures;
	static void SetupManager();
	static uint32_t CheckExist(const char* path);
	static void PurgeTextures();
};