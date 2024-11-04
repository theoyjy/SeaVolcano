// Windows includes (For Time, IO, etc.)
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
#define VOLCANO_MESH "Assets/volcano.fbx"
#define FISH1_MESH "Assets/sardine.fbx"


/*-----------------------------------------------------------------------s-----
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
	int boneID = 0;                   // 骨骼ID（在mBoneNames中的索引）
	glm::mat4 currentTransform;   // 当前骨骼变换矩阵
	glm::mat4 offsetMatrix;
	string name;
	BoneNode* parent;             // 指向父骨骼的指针
	std::vector<BoneNode*> children; // 子骨骼列表
};

// vertex of an animated model
struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec4 boneIds = glm::vec4(0);
	glm::vec4 boneWeights = glm::vec4(0.0f);
};

typedef struct ModelData
{
	size_t mPointCount = 0;
	size_t mBoneCount = 0;
	size_t mAnimationCount = 0;
	float mTicksPerSecond = 0.0f;
	float mDuration = 0.0f;

	vector<Vertex> mVertices;
	vector<string> mBoneNames;

	vector<vector<KeyFrame>> mBoneKeyframes;      // 每个骨骼的关键帧
	vector<glm::mat4> currentBoneTransformation;    // 每个骨骼的动画矩阵
	std::vector<BoneNode*> boneNodes;          // hierarchy of bones

	glm::mat4 globalInverseTransform; // 全局反向变换矩阵

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

} ModelData;
#pragma endregion SimpleTypes

#define TERRIAN 1
#define FISH1 0

auto startTime = std::chrono::high_resolution_clock::now();

using namespace std;
GLuint terrianShaderProgramID, animationShaderProgramID, boneShaderProgramID;


ModelData volcano_terrian_mesh;
ModelData fish1_mesh;
unsigned int mesh_vao = 0;
int width = 800;
int height = 600;


GLfloat rotate_y = 0.0f;

#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

void processNodeHierarchy(const aiNode* node, BoneNode* parentBone, ModelData& modelData) {
	// Check if this node is a bone by looking for its name in the model's bone list
	std::string nodeName(node->mName.C_Str());
	int boneID = modelData.GetBoneIDByName(nodeName);

	BoneNode* currentBone = nullptr;
	if (boneID != modelData.mBoneNames.size()) {
		// This is a bone, create a BoneNode for it
		currentBone = new BoneNode();
		currentBone->boneID = boneID;
		currentBone->parent = parentBone;
		currentBone->name = nodeName;
		currentBone->offsetMatrix = glm::transpose(glm::make_mat4(&node->mTransformation.a1));
		// Add this bone to the modelData bone hierarchy list
		modelData.boneNodes[boneID] = currentBone;

		// If there's a parent, add this bone to the parent's children list
		if (parentBone != nullptr) {
			parentBone->children.push_back(currentBone);
		}
	}

	// Process child nodes recursively
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNodeHierarchy(node->mChildren[i], currentBone, modelData);
	}
}

void buildBoneHierarchy(const aiScene* scene, ModelData& modelData) {
	const aiNode* rootNode = scene->mRootNode;
	modelData.boneNodes.resize(modelData.mBoneCount);
	processNodeHierarchy(rootNode, nullptr, modelData);
}


ModelData load_mesh(const char* file_name, bool with_anime) {
	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */

	unsigned int pFlags = with_anime ? (aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights)
		: (aiProcess_PreTransformVertices | aiProcess_GlobalScale);


	const aiScene* scene = aiImportFile(file_name, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights);

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", filesystem::path(file_name).c_str());
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);
	printf("  %i animation\n", scene->mAnimations);


	modelData.globalInverseTransform = glm::inverse(glm::transpose(
		glm::make_mat4(&scene->mRootNode->mTransformation.a1)));;

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		printf("    %i bones in mesh\n", mesh->mNumBones);

		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			Vertex vertex;
			vertex.position = glm::vec3(mesh->mVertices[v_i].x, mesh->mVertices[v_i].y, mesh->mVertices[v_i].z);
			vertex.normal = glm::vec3(mesh->mNormals[v_i].x, mesh->mNormals[v_i].y, mesh->mNormals[v_i].z);
			//vertex.uv = glm::vec2(mesh->mTextureCoords[0][v_i].x, mesh->mTextureCoords[0][v_i].y);
			vertex.boneIds = glm::ivec4(0);
			vertex.boneWeights = glm::vec4(0.0f);
			modelData.mVertices.push_back(vertex);
		}

		if (mesh->HasBones())
		{
			modelData.mBoneCount += mesh->mNumBones;

			// bones
			for (unsigned int m_b = 0; m_b < mesh->mNumBones; m_b++)
			{
				aiBone* bone = mesh->mBones[m_b];
				int boneID = modelData.GetBoneIDByName(bone->mName.C_Str());
				if (boneID == modelData.mBoneNames.size())
				{
					modelData.mBoneNames.push_back(bone->mName.C_Str());
				}

				for (unsigned int k = 0; k < bone->mNumWeights; k++)
				{
					aiVertexWeight& weight = bone->mWeights[k];
					int vertexID = weight.mVertexId;
					float boneWeight = weight.mWeight;

					for (int i = 0; i < 4; i++)
					{
						if (modelData.mVertices[vertexID].boneWeights[i] == 0.0f)
						{
							modelData.mVertices[vertexID].boneIds[i] = boneID;
							modelData.mVertices[vertexID].boneWeights[i] = boneWeight;
							break;
						}
					}
				}

			}

			//normalize weights to make all weights sum 1
			for (int i = 0; i < modelData.mPointCount; i++) {
				auto& boneWeights = modelData.mVertices[i].boneWeights;
				float totalWeight = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;
				if (totalWeight > 0.0f) {
					boneWeights = glm::vec4(
						boneWeights.x / totalWeight,
						boneWeights.y / totalWeight,
						boneWeights.z / totalWeight,
						boneWeights.w / totalWeight
					);
				}
			}

			buildBoneHierarchy(scene, modelData);
		}
	}

	if (scene->HasAnimations()) {
		aiAnimation* animation = scene->mAnimations[0];

		// initialize bone keyframes and animations
		modelData.mBoneKeyframes.resize(animation->mNumChannels);
		modelData.currentBoneTransformation.resize(animation->mNumChannels);

		// record animation data
		modelData.mAnimationCount = animation->mNumChannels;
		modelData.mTicksPerSecond = animation->mTicksPerSecond;
		modelData.mDuration = animation->mDuration; // in ticks

		cout << "total time: " << modelData.mDuration / modelData.mTicksPerSecond << "s\n";

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

	aiReleaseImport(scene);
	cout << "finish load mesh\n";
	return modelData;
}
#pragma endregion MESH LOADING

