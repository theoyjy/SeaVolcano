// Windows includes(For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <vector> // STL dynamic memory.
#include <unordered_map>
#include <cstdlib>

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

// Project includes
#include "maths_funcs.h"
#include <filesystem>
namespace fs = std::filesystem;

#include <glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <gtx/string_cast.hpp> 
#include <gtc/type_ptr.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>

#include "CameraControl.hpp"
#include "TextureManager.h"
#include "ParticleSystem.h"
#include "ShaderUtility.h"
#include "ProgramSetting.h"
#include "ModelStructure.h"
#include "lava.h"
#include <functional>

/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here


#define CRAB_FOLDER "Assets/Crabs/"
#define STATIC_MODEL_FOLDER "Assets/StaticModels/"
#define FISH_MODEL "Assets/animationModels/Fish.fbx"
#define LAVA_TEXTURE "Assets/Textures/lava.jpg"

/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/
using namespace std;



auto startTime = std::chrono::high_resolution_clock::now();
float timeInSeconds = 0.0f;


GLuint terrianShaderProgramID;

// all the meshes in the scene

//ModelData smoke_mesh;
//vector<ModelData> animation_meshes;

#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

glm::mat4 ConvertToGLMMat4(const aiMatrix4x4& aiMat) {
	return glm::mat4(
		aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
		aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
		aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
		aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
	);
}

ModelData load_mesh(const char* file_name, bool b_hierarchical_mesh) {
	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */

	// if the model contains hierarchical meshes, then no aiProcess_PreTransformVertices flag is needed
	// because it would flatten the hierarchy
	unsigned int pFlags = b_hierarchical_mesh ? (aiProcess_FlipUVs | aiProcess_GenSmoothNormals)
		: (aiProcess_PreTransformVertices | aiProcess_GlobalScale);

	const aiScene* scene = aiImportFile(file_name, pFlags | aiProcess_Triangulate);

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", filesystem::path(file_name).c_str());
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);
	//printf("  %i animation\n", scene->mAnimations);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		printf("    %i bones in mesh\n", mesh->mNumBones);

		ModelData* modelPtr = nullptr;
		if (m_i == 0) // load root mesh
		{
			modelPtr = &modelData;
			modelPtr->mLocalTransform = glm::mat4(1.0f);
		}
		else if (m_i >= 1) // load child mesh
		{
			modelData.mChildMeshes.push_back(ModelData());
			modelPtr = &modelData.mChildMeshes.back();

			// each child mesh has a local transform from its parent
			if (b_hierarchical_mesh)
			{
				modelPtr->mLocalTransform = ConvertToGLMMat4(scene->mRootNode->mChildren[0]->mChildren[m_i - 1]->mTransformation);
			}
		}

		// Load vertices, normals, colors, and texture coordinates
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelPtr->mVertices.push_back(glm::vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasVertexColors(0)) { // Ensure vertex colors exist
				aiColor4D maskColor = mesh->mColors[0][v_i];
				modelPtr->mColors.push_back(glm::vec4(maskColor.r, maskColor.g, maskColor.b, maskColor.a));
			}
			else {
				modelPtr->mColors.push_back(glm::vec4(1.0, 1.0, 1.0, 0.0)); // Default white color
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelPtr->mNormals.push_back(glm::vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelPtr->mTextureCoords.push_back(glm::vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
				/* You can extract tangents and bitangents here              */
				/* Note that you might need to make Assimp generate this     */
				/* data for you. Take a look at the flags that aiImportFile  */
				/* can take.                                                 */
			}
		}

		modelPtr->mPointCount += mesh->mNumVertices;

		for(unsigned int i = 0; i < scene->mNumMaterials; i++)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

			// find the texture path in the material properties
			for (unsigned int i = 0; i < material->mNumProperties; ++i) {
				aiMaterialProperty* prop = material->mProperties[i];

				// Check for the $tex.file or $raw.DiffuseColor|file property
				if (std::string(prop->mKey.C_Str()) == "$tex.file" ||
					std::string(prop->mKey.C_Str()) == "$raw.DiffuseColor|file") {

					std::string rawPath(prop->mData, prop->mData + prop->mDataLength);
					std::cout << "Raw texture path: " << rawPath << std::endl;

					// Remove the unwanted prefix (first character '8')
					std::string texturePath = rawPath.substr(4); // Remove first character
					texturePath = texturePath.substr(0, texturePath.length() - 1);
					size_t pos = 0;
					while ((pos = texturePath.find("\\")) != std::string::npos) {
						texturePath.replace(pos, 1, "/");
					}
					// if(fs::exists(texturePath))
					// 	std::cout << "Cleaned texture path: " << texturePath << std::endl;

					// Use the cleaned path for loading the texture
					modelPtr->mTexturePath = texturePath;
					modelPtr->mTextureId = TextureManager::Instance()->LoadTexture(texturePath);
				}
			}

			aiString name;
			material->Get(AI_MATKEY_NAME, name);
			printf("Material name: %s\n", name.C_Str());
			aiColor3D color(0.f, 0.f, 0.f);
			material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
			printf("Diffuse color: %f %f %f\n", color.r, color.g, color.b);
			material->Get(AI_MATKEY_COLOR_AMBIENT, color);
			printf("Ambient color: %f %f %f\n", color.r, color.g, color.b);
			material->Get(AI_MATKEY_COLOR_SPECULAR, color);
			printf("Specular color: %f %f %f\n", color.r, color.g, color.b);
			material->Get(AI_MATKEY_COLOR_EMISSIVE, color);
			printf("Emissive color: %f %f %f\n", color.r, color.g, color.b);
			float shininess;
			material->Get(AI_MATKEY_SHININESS, shininess);
			printf("Shininess: %f\n", shininess);
		}
	}

	aiReleaseImport(scene);
	cout << "finish load mesh\n";
	return modelData;
}

