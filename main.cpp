// Windows includes(For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.
#include <unordered_map>
#include <cstdlib>
#include <ctime>

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
#define SMOKE_MESH "Assets/Smoke.fbx"
#define ANIMATION_FOLDER "Assets/animationModels/"

#define TERRIAN 1
#define SKELETON 0
#define COLORANIMATION 0

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
	vector<vec2> mTextureCoords;
	glm::mat4 mLocalTransform;
	vector<ModelData> mChildMeshes;

} ModelData;
#pragma endregion SimpleTypes



auto startTime = std::chrono::high_resolution_clock::now();

GLuint terrianShaderProgramID;

// all the meshes in the scene
ModelData volcano_terrian_mesh;
ModelData smoke_mesh;
vector<ModelData> animation_meshes;

int width = 1440;
int height = 720;

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
	printf("  %i animation\n", scene->mAnimations);

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
				modelPtr->mTextureCoords.push_back(vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
				/* You can extract tangents and bitangents here              */
				/* Note that you might need to make Assimp generate this     */
				/* data for you. Take a look at the flags that aiImportFile  */
				/* can take.                                                 */
			}
		}

		modelPtr->mPointCount += mesh->mNumVertices;
	}

	aiReleaseImport(scene);
	cout << "finish load mesh\n";
	return modelData;
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
	return 1;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand



#pragma region VBO_FUNCTIONS

vector<string> GetAllAnimationModelPath()
{
	vector<string> animationModelPaths;
	// Iterate through the animation model folder and store the paths in a vector
	try {
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
	return animationModelPaths;
}

void generateObjectBufferMesh() {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	//Might be an idea to do a check for that before generating and binding the buffer.
	

	// load terrain mesh
	volcano_terrian_mesh = load_mesh(TERRAIN_MESH, false);

	// load smoke mesh
	smoke_mesh = load_mesh(SMOKE_MESH, false);

	// get all the animation model paths
	vector<string> animationModelPaths = GetAllAnimationModelPath();

	// Load each animation model
	for (size_t i = 0; i < animationModelPaths.size(); i++)
	{
		animation_meshes.push_back(load_mesh(animationModelPaths[i].c_str(), true));
	}

	// Set up the VAO and VBOs for terrain and all animation models
	GLuint loc1 = glGetAttribLocation(terrianShaderProgramID, "vertex_position");
	GLuint loc2 = glGetAttribLocation(terrianShaderProgramID, "vertex_normal");
	if (loc1 == -1 || loc2 == -1)
	{
		std::cerr << "Error getting attribute location" << std::endl;
		exit(1);
	}

	function<void(ModelData&)> SetUpModelBuffers = [&](ModelData& model) {
		glGenVertexArrays(1, &model.mVao);
		glBindVertexArray(model.mVao);
		GLuint vbos[2] = { 0, 0 };
		glGenBuffers(2, vbos);

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

		glBindVertexArray(0); // unbind VAO
		glBindBuffer(GL_ARRAY_BUFFER, 0); // unbind VBO

		for (int j = 0; j < model.mChildMeshes.size(); ++j)
		{
			SetUpModelBuffers(model.mChildMeshes[j]);
		}
	};

	glUseProgram(terrianShaderProgramID);
	SetUpModelBuffers(volcano_terrian_mesh);
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


void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.004f, 0.361f, 0.588f, 0.8f); // background color to blue
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(terrianShaderProgramID);
	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(terrianShaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(terrianShaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(terrianShaderProgramID, "proj");
	int is_smoke_location = glGetUniformLocation(terrianShaderProgramID, "isSmoke");

	// projection matrix
	mat4 persp_proj = perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
	
	// Update the view matrix by using the camera's position and orientation
	glm::vec3 forward(0.0);
	forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward.y = sin(glm::radians(pitch));
	forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward = glm::normalize(forward);
	glm::vec3 cameraTarget = cameraPosition + forward;
	glm::mat4 view = glm::lookAt(cameraPosition, cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));



	// update terrain uniforms & draw
	function<void(const ModelData&, glm::mat4&, bool)> UpdateNormalMeshUniforms = [&](const ModelData& mesh, glm::mat4& modelMatrix, bool isSmoke) {
		glBindVertexArray(mesh.mVao);
		glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
		glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(modelMatrix));
		glUniform1i(is_smoke_location, isSmoke);
		glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(mesh.mPointCount));
	};

	glm::mat4 model(1.0f);
	// Draw terrain
	UpdateNormalMeshUniforms(volcano_terrian_mesh, model, false);

	// Draw smoke
	glUniform3f(glGetUniformLocation(terrianShaderProgramID, "smokeColor"), 0.5f, 0.5f, 0.6f);
	glUniform1f(glGetUniformLocation(terrianShaderProgramID, "density"), 0.01f);
	glUniform3f(glGetUniformLocation(terrianShaderProgramID, "viewPos"), cameraPosition.x, cameraPosition.y, cameraPosition.z);
	UpdateNormalMeshUniforms(smoke_mesh, model, true);

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

		for(int j = 0; j < animation_meshes[i].mChildMeshes.size(); ++j)
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


	renderBitmapText(-1.0, 0.9, GLUT_BITMAP_HELVETICA_18, "The scene consists of a shining volcano, swiming fishes, smoke out of volcano, rocks, coral");
	renderBitmapText(-1.0, 0.85, GLUT_BITMAP_HELVETICA_18, "All the fishes are moving towards its heading direction");
	renderBitmapText(-1.0, 0.8, GLUT_BITMAP_HELVETICA_18, "Each Fish contains hierarchical meshes that fin and tail would move relatively to its body");
	renderBitmapText(-1.0, 0.75, GLUT_BITMAP_HELVETICA_18, "Key Control : W(forward) S(backward) A(left) D(right) Q(upward) E(downward). Shift + Key Control : Speed up");
	renderBitmapText(-1.0, 0.7, GLUT_BITMAP_HELVETICA_18, "Mouse Control : Left click and drag to control camera's yaw and pitch");

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

void updateScene() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsedTime = currentTime - startTime;
    float timeInSeconds = elapsedTime.count(); // Get elapsed time in seconds

	// Fin and head of a fish rotate in a sinusoidal pattern
    rotate_fin = 0.4f * sin(timeInSeconds);
	rotate_body = 0.4f * sin(timeInSeconds + PI / 4.0f);
    rotate_head = 0.4f * sin(timeInSeconds + PI / 2.0f);

	for(int i = 0; i < fish_translations.size(); ++i)
	{
		fish_translations[i] += fish_translate_directions[i] * 0.01f;
	}

    keyControl::updateCameraPosition();

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