///////////////////////////////////////////////////////////////////////////////



// structure to hold bone tree (skeleton)
struct Bone {
	int id = 0; // position of the bone in final upload array
	std::string name = "";
	glm::mat4 offset = glm::mat4(1.0f);
	std::vector<Bone> children = {};
};

// sturction representing an animation track
struct BoneTransformTrack {
	std::vector<float> positionTimestamps = {};
	std::vector<float> rotationTimestamps = {};
	std::vector<float> scaleTimestamps = {};

	std::vector<glm::vec3> positions = {};
	std::vector<glm::quat> rotations = {};
	std::vector<glm::vec3> scales = {};
};

// structure containing animation information
struct Animation {
	float duration = 0.0f;
	float ticksPerSecond = 1.0f;
	std::unordered_map<std::string, BoneTransformTrack> boneTransforms = {};
};


inline glm::mat4 assimpToGlmMatrix(aiMatrix4x4 mat) {
	glm::mat4 m;
	for (int y = 0; y < 4; y++)
	{
		for (int x = 0; x < 4; x++)
		{
			m[x][y] = mat[y][x];
		}
	}
	return m;
}
inline glm::vec3 assimpToGlmVec3(aiVector3D vec) {
	return glm::vec3(vec.x, vec.y, vec.z);
}

inline glm::quat assimpToGlmQuat(aiQuaternion quat) {
	glm::quat q;
	q.x = quat.x;
	q.y = quat.y;
	q.z = quat.z;
	q.w = quat.w;

	return q;
}


