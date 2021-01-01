
#include <iostream>

#define FBXSDK_SHARED 1
#include "fbxsdk.h"

#include "OpenGLcore.h"
#include "Material.h"
#include "Mesh.h"
#include "Texture.h"

// materiau par defaut (couleur ambiante, couleur diffuse, couleur speculaire, shininess, tex ambient, tex diffuse, tex specular)
Material Material::defaultMaterial = { { 0.f, 0.f, 0.f }, { 1.f, 1.f, 1.f }, { 0.f, 0.f, 0.f }, 256.f, 0, 1, 0 };

void Mesh::Destroy()
{
	// On n'oublie pas de détruire les objets OpenGL
	for (uint32_t i = 0; i < meshCount; i++)
	{
		// Comme les VBO ont ete "detruits" a l'initialisation
		// seul le VAO contient une reference vers ces VBO/IBO
		// detruire le VAO entraine donc la veritable destruction/deallocation des VBO/IBO
		//DeleteBufferObject(meshes[i].VBO);
		//DeleteBufferObject(meshes[i].IBO);
		glDeleteVertexArrays(1, &meshes[i].VAO);
		meshes[i].VAO = 0;
	}
	// on supprime le tableau de SubMesh
	delete[] meshes;
}

float GetProperty(FbxProperty property)
{
	return (float)property.Get<FbxDouble>();
}

vec3 GetProperty3(FbxProperty property, FbxProperty factorProperty)
{
	vec3 rgb = {};
	//if (property.IsValid())
	{
		FbxDouble3 color = property.Get<FbxDouble3>();
		if (factorProperty.IsValid()) {
			// le facteur s’applique generalement sur la propriete (ici RGB) 
			double factor = factorProperty.Get<FbxDouble>();
			// il arrive que le facteur soit egal a zero, il faudrait je pense l'ignorer dans ce cas
			factor = factor == 0 ? 1.0 : factor;
			color[0] *= factor; color[1] *= factor; color[2] *= factor;
		}
		rgb.x = (float)color[0]; rgb.y = (float)color[1]; rgb.z = (float)color[2];
	}
	return rgb;
}

