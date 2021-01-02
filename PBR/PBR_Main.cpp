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
	GLShader copyShader;			// remplace glBlitFrameBuffer

	// dimensions du back buffer / Fenetre
	int32_t width;
	int32_t height;

	uint32_t matrixUBO;

	uint32_t materialUBO;

	Framebuffer offscreenBuffer;	// rendu hors ecran

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

		// on utilise un texture manager afin de ne pas recharger une texture deja en memoire
		// de meme on va definir une ou plusieurs textures par defaut
		Texture::SetupManager();

		opaqueShader.LoadVertexShader("opaque.vs.glsl");
		opaqueShader.LoadFragmentShader("opaque.fs.glsl");
		opaqueShader.Create();
		copyShader.LoadVertexShader("copy.vs.glsl");
		copyShader.LoadFragmentShader("copy.fs.glsl");
		copyShader.Create();

		object = new Mesh();

		Mesh::ParseFBX(object, "model/Glados.fbx");

		//modifier createBufferObject() pour gerer les UBO
		//1 -> creation de l'ubo
		glGenBuffers(1, &matrixUBO);
		//2 -> bind l'ubo pour l'utiliser
		glBindBuffer(GL_UNIFORM_BUFFER, matrixUBO);
		//3-> allocation mémoire de l'ubo
		glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(mat4), nullptr, GL_STREAM_DRAW);
		//4-> connexion UBO to shader (block)
		//le deuxieme parametre indique le binding du block
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, matrixUBO);

		//bind UBO material
		glGenBuffers(1, &materialUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, materialUBO);
		glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(vec4), nullptr, GL_STREAM_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 1, materialUBO);

		int32_t program = opaqueShader.GetProgram();
		glUseProgram(program);
		// on connait deja les attributs que l'on doit assigner dans le shader
		//permet de déterminer la location de l'attribut et plus récupérer celle que donne openGl
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
			// Specifie la structure des donnees envoyees au GPU
			glVertexAttribPointer(positionLocation, 3, GL_FLOAT, false, sizeof(Vertex), 0);
			glVertexAttribPointer(normalLocation, 3, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, normal));
			glVertexAttribPointer(texcoordsLocation, 2, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, texcoords));
			glVertexAttribPointer(tangentLocation, 4, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

			
			// indique que les donnees sont sous forme de tableau
			glEnableVertexAttribArray(positionLocation);
			glEnableVertexAttribArray(normalLocation);
			glEnableVertexAttribArray(texcoordsLocation);
			glEnableVertexAttribArray(tangentLocation);

			// ATTENTION, les instructions suivantes ne detruisent pas immediatement les VBO/IBO
			// Ceci parcequ'ils sont référencés par le VAO. Ils ne seront détruit qu'au moment
			// de la destruction du VAO
			glBindVertexArray(0);
			DeleteBufferObject(mesh.VBO);
			DeleteBufferObject(mesh.IBO);
		}


		// force le framebuffer sRGB
		glEnable(GL_FRAMEBUFFER_SRGB);

		offscreenBuffer.CreateFramebuffer(width, height, true);
		{
			vec2 quad[] = { {-1.f, 1.f}, {-1.f, -1.f}, {1.f, 1.f}, {1.f, -1.f} };

			// VAO du carré plein ecran pour le shader de copie
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

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glUseProgram(0);
	}

	// la scene est rendue hors ecran
	void RenderOffscreen()
	{
		//offscreenBuffer.EnableRender();
		glBindFramebuffer(GL_FRAMEBUFFER, offscreenBuffer.FBO);
		glViewport(0, 0, offscreenBuffer.width, offscreenBuffer.height);

		glClearColor(0.973f, 0.514f, 0.475f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Defini le viewport en pleine fenetre
		glViewport(0, 0, width, height);

		// En 3D il est usuel d'activer le depth test pour trier les faces, et cacher les faces arrières
		// Par défaut OpenGL considère que les faces anti-horaires sont visibles (Counter Clockwise, CCW)
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		uint32_t program = opaqueShader.GetProgram();
		glUseProgram(program);

		// calcul des matrices model (une simple rotation), view (une translation inverse) et projection
		// ces matrices sont communes à tous les SubMesh
		mat4 world(1.f), view, perspective;
		
		world = glm::rotate(world, (float)glfwGetTime(), vec3{ 0.f, 1.f, 0.f });
		vec3 position = {-4.f, 2.f, 1.f };
		view = glm::lookAt(position, vec3{ 0.1f, 3.f, 0.1f }, vec3{ 0.f, 1.f, 0.f });
		perspective = glm::perspectiveFov(45.f, (float)width, (float)height, 0.1f, 1000.f);
		
		//copier les matrices dans l'ubo
		glBindBuffer(GL_UNIFORM_BUFFER, matrixUBO);
		//memory mapping
		mat4* matricesMat = (mat4*)glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
		matricesMat[0] = world;
		matricesMat[1] = view;
		matricesMat[2] = perspective;
		glUnmapBuffer(GL_UNIFORM_BUFFER);

		// position de la camera
		int32_t camPosLocation = glGetUniformLocation(program, "u_CameraPosition");
		glUniform3fv(camPosLocation, 1, &position.x);

		// On va maintenant affecter les valeurs du matériau à chaque SubMesh
		// On peut éventuellement optimiser cette boucle en triant les SubMesh par materialID
		// On ne devra modifier les uniformes que lorsque le materialID change
		int32_t ambientLocation = glGetUniformLocation(program, "u_Material.AmbientColor");
		int32_t diffuseLocation = glGetUniformLocation(program, "u_Material.DiffuseColor");
		int32_t specularLocation = glGetUniformLocation(program, "u_Material.SpecularColor");
		int32_t shininessLocation = glGetUniformLocation(program, "u_Material.Shininess");


		// on indique au shader que l'on va bind la texture diffuse sur le sampler 0 (TEXTURE0)
		//on récupère dans le shader la location avec "binding"
		//int32_t samplerLocation = glGetUniformLocation(program, "u_DiffuseTexture");
		//glUniform1i(samplerLocation, 0);

		for (uint32_t i = 0; i < object->meshCount; i++)
		{
			SubMesh& mesh = object->meshes[i];
			Material& mat = mesh.materialId > -1 ? object->materials[mesh.materialId] : Material::defaultMaterial;

			//copier les materials dans l'ubo
			glBindBuffer(GL_UNIFORM_BUFFER, materialUBO);
			vec4* materialMat = (vec4*)glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
			materialMat[0] = vec4(mat.ambientColor, 0.f);
			materialMat[1] = vec4(mat.diffuseColor, 0.f);
			materialMat[2] = vec4(mat.specularColor, mat.shininess);
			glUnmapBuffer(GL_UNIFORM_BUFFER);

			// glActiveTexture() n'est pas strictement requis ici car nous n'avons qu'une seule texture pour le moment
			//on n'a pas besoin de faire le samplerLocation parce que glActiveTexture le fait pour nous
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, mat.diffuseTexture);
			//Texture normal
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, mat.normalTexture);
			//Texture spec
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, mat.specularTexture);
			// bind implicitement les VBO et IBO rattaches, ainsi que les definitions d'attributs
			glBindVertexArray(mesh.VAO);
			// dessine les triangles
			glDrawElements(GL_TRIANGLES, object->meshes[i].indicesCount, GL_UNSIGNED_INT, 0);
		}
	}

	void Render()
	{
		// Rendu 3D hors ecran ---

		glEnable(GL_DEPTH_TEST);	// Active le test de profondeur (3D)
		
		RenderOffscreen();

		// Rendu 2D vers le backbuffer (copie) ---

		glDisable(GL_DEPTH_TEST);	// desactive le test de profondeur (2D)
		
		// on va maintenant dessiner un quadrilatere plein ecran
		// pour copier (sampler et inscrire dans le back-buffer) le color buffer du FBO
		//Framebuffer::RenderToBackBuffer(width, height);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height);

		uint32_t program = copyShader.GetProgram();
		glUseProgram(program);
		
		// on indique au shader que l'on va bind la texture sur le sampler 0 (TEXTURE0)
		// pas necessaire techniquement car c'est le sampler par defaut
		int32_t samplerLocation = glGetUniformLocation(program, "u_Texture");
		glUniform1i(samplerLocation, 0);

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
			// le plus simple est de detruire et recreer tout
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

		// On n'oublie pas de détruire les objets OpenGL

		Texture::PurgeTextures();
		
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