// a recursive function to read all bones and form skeleton
bool readSkeleton(Bone& boneOutput, aiNode* node, std::unordered_map<std::string, std::pair<int, glm::mat4>>& boneInfoTable) {

	if (boneInfoTable.find(node->mName.C_Str()) != boneInfoTable.end()) { // if node is actually a bone
		boneOutput.name = node->mName.C_Str();
		boneOutput.id = boneInfoTable[boneOutput.name].first;
		boneOutput.offset = boneInfoTable[boneOutput.name].second;

		for (int i = 0; i < node->mNumChildren; i++) {
			Bone child;
			readSkeleton(child, node->mChildren[i], boneInfoTable);
			boneOutput.children.push_back(child);
		}
		return true;
	}
	else { // find bones in children
		for (int i = 0; i < node->mNumChildren; i++) {
			if (readSkeleton(boneOutput, node->mChildren[i], boneInfoTable)) {
				return true;
			}

		}
	}
	return false;
}

void loadModel(const aiScene* scene, aiMesh* mesh, std::vector<Vertex>& verticesOutput, std::vector<size_t>& indicesOutput,
	Bone& skeletonOutput, size_t& nBoneCount) {

	verticesOutput = {};
	indicesOutput = {};
	//load position, normal, uv
	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		//process position 
		Vertex vertex;
		glm::vec3 vector;
		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vertex.position = vector;
		//process normal
		vector.x = mesh->mNormals[i].x;
		vector.y = mesh->mNormals[i].y;
		vector.z = mesh->mNormals[i].z;
		vertex.normal = vector;

		vertex.boneIds = glm::ivec4(0);
		vertex.boneWeights = glm::vec4(0.0f);

		verticesOutput.push_back(vertex);
	}

	//load boneData to vertices
	std::unordered_map<std::string, std::pair<int, glm::mat4>> boneInfo = {};
	std::vector<size_t> boneCounts;
	boneCounts.resize(verticesOutput.size(), 0);
	nBoneCount = mesh->mNumBones;

	//loop through each bone
	for (size_t i = 0; i < nBoneCount; i++) {
		aiBone* bone = mesh->mBones[i];
		glm::mat4 m = assimpToGlmMatrix(bone->mOffsetMatrix);
		boneInfo[bone->mName.C_Str()] = { i, m };

		//loop through each vertex that have that bone
		for (int j = 0; j < bone->mNumWeights; j++) {
			size_t id = bone->mWeights[j].mVertexId;
			float weight = bone->mWeights[j].mWeight;
			boneCounts[id]++;
			switch (boneCounts[id]) {
			case 1:
				verticesOutput[id].boneIds.x = i;
				verticesOutput[id].boneWeights.x = weight;
				break;
			case 2:
				verticesOutput[id].boneIds.y = i;
				verticesOutput[id].boneWeights.y = weight;
				break;
			case 3:
				verticesOutput[id].boneIds.z = i;
				verticesOutput[id].boneWeights.z = weight;
				break;
			case 4:
				verticesOutput[id].boneIds.w = i;
				verticesOutput[id].boneWeights.w = weight;
				break;
			default:
				//std::cout << "err: unable to allocate bone to vertex" << std::endl;
				break;

			}
		}
	}



	//normalize weights to make all weights sum 1
	for (int i = 0; i < verticesOutput.size(); i++) {
		glm::vec4& boneWeights = verticesOutput[i].boneWeights;
		float totalWeight = boneWeights.x + boneWeights.y + boneWeights.z + boneWeights.w;
		if (totalWeight > 0.0f) {
			verticesOutput[i].boneWeights = glm::vec4(
				boneWeights.x / totalWeight,
				boneWeights.y / totalWeight,
				boneWeights.z / totalWeight,
				boneWeights.w / totalWeight
			);
		}
	}


	//load indices
	for (int i = 0; i < mesh->mNumFaces; i++) {
		aiFace& face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indicesOutput.push_back(face.mIndices[j]);
	}

	// create bone hirerchy
	readSkeleton(skeletonOutput, scene->mRootNode, boneInfo);
}

