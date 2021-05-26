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

#include <iostream>
#include <fstream>

#define FBXSDK_SHARED 1
#include "fbxsdk.h"

#include "stb_image.h"

//ImGUI
#include "imgui.cpp"
#include "examples\\imgui_impl_glfw.cpp"
#include "examples\\imgui_impl_opengl3.cpp"
#include "imgui_draw.cpp"
#include "imgui_widgets.cpp"
#include "imgui_demo.cpp"

#include "../common/GLShader.h"
#include "Texture.h"
#include "Mesh.h"
#include "Framebuffer.h"
#include "IBL.h"

const char* glsl_version = "#version 420";

//PBR settings for GUI
ImColor albedo = ImColor(1.0f, 0.0f, 0.0f);
float roughness;
float metallic;
bool displaySphere = false;
bool displayIBL = false;

//Objects
Mesh* sphereMesh;
Mesh* otherMesh;

//Rendering
struct Application
{
	Mesh* object;

	uint32_t quadVAO;

	GLShader opaqueShader;
	GLShader postProcessShader;	
	GLShader blurShader;

	int32_t width;
	int32_t height;

	uint32_t matrixUBO;
	uint32_t materialUBO;
	unsigned int captureFBO;
	unsigned int captureRBO;
	unsigned int colorBuffers[2];
	unsigned int pingpongFBO[2];
	unsigned int pingpongColorbuffers[2];

	Framebuffer offscreenBuffer;

	//cubemap 
	uint32_t cubeMapID;
	uint32_t irradianceMapID;
	uint32_t radianceMapID;
	uint32_t prefilteredMap;
	uint32_t brdfLUTTextureID;
	GLShader g_skyboxShader;
	GLuint skyboxVAO, skyboxVBO;

	unsigned int quadVBO;
	void renderQuad()
	{
		if (quadVAO == 0)
		{
			float quadVertices[] = {
				// positions        // texture Coords
				-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
				-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
				 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
				 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
			};
			// setup plane VAO
			glGenVertexArrays(1, &quadVAO);
			glGenBuffers(1, &quadVBO);
			glBindVertexArray(quadVAO);
			glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		}
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
	}

	void GenerateBuffers(Mesh* object)
	{
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
	}

	void InitFrameBuffer() {
		glGenFramebuffers(1, &captureFBO);
		glGenRenderbuffers(1, &captureRBO);

		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
	}

	void InitCubeMap()
	{
		const char* pathes[6] = {
			"../data/envmaps/Studio_posx.png",
			"../data/envmaps/Studio_negx.png",
			"../data/envmaps/Studio_posy.png",
			"../data/envmaps/Studio_negy.png",
			"../data/envmaps/Studio_posz.png",
			"../data/envmaps/Studio_negz.png"
		};

		cubeMapID = LoadCubemap(pathes);
	}

	void InitRadianceMap()
	{
		const char* pathes[6] = {
			"../data/envmaps/Studio_Radiance_posx.png",
			"../data/envmaps/Studio_Radiance_negx.png",
			"../data/envmaps/Studio_Radiance_posy.png",
			"../data/envmaps/Studio_Radiance_negy.png",
			"../data/envmaps/Studio_Radiance_posz.png",
			"../data/envmaps/Studio_Radiance_negz.png"
		};

		radianceMapID = LoadCubemap(pathes);
	}

	void InitBloomBuffer()
	{
		glGenTextures(2, colorBuffers);
		for (size_t i = 0; i < 2; i++)
		{
			glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
		}

		unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, attachments);

