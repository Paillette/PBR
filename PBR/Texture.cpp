#include <iostream>

#include "texture.h" 

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "OpenGLcore.h"

uint32_t Texture::CheckExist(const char* path)
{
	// "ranged-for" du C++. Equivalent d'un "foreach" en C#
	for (Texture& tex : textures) {
		if (tex.name == path)
			return tex.id;
	}
	return 0;
}

void Texture::SetupManager()
{
	// création d'une texture par défaut 1x1 blanche
	if (textures.size() == 0) {
		const uint8_t data[] = { 255,255,255,255 };
		uint32_t textureID = CreateTextureRGBA(1, 1, data);
		textures.push_back({ "", textureID });
	}
}

void Texture::PurgeTextures()
{
	for (uint32_t i = 0; i < Texture::textures.size(); ++i)
	{
		glDeleteTextures(1, &Texture::textures[i].id);
	}
	// change le nombre d'element (size) à zero, mais conserve la meme capacite
	textures.clear();
	// force capacite = size
	textures.shrink_to_fit();
}

uint32_t Texture::LoadTexture(const char* filepath)
{
	uint32_t textureID = Texture::CheckExist(filepath);
	if (textureID > 0)
		return textureID;
	
	std::string relativePath = "../data/";
	relativePath += filepath;

	int width, height, c;
	uint8_t* data = stbi_load(filepath, &width, &height, &c, STBI_rgb_alpha);
	if (data == nullptr) {
		std::cout << "l'image " << relativePath << " est introuvable." << std::endl;
		// la premiere texture dans le texture manager est la texture par defaut blanche
		return textures[0].id;
	}

	textureID = CreateTextureRGBA(width, height, data, true);
	stbi_image_free(data);

	textures.push_back({ filepath, textureID });
	return textureID;
}

// version très basique d'un texture manager
std::vector<Texture> Texture::textures;