void loadAnimation(const aiScene* scene, Animation& animation) {
	//loading  first Animation
	aiAnimation* anim = scene->mAnimations[0];

	if (anim->mTicksPerSecond != 0.0f)
		animation.ticksPerSecond = anim->mTicksPerSecond;
	else
		animation.ticksPerSecond = 1;


	animation.duration = anim->mDuration * anim->mTicksPerSecond;
	animation.boneTransforms = {};

	//load positions rotations and scales for each bone
	// each channel represents each bone
	for (int i = 0; i < anim->mNumChannels; i++) {
		aiNodeAnim* channel = anim->mChannels[i];
		BoneTransformTrack track;
		for (int j = 0; j < channel->mNumPositionKeys; j++) {
			track.positionTimestamps.push_back(channel->mPositionKeys[j].mTime);
			track.positions.push_back(assimpToGlmVec3(channel->mPositionKeys[j].mValue));
		}
		for (int j = 0; j < channel->mNumRotationKeys; j++) {
			track.rotationTimestamps.push_back(channel->mRotationKeys[j].mTime);
			track.rotations.push_back(assimpToGlmQuat(channel->mRotationKeys[j].mValue));

		}
		for (int j = 0; j < channel->mNumScalingKeys; j++) {
			track.scaleTimestamps.push_back(channel->mScalingKeys[j].mTime);
			track.scales.push_back(assimpToGlmVec3(channel->mScalingKeys[j].mValue));

		}
		animation.boneTransforms[channel->mNodeName.C_Str()] = track;
	}
}


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

#endif

#if FISH1
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

	boneShaderProgramID = glCreateProgram();
	if (boneShaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	AddShader(boneShaderProgramID, "boneVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(boneShaderProgramID, "boneFragmentShader.txt", GL_FRAGMENT_SHADER);
	linkShader(boneShaderProgramID);

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
		volcano_terrian_mesh = load_mesh(VOLCANO_MESH, false);

		GLuint vbo;

		glGenVertexArrays(1, &mesh_vao);
		glGenBuffers(1, &vbo);

		glBindVertexArray(mesh_vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * volcano_terrian_mesh.mPointCount, &volcano_terrian_mesh.mVertices[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, position));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, normal));
	}
#endif

#if FISH1
	{ // replace animationVertexShader.txt with simpleVertexShader.txt
		fish1_mesh = load_mesh(FISH1_MESH, true);
		GLuint vbo;

		glGenVertexArrays(1, &mesh_vao);
		glGenBuffers(1, &vbo);

		glBindVertexArray(mesh_vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * fish1_mesh.mPointCount, &fish1_mesh.mVertices[0], GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, position));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, normal));
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, boneIds));
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, boneWeights));



		const aiScene* scene = aiImportFile(FISH1_MESH, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals);
		aiMesh* mesh = scene->mMeshes[0];

		std::vector<Vertex> vertices = {};
		std::vector<size_t> indices = {};
		size_t boneCount = 0;
		Animation animation;
		size_t vao = 0;
		Bone skeleton;
		size_t diffuseTexture;

		//as the name suggests just inverse the global transform
		glm::mat4 globalInverseTransform = assimpToGlmMatrix(scene->mRootNode->mTransformation);
		globalInverseTransform = glm::inverse(globalInverseTransform);


		loadModel(scene, mesh, vertices, indices, skeleton, boneCount);
		loadAnimation(scene, animation);

	}
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

#endif // TERRIAN

#if FISH1
	glUseProgram(animationShaderProgramID);
	glBindVertexArray(mesh_vao);

	int matrix_location = glGetUniformLocation(animationShaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(animationShaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(animationShaderProgramID, "proj");
	GLuint boneTransformLoc = glGetUniformLocation(animationShaderProgramID, "boneTransforms");

	mat4 persp_proj = perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
	glm::mat4 model(1.0f);
	glm::vec3 forward(0.0);

	forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward.y = sin(glm::radians(pitch));
	forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward = glm::normalize(forward);
	glm::vec3 cameraTarget = cameraPosition + forward;
	glm::mat4 view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));

	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(model));
	glUniformMatrix4fv(boneTransformLoc, std::min(100, static_cast<int>(fish1_mesh.currentBoneTransformation.size())),
		GL_FALSE, glm::value_ptr(fish1_mesh.currentBoneTransformation[0]));
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
			if (deltaTime <= 0.0f) deltaTime = 1.0f;  // Avoid division by zero
			float factor = (animationTime - start) / deltaTime;
			return { i, factor };
		}
	}
	return { 1, 0.0f }; // Return first frame if looping
}

