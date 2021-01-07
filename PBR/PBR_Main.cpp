//
//
//

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

#include <iostream>
#include <fstream>

#define FBXSDK_SHARED 1
#include "fbxsdk.h"

#include "stb_image.h"

#include "../common/GLShader.h"
#include "mat4.h"
#include "Texture.h"
#include "Mesh.h"
#include "Framebuffer.h"

struct Application
{
	Mesh* object;
	uint32_t quadVAO;

	GLShader opaqueShader;
	GLShader copyShader;	

	int32_t width;
	int32_t height;

	uint32_t matrixUBO;

	uint32_t materialUBO;

	Framebuffer offscreenBuffer;

	//cubemap 
	uint32_t cubeMapID;
	GLShader g_skyboxShader;
	GLuint skyboxVAO, skyboxVBO;

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
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		return cubemapTexture;
	}

	void InitCubeMap()
	{
		const char* pathes[6] = {
			"../data/envmaps/pisa_posx.jpg",
			"../data/envmaps/pisa_negx.jpg",
			"../data/envmaps/pisa_posy.jpg",
			"../data/envmaps/pisa_negy.jpg",
			"../data/envmaps/pisa_posz.jpg",
			"../data/envmaps/pisa_negz.jpg"
		};

		cubeMapID = LoadCubemap(pathes);
	}

	void Initialize()
	{
		GLenum error = glewInit();
		if (error != GLEW_OK) {
			std::cout << "erreur d'initialisation de GLEW!"
				<< std::endl;
		}

		std::cout << "Version : " << glGetString(GL_VERSION) << std::endl;
		std::cout << "Vendor : " << glGetString(GL_VENDOR) << std::endl;
		std::cout << "Renderer : " << glGetString(GL_RENDERER) << std::endl;


		Texture::SetupManager();

		opaqueShader.LoadVertexShader("opaque.vs.glsl");
		opaqueShader.LoadFragmentShader("opaque.fs.glsl");
		opaqueShader.Create();
		copyShader.LoadVertexShader("copy.vs.glsl");
		copyShader.LoadFragmentShader("copy.fs.glsl");
		copyShader.Create();

		InitCubeMap();

		object = new Mesh();

		Mesh::ParseFBX(object, "model/TestSphere.fbx");

		glGenBuffers(1, &matrixUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, matrixUBO);
		glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(mat4), nullptr, GL_STREAM_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, matrixUBO);

		//bind UBO material
		glGenBuffers(1, &materialUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, materialUBO);
		glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(vec4), nullptr, GL_STREAM_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, materialUBO);

		int32_t program = opaqueShader.GetProgram();
		glUseProgram(program);
		int32_t positionLocation = 0; //  glGetAttribLocation(program, "a_Position");
		int32_t normalLocation = 1; // glGetAttribLocation(program, "a_Normal");
		int32_t texcoordsLocation = 2; // glGetAttribLocation(program, "a_TexCoords");
		int32_t tangentLocation = 3;

		for (uint32_t i = 0; i < object->meshCount; i++)
		{
			SubMesh& mesh = object->meshes[i];

			glGenVertexArrays(1, &mesh.VAO);
			glBindVertexArray(mesh.VAO);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.IBO);

			glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
			glVertexAttribPointer(positionLocation, 3, GL_FLOAT, false, sizeof(Vertex), 0);
			glVertexAttribPointer(normalLocation, 3, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, normal));
			glVertexAttribPointer(texcoordsLocation, 2, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, texcoords));
			glVertexAttribPointer(tangentLocation, 4, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

			glEnableVertexAttribArray(positionLocation);
			glEnableVertexAttribArray(normalLocation);
			glEnableVertexAttribArray(texcoordsLocation);
			glEnableVertexAttribArray(tangentLocation);

			glBindVertexArray(0);
			DeleteBufferObject(mesh.VBO);
			DeleteBufferObject(mesh.IBO);
		}

		glEnable(GL_FRAMEBUFFER_SRGB);

		offscreenBuffer.CreateFramebuffer(width, height, true);
		{
			vec2 quad[] = { {-1.f, 1.f}, {-1.f, -1.f}, {1.f, 1.f}, {1.f, -1.f} };

			glGenVertexArrays(1, &quadVAO);
			glBindVertexArray(quadVAO);
			uint32_t vbo = CreateBufferObject(BufferType::VBO, sizeof(quad), quad);

			program = copyShader.GetProgram();
			glUseProgram(program);
			int32_t positionLocation = glGetAttribLocation(program, "a_Position");
			glVertexAttribPointer(positionLocation, 2, GL_FLOAT, false, sizeof(vec2), 0);
			glEnableVertexAttribArray(positionLocation);

			glBindVertexArray(0);
			DeleteBufferObject(vbo);
		}

		//Cubemap
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapID);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glUseProgram(0);
	}

	void RenderOffscreen()
	{
		//offscreenBuffer.EnableRender();
		glBindFramebuffer(GL_FRAMEBUFFER, offscreenBuffer.FBO);
		glViewport(0, 0, offscreenBuffer.width, offscreenBuffer.height);

		glClearColor(0.02f, 0.02f, 0.02f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glViewport(0, 0, width, height);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		uint32_t program = opaqueShader.GetProgram();
		glUseProgram(program);

		mat4 world(1.f), view, perspective;
		
		world = glm::rotate(world, (float)glfwGetTime(), vec3{ 0.f, 1.f, 0.f });
		vec3 position = {0.f, 0.2f, 1.f };
		//vec3 position = {0.f, 20.f, 35.f };
		view = glm::lookAt(position, vec3{ 0.0f, 0.2f, 0.0f }, vec3{ 0.f, 1.f, 0.f });
		perspective = glm::perspectiveFov(45.f, (float)width, (float)height, 0.1f, 1000.f);
		
		//copy matrix in ubo
		glBindBuffer(GL_UNIFORM_BUFFER, matrixUBO);
		//memory mapping
		mat4* matricesMat = (mat4*)glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
		matricesMat[0] = world;
		matricesMat[1] = view;
		matricesMat[2] = perspective;
		glUnmapBuffer(GL_UNIFORM_BUFFER);

		// camera position
		int32_t camPosLocation = glGetUniformLocation(program, "u_CameraPosition");
		glUniform3fv(camPosLocation, 1, &position.x);

		int32_t ambientLocation = glGetUniformLocation(program, "u_Material.AmbientColor");
		int32_t diffuseLocation = glGetUniformLocation(program, "u_Material.DiffuseColor");
		int32_t specularLocation = glGetUniformLocation(program, "u_Material.SpecularColor");
		int32_t shininessLocation = glGetUniformLocation(program, "u_Material.Shininess");

		for (uint32_t i = 0; i < object->meshCount; i++)
		{
			SubMesh& mesh = object->meshes[i];
			Material& mat = mesh.materialId > -1 ? object->materials[mesh.materialId] : Material::defaultMaterial;

			//copy in UBO
			glBindBuffer(GL_UNIFORM_BUFFER, materialUBO);
			vec4* materialMat = (vec4*)glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
			materialMat[0] = vec4(mat.ambientColor, 0.f);
			materialMat[1] = vec4(mat.diffuseColor, 0.f);
			materialMat[2] = vec4(mat.specularColor, mat.shininess);
			glUnmapBuffer(GL_UNIFORM_BUFFER);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, mat.diffuseTexture);
			//Texture normal
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, mat.normalTexture);
			//Texture spec
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, mat.specularTexture);
			glBindVertexArray(mesh.VAO);
			glDrawElements(GL_TRIANGLES, object->meshes[i].indicesCount, GL_UNSIGNED_INT, 0);
		}
	}

	void Render()
	{
		glEnable(GL_DEPTH_TEST);
		RenderOffscreen();

		glDisable(GL_DEPTH_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height);

		uint32_t program = copyShader.GetProgram();
		glUseProgram(program);

		int32_t samplerLocation = glGetUniformLocation(program, "u_Texture");
		glUniform1i(samplerLocation, 0);

		float time = (float)glfwGetTime();
		GLint locTime = glGetUniformLocation(program, "u_Time");
		glUniform1f(locTime, time);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, offscreenBuffer.colorBuffer);

		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	void Resize(int w, int h)
	{
		if (width != w || height != h)
		{
			width = w;
			height = h;
			offscreenBuffer.DestroyFramebuffer();
			offscreenBuffer.CreateFramebuffer(width, height, true);
		}
	}

	void Shutdown()
	{
		glDeleteBuffers(1, &matrixUBO);
		glDeleteBuffers(1, &materialUBO);
		glDeleteVertexArrays(1, &quadVAO);
		quadVAO = 0;
		
		object->Destroy();
		delete object;

		Texture::PurgeTextures();
		glDeleteTextures(1, &cubeMapID);

		copyShader.Destroy();
		opaqueShader.Destroy();
	}
};


void ResizeCallback(GLFWwindow* window, int w, int h)
{
	Application* app = (Application *)glfwGetWindowUserPointer(window);
	app->Resize(w, h);
}

void MouseCallback(GLFWwindow* , int, int, int) {

}


int main(int argc, const char* argv[])
{
	GLFWwindow* window;

	/* Initialize the library */
	if (!glfwInit())
		return -1;

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(960, 720, "FBX Viewer (FBO)", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	Application app;

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	// Passe l'adresse de notre application a la fenetre
	glfwSetWindowUserPointer(window, &app);

	// inputs
	glfwSetMouseButtonCallback(window, MouseCallback);

	// recuperation de la taille de la fenetre
	glfwGetWindowSize(window, &app.width, &app.height);

	// definition de la fonction a appeler lorsque la fenetre est redimensionnee
	// c'est necessaire afin de redimensionner egalement notre FBO
	glfwSetWindowSizeCallback(window, &ResizeCallback);

	// toutes nos initialisations vont ici
	app.Initialize();

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		/* Render here */
		glfwGetWindowSize(window, &app.width, &app.height);

		app.Render();

		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();
	}

	// ne pas oublier de liberer la memoire etc...
	app.Shutdown();

	glfwTerminate();
	return 0;
}