void ParseMaterial(Material& mat, FbxNode* node)
{
	// Exemple simple de gestion des textures, on s'interesse ici surtout aux textures 
	// diffuses (parfois appellees albedo maps, couleur primaire des fragment de l'objet)
	// ATTENTION: les materiaux sont stockes dans le node et pas dans le mesh
	int materialCount = node->GetMaterialCount();
	if (materialCount)
	{

		// techniquement il faudrait dans certains cas boucler sur l'ensemble des materiaux
		FbxSurfaceMaterial *fbx_material = node->GetMaterial(0);

		// La propriete ‘diffuse’ contient une couleur, un facteur et une texture
		const FbxProperty property_diff = fbx_material->FindProperty(FbxSurfaceMaterial::sDiffuse);
		const FbxProperty factor_diff = fbx_material->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);

		if (property_diff.IsValid())
		{
			mat.diffuseColor = GetProperty3(property_diff, factor_diff);

			const int textureCount = property_diff.GetSrcObjectCount<FbxFileTexture>();
			if (textureCount) {
				// techniquement il faudrait dans certains cas boucler selon textureCount
				const FbxFileTexture* texture = property_diff.GetSrcObject<FbxFileTexture>(0);
				if (texture)
				{
					// exemple de metadata que l'on peut recuperer
					const char *name = texture->GetName();
					const char *filename = texture->GetFileName();
					const char *relativeFilename = texture->GetRelativeFileName();
					// texture->GetUserDataPtr() peut etre utile, il permet a l’artiste d’y stocker des donnees custom
					mat.diffuseTexture = Texture::LoadTexture(filename);
				}
			}
		}

		// La propriete ‘ambient’ contient une couleur, un facteur et une texture
		const FbxProperty property_amb = fbx_material->FindProperty(FbxSurfaceMaterial::sAmbient);
		const FbxProperty factor_amb = fbx_material->FindProperty(FbxSurfaceMaterial::sAmbientFactor);
		if (property_amb.IsValid())
		{
			mat.ambientColor = GetProperty3(property_amb, factor_amb);

			const int textureCount = property_amb.GetSrcObjectCount<FbxFileTexture>();
			if (textureCount) {
				const FbxFileTexture* texture = property_amb.GetSrcObject<FbxFileTexture>(0);
				if (texture) {
					const char *filename = texture->GetFileName();
					mat.ambientTexture = Texture::LoadTexture(filename);
				}
			}
		}

		// La propriete ‘specular’ contient une couleur, un facteur et une texture ainsi que shininess
		const FbxProperty property_spec = fbx_material->FindProperty(FbxSurfaceMaterial::sSpecular);
		const FbxProperty factor_spec = fbx_material->FindProperty(FbxSurfaceMaterial::sSpecularFactor);
		const FbxProperty property_shiny = fbx_material->FindProperty(FbxSurfaceMaterial::sShininess);
		if (property_spec.IsValid())
		{
			mat.specularColor = GetProperty3(property_spec, factor_spec);
			// reparametrage du facteur de brillance speculaire de [0-2048]
			mat.shininess = GetProperty(property_shiny) * 1024.f;

			const int textureCount = property_spec.GetSrcObjectCount<FbxFileTexture>();
			if (textureCount) {
				const FbxFileTexture* texture = property_spec.GetSrcObject<FbxFileTexture>(0);
				if (texture) {
					const char *filename = texture->GetFileName();
					mat.specularTexture = Texture::LoadTexture(filename);
				}
			}
		}

		//Normal Map
		const FbxProperty property_diff_normal = fbx_material->FindProperty(FbxSurfaceMaterial::sBump);
		const FbxProperty factor_diff_normal = fbx_material->FindProperty(FbxSurfaceMaterial::sBumpFactor);
		
		if (property_diff_normal.IsValid())
		{
			const int textureCount = property_diff_normal.GetSrcObjectCount<FbxFileTexture>();
			if (textureCount) {
				// techniquement il faudrait dans certains cas boucler selon textureCount
				const FbxFileTexture* texture = property_diff_normal.GetSrcObject<FbxFileTexture>(0);
				if (texture)
				{
					// exemple de metadata que l'on peut recuperer
					const char* filename = texture->GetFileName();
					// texture->GetUserDataPtr() peut etre utile, il permet a l’artiste d’y stocker des donnees custom
					mat.normalTexture = Texture::LoadTexture(filename);
				}
			}
		}
	}
}

