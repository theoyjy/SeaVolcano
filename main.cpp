// Windows includes(For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.
#include <unordered_map>

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
#include <functional>

/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define TERRAIN_MESH "Assets/terrain.fbx"
#define FISH1_MESH "Assets/Armature.001_with_mesh.fbx"
#define ANIMATION_FOLDER "Assets/animationModels/"

#define TERRIAN 1
#define SKELETON 0
#define COLORANIMATION 0

/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/
using namespace std;
#pragma region SimpleTypes
struct KeyFrame {
	float positionTimestamp = 0.0f;
	float rotationTimestamp = 0.0f;
	float scaleTimestamp = 0.0f;

	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale;
	float time;
};

struct BoneNode {
	int boneID;                   // 骨骼ID（在mBoneNames中的索引）
	glm::mat4 currentTransform;   // 当前骨骼变换矩阵
	glm::mat4 offsetMatrix;
	string name;
	BoneNode* parent;             // 指向父骨骼的指针
	std::vector<BoneNode*> children; // 子骨骼列表
};

typedef struct ModelData
{
	size_t mPointCount = 0;
	size_t mPointEffectedByBone = 0;
	size_t mBoneCount = 0;
	size_t mAnimationCount = 0;
	float mTicksPerSecond = 0.0f;
	float mDuration = 0.0f;
	vector<glm::vec3> mVertices;
	vector<glm::vec3> mNormals;
	vector<glm::vec4> mColors;
	vector<vec2> mTextureCoords;

#if SKELETON
	vector<string> mBoneNames;          // 骨骼名称
	vector<glm::ivec4> mBoneIDs;             // 每个顶点受影响的骨骼ID (最多4个)
	vector<glm::vec4> mWeights;              // 每个顶点受影响骨骼的权重 (最多4个)
	vector<vector<KeyFrame>> mBoneKeyframes;      // 每个骨骼的关键帧
	vector<glm::mat4> currentBoneTransformation;    // 每个骨骼的动画矩阵
	std::vector<BoneNode*> boneNodes;          // hierarchy of bones
	glm::mat4 globalInverseTransform;          // root bone

	int GetBoneIDByName(const std::string& name)
	{
		for (int i = 0; i < mBoneNames.size(); i++)
		{
			if (name == mBoneNames[i])
			{
				return i;
			}
		}
		return mBoneNames.size();
	}

	bool HasAnimations() const
	{
		return mBoneKeyframes.size() > 0;
	}
#endif

} ModelData;
#pragma endregion SimpleTypes



auto startTime = std::chrono::high_resolution_clock::now();

using namespace std;
GLuint terrianShaderProgramID, animationShaderProgramID;


ModelData volcano_terrian_mesh;
ModelData fish1_mesh;
vector<ModelData> animation_meshes;
std::vector<GLuint> animation_vaos;
unsigned int mesh_vao = 0;
int width = 1440;
int height = 720;

float timeInSeconds = 0.f;
GLfloat rotate_y = 0.0f;

#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/
#if SKELETON
void processNodeHierarchy(const aiNode* node, BoneNode* parentBone, ModelData& modelData) {
	// Check if this node is a bone by looking for its name in the model's bone list
	std::string nodeName(node->mName.C_Str());
	int boneID = modelData.GetBoneIDByName(nodeName);
	BoneNode* currentBone = nullptr;
	if(boneID < modelData.mBoneNames.size())
	{
		currentBone = modelData.boneNodes[boneID];
	}
 
	// If there's a parent, add this bone to the parent's children list
	if (parentBone != nullptr) {
		parentBone->children.push_back(currentBone);
	}

	// Process child nodes recursively
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNodeHierarchy(node->mChildren[i], currentBone, modelData);
	}
}

void buildBoneHierarchy(const aiScene* scene, ModelData& modelData) {
	const aiNode* rootNode = scene->mRootNode;
	processNodeHierarchy(rootNode, nullptr, modelData);
}
#endif

