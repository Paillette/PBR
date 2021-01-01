
#include <iostream>

#include "OpenGLcore.h"
#include "Framebuffer.h"

void Framebuffer::CreateFramebuffer(const uint32_t w, const uint32_t h, bool useDepth)
{
	width = (uint16_t)w;
	height = (uint16_t)h;

	//default : 0
	glActiveTexture(GL_TEXTURE0);
	//color buffer creation
	glGenTextures(1, &colorBuffer);
	glBindTexture(GL_TEXTURE_2D, colorBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	if (useDepth) {
		glGenTextures(1, &depthBuffer);
		glBindTexture(GL_TEXTURE_2D, depthBuffer);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);

	if (useDepth) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthBuffer, 0);
	}

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Framebuffer invalide, code erreur = " << status << std::endl;
	}
}

void Framebuffer::DestroyFramebuffer()
{
	if (depthBuffer)
		glDeleteTextures(1, &depthBuffer);
	if (colorBuffer)
		glDeleteTextures(1, &colorBuffer);
	if (FBO)
		glDeleteFramebuffers(1, &FBO);
	depthBuffer = 0;
	colorBuffer = 0;
	FBO = 0;
}

void Framebuffer::EnableRender()
{
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	glViewport(0, 0, width, height);
}

void Framebuffer::RenderToBackBuffer(const uint32_t w, const uint32_t h)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (w != 0 && h != 0)
		glViewport(0, 0, w, h);
}