		// ping-pong-framebuffer for blurring
		glGenFramebuffers(2, pingpongFBO);
		glGenTextures(2, pingpongColorbuffers);
		for (unsigned int i = 0; i < 2; i++)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
			glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
			// also check if framebuffers are complete (no need for depth buffer)
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cout << "Framebuffer not complete!" << std::endl;
		}
	}

	void RenderBloom()
	{
		bool horizontal = true, first_iteration = true;
		unsigned int amount = 10;
		int32_t program = blurShader.GetProgram();
		glUseProgram(program);
		for (unsigned int i = 0; i < amount; i++)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
			int32_t horizontalLocation = glGetAttribLocation(program, "horizontal");
			glUniform1i(horizontalLocation, horizontal);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
			renderQuad();
			horizontal = !horizontal;
			if (first_iteration)
				first_iteration = false;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		program = postProcessShader.GetProgram();
		glUseProgram(program);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
		glActiveTexture(GL_TEXTURE1);
		//glBindTexture(GL_TEXTURE_2D, colorBuffers[1]);
		glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);

		renderQuad();
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
		postProcessShader.LoadVertexShader("postProcess.vs.glsl");
		postProcessShader.LoadFragmentShader("postProcess.fs.glsl");
		postProcessShader.Create();
		blurShader.LoadVertexShader("blur.vs.glsl");
		blurShader.LoadFragmentShader("blur.fs.glsl");
		blurShader.Create();

		InitFrameBuffer();
		InitCubeMap();
		GenerateMipmaps(cubeMapID);
		GenerateIrradiance(irradianceMapID, captureFBO, captureRBO);
		//SolveDiffuseIntegrale(irradianceShader, cubeMapID, irradianceMapID); TODO : shader
		CreatePrefilteredMap(prefilteredMap);
		//GeneratePrefilteredMap(prefilteredMap, cubeMapID, prefilterShader, captureFBO, captureRBO); TODO : shader
		//GenerateBRDFLutTexture(brdfLUTTextureID, brdfShader, captureFBO, captureRBO); TODO : shader

		InitBloomBuffer();

		//Load meshes
		otherMesh = new Mesh();
		sphereMesh = new Mesh();
		Mesh::ParseFBX(otherMesh, "model/Mando_Helmet.fbx");
		Mesh::ParseFBX(sphereMesh, "model/testSphere.fbx");
		object = otherMesh;

		glGenBuffers(1, &matrixUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, matrixUBO);
		glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(mat4), nullptr, GL_STREAM_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, matrixUBO);

		//bind UBO material
		glGenBuffers(1, &materialUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, materialUBO);
		glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(vec4), nullptr, GL_STREAM_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, materialUBO);
		
		GenerateBuffers(otherMesh);
		GenerateBuffers(sphereMesh);

		glEnable(GL_FRAMEBUFFER_SRGB);

		offscreenBuffer.CreateFramebuffer(width, height, true);
		{
			vec2 quad[] = { {-1.f, 1.f}, {-1.f, -1.f}, {1.f, 1.f}, {1.f, -1.f} };

			glGenVertexArrays(1, &quadVAO);
			glBindVertexArray(quadVAO);
			uint32_t vbo = CreateBufferObject(BufferType::VBO, sizeof(quad), quad);

			uint32_t program = postProcessShader.GetProgram();
			glUseProgram(program);
			int32_t positionLocation = glGetAttribLocation(program, "a_Position");
			glVertexAttribPointer(positionLocation, 2, GL_FLOAT, false, sizeof(vec2), 0);
			glEnableVertexAttribArray(positionLocation);

			glBindVertexArray(0);
			DeleteBufferObject(vbo);
		}

		glDisable(GL_FRAMEBUFFER_SRGB);

		//Cubemap
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMapID);

		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_CUBE_MAP, radianceMapID);


		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMapID);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glUseProgram(0);
	}

	void RenderOffscreen()
	{
		//offscreenBuffer.EnableRender();

		glBindFramebuffer(GL_FRAMEBUFFER, offscreenBuffer.FBO);
		glViewport(0, 0, offscreenBuffer.width, offscreenBuffer.height);

		glClearColor(0.1f, 0.1f, 0.1f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glViewport(0, 0, width, height);

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		uint32_t program = opaqueShader.GetProgram();
		glUseProgram(program);

		mat4 world(1.f), view, perspective;
		
		world = glm::rotate(world, (float)glfwGetTime() * 0.5f, vec3{ 0.f, 1.f, 0.f });
		vec3 position = {0.f, 0.4f, 1.2f };
		
		view = glm::lookAt(position, vec3{ 0.0f, 0.1f, 0.0f }, vec3{ 0.f, 1.f, 0.f });

		if (!displaySphere)
		{
			position = { 0.f, 0.8f, 1.2f };
			view = glm::lookAt(position, vec3{ 0.0f, 0.4f, 0.0f }, vec3{ 0.f, 1.f, 0.f });
			world = glm::scale(world, vec3(0.017));
		}

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

		//GUI uniforms
		int32_t locAlbedo = glGetUniformLocation(program, "u_albedo");
		glUniform3fv(locAlbedo, 1, &albedo.Value.x);

		int32_t locRoughness = glGetUniformLocation(program, "u_roughness");
		glUniform1f(locRoughness, roughness);

		int32_t locmetallic = glGetUniformLocation(program, "u_metallic");
		glUniform1f(locmetallic, metallic);

		int32_t locDisplayShere = glGetUniformLocation(program, "u_displaySphere");
		glUniform1i(locDisplayShere, displaySphere);

		int32_t locDisplayIBL = glGetUniformLocation(program, "u_displayIBL");
		glUniform1i(locDisplayIBL, displayIBL);

		for (uint32_t i = 0; i < object->meshCount; i++)
		{
			SubMesh& mesh = object->meshes[i];
			glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
			Material& mat = mesh.materialId > -1 ? object->materials[mesh.materialId] : Material::defaultMaterial;

			//copy in UBO
			glBindBuffer(GL_UNIFORM_BUFFER, materialUBO);
			vec4* materialMat = (vec4*)glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
			materialMat[0] = vec4(mat.ambientColor, 0.f);
			materialMat[1] = vec4(mat.diffuseColor, mat.emissiveIntensity);
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
			//Emissive texture
			glActiveTexture(GL_TEXTURE6);
			glBindTexture(GL_TEXTURE_2D, mat.emissiveTexure);
			
			glBindVertexArray(mesh.VAO);
			glDrawElements(GL_TRIANGLES, object->meshes[i].indicesCount, GL_UNSIGNED_INT, 0);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		RenderBloom();
	}

	void Render()
	{
		glEnable(GL_DEPTH_TEST);
		RenderOffscreen();

		glDisable(GL_DEPTH_TEST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height);

		uint32_t program = postProcessShader.GetProgram();
		glUseProgram(program);

		int32_t samplerLocation = glGetUniformLocation(program, "u_Texture");
		glUniform1i(samplerLocation, 0);

		float time = (float)glfwGetTime();
		GLint locTime = glGetUniformLocation(program, "u_Time");
		glUniform1f(locTime, time);

		//Textures
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
		
		sphereMesh->Destroy();
		delete sphereMesh;

		otherMesh->Destroy();
		delete otherMesh;

		Texture::PurgeTextures();
		glDeleteTextures(1, &cubeMapID);

		postProcessShader.Destroy();
		opaqueShader.Destroy();
		blurShader.Destroy();
	}
};

#pragma region Inputs

void ResizeCallback(GLFWwindow* window, int w, int h)
{
	Application* app = (Application *)glfwGetWindowUserPointer(window);
	app->Resize(w, h);
}

void MouseCallback(GLFWwindow* , int, int, int) {

}

#pragma endregion

#pragma region GUI

//Init UI
void InitialiseGUI(GLFWwindow* window)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);
	ImGui::StyleColorsClassic();
}