void AddMesh(Mesh* obj, FbxNode *node, FbxNode *parent)
{
	FbxMesh* mesh = node->GetMesh();
	// preparation du mesh (clean, triangulation ....)
	mesh->RemoveBadPolygons();
	if (!mesh->IsTriangleMesh()) {

	}

	SubMesh* submesh = &obj->meshes[obj->meshCount];
	++obj->meshCount;

	// si tous les meshes sont triangularises c'est ok...
	// pour le moment on ne fait pas de deduplication, on stocke tout en brut
	int vertexCount = mesh->GetPolygonCount() * 3;

	// buffer temporaire, on va tout stocker côté GPU
	Vertex* vertices = new Vertex[vertexCount];
	submesh->verticesCount = 0;
	uint32_t* indices = new uint32_t[vertexCount];
	submesh->indicesCount = 0;


	// Matrices geometriques, différentes pour chaque mesh ---

	// etape 1. calcul de la matrice geometrique
	FbxVector4 translation = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	FbxVector4 rotation = node->GetGeometricRotation(FbxNode::eSourcePivot);
	FbxVector4 scaling = node->GetGeometricScaling(FbxNode::eSourcePivot);
	FbxAMatrix geometryTransform;
	geometryTransform.SetTRS(translation, rotation, scaling);

	// etape 2. on recupere la matrice global (world) du mesh
	const FbxAMatrix globalTransform = node->EvaluateGlobalTransform();

	// etape 3. on concatene les deux matrices, ce qui donne la matrice world finale
	FbxAMatrix finalGlobalTransform = globalTransform * geometryTransform;

	// Metadata du mesh (canaux UVs ...)
	FbxStringList nameListUV;
	mesh->GetUVSetNames(nameListUV);
	int totalUVChannels = nameListUV.GetCount();
	
	// on ne s'interesse qu'au premier canal UV
	int uv_channel = 0;
	const char *nameUV = nameListUV.GetStringAt(uv_channel);

	int polyCount = mesh->GetPolygonCount();								// retourne le nombre de polygones
	for (int polyIndex = 0; polyIndex < polyCount; polyIndex++)
	{
		int polySize = mesh->GetPolygonSize(polyIndex);				// retourne le nombre de vertex par polygones

		for (int vertexIndex = 0; vertexIndex < polySize; ++vertexIndex)
		{
			int controlPointIndex = mesh->GetPolygonVertex(polyIndex, vertexIndex);	//retourne l’id du v-ieme vertex du polygon p
			
			FbxVector4 position = mesh->GetControlPointAt(controlPointIndex);

			FbxVector2 uv;
			bool isUnMapped;
			bool hasUV = mesh->GetPolygonVertexUV(polyIndex, vertexIndex, nameUV, uv, isUnMapped);

			FbxVector4 normal;
			mesh->GetPolygonVertexNormal(polyIndex, vertexIndex, normal);

			// on applique l'ensemble des transformations locales stockees dans les noeuds (scenegraph) au mesh
			position = finalGlobalTransform.MultT(position);
			normal = finalGlobalTransform.MultT(normal);
			normal.Normalize();

			// Normal Map

			// Tangents
			// determine la presence de tangentes
			FbxLayerElementTangent* meshTangents = mesh->GetElementTangent(0);
			if (meshTangents == nullptr) {
				// sinon on genere des tangentes (pour le tangent space normal mapping)
				bool test = mesh->GenerateTangentsDataForAllUVSets(true);
				meshTangents = mesh->GetElementTangent(0);
			}
			FbxLayerElement::EMappingMode tangentMode = meshTangents->GetMappingMode();
			FbxLayerElement::EReferenceMode tangentRefMode = meshTangents->GetReferenceMode();

			FbxVector4 tangent;
			if (tangentRefMode == FbxLayerElement::eTangent) {
				tangent = meshTangents->GetDirectArray().GetAt(vertexIndex);
			}
			else //if (tangentRefMode == FbxLayerElement::eIndexToDirect)
			{
				int indirectIndex = meshTangents->GetIndexArray().GetAt(vertexIndex);
				tangent = meshTangents->GetDirectArray().GetAt(indirectIndex);
			}

			//BiTangents
			FbxLayerElementBinormal* meshBiTangents = mesh->GetElementBinormal(0);
			if (meshBiTangents == nullptr) {
				// sinon on genere des tangentes (pour le tangent space normal mapping)
				bool test = mesh->GenerateTangentsDataForAllUVSets(true);
				meshBiTangents = mesh->GetElementBinormal(0);
			}
			FbxLayerElement::EMappingMode biTangentMode = meshBiTangents->GetMappingMode();
			FbxLayerElement::EReferenceMode biTangentRefMode = meshBiTangents->GetReferenceMode();


			FbxVector4 biTangent;
			if (biTangentRefMode == FbxLayerElement::eBiNormal) {
				biTangent = meshBiTangents->GetDirectArray().GetAt(vertexIndex);
			}
			else //if (tangentRefMode == FbxLayerElement::eIndexToDirect)
			{
				int indirectIndex = meshBiTangents->GetIndexArray().GetAt(vertexIndex);
				biTangent = meshBiTangents->GetDirectArray().GetAt(indirectIndex);
			}

			vertices[submesh->verticesCount] = { { (float)position[0], (float)position[1], (float)position[2] },
												 { (float)normal[0], (float)normal[1], (float)normal[2] },
												 { (float)uv[0], (float)uv[1] },
												 { (float)tangent[0], (float)tangent[1], (float)tangent[2], (float)tangent[3] },
												 { (float)biTangent[0], (float)biTangent[1], (float)biTangent[2], (float)biTangent[3] }
			};
			submesh->verticesCount++;

			// l'index buffer n'est pas vraiment utile actuellement
			indices[submesh->indicesCount] = submesh->indicesCount;
			submesh->indicesCount++;
		}
	}

	// materiaux ---

	Material& mat = obj->materials[obj->materialCount];
	int materialId = obj->materialCount;
	ParseMaterial(mat, node);
	obj->materialCount++;

	submesh->materialId = materialId;

	// notez que je ne cree pas le VAO ici
	// Un VAO fait le lien entre le VBO (+ EBO/IBO) et les attributs d'un vertex shader
	submesh->VBO = CreateBufferObject(BufferType::VBO, sizeof(Vertex) * submesh->verticesCount, vertices);
	submesh->IBO = CreateBufferObject(BufferType::IBO, sizeof(uint32_t) * submesh->indicesCount, indices);

	// important, bien libérer la mémoire des buffers temporaires
	delete[] indices;
	delete[] vertices;
}

