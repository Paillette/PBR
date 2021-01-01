#pragma once

#include <cstdint>

// todo: mettre dans un fichier common
//#define GLEW_STATIC
#include <GL/glew.h>

enum BufferType
{
	VBO,
	IBO,
	MAX
};

uint32_t CreateBufferObject(BufferType type, const size_t size, const void* data);

void DeleteBufferObject(uint32_t& BO);

// notez que le format de donnee interne (GL_RGBA8) et image (GL_RGBA + GL_UNSIGNED_BYTE)
// sont predefinis. Idem pour le filtrage qui est bilineaire. A vous de generaliser cette fonction.
// Pensez egalement aux formats internes SRGB qui effectuent automatiquement la decompression du gamma
uint32_t CreateTextureRGBA(const uint32_t width, const uint32_t height, const void* data, bool enableMipmaps = false);