//Draw
void DrawGUI(Application& app)
{
	// feed inputs to dear imgui, start new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	//ImGui::SetNextWindowSize(ImVec2(100, 100));
	ImGui::SetNextWindowPos(ImVec2(0, 0));

	// render your GUI
	ImGui::Begin("PBR settings", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
	
	//Change model
	if (ImGui::Checkbox("Display Sphere", &displaySphere))
	{
		if (displaySphere)
		{
			app.object = sphereMesh;
		}
		else
		{
			app.object = otherMesh;
		}
	}

	if (displaySphere)
	{
		//Albedo
		static float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
		if (ImGui::ColorEdit3("Albedo", color))
		{
			albedo = ImColor(color[0], color[1], color[2]);
		}

		//Roughness
		static float f1 = 0.001f;
		ImGui::SliderFloat("Roughness", &f1, 0.001f, 1.0f, "%.3f");
		roughness = f1;

		//Metallic
		static float f2 = 0.123f;
		ImGui::SliderFloat("Metallic", &f2, 0.0f, 1.0f, "%.3f");
		metallic = f2;
	}

	ImGui::Checkbox("Display fake IBL", &displayIBL);

	ImGui::End();

	// Render dear imgui into screen
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

#pragma endregion

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

	InitialiseGUI(window);

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		/* Render here */
		glfwGetWindowSize(window, &app.width, &app.height);

		app.Render();

		DrawGUI(app);

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