void ProcessNode(Mesh* obj, FbxNode *node, FbxNode *parent) 
{
	const FbxNodeAttribute* att = node->GetNodeAttribute();
	if (att != nullptr)
	{
		FbxNodeAttribute::EType type = att->GetAttributeType();
		switch (type)
		{
		case FbxNodeAttribute::eMesh:
			AddMesh(obj, node, parent);
			break;
		case FbxNodeAttribute::eSkeleton:
			// illustratif, nous traiterons du cas des squelettes dans une function spécifique
			//AddJoint(node, parent);
			break;
		}
	}

	int childCount = node->GetChildCount();
	for (int i = 0; i < childCount; i++) {
		FbxNode* child = node->GetChild(i);
		ProcessNode(obj, child, node);
	}
}


bool Mesh::ParseFBX(Mesh* obj, const char* filepath)
{
	std::string relativePath = "../data/";
	FbxManager *g_fbxManager;
	FbxScene *g_scene;

	g_fbxManager = FbxManager::Create();
	FbxIOSettings *ioSettings = FbxIOSettings::Create(g_fbxManager, IOSROOT);
	g_fbxManager->SetIOSettings(ioSettings);

	g_scene = FbxScene::Create(g_fbxManager, "Le Nom de Ma Scene");
	
	FbxImporter *importer = FbxImporter::Create(g_fbxManager, "");
	relativePath += filepath;
	bool status = importer->Initialize(relativePath.c_str(), -1, g_fbxManager->GetIOSettings());
	status = importer->Import(g_scene);
	importer->Destroy();
	
	if (status == true)
	{

		// preprocess de la scene ---

		// todo: check triangulate
		//FbxGeometryConverter geometryConverter(g_fbxManager);
		//geometryConverter.Triangulate(scene, true);

		// On compare le repère de la scene avec le repere souhaite
		FbxAxisSystem SceneAxisSystem = g_scene->GetGlobalSettings().GetAxisSystem();
		FbxAxisSystem OurAxisSystem(FbxAxisSystem::eYAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded);
		if (SceneAxisSystem != OurAxisSystem) {
			OurAxisSystem.ConvertScene(g_scene);
		}

		FbxSystemUnit SceneSystemUnit = g_scene->GetGlobalSettings().GetSystemUnit();
		// L'unite standard du Fbx est le centimetre, que l'on peut tester ainsi
		if (SceneSystemUnit != FbxSystemUnit::cm) {
			printf("[warning] FbxSystemUnit vaut %f cm (= 1 %s) !\n", SceneSystemUnit.GetScaleFactor(), SceneSystemUnit.GetScaleFactorAsString().Buffer());
			FbxSystemUnit::cm.ConvertScene(g_scene);
		}

		// chargement et conversion des donnees ---

		{
			int materialCount = g_scene->GetMaterialCount();
			obj->materials = new Material[materialCount]();

			int geometryCount = g_scene->GetGeometryCount();
			obj->meshes = new SubMesh[geometryCount]();

			FbxNode *root_node = g_scene->GetRootNode();
			ProcessNode(obj, root_node, nullptr);
		}

	}

	g_scene->Destroy();
	g_fbxManager->Destroy();

	return true;
}