#pragma endregion MESH LOADING

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS
GLuint CompileTerrianShader()
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	terrianShaderProgramID = glCreateProgram();
	if (terrianShaderProgramID == 0) {
		cerr << "Error creating shader program..." << endl;
		cerr << "Press enter/return to exit..." << endl;
		cin.get();
		exit(1);
	}
	AddShader(terrianShaderProgramID, "simpleVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(terrianShaderProgramID, "simpleFragmentShader.txt", GL_FRAGMENT_SHADER);
	linkShader(terrianShaderProgramID);

	// set static uniforms
	glUniform3f(glGetUniformLocation(terrianShaderProgramID, "lightAmbient"), 0.3f, 0.3f, 0.3f);
	glUniform3f(glGetUniformLocation(terrianShaderProgramID, "lightDiffuse"), 1.5f, 1.5f, 1.5f);
	glUniform1f(glGetUniformLocation(terrianShaderProgramID, "depthFalloff"), 0.00001f);
	glUniform1f(glGetUniformLocation(terrianShaderProgramID, "causticIntensity"), 0.6f);
	return terrianShaderProgramID;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand

#pragma region VBO_FUNCTIONS

vector<string> GetAllModelsInPath(const char* path)
{
	vector<string> ModelPaths;
	// Iterate through the animation model folder and store the paths in a vector
	try {
		for (const auto& entry : fs::directory_iterator(path)) {
			// Check if the entry is a file (not a directory)
			if (entry.is_regular_file()) {
				ModelPaths.push_back(entry.path().string());
				std::cout << entry.path().filename() << std::endl;
			}
		}
	}
	catch (const fs::filesystem_error& e) {
		std::cerr << "Filesystem error: " << e.what() << std::endl;
	}
	catch (const std::exception& e) {
		std::cerr << "General exception: " << e.what() << std::endl;
	}
	return ModelPaths;
}

inline void CalcFishInstanceTransform(const FishInstance& fish, glm::mat4& transform)
{
	// Compute position on the circle, including vertical offset (y)
	float x = fish.radius * cos(fish.angle);
	float z = fish.radius * sin(fish.angle);
	glm::vec3 position(x, fish.y, z); // Include vertical offset in position

	// Compute tangent direction (fish's forward direction)
	glm::vec3 forwardDirection(
		-sin(fish.angle),
		0.0f,
		cos(fish.angle)
	);
	forwardDirection = glm::normalize(forwardDirection);

	// Compute rotation matrix to align fish with the forward direction
	glm::vec3 up(0.0f, 1.0f, 0.0f);
	glm::vec3 right = glm::normalize(glm::cross(up, forwardDirection));
	glm::vec3 correctedUp = glm::cross(forwardDirection, right);

	// update the fish transform matrix
	glm::mat4 rotation = glm::mat4(1.0f);
	rotation[0] = glm::vec4(right, 0.0f);
	rotation[1] = glm::vec4(correctedUp, 0.0f);
	rotation[2] = glm::vec4(-forwardDirection, 0.0f);

	transform = glm::translate(glm::mat4(1.0f), position) * rotation;
}

void InitializeFishInstances() 
{
	fishInstanceTransforms.resize(FISHCOUNT, glm::mat4(1.0f));

	for (float i = 0; i < FISHCOUNT; ++i)
	{
		FishInstance instance;
		instance.angle = 0.3f * glm::two_pi<float>() * i;
		instance.speed = 0.075f;
		instance.radius = 40.f + 1.f * i;
		instance.y = 40.f + 5.f * i;
		CalcFishInstanceTransform(instance, fishInstanceTransforms[int(i)]);
		fishInstances.push_back(instance);
	}
}

void generateObjectBufferMesh() {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	// get all the static model paths
	vector<string> StaticModelPaths = GetAllModelsInPath(STATIC_MODEL_FOLDER);
	// load each model
	for(auto& path : StaticModelPaths)
	{
		 staticModels.push_back(load_mesh(path.c_str(), false));
	}

	const vector<pair<glm::vec3, glm::vec3>> CrabInitData = {
		{{ -65, 8, 30}, {0, glm::radians(-64.8), 0}},
		{{ -94, 9, 23}, {0, glm::radians(74.f), 0}},
		{{ -93, 9, 14}, {0, glm::radians(-30.f), 0}},
		{{ -93,7, 17}, {0, glm::radians(-53.f), 0}},
		{{ -84.4,7,24.7}, {0, glm::radians(119.f), 0}}
	};

	vector<string> CrabsPaths = GetAllModelsInPath(CRAB_FOLDER);
	// load each model
	for (int i = 0; i < CrabsPaths.size(); ++i)
	{
		Crab crab;
		crab.model = load_mesh(CrabsPaths[i].c_str(), false);
		for (const auto& vertex : crab.model.mVertices)
		{
			crab.translation += vertex;
		}
		crab.translation /= static_cast<float>(crab.model.mVertices.size());
		crab.translation = CrabInitData[i].first;
		crab.rotation = CrabInitData[i].second;
		CrabModels.emplace_back(crab);
	}

	fishModel = load_mesh(FISH_MODEL, true);
	InitializeFishInstances();

	// Generate lava mesh
	lavaModel = generateLavaPlane(lavaWidth, lavaWidth, 100, 100);
	lavaModel.mTexturePath = LAVA_TEXTURE;
	lavaModel.mTextureId = TextureManager::Instance()->LoadTexture(LAVA_TEXTURE);

	// Set up the VAO and VBOs for terrain and all animation models
	GLuint loc1 = glGetAttribLocation(terrianShaderProgramID, "vertex_position");
	GLuint loc2 = glGetAttribLocation(terrianShaderProgramID, "vertex_normal");
	GLuint loc3 = glGetAttribLocation(terrianShaderProgramID, "tex_coords");

	function<void(ModelData&, Type)> SetUpModelBuffers = [&](ModelData& model, Type type) {
		glGenVertexArrays(1, &model.mVao);
		glBindVertexArray(model.mVao);
		glGenBuffers(3, model.mVBOs);
		glGenBuffers(1, &model.mEBO);


		// Vertices VBO
		glBindBuffer(GL_ARRAY_BUFFER, model.mVBOs[0]);
		glBufferData(GL_ARRAY_BUFFER, model.mPointCount * sizeof(glm::vec3), model.mVertices.data(), type == Type::LAVA? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(loc1);

		// Normals VBO
		glBindBuffer(GL_ARRAY_BUFFER, model.mVBOs[1]);
		glBufferData(GL_ARRAY_BUFFER, model.mPointCount * sizeof(glm::vec3), model.mNormals.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(loc2);

		// Texture VBO
		glBindBuffer(GL_ARRAY_BUFFER, model.mVBOs[2]);
		glBufferData(GL_ARRAY_BUFFER, model.mPointCount * sizeof(glm::vec2), model.mTextureCoords.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(loc3, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
		glEnableVertexAttribArray(loc3);

		switch (type)
		{
		case Type::FISH: // Instance
			glGenBuffers(1, &model.instanceVBO);
			glBindBuffer(GL_ARRAY_BUFFER, model.instanceVBO);

			// Allocate space for instance data (e.g., model matrices)
			glBufferData(GL_ARRAY_BUFFER, fishInstanceTransforms.size() * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

			// Set up instanced matrix attribute (mat4 occupies 4 attribute slots)
			for (int i = 0; i < 4; i++) {
				glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * sizeof(glm::vec4)));
				glEnableVertexAttribArray(3 + i);
				glVertexAttribDivisor(3 + i, 1); // Tell OpenGL this is per-instance data
			}
			break;
		case Type::LAVA:
			// EBO
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.mEBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.mIndices.size() * sizeof(unsigned int), model.mIndices.data(), GL_STATIC_DRAW);
			break;
		default:
			break;
		}

		glBindVertexArray(0); 
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		for (int j = 0; j < model.mChildMeshes.size(); ++j)
		{
			SetUpModelBuffers(model.mChildMeshes[j], type);
		}
	};

	glUseProgram(terrianShaderProgramID);
	for(auto& model : staticModels)
	{
		SetUpModelBuffers(model, Type::STATIC);
	}

	for (auto& crab : CrabModels)
	{
		SetUpModelBuffers(crab.model, Type::CRAB);
	}

	
	SetUpModelBuffers(fishModel, Type::FISH);
	
	SetUpModelBuffers(lavaModel, Type::LAVA);

	
	std::cout << "finish generate object buffer mesh\n";

}

#pragma endregion VBO_FUNCTIONS

void renderBitmapText(float x, float y, void* font, const char* text) {
	glRasterPos2f(x, y);
	while (*text) {
		glutBitmapCharacter(font, *text);
		text++;
	}
}


void renderModels()
{
	glUseProgram(terrianShaderProgramID);

	// following uniforms set up once per frame
	glUniformMatrix4fv(glGetUniformLocation(terrianShaderProgramID, "proj"), 1, GL_FALSE, glm::value_ptr(persp_proj));
	glUniformMatrix4fv(glGetUniformLocation(terrianShaderProgramID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniform3f(glGetUniformLocation(terrianShaderProgramID, "viewPos"), cameraPosition.x, cameraPosition.y, cameraPosition.z);
	glUniform3f(glGetUniformLocation(terrianShaderProgramID, "lightDirection"), lightDirection.x, lightDirection.y, lightDirection.z);
	glUniform1f(glGetUniformLocation(terrianShaderProgramID, "time"), timeInSeconds);


	int matrix_loc = glGetUniformLocation(terrianShaderProgramID, "model");
	//  update uniforms & draw
	function<void(const ModelData&, glm::mat4&, Type)> UpdateShaderVariables = [&](const ModelData& mesh, glm::mat4& modelMatrix, Type type) {
		// bind texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mesh.mTextureId);
		// bind VAO
		glBindVertexArray(mesh.mVao);

		// update common uniforms
		glUniformMatrix4fv(matrix_loc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
		glUniform1i(glGetUniformLocation(terrianShaderProgramID, "texture1"), 0);
		glUniform1i(glGetUniformLocation(terrianShaderProgramID, "type"), int(type));
		if (type == Type::LAVA)
		{
			glDrawElements(GL_TRIANGLES, mesh.mIndices.size(), GL_UNSIGNED_INT, 0);
		}
		else
		{
			glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.mPointCount));
		}
		// Cleanup
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	};

	function<void(const ModelData&, const vector<glm::mat4>&)> UpdateAnimationInstances = [&](const ModelData& mesh, const vector<glm::mat4>& transforms)
	{	
		// bind texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mesh.mTextureId);

		// bind VAO
		glBindVertexArray(mesh.mVao);

		glUniform1i(glGetUniformLocation(terrianShaderProgramID, "texture1"), 0);

		// bind isInstanced flag
		glUniform1i(glGetUniformLocation(terrianShaderProgramID, "type"), int(Type::FISH));

		// Update instance mesh
		if (!fishInstances.empty() && mesh.instanceVBO != 0)
		{
			// Bind the instance VBO
			glBindBuffer(GL_ARRAY_BUFFER, mesh.instanceVBO);

			// Map the buffer and copy new instance data
			glm::mat4* mappedBuffer = static_cast<glm::mat4*>(
				glMapBufferRange(GL_ARRAY_BUFFER, 0, transforms.size() * sizeof(glm::mat4),
					GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

			// Copy the updated model matrices to the buffer
			std::memcpy(mappedBuffer, transforms.data(), transforms.size() * sizeof(glm::mat4));

			// Unmap the buffer
			glUnmapBuffer(GL_ARRAY_BUFFER);
			glDrawArraysInstanced(GL_TRIANGLES, 0, static_cast<GLsizei>(fishModel.mPointCount), transforms.size());
		}

		// Cleanup
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
	};
	glm::mat4 modelMat(1.0f);

	// Draw static models
	for (const auto& staticModel : staticModels)
	{
		UpdateShaderVariables(staticModel, modelMat, Type::STATIC);
	}

	// Draw crabs
	for (const auto& crab : CrabModels)
	{
		modelMat = crab.GetModelTransform();
		UpdateShaderVariables(crab.model, modelMat, Type::CRAB);
	}

	// Draw lava
	modelMat = glm::mat4(1.0f);
	modelMat = glm::translate(modelMat, lavaPosition);
	UpdateShaderVariables(lavaModel, modelMat, Type::LAVA);


	// Draw instanced fish animation 
	// Calcualte the fish body rotation matrix
	glm::mat4 rotateBodyMtx = glm::mat4(1.f);
	rotateBodyMtx = glm::rotate(rotateBodyMtx, glm::radians(rotate_body), glm::vec3(0.0f, 1.0f, 0.0f));
	vector<glm::mat4> TmpTransforms(FISHCOUNT, rotateBodyMtx);

	// Calculate the transforms of all fish body instances
	std::transform(fishInstanceTransforms.begin(), fishInstanceTransforms.end(), TmpTransforms.begin(),
		[&](const glm::mat4& transform) { return transform * rotateBodyMtx; });

	// pass transforms to the shader
	UpdateAnimationInstances(fishModel, TmpTransforms);

	for (int i = 0; i < fishModel.mChildMeshes.size(); ++i)
	{
		//  Calcualte the child rotation matrix
		auto& childMesh = fishModel.mChildMeshes[i];
		glm::mat4 rotateChildMtx(1.0);
		if (i == 0) // first child mesh is the head
		{
			rotateChildMtx = glm::rotate(rotateChildMtx, glm::radians(rotate_head), glm::vec3(0.0f, 1.0f, 0.0f));
		}
		else // second child mesh is the fin
		{
			rotateChildMtx = glm::rotate(rotateChildMtx, glm::radians(rotate_fin), glm::vec3(0.0f, 1.0f, 0.0f));
		}
		
		// Calculate the transforms of all child instances for the same part
		std::transform(fishInstanceTransforms.begin(), fishInstanceTransforms.end(), TmpTransforms.begin(),
			[&](const glm::mat4& transform) { return transform * rotateBodyMtx * fishModel.mChildMeshes[i].mLocalTransform * rotateChildMtx; });
	
		// pass transforms of child instances to the shader
		UpdateAnimationInstances(childMesh, TmpTransforms);
	}
}

void display() {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// Background color to blue
	glClearColor(0.004f, 0.361f, 0.588f, 0.8f);

	// update view matrix
	glm::vec3 forward(0.0);
	forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward.y = sin(glm::radians(pitch));
	forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward = glm::normalize(forward);
	glm::vec3 cameraTarget = cameraPosition + forward;
	view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

	renderModels();
	ParticleSystem::Instance()->renderParticles();

	glutSwapBuffers();
}


const float PI = 3.14159265358979323846f;

void updateIllumination()
{

	// Oscillate light position along an axis
	lightPosition = glm::vec3(
		10.0f * sin(timeInSeconds * 2.0), // Faster horizontal oscillation
		15.0f + sin(timeInSeconds * 2.0) * 2.0f, // Smaller vertical oscillation
		10.0f * cos(timeInSeconds * 2.0)  // Faster horizontal oscillation
	);

	// Calculate light direction based on the position of the light
	lightDirection = glm::normalize(lightPosition - glm::vec3(0.0f));
}

float currentAngle = 0.0f;
void updateAnimation()
{
	// Fin and head of a fish rotate in a sinusoidal pattern
	rotate_fin = 4.f * sin(timeInSeconds*2.5f);
	rotate_body = 2.f * sin(timeInSeconds*2.5f + glm::radians(45.0f));
	rotate_head = 4.f * sin(timeInSeconds*2.5f + glm::radians(90.0f));

	const float deltaTime = 0.016f;
	for (int i = 0; i < FISHCOUNT; ++i) {
		auto& fish = fishInstances[i];
		// Update angle based on speed
		fish.angle += fish.speed * deltaTime;
		if (fish.angle > glm::two_pi<float>()) {
			fish.angle -= glm::two_pi<float>();
		}
		CalcFishInstanceTransform(fish, fishInstanceTransforms[i]);
	}

}

float NormalizeAngle(float angle) {
	while (angle > glm::pi<float>()) angle -= glm::two_pi<float>();
	while (angle < -glm::pi<float>()) angle += glm::two_pi<float>();
	return angle;
}

void updateCrabMovement() {
	const float deltaTime = 0.016f;

	for (auto& crab : CrabModels) 
	{
		if (crab.runAway) 
		{
			glm::vec3 direction = glm::normalize(crab.CurrentPosition() - cameraPosition);
			crab.translation += direction * crab.velocity * deltaTime;

			/// Update crab's rotation to "face away" from the camera
			float targetYaw = NormalizeAngle((atan2(direction.x, direction.z))); // Yaw angle for local Y-axis rotation

			// Smoothly interpolate rotation to target
			if(crab.runAwayTime  >= (RUNAWAYTIME / 2.f))
				crab.rotation.y = crab.rotation.y + NormalizeAngle(targetYaw - crab.rotation.y) * (RUNAWAYTIME - crab.runAwayTime) / RUNAWAYTIME * 2.f;

			// Stop movement after a certain distance
			cout << "Crab run away time: " << crab.runAwayTime << endl;
			if (crab.runAwayTime <= 0.f) {
				crab.runAway = false; // Stop the crab
			}
			crab.runAwayTime -= deltaTime;
		}
	}
}

void updateScene() {

	static auto lastTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();

	// Elapsed time in seconds
	timeInSeconds = (currentTime - startTime).count() * 1e-9;

	// update sun light position and direction
	updateIllumination();

	// update fish animation
	updateAnimation();

	// update crab movement
	updateCrabMovement();

	// update smoke particles
	ParticleSystem::Instance()->updateParticles(0.01, 3);

	// update lava
	updatePlaneVerticesWithHeight(lavaModel, 100, 100, timeInSeconds);

    
	// Update the camera position based on user input
	keyControl::updateCameraPosition();

    glutPostRedisplay();
}

// set shaders and generate object buffers
void init()
{
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	CompileTerrianShader();
	generateObjectBufferMesh();
	ParticleSystem::Instance()->Init();
}


int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("Underwater volcano");

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);

	glutKeyboardFunc(keyControl::keypress);
	glutKeyboardUpFunc(keyControl::keyRelease);
	glutSpecialFunc(keyControl::specialKeypress);
	glutSpecialUpFunc(keyControl::specialKeyRelease);
	glutMouseFunc(keyControl::mouseButton);
	glutMotionFunc(keyControl::mouseMotion);


	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
	return 0;
}
