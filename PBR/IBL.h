#pragma once

#include "OpenGLcore.h"
#include <GLFW/glfw3.h>

#if defined(_WIN32) && defined(_MSC_VER)
#pragma comment(lib, "glfw3dll.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "libfbxsdk.lib")
#elif defined(__APPLE__)
#elif defined(__linux__)
#endif

#include "stb_image.h"
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../common/GLShader.h"

uint32_t LoadCubemap(const char* pathes[6]);
uint32_t CreateCubemap();
void GenerateMipmaps(uint32_t& map);
void GenerateIrradiance(uint32_t& irradianceMap, uint32_t& fbo, uint32_t& rbo);
void SolveDiffuseIntegrale(GLShader irradianceShader, uint32_t& cubeMap, uint32_t& irradianceMap, uint32_t& fbo, unsigned int cubeVAO, unsigned int cubeVBO);
void CreatePrefilteredMap(uint32_t& prefilteredMap);
void GeneratePrefilteredMap(uint32_t& prefilteredMap, uint32_t& cubeMap, GLShader prefilterShader, uint32_t& fbo, uint32_t& rbo);
void GenerateBRDFLutTexture(uint32_t& brdfLUTTexture, GLShader brdfShader, uint32_t& fbo, uint32_t& rbo);
void renderCube(unsigned int cubeVAO, unsigned int cubeVBO);