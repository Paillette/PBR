#pragma once
#include "IBL.h"

//1. setup cubemap
//2. generate mipmap de la cubemap
//3. creer une irradiance map
//4. rescale fbo a la taille de la map
//5. resoudre la diffuse int�grale
//6. cr�er une pr�filtered cubemap
//7. rescale le fbo a la taille de la prefiltered
//8. recalculer la cubemap avec un monte carlo simulation
//9. 2D LUT
//10. reconfigurer le FBO a la taille

glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

//Toute les orientation des h�misphere
glm::mat4 captureViews[] =
{
	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
};

uint32_t LoadCubemap(const char* pathes[6])
{
	unsigned int cubemapTexture;
	glGenTextures(1, &cubemapTexture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);

	int width, height, c;
	for (int i = 0; i < 6; i++)
	{
		uint8_t* data = stbi_load(pathes[i], &width, &height, &c, STBI_rgb_alpha);
		if (data)
		{
			std::cout << "Cubemap texture load at path: " << pathes[i] << std::endl;
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << GL_TEXTURE_CUBE_MAP_POSITIVE_X + i << std::endl;
		}
		stbi_image_free(data);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	return cubemapTexture;
}

uint32_t CreateCubemap()
{
	unsigned int cubeMap;
	glGenTextures(1, &cubeMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);

	for (unsigned int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return cubeMap;
}

//Irradiance
void GenerateMipmaps(uint32_t& map)
{
	glBindTexture(GL_TEXTURE_CUBE_MAP, map);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

void GenerateIrradiance(uint32_t& irradianceMap, uint32_t& fbo, uint32_t& rbo)
{
	irradianceMap = CreateCubemap();
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
}

void SolveDiffuseIntegrale(GLShader irradianceShader, uint32_t& cubeMap, uint32_t& irradianceMap, uint32_t& fbo)
{
	int32_t program = irradianceShader.GetProgram();
	glUseProgram(program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);

	uint32_t locProjection = glGetUniformLocation(program, "projection");
	glUniformMatrix4fv(locProjection, 1, false, glm::value_ptr(captureProjection[0]));

	glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	for (unsigned int i = 0; i < 6; ++i)
	{
		uint32_t locView = glGetUniformLocation(program, "view");
		glUniformMatrix4fv(locView, 1, false, glm::value_ptr(captureViews[0]));

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CreatePrefilteredMap(uint32_t& prefilteredMap)
{
	glGenTextures(1, &prefilteredMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilteredMap);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
	}
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minification filter to mip_linear 
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

void GeneratePrefilteredMap(uint32_t& prefilteredMap, uint32_t& cubeMap, GLShader prefilterShader, uint32_t& fbo, uint32_t& rbo)
{
	uint32_t program = prefilterShader.GetProgram();
	glUseProgram(program);

	uint32_t cubeMapLocation = glGetUniformLocation(program, "environmentMap");
	glUniform1i(cubeMapLocation, 0);
	uint32_t projectionLocation = glGetUniformLocation(program, "projection");
	glUniformMatrix4fv(projectionLocation, 1, false, &captureProjection[0][0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	unsigned int maxMipLevels = 5;
	for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
	{
		// reisze framebuffer according to mip-level size.
		unsigned int mipWidth = 128 * std::pow(0.5, mip);
		unsigned int mipHeight = 128 * std::pow(0.5, mip);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
		glViewport(0, 0, mipWidth, mipHeight);

		float roughness = (float)mip / (float)(maxMipLevels - 1);
		uint32_t roughnessLocation = glGetUniformLocation(program, "roughness");
		glUniform1f(roughnessLocation, roughness);

		for (unsigned int i = 0; i < 6; ++i)
		{
			uint32_t locView = glGetUniformLocation(program, "view");
			glUniformMatrix4fv(locView, 1, false, &captureViews[i][0][0]);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilteredMap, mip);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GenerateBRDFLutTexture(uint32_t& brdfLUTTexture, GLShader brdfShader, uint32_t& fbo, uint32_t& rbo)
{
	glGenTextures(1, &brdfLUTTexture);

	// pre-allocate enough memory for the LUT texture.
	glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
	// be sure to set wrapping mode to GL_CLAMP_TO_EDGE
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

	glViewport(0, 0, 512, 512);
	glUseProgram(brdfShader.GetProgram());
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}