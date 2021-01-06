
#include <iostream>

#define FBXSDK_SHARED 1
#include "fbxsdk.h"

#include "OpenGLcore.h"
#include "Material.h"
#include "Mesh.h"
#include "Texture.h"

Material Material::defaultMaterial = { { 0.f, 0.f, 0.f }, { 1.f, 1.f, 1.f }, { 0.f, 0.f, 0.f }, 256.f, 0, 1, 0 };

void Mesh::Destroy()
{
	for (uint32_t i = 0; i < meshCount; i++)
	{
		//DeleteBufferObject(meshes[i].VBO);
		//DeleteBufferObject(meshes[i].IBO);
		glDeleteVertexArrays(1, &meshes[i].VAO);
		meshes[i].VAO = 0;
	}
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
			double factor = factorProperty.Get<FbxDouble>();
			factor = factor == 0 ? 1.0 : factor;
			color[0] *= factor; color[1] *= factor; color[2] *= factor;
		}
		rgb.x = (float)color[0]; rgb.y = (float)color[1]; rgb.z = (float)color[2];
	}
	return rgb;
}

void ParseMaterial(Material& mat, FbxNode* node)
{
	int materialCount = node->GetMaterialCount();
	if (materialCount)
	{
		FbxSurfaceMaterial *fbx_material = node->GetMaterial(0);

		const FbxProperty property_diff = fbx_material->FindProperty(FbxSurfaceMaterial::sDiffuse);
		const FbxProperty factor_diff = fbx_material->FindProperty(FbxSurfaceMaterial::sDiffuseFactor);

		if (property_diff.IsValid())
		{
			mat.diffuseColor = GetProperty3(property_diff, factor_diff);

			const int textureCount = property_diff.GetSrcObjectCount<FbxFileTexture>();
			if (textureCount) {
				const FbxFileTexture* texture = property_diff.GetSrcObject<FbxFileTexture>(0);
				if (texture)
				{
					const char *name = texture->GetName();
					const char *filename = texture->GetFileName();
					const char *relativeFilename = texture->GetRelativeFileName();
					mat.diffuseTexture = Texture::LoadTexture(filename);
				}
			}
		}

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

		const FbxProperty property_spec = fbx_material->FindProperty(FbxSurfaceMaterial::sEmissive);
		const FbxProperty factor_spec = fbx_material->FindProperty(FbxSurfaceMaterial::sEmissiveFactor);
		const FbxProperty property_shiny = fbx_material->FindProperty(FbxSurfaceMaterial::sShininess);
		if (property_spec.IsValid())
		{

			mat.specularColor = GetProperty3(property_spec, factor_spec);
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
		const FbxProperty property_diff_normal = fbx_material->FindProperty(FbxSurfaceMaterial::sNormalMap);
		const FbxProperty factor_diff_normal = fbx_material->FindProperty(FbxSurfaceMaterial::sBumpFactor);
		
		if (property_diff_normal.IsValid())
		{
			const int textureCount = property_diff_normal.GetSrcObjectCount<FbxFileTexture>();
			if (textureCount) {
				const FbxFileTexture* texture = property_diff_normal.GetSrcObject<FbxFileTexture>(0);
				if (texture)
				{
					const char* filename = texture->GetFileName();
					mat.normalTexture = Texture::LoadTexture(filename);
				}
			}
		}
	}
}

void AddMesh(Mesh* obj, FbxNode *node, FbxNode *parent)
{
	FbxMesh* mesh = node->GetMesh();
	mesh->RemoveBadPolygons();
	if (!mesh->IsTriangleMesh()) {

	}

	SubMesh* submesh = &obj->meshes[obj->meshCount];
	++obj->meshCount;

	int vertexCount = mesh->GetPolygonCount() * 3;

	Vertex* vertices = new Vertex[vertexCount];
	submesh->verticesCount = 0;
	uint32_t* indices = new uint32_t[vertexCount];
	submesh->indicesCount = 0;

	FbxVector4 translation = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	FbxVector4 rotation = node->GetGeometricRotation(FbxNode::eSourcePivot);
	FbxVector4 scaling = node->GetGeometricScaling(FbxNode::eSourcePivot);
	FbxAMatrix geometryTransform;
	geometryTransform.SetTRS(translation, rotation, scaling);

	const FbxAMatrix globalTransform = node->EvaluateGlobalTransform();

	FbxAMatrix finalGlobalTransform = globalTransform * geometryTransform;

	FbxStringList nameListUV;
	mesh->GetUVSetNames(nameListUV);
	int totalUVChannels = nameListUV.GetCount();
	
	int uv_channel = 0;
	const char *nameUV = nameListUV.GetStringAt(uv_channel);

	int polyCount = mesh->GetPolygonCount();							
	for (int polyIndex = 0; polyIndex < polyCount; polyIndex++)
	{
		int polySize = mesh->GetPolygonSize(polyIndex);			

		for (int vertexIndex = 0; vertexIndex < polySize; ++vertexIndex)
		{
			int controlPointIndex = mesh->GetPolygonVertex(polyIndex, vertexIndex);
			
			FbxVector4 position = mesh->GetControlPointAt(controlPointIndex);

			FbxVector2 uv;
			bool isUnMapped;
			bool hasUV = mesh->GetPolygonVertexUV(polyIndex, vertexIndex, nameUV, uv, isUnMapped);

			FbxVector4 normal;
			mesh->GetPolygonVertexNormal(polyIndex, vertexIndex, normal);

			position = finalGlobalTransform.MultT(position);
			normal = finalGlobalTransform.MultT(normal);
			normal.Normalize();

			// Normal Map

			// Tangents
			FbxLayerElementTangent* meshTangents = mesh->GetElementTangent(0);
			if (meshTangents == nullptr) {
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
	submesh->VBO = CreateBufferObject(BufferType::VBO, sizeof(Vertex) * submesh->verticesCount, vertices);
	submesh->IBO = CreateBufferObject(BufferType::IBO, sizeof(uint32_t) * submesh->indicesCount, indices);

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
