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
#include <functional>

/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here


#define ANIMATION_FOLDER "Assets/animationModels/"
#define STATIC_MODEL_FOLDER "Assets/StaticModels/"


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/
using namespace std;

#pragma region SimpleTypes

typedef struct ModelData
{
	size_t mPointCount = 0;
	GLuint mVao = 0;
	vector<glm::vec3> mVertices;
	vector<glm::vec3> mNormals;
	vector<glm::vec4> mColors;
	vector<glm::vec2> mTextureCoords;
	glm::mat4 mLocalTransform;

	string mTexturePath;
	GLuint mTextureId;

	vector<ModelData> mChildMeshes;
	
} ModelData;
#pragma endregion SimpleTypes



auto startTime = std::chrono::high_resolution_clock::now();
float timeInSeconds = 0.0f;


GLuint terrianShaderProgramID;

// all the meshes in the scene
vector<ModelData> staticModels;
ModelData smoke_mesh;
vector<ModelData> animation_meshes;

// fish hierarchical mesh rotation for each fish part
GLfloat rotate_head = 0.0f, rotate_fin = 0.0f, rotate_body = 0.f;

// whole translation for each fish model
vector<glm::vec3> fish_translations(9, glm::vec3(0.0f));

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
	glUniform1f(glGetUniformLocation(terrianShaderProgramID, "causticIntensity"), 0.75f);
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