ModelData load_mesh(const char* file_name, bool with_anime) {
	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */

	unsigned int pFlags = with_anime ? (aiProcess_FlipUVs | aiProcess_GenSmoothNormals)
		: (aiProcess_PreTransformVertices | aiProcess_GlobalScale);


	const aiScene* scene = aiImportFile(file_name, pFlags | aiProcess_Triangulate);

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", filesystem::path(file_name).c_str());
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);
	printf("  %i animation\n", scene->mAnimations);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		printf("    %i bones in mesh\n", mesh->mNumBones);

		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(glm::vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasVertexColors(0)) { // Ensure vertex colors exist
				aiColor4D maskColor = mesh->mColors[0][v_i];
				modelData.mColors.push_back(glm::vec4(maskColor.r, maskColor.g, maskColor.b, maskColor.a));
			}
			else {
				modelData.mColors.push_back(glm::vec4(1.0, 1.0, 1.0, 1.0)); // Default white color
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(glm::vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelData.mTextureCoords.push_back(vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
				/* You can extract tangents and bitangents here              */
				/* Note that you might need to make Assimp generate this     */
				/* data for you. Take a look at the flags that aiImportFile  */
				/* can take.                                                 */
			}
		}

		size_t baseIndex = modelData.mPointCount;
		modelData.mPointCount += mesh->mNumVertices;

#if SKELETON
		modelData.mBoneIDs.resize(modelData.mPointCount, glm::vec4(0.0f));
		modelData.mWeights.resize(modelData.mPointCount, glm::vec4(0.0f));

		if (mesh->HasBones() && with_anime)
		{
			modelData.mBoneCount += mesh->mNumBones;
			modelData.boneNodes.resize(modelData.mBoneCount, nullptr);

			glm::mat4 GlobalInverseTransform = glm::inverse(glm::transpose(glm::make_mat4(&scene->mRootNode->mTransformation.a1)));
			modelData.globalInverseTransform = GlobalInverseTransform;
			cout << "Global Inverse Transform:\n" << glm::to_string(GlobalInverseTransform) << std::endl;
			// animation
			for (unsigned int m_b = 0; m_b < mesh->mNumBones; m_b++)
			{
				// to do: add armature name in bones' name
				aiBone* bone = mesh->mBones[m_b];
				int boneID = modelData.GetBoneIDByName(bone->mName.C_Str());
				if (boneID == modelData.mBoneNames.size())
				{
					modelData.mBoneNames.push_back(bone->mName.C_Str());
					modelData.boneNodes[boneID] = new BoneNode();
				}
				modelData.boneNodes[boneID]->boneID = boneID;
				modelData.boneNodes[boneID]->parent = NULL;
				modelData.boneNodes[boneID]->name = bone->mName.C_Str();
				modelData.boneNodes[boneID]->offsetMatrix = glm::transpose(glm::make_mat4(&bone->mOffsetMatrix.a1));
	
				std::cout << "Bone Name: " << modelData.boneNodes[boneID]->name << std::endl;
				std::cout << "Offset Matrix:\n" << glm::to_string(modelData.boneNodes[boneID]->offsetMatrix) << std::endl;

				for (unsigned int k = 0; k < bone->mNumWeights; k++)
				{
					aiVertexWeight& weight = bone->mWeights[k];
					int vertexID = weight.mVertexId;
					float boneWeight = weight.mWeight;

					for (int i = 0; i < 4; i++)
					{
						if (modelData.mBoneIDs[baseIndex + vertexID][i] == 0)
						{
							modelData.mBoneIDs[baseIndex + vertexID][i] = boneID;
							modelData.mWeights[baseIndex + vertexID][i] = boneWeight;
							break;
						}
					}
				}

			}

			buildBoneHierarchy(scene, modelData);

			for (auto& weights : modelData.mWeights) {
				float totalWeight = weights.x + weights.y + weights.z + weights.w;
				if (totalWeight > 0.0f) {
					weights /= totalWeight; // Normalize weights
				}
			}
		}
#endif
	}