void updateBoneTransforms(ModelData& modelData, float timeInSeconds) {
	if (!modelData.HasAnimations()) {
		return;
	}

	float ticksPerSecond = (modelData.mTicksPerSecond != 0) ? modelData.mTicksPerSecond : 25.0f;
	float timeInTicks = timeInSeconds * ticksPerSecond;
	float animationTime = fmod(timeInTicks, modelData.mDuration);

	//cout << "Frame: " << animationTime << "\n";

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
		globalTransform = modelData.globalInverseTransform * globalTransform * bone->offsetMatrix;
		bone->currentTransform = globalTransform;
		modelData.currentBoneTransformation[boneID] = globalTransform;

		// Update child bones
		for (BoneNode* child : bone->children) {
			updateBone(child, globalTransform);
		}
		if (modelData.mBoneNames[boneID] == "X")
		{
			std::cout << "Bone : " << modelData.mBoneNames[boneID] << "\n";
			std::cout << "  Global Position: " << glm::to_string(glm::vec3(globalTransform[3])) << "\n";  // Extract translation part
			glm::quat globalRotation = glm::quat_cast(globalTransform);
			std::cout << "  Global Rotation (Quaternion): " << glm::to_string(globalRotation) << "\n";
			std::cout << "  Scale: " << glm::to_string(interpolatedScale) << "\n";  // Scale directly
			std::cout << "\n";
		}
		};

	for (BoneNode* bone : modelData.boneNodes) {
		if (bone && !bone->parent) {
			updateBone(bone, glm::mat4(1.0f));
		}
	}
}

void setMat4(GLuint shaderProgram, const std::string& name, const glm::mat4& mat) {
	GLuint location = glGetUniformLocation(shaderProgram, name.c_str());
	glUniformMatrix4fv(location, 1, GL_FALSE, &mat[0][0]);
}


// Function to draw bones recursively in your current scene
void drawBoneRecursive(BoneNode* bone, glm::mat4 parentTransform, GLuint shaderProgram) {
	// Calculate the current bone's global transformation
	glm::mat4 currentTransform = parentTransform * bone->currentTransform;

	// Extract the position of the current bone
	glm::vec3 currentPosition = glm::vec3(currentTransform[3]);

	glm::mat4 persp_proj = glm::perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
	glm::mat4 model(1.0f);
	glm::vec3 forward(0.0);

	forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward.y = sin(glm::radians(pitch));
	forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward = glm::normalize(forward);
	glm::vec3 cameraTarget = cameraPosition + forward;
	glm::mat4 view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));


	// Use the shader to draw the bones
	glUseProgram(shaderProgram);
	setMat4(shaderProgram, "view", view);
	setMat4(shaderProgram, "projection", persp_proj);

	// Set line width for better visualization underwater
	glLineWidth(3.0f);

	// Draw the bones recursively
	for (BoneNode* child : bone->children) {
		// Calculate child transformation
		glm::mat4 childTransform = currentTransform * child->currentTransform;
		glm::vec3 childPosition = glm::vec3(childTransform[3]);

		// Vertex data for the line representing the bone
		float vertices[] = {
			currentPosition.x, currentPosition.y, currentPosition.z,
			childPosition.x, childPosition.y, childPosition.z
		};

		// Set up VAO/VBO for line drawing
		GLuint VBO, VAO;
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		// Draw line between current bone and child
		glBindVertexArray(VAO);
		glDrawArrays(GL_LINES, 0, 2);

		// Clean up VAO and VBO
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);

		// Recursively draw the children bones
		drawBoneRecursive(child, currentTransform, shaderProgram);
	}
}

float t = 0;
void updateScene() {

	auto currentTime = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float> elapsedTime = currentTime - startTime;
	float timeInSeconds = elapsedTime.count(); // Get elapsed time in seconds

	keyControl::updateCameraPosition();

	//if(t < fish1_mesh.mDuration / fish1_mesh.mTicksPerSecond)
	{
		//t += 1.0f / fish1_mesh.mTicksPerSecond;
		updateBoneTransforms(fish1_mesh, timeInSeconds);
	}

	// Draw the bones
	for (auto& bone : fish1_mesh.boneNodes)
	{
		if (bone && !bone->parent)
		{
			drawBoneRecursive(bone, glm::mat4(1.0f), boneShaderProgramID);
		}
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
