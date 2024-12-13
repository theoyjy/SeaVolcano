#pragma once
#define FISHCOUNT 30
#define RUNAWAYTIME 3.f

enum Type { STATIC, CRAB, FISH, LAVA };

struct FishInstance {
	float angle = 0.f;        // Current angle along the circular path
	float speed = 0.f;        // Speed of this fish
	float radius = 0.f;       // Radius of the circular path
	float y = 0.f;            // Vertical offset for the fish's circle
};

using namespace std;
struct ModelData
{
	size_t mPointCount = 0;
	GLuint mVao = 0;
	GLuint mVBOs[3] = {0,0,0};
	GLuint mEBO = 0;
	GLuint instanceVBO;
	vector<glm::vec3> mVertices;
	vector<glm::vec3> mNormals;
	vector<glm::vec4> mColors;
	vector<glm::vec2> mTextureCoords;
	vector<unsigned int> mIndices;
	glm::mat4 mLocalTransform;

	string mTexturePath;
	GLuint mTextureId;

	vector<ModelData> mChildMeshes;
};

struct Crab
{
	ModelData model;           // The crab's model data
	float radius = 3.f;              // Used for collision detection
	glm::vec3 translation;     // Current translation from the origin(0,0,0)
	glm::vec3 velocity;        // Current speed and direction
	glm::vec3 rotation;        // Current rotation (X, Y, Z)
	bool runAway = false;
	float runAwayTime = 0.f;
	Crab() : translation(0.f), rotation(0.0), velocity(5.0f, 0.f, 5.0f){}

	glm::vec3 CurrentPosition() const
	{
		// Transform the origin (0, 0, 0) using the model matrix
		glm::vec4 worldPosition = GetModelTransform() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		return glm::vec3(worldPosition);
	}

	glm::mat4 GetModelTransform() const
	{
		// Apply rotation (around the local axes, X, Y, Z)
		glm::mat4 rotationMtx = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Rotate around X-axis
		rotationMtx = glm::rotate(rotationMtx, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate around Y-axis
		rotationMtx = glm::rotate(rotationMtx, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate around Z-axis

		// Apply translation
		glm::mat4 translationMtx = glm::translate(glm::mat4(1.0f), translation);

		return translationMtx * rotationMtx;
	}

	void StartRun()
	{
		runAway = true;
		runAwayTime = RUNAWAYTIME;
		//rotationAngle = (((rand()) % 100) * glm::pi<float>()) - 0.5 * glm::pi<float>();
	}
};

vector<ModelData> staticModels;
vector<Crab> CrabModels;
ModelData fishModel;

// fish hierarchical mesh rotation for each fish part
GLfloat rotate_head = 0.0f, rotate_fin = 0.0f, rotate_body = 0.f;

// whole translation for each fish model
vector<glm::mat4> fish_transforms;
vector<glm::vec3> fish_centers;
vector<FishInstance> fishInstances;
vector<glm::mat4> fishInstanceTransforms;
ModelData lavaModel;
glm::vec3 lavaPosition = glm::vec3(10.0f, 3.0f, 0.0f);
float lavaWidth = 150.0f; 
float lavaDepth = 150.0f;