#if SKELETON
	if (scene->HasAnimations() && with_anime) {
		
		aiAnimation* animation = scene->mAnimations[0];

		// initialize bone keyframes and animations
		modelData.mAnimationCount += animation->mNumChannels;
		modelData.mBoneKeyframes.resize(modelData.mAnimationCount);
		modelData.currentBoneTransformation.resize(modelData.mAnimationCount);

		// record animation data
		modelData.mTicksPerSecond = animation->mTicksPerSecond;
		modelData.mDuration = animation->mDuration;

		// record keyframes
		for (unsigned int i = 0; i < animation->mNumChannels; i++) {
			aiNodeAnim* channel = animation->mChannels[i];
			int boneID = modelData.GetBoneIDByName(channel->mNodeName.C_Str());
			if (boneID == modelData.mBoneNames.size())
			{
				continue;
			}
			unsigned int keyCount = std::min({ channel->mNumPositionKeys, channel->mNumRotationKeys, channel->mNumScalingKeys });

			for (unsigned int j = 0; j < keyCount; j++) {
				aiVector3D pos = channel->mPositionKeys[j].mValue;
				aiQuaternion rot = channel->mRotationKeys[j].mValue;
				aiVector3D scale = channel->mScalingKeys[j].mValue;
				double time = channel->mPositionKeys[j].mTime;

				// 保存关键帧数据
				KeyFrame keyframe;
				keyframe.position = glm::vec3(pos.x, pos.y, pos.z);
				keyframe.positionTimestamp = channel->mPositionKeys[j].mTime;
				keyframe.rotation = glm::quat(rot.w, rot.x, rot.y, rot.z);
				keyframe.rotationTimestamp = channel->mRotationKeys[j].mTime;
				keyframe.scale = glm::vec3(scale.x, scale.y, scale.z);
				keyframe.scaleTimestamp = channel->mScalingKeys[j].mTime;
				keyframe.time = time / double(modelData.mTicksPerSecond);

				modelData.mBoneKeyframes[boneID].push_back(keyframe);
			}
		}
	}
#endif //  SKELETON

	aiReleaseImport(scene);
	cout << "finish load mesh\n";
	return modelData;
}

