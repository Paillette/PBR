
#include "OpenGLcore.h"

uint32_t CreateBufferObject(BufferType type, const size_t size, const void* data)
{
	uint32_t BO;
	glGenBuffers(1, &BO);
	GLenum target;
	switch (type) 
	{
	case BufferType::IBO:
		target = GL_ELEMENT_ARRAY_BUFFER;
		break;
	default:
	case BufferType::VBO: 
		target = GL_ARRAY_BUFFER; 
		break;
	}
	glBindBuffer(target, BO);
	glBufferData(target, size, data, GL_STATIC_DRAW);
	return BO;
}

void DeleteBufferObject(uint32_t& BO) {
	glDeleteBuffers(1, &BO);
	BO = 0;
}

// notez que le format de donnee interne (GL_RGBA8) et image (GL_RGBA + GL_UNSIGNED_BYTE)
// sont predefinis. Idem pour le filtrage. A vous de generaliser cette fonction.
// Pensez egalement aux formats internes SRGB qui effectuent automatiquement la decompression du gamma
uint32_t CreateTextureRGBA(const uint32_t width, const uint32_t height, const void* data, bool enableMipmaps)
{
	uint32_t textureID;

	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (enableMipmaps) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	}
	return textureID;
}