void generateObjectBufferMesh() {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	//Might be an idea to do a check for that before generating and binding the buffer.

	// get all the static model paths
	vector<string> StaticModelPaths = GetAllModelsInPath(STATIC_MODEL_FOLDER);
	// load each model
	for(auto& path : StaticModelPaths)
	{
		 staticModels.push_back(load_mesh(path.c_str(), false));
	}

	// get all the animation model paths
	vector<string> animationModelPaths = GetAllModelsInPath(ANIMATION_FOLDER);

	// Load each animation model
	for (size_t i = 0; i < animationModelPaths.size(); i++)
	{
		animation_meshes.push_back(load_mesh(animationModelPaths[i].c_str(), true));
	}

	
	// Set up the VAO and VBOs for terrain and all animation models
	GLuint loc1 = glGetAttribLocation(terrianShaderProgramID, "vertex_position");
	GLuint loc2 = glGetAttribLocation(terrianShaderProgramID, "vertex_normal");
	GLuint loc3 = glGetAttribLocation(terrianShaderProgramID, "tex_coords");
	//if (loc1 == -1 || loc2 == -1 || loc3 == -1)
	//{
	//	std::cerr << "Error getting attribute location" << std::endl;
	//	exit(1);
	//}

	function<void(ModelData&)> SetUpModelBuffers = [&](ModelData& model) {
		glGenVertexArrays(1, &model.mVao);
		glBindVertexArray(model.mVao);
		GLuint vbos[3] = { 0, 0, 0};
		glGenBuffers(3, vbos);

		// Vertices VBO
		glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
		glBufferData(GL_ARRAY_BUFFER, model.mPointCount * sizeof(glm::vec3), model.mVertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(loc1);

		// Normals VBO
		glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
		glBufferData(GL_ARRAY_BUFFER, model.mPointCount * sizeof(glm::vec3), model.mNormals.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(loc2);

		// Texture VBO
		glBindBuffer(GL_ARRAY_BUFFER, vbos[2]);
		glBufferData(GL_ARRAY_BUFFER, model.mPointCount * sizeof(glm::vec2), model.mTextureCoords.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(loc3, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
		glEnableVertexAttribArray(loc3);

		glBindVertexArray(0); // unbind VAO
		glBindBuffer(GL_ARRAY_BUFFER, 0); // unbind VBO

		for (int j = 0; j < model.mChildMeshes.size(); ++j)
		{
			SetUpModelBuffers(model.mChildMeshes[j]);
		}
	};

	glUseProgram(terrianShaderProgramID);
	for(auto& model : staticModels)
	{
		SetUpModelBuffers(model);
	}
	
	SetUpModelBuffers(smoke_mesh);
	
	for (int i = 0; i < animationModelPaths.size(); ++i) {
		SetUpModelBuffers(animation_meshes[i]);
	}

	
	cout << "finish generate object buffer mesh\n";

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

	int matrix_loc = glGetUniformLocation(terrianShaderProgramID, "model");
	int view_mat_loc = glGetUniformLocation(terrianShaderProgramID, "view");
	int proj_mat_loc = glGetUniformLocation(terrianShaderProgramID, "proj");
	
	// update terrain uniforms & draw
	function<void(const ModelData&, glm::mat4&, bool)> UpdateNormalMeshUniforms = [&](const ModelData& mesh, glm::mat4& modelMatrix, bool isSmoke) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mesh.mTextureId);
		glBindVertexArray(mesh.mVao);
		glUniformMatrix4fv(proj_mat_loc, 1, GL_FALSE, persp_proj.m);
		glUniformMatrix4fv(view_mat_loc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(matrix_loc, 1, GL_FALSE, glm::value_ptr(modelMatrix));


		glUniform3f(glGetUniformLocation(terrianShaderProgramID, "viewPos"), cameraPosition.x, cameraPosition.y, cameraPosition.z);
		glUniform1i(glGetUniformLocation(terrianShaderProgramID, "texture1"), 0);

		//uniform vec3 lightDirection;    // Direction of sunlight (normalized)
		//uniform vec3 lightAmbient;      // Ambient light color					// static
		//uniform vec3 lightDiffuse;      // Diffuse light color					// static
		//uniform float depthFalloff;     // Light attenuation factor with depth
		//uniform float causticIntensity; // Strength of sunlight caustics
		//uniform float time;             // Animation time

		glUniform3f(glGetUniformLocation(terrianShaderProgramID, "lightDirection"), lightDirection.x, lightDirection.y, lightDirection.z);
		glUniform1f(glGetUniformLocation(terrianShaderProgramID, "time"), timeInSeconds);
		

		glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.mPointCount));
	};

	glm::mat4 modelMat(1.0f);

	// Draw terrain
	for (const auto& staticModel : staticModels)
	{
		UpdateNormalMeshUniforms(staticModel, modelMat, false);
	}

	// Draw animation models
	for (int i = 0; i < animation_meshes.size(); ++i)
	{
		// translate the root mesh
		glm::mat4 rootMesh = glm::mat4(1.f);
		rootMesh = glm::rotate(rootMesh, glm::radians(rotate_body), glm::vec3(0.0f, 1.0f, 0.0f));
		rootMesh = glm::translate(rootMesh, fish_translations[i]);
		UpdateNormalMeshUniforms(animation_meshes[i], rootMesh, false);

		// body rotation not effect the child meshes
		rootMesh = glm::mat4(1.f);
		rootMesh = glm::translate(rootMesh, fish_translations[i]);

		for (int j = 0; j < animation_meshes[i].mChildMeshes.size(); ++j)
		{
			// rotate the child mesh
			auto& childMesh = animation_meshes[i].mChildMeshes[j];
			glm::mat4 child(1.0);
			if (j == 0) // first child mesh is the head
			{
				child = glm::rotate(child, glm::radians(rotate_head), glm::vec3(0.0f, 1.0f, 0.0f));
			}
			else // second child mesh is the fin
			{
				child = glm::rotate(child, glm::radians(rotate_fin), glm::vec3(0.0f, 1.0f, 0.0f));
			}

			// update the child transform matrix by multiplying the root mesh transform and its local transform
			child = rootMesh * animation_meshes[i].mChildMeshes[j].mLocalTransform * child;

			UpdateNormalMeshUniforms(childMesh, child, false);
		}
	}

	// renderBitmapText(-1.0, 0.9, GLUT_BITMAP_HELVETICA_18, "The scene consists of a shining volcano, swiming fishes, smoke out of volcano, rocks, coral");
	// renderBitmapText(-1.0, 0.85, GLUT_BITMAP_HELVETICA_18, "All the fishes are moving towards its heading direction");
	// renderBitmapText(-1.0, 0.8, GLUT_BITMAP_HELVETICA_18, "Each Fish contains hierarchical meshes that fin and head would move relatively to its body");
	// renderBitmapText(-1.0, 0.75, GLUT_BITMAP_HELVETICA_18, "Key Control : W(forward) S(backward) A(left) D(right) Q(upward) E(downward). Shift + Key Control : Speed up");
	// renderBitmapText(-1.0, 0.7, GLUT_BITMAP_HELVETICA_18, "Mouse Control : Left click and drag to control camera's yaw and pitch");
}

void display() {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.004f, 0.361f, 0.588f, 0.8f); // Background color to blue

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
const vector<glm::vec3> fish_translate_directions = { 
	glm::vec3(0.0f, 0.0f, -0.1f), 
	glm::vec3(0.1f, 0.0f, -0.1f),
	glm::vec3(0.1f, 0.0f, -0.1f),
	glm::vec3(-0.1f, 0.0f, -0.1f),
	glm::vec3(-0.1f, 0.0f, 0.0f),
	glm::vec3(0.1f, 0.0f, -0.1f),
	glm::vec3(0.1f, 0.0f, -0.1f),
	glm::vec3(-0.1f, 0.0f, 0.1f),
	glm::vec3(-0.1f, 0.0f, -0.1f),
};


void updateIllumination()
{

	// Oscillate light position along an axis
	lightPosition = glm::vec3(
		10.0f * sin(timeInSeconds * 2.0), // Faster horizontal oscillation
		15.0f + sin(timeInSeconds * 2.0) * 2.0f, // Smaller vertical oscillation
		10.0f * cos(timeInSeconds * 2.0)  // Faster horizontal oscillation
	);

	// Calculate light direction (normalized)
	lightDirection = glm::normalize(lightPosition - glm::vec3(0.0f));
}


void updateAnimation()
{
	// Fin and head of a fish rotate in a sinusoidal pattern
	rotate_fin = 0.4f * sin(timeInSeconds);
	rotate_body = 0.4f * sin(timeInSeconds + PI / 4.0f);
	rotate_head = 0.4f * sin(timeInSeconds + PI / 2.0f);

	for (int i = 0; i < fish_translations.size(); ++i)
	{
		fish_translations[i] += fish_translate_directions[i] * 0.01f;
	}
}

void updateScene() {

	static auto lastTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();

	// Elapsed time in seconds
	timeInSeconds = (currentTime - startTime).count() * 1e-9;

	updateIllumination();

	updateAnimation();

	ParticleSystem::Instance()->updateParticles(0.01, 3);
    
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