void loadAnimationModels() {

	vector<string> animationModelPaths;
	try {
		// Iterate through the directory
		for (const auto& entry : fs::directory_iterator(ANIMATION_FOLDER)) {
			// Check if the entry is a file (not a directory)
			if (entry.is_regular_file()) {
				animationModelPaths.push_back(entry.path().string());
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

	for(size_t i = 0; i < animationModelPaths.size(); i++)
	{
		animation_meshes.push_back(load_mesh(animationModelPaths[i].c_str(), true));
	}

	animation_vaos.resize(animationModelPaths.size());
	std::vector<GLuint> vbos(animationModelPaths.size() * 3); // 3 VBOs: vertices, normals, texCoords

	glUseProgram(animationShaderProgramID);
	glGenVertexArrays(animationModelPaths.size(), animation_vaos.data());
	glGenBuffers(animationModelPaths.size() * 3, vbos.data());

	for (int i = 0; i < animationModelPaths.size(); ++i) {
		ModelData& model = animation_meshes[i];

		glBindVertexArray(animation_vaos[i]);

		// Vertices VBO
		glBindBuffer(GL_ARRAY_BUFFER, vbos[i * 3]);
		glBufferData(GL_ARRAY_BUFFER, model.mPointCount * sizeof(glm::vec3), model.mVertices.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, vbos[i * 3 + 1]);
		glBufferData(GL_ARRAY_BUFFER, model.mPointCount * sizeof(glm::vec4), model.mColors.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
		glEnableVertexAttribArray(1);

		// Normals VBO
		glBindBuffer(GL_ARRAY_BUFFER, vbos[i * 3 + 1]);
		glBufferData(GL_ARRAY_BUFFER, model.mPointCount * sizeof(glm::vec3), model.mNormals.data(), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(2);

		//// Texture Coordinates VBO
		//glBindBuffer(GL_ARRAY_BUFFER, vbos[i * 3 + 2]);
		//glBufferData(GL_ARRAY_BUFFER, model.texCoords.size() * sizeof(float), model.texCoords.data(), GL_STATIC_DRAW);
		//glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
		//glEnableVertexAttribArray(2);

		////// Element Buffer Object (EBO)
		//GLuint ebo;
		//glGenBuffers(1, &ebo);
		//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		//glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indices.size() * sizeof(unsigned int), model.indices.data(), GL_STATIC_DRAW);

		glBindVertexArray(0); // 解绑VAO
	}
}
#pragma endregion MESH LOADING

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS
char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	fopen_s(&fp, shaderFile, "rb");

	filesystem::path p(shaderFile);

	if (fp == NULL) {
		cout << "application current path " << filesystem::current_path() << endl;
		cout << "file path " << filesystem::absolute(p) << endl;
		return NULL;
	}

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		std::cerr << "Error creating shader..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024] = { '\0' };
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		std::cerr << "Error compiling "
			<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< " shader program: " << InfoLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}


GLuint CompileShaders()
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID

	auto linkShader = [](GLuint programID)
		{
			GLint Success = 0;
			GLchar ErrorLog[1024] = { '\0' };
			// After compiling all shader objects and attaching them to the program, we can finally link it
			glLinkProgram(programID);
			// check for program related errors using glGetProgramiv
			glGetProgramiv(programID, GL_LINK_STATUS, &Success);
			if (Success == 0) {
				glGetProgramInfoLog(programID, sizeof(ErrorLog), NULL, ErrorLog);
				std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
				std::cerr << "Press enter/return to exit..." << std::endl;
				std::cin.get();
				exit(1);
			}
			// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
			glValidateProgram(programID);
			// check for program related errors using glGetProgramiv
			glGetProgramiv(programID, GL_VALIDATE_STATUS, &Success);
			if (!Success) {
				glGetProgramInfoLog(programID, sizeof(ErrorLog), NULL, ErrorLog);
				std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
				std::cerr << "Press enter/return to exit..." << std::endl;
				std::cin.get();
				exit(1);
			}
			// Finally, use the linked shader program
			// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
			glUseProgram(programID);
		};

#if TERRIAN
	terrianShaderProgramID = glCreateProgram();
	if (terrianShaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	AddShader(terrianShaderProgramID, "simpleVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(terrianShaderProgramID, "simpleFragmentShader.txt", GL_FRAGMENT_SHADER);
	linkShader(terrianShaderProgramID);

	animationShaderProgramID = glCreateProgram();
	if (animationShaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	AddShader(animationShaderProgramID, "ColorVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(animationShaderProgramID, "ColorFragmentShader.txt", GL_FRAGMENT_SHADER);
	linkShader(animationShaderProgramID);
#endif

#if SKELETON
	animationShaderProgramID = glCreateProgram();
	if (animationShaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	AddShader(animationShaderProgramID, "animationVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(animationShaderProgramID, "simpleFragmentShader.txt", GL_FRAGMENT_SHADER);
	linkShader(animationShaderProgramID);
#endif

#if COLORANIMATION
	animationShaderProgramID = glCreateProgram();
	if (animationShaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	AddShader(animationShaderProgramID, "ColorVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(animationShaderProgramID, "ColorFragmentShader.txt", GL_FRAGMENT_SHADER);
	linkShader(animationShaderProgramID);
#endif
	return 1;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand



#pragma region VBO_FUNCTIONS
void generateObjectBufferMesh() {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	//Might be an idea to do a check for that before generating and binding the buffer.
#if TERRIAN
	{
		volcano_terrian_mesh = load_mesh(TERRAIN_MESH, false);
		glUseProgram(terrianShaderProgramID);
		glGenVertexArrays(1, &mesh_vao);
		glBindVertexArray(mesh_vao);

		GLuint loc1 = glGetAttribLocation(terrianShaderProgramID, "vertex_position");
		GLuint loc2 = glGetAttribLocation(terrianShaderProgramID, "vertex_normal");

		//GLuint loc3 = glGetAttribLocation(terrianShaderProgramID, "vertex_texture");
		GLuint vp_vbo = 0, vn_vbo = 0;

		glGenVertexArrays(1, &mesh_vao);
		glBindVertexArray(mesh_vao);

		// Vertex positions
		glGenBuffers(1, &vp_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
		glBufferData(GL_ARRAY_BUFFER, volcano_terrian_mesh.mPointCount * sizeof(glm::vec3), volcano_terrian_mesh.mVertices.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(loc1);
		glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		// Vertex normals
		glGenBuffers(1, &vn_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
		glBufferData(GL_ARRAY_BUFFER, volcano_terrian_mesh.mPointCount * sizeof(glm::vec3), volcano_terrian_mesh.mNormals.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(loc2);
		glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		// Unbind VAO and VBO
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		//	This is for texture coordinates which you don't currently need, so I have commented it out
		//	unsigned int vt_vbo = 0;
		//	glGenBuffers (1, &vt_vbo);
		//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
		//	glBufferData (GL_ARRAY_BUFFER, monkey_head_data.mTextureCoords * sizeof (vec2), &monkey_head_data.mTextureCoords[0], GL_STATIC_DRAW);

		loadAnimationModels();
	}
#endif

#if SKELETON
	{ // replace animationVertexShader.txt with simpleVertexShader.txt
		fish1_mesh = load_mesh(FISH1_MESH, true);
		GLuint loc1 = glGetAttribLocation(animationShaderProgramID, "position");
		GLuint loc2 = glGetAttribLocation(animationShaderProgramID, "normal");
		GLuint loc3 = glGetAttribLocation(animationShaderProgramID, "boneIDs");
		GLuint loc4 = glGetAttribLocation(animationShaderProgramID, "weights");
		if (loc1 == -1 || loc2 == -1 || loc3 == -1 || loc4 == -1)
		{
			std::cerr << "position, normal, boneIDs, or weights attribute not found" << std::endl;
		}

		glGenVertexArrays(1, &mesh_vao);
		glBindVertexArray(mesh_vao);

		unsigned int vp_vbo = 0;
		glGenBuffers(1, &vp_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
		glBufferData(GL_ARRAY_BUFFER, fish1_mesh.mPointCount * sizeof(glm::vec3), fish1_mesh.mVertices.data(), GL_STATIC_DRAW);
		unsigned int vn_vbo = 0;
		glGenBuffers(1, &vn_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
		glBufferData(GL_ARRAY_BUFFER, fish1_mesh.mPointCount * sizeof(glm::vec3), fish1_mesh.mNormals.data(), GL_STATIC_DRAW);
		unsigned int vb_vbo = 0;
		glGenBuffers(1, &vb_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vb_vbo);
		glBufferData(GL_ARRAY_BUFFER, fish1_mesh.mPointEffectedByBone * sizeof(glm::ivec4), fish1_mesh.mBoneIDs.data(), GL_STATIC_DRAW);
		unsigned int vw_vbo = 0;
		glGenBuffers(1, &vw_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vw_vbo);
		glBufferData(GL_ARRAY_BUFFER, fish1_mesh.mPointEffectedByBone * sizeof(glm::vec4), fish1_mesh.mWeights.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(loc1);
		glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
		glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		glEnableVertexAttribArray(loc2);
		glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
		glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		glEnableVertexAttribArray(loc3);
		glBindBuffer(GL_ARRAY_BUFFER, vb_vbo);
		glVertexAttribIPointer(loc3, 4, GL_INT, 0, NULL);

		glEnableVertexAttribArray(loc4);
		glBindBuffer(GL_ARRAY_BUFFER, vw_vbo);
		glVertexAttribPointer(loc4, 4, GL_FLOAT, GL_FALSE, 0, NULL);

	}
#endif

#if COLORANIMATION
	fish1_mesh = load_mesh(FISH1_MESH, false);

	glGenVertexArrays(1, &mesh_vao);
	glBindVertexArray(mesh_vao);

	GLuint loc1 = glGetAttribLocation(animationShaderProgramID, "position");
	GLuint loc2 = glGetAttribLocation(animationShaderProgramID, "maskColor");
	if(loc1 == -1 || loc2 == -1)
	{
		std::cerr << "position or maskColor attribute not found" << std::endl;
	}
	//GLuint loc3 = glGetAttribLocation(terrianShaderProgramID, "vertex_texture");
	GLuint vp_vbo = 0, vc_vbo = 0;

	// Vertex positions
	glGenBuffers(1, &vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, fish1_mesh.mPointCount * sizeof(glm::vec3), fish1_mesh.mVertices.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(loc1);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	glGenBuffers(1, &vc_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vc_vbo);
	glBufferData(GL_ARRAY_BUFFER, fish1_mesh.mPointCount * sizeof(glm::vec4), fish1_mesh.mColors.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(loc2);
	glVertexAttribPointer(loc2, 4, GL_FLOAT, GL_FALSE, 0, NULL); // vec4 for RGBA


	// Unbind VAO and VBO
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif
	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	glEnableVertexAttribArray (loc3);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);

	cout << "finish generate object buffer mesh\n";
}
#pragma endregion VBO_FUNCTIONS


void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.004f, 0.361f, 0.588f, 0.8f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#if TERRIAN

	glUseProgram(terrianShaderProgramID);
	glBindVertexArray(mesh_vao);
	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(terrianShaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(terrianShaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(terrianShaderProgramID, "proj");

	// Root of the Hierarchy
	mat4 persp_proj = perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
	glm::mat4 model(1.0f);


	// Update the view matrix
	glm::vec3 forward(0.0);
	forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward.y = sin(glm::radians(pitch));
	forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward = glm::normalize(forward);
	glm::vec3 cameraTarget = cameraPosition + forward;
	glm::mat4 view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));


	// update uniforms & draw
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(model));
	glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(volcano_terrian_mesh.mPointCount));


	glUseProgram(animationShaderProgramID);
	for (int i = 0; i < animation_vaos.size(); ++i) {
		glBindVertexArray(animation_vaos[i]);
		int matrix_location = glGetUniformLocation(animationShaderProgramID, "model");
		int view_mat_location = glGetUniformLocation(animationShaderProgramID, "view");
		int proj_mat_location = glGetUniformLocation(animationShaderProgramID, "projection");
		GLuint timeLocation = glGetUniformLocation(animationShaderProgramID, "time");

		glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
		glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(model));
		glUniform1f(timeLocation, timeInSeconds);
		glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(animation_meshes[i].mPointCount));
	}

#endif // TERRIAN

#if SKELETON

	glUseProgram(animationShaderProgramID);
	glBindVertexArray(mesh_vao);

	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(animationShaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(animationShaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(animationShaderProgramID, "proj");
	GLuint boneTransformLoc = glGetUniformLocation(animationShaderProgramID, "boneTransforms");
	if (matrix_location == -1 || view_mat_location == -1 || proj_mat_location == -1 || boneTransformLoc == -1)
	{
		std::cerr << "uniform not found" << std::endl;
	}
	// Root of the Hierarchy
	mat4 persp_proj = perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
	glm::mat4 model(1.0f);


	// Update the view matrix
	glm::vec3 forward(0.0);
	forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward.y = sin(glm::radians(pitch));
	forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward = glm::normalize(forward);
	glm::vec3 cameraTarget = cameraPosition + forward;
	glm::mat4 view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));


	// update uniforms & draw
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(model));
	glUniformMatrix4fv(boneTransformLoc, std::min(100,
		static_cast<int>(fish1_mesh.currentBoneTransformation.size())), GL_FALSE,
		glm::value_ptr(fish1_mesh.currentBoneTransformation[0]));
	glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(fish1_mesh.mPointCount));
#endif

#if COLORANIMATION
	glUseProgram(animationShaderProgramID);
	glBindVertexArray(mesh_vao);
	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(animationShaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(animationShaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(animationShaderProgramID, "projection");
	GLuint timeLocation = glGetUniformLocation(animationShaderProgramID, "time");

	// Root of the Hierarchy
	mat4 persp_proj = perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
	glm::mat4 model(1.0f);


	// Update the view matrix
	glm::vec3 forward(0.0);
	forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward.y = sin(glm::radians(pitch));
	forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward = glm::normalize(forward);
	glm::vec3 cameraTarget = cameraPosition + forward;
	glm::mat4 view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

	// update uniforms & draw
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(model));
	glUniform1f(timeLocation, timeInSeconds);
	glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(fish1_mesh.mPointCount));
#endif
	glutSwapBuffers();
}

#include <utility> // for std::pair

std::pair<int, float> getTimeFraction(const std::vector<KeyFrame>& keyframes, float animationTime, const std::string& type) {
	int frameCount = keyframes.size();
	for (int i = 1; i < frameCount; ++i) {
		float start = (type == "position") ? keyframes[i - 1].positionTimestamp :
			(type == "rotation") ? keyframes[i - 1].rotationTimestamp :
			keyframes[i - 1].scaleTimestamp;
		float end = (type == "position") ? keyframes[i].positionTimestamp :
			(type == "rotation") ? keyframes[i].rotationTimestamp :
			keyframes[i].scaleTimestamp;
		if (animationTime < end) {
			float deltaTime = end - start;
			float factor = (animationTime - start) / deltaTime;
			return { i, factor };
		}
	}
	return { 1, 0.0f }; // Return first frame if looping
}
#if SKELETON
void updateBoneTransforms(ModelData& modelData, float timeInSeconds) {

	if (!modelData.HasAnimations()) {
		return;
	}

	float ticksPerSecond = (modelData.mTicksPerSecond != 0) ? modelData.mTicksPerSecond : 25.0f;
	float timeInTicks = timeInSeconds * ticksPerSecond;
	float animationTime = fmod(timeInTicks, modelData.mDuration);

	function<void(BoneNode*, const glm::mat4&)> updateBone = [&](BoneNode* bone, const glm::mat4& parentTransform) {
		if (!bone) return;
		int boneID = bone->boneID;

		const vector<KeyFrame>& keyframes = modelData.mBoneKeyframes[boneID];
		if (keyframes.empty()) return;

		// Interpolate position
		auto [posIndex, posFactor] = getTimeFraction(keyframes, animationTime, "position");
		glm::vec3 interpolatedPosition = glm::mix(
			keyframes[posIndex - 1].position,
			keyframes[posIndex].position,
			posFactor
		);

		// Interpolate rotation
		auto [rotIndex, rotFactor] = getTimeFraction(keyframes, animationTime, "rotation");
		glm::quat interpolatedRotation = glm::slerp(
			keyframes[rotIndex - 1].rotation,
			keyframes[rotIndex].rotation,
			rotFactor
		);

		// Interpolate scale
		auto [scaleIndex, scaleFactor] = getTimeFraction(keyframes, animationTime, "scale");
		glm::vec3 interpolatedScale = glm::mix(
			keyframes[scaleIndex - 1].scale,
			keyframes[scaleIndex].scale,
			scaleFactor
		);

		glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), interpolatedPosition) *
			glm::mat4_cast(interpolatedRotation) *
			glm::scale(glm::mat4(1.0f), interpolatedScale);

		glm::mat4 globalTransform = parentTransform * localTransform;
		glm::mat4 correction = glm::mat4(1.0f);
		correction[1][1] = 0; correction[1][2] = 1;
		correction[2][1] = 1; correction[2][2] = 0;  // Swap Y and Z axes

		//globalTransform = correction * globalTransform;
		//if (bone->name == "Bone01")
		//{

		//	std::cout << "Global Transform (before GlobalInverse): " << glm::to_string(globalTransform) << std::endl;
		//	std::cout << "local Transform: " << glm::to_string(localTransform) << std::endl;
		//}
		modelData.currentBoneTransformation[boneID] = modelData.globalInverseTransform * globalTransform * bone->offsetMatrix;
		//if (bone->name == "Bone01")
		//std::cout << "Final Transform: " << glm::to_string(modelData.currentBoneTransformation[boneID]) << std::endl;
		// Update child bones
		for (BoneNode* child : bone->children) {
			updateBone(child, globalTransform);
		}

		//if (bone->name == "Bone01")
		//{
		//	std::cout << timeInSeconds << "  " << globalTransform[3][0] << " "
		//		<< globalTransform[3][1] << " " << globalTransform[3][2] << std::endl;
		//	std::cout << glm::to_string(bone->offsetMatrix) << std::endl;

		//}
	};

	for (BoneNode* bone : modelData.boneNodes) {
		if (bone && !bone->parent) {
			updateBone(bone, glm::mat4(1.0f));
		}
	}

}
#endif //  SKELETON

bool show = false;
int frame = 1;
void updateScene() {

	//if (!show)
	//{
	//	updateBoneTransforms(fish1_mesh, 0);
	//	show = true;
	//}
	auto currentTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float> elapsedTime = currentTime - startTime;
	timeInSeconds = elapsedTime.count(); // Get elapsed time in seconds



	keyControl::updateCameraPosition();

	//if(frame <= 20)
	{
		//float time = float(frame) / fish1_mesh.mTicksPerSecond ;
#if SKELETON
		updateBoneTransforms(fish1_mesh, timeInSeconds);
#endif
		//frame++;
	}


	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	// Set up the shaders
	GLuint shaderProgramID = CompileShaders();
	// load mesh into a vertex buffer array
	generateObjectBufferMesh();

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