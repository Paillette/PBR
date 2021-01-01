#pragma once

struct Framebuffer
{
	uint32_t FBO;
	uint32_t colorBuffer;
	uint32_t depthBuffer;
	uint16_t width;
	uint16_t height;

	Framebuffer() : FBO(0), colorBuffer(0), depthBuffer(0) {}

	void CreateFramebuffer(const uint32_t w, const uint32_t h, bool useDepth = false);

	void DestroyFramebuffer();

	void EnableRender();

	static void RenderToBackBuffer(const uint32_t w = 0, const uint32_t h = 0);
};