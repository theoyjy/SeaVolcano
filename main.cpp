// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.

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


/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define VOLCANO_MESH "Assets/Volcano.dae"
//#define MUSSELS_MESH "../Assets/Mussels.dae"
/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

#pragma region SimpleTypes
typedef struct ModelData
{
	size_t mPointCount = 0;
	std::vector<vec3> mVertices;
	std::vector<vec3> mNormals;
	std::vector<vec2> mTextureCoords;
	std::vector<float> normalLines;
} ModelData;
#pragma endregion SimpleTypes

using namespace std;
GLuint shaderProgramID/*, normalShaderID*/;


ModelData mesh_data;
unsigned int mesh_vao = 0;
int width = 800;
int height = 600;

//GLuint normalVBO, normalVAO;


GLuint loc1, loc2, loc3;
GLfloat rotate_y = 0.0f;
vec3 translation{ 0.0f, 0.0f, 0.0f };


#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

ModelData load_mesh(const char* file_name) {
	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */
	/*filesystem::path p(file_name);
	cout << "application current path " << filesystem::current_path() << endl;
	cout << "file path " << filesystem::absolute(p) << endl;*/

	const aiScene* scene = aiImportFile(
		file_name,
		aiProcess_Triangulate | aiProcess_PreTransformVertices
	);

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", filesystem::path(file_name).c_str());
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(vec3(vn->x, vn->y, vn->z));
				//// 顶点位置
				//aiVector3D vertex = mesh->mVertices[v_i];
				//// 法线向量
				//aiVector3D normal = mesh->mNormals[v_i];

				//// 线段起点
				//modelData.normalLines.push_back(vertex.x);
				//modelData.normalLines.push_back(vertex.y);
				//modelData.normalLines.push_back(vertex.z);

				//// 线段终点（起点 + 缩放后的法线）
				//modelData.normalLines.push_back(vertex.x + normal.x * 100);
				//modelData.normalLines.push_back(vertex.y + normal.y * 100);
				//modelData.normalLines.push_back(vertex.z + normal.z * 100);
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
	}

	aiReleaseImport(scene);
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
	shaderProgramID = glCreateProgram();
	if (shaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID, "simpleVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(shaderProgramID, "simpleFragmentShader.txt", GL_FRAGMENT_SHADER);


	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID);


	//normalShaderID = glCreateProgram();
	//AddShader(normalShaderID, "../NormalVertexShader.txt", GL_VERTEX_SHADER);
	//AddShader(normalShaderID, "../NormalFragmentShader.txt", GL_FRAGMENT_SHADER);
	//glLinkProgram(normalShaderID);
	//glGetProgramiv(normalShaderID, GL_LINK_STATUS, &Success);
	//if (Success == 0) {
	//	glGetProgramInfoLog(normalShaderID, sizeof(ErrorLog), NULL, ErrorLog);
	//	std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
	//	std::cerr << "Press enter/return to exit..." << std::endl;
	//	std::cin.get();
	//	exit(1);
	//}

	//// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	//glValidateProgram(normalShaderID);
	//// check for program related errors using glGetProgramiv
	//glGetProgramiv(normalShaderID, GL_VALIDATE_STATUS, &Success);
	//if (!Success) {
	//	glGetProgramInfoLog(normalShaderID, sizeof(ErrorLog), NULL, ErrorLog);
	//	std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
	//	std::cerr << "Press enter/return to exit..." << std::endl;
	//	std::cin.get();
	//	exit(1);
	//}
	//// Finally, use the linked shader program
	//// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	//glUseProgram(normalShaderID);

	return shaderProgramID;
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

	mesh_data = load_mesh(VOLCANO_MESH);
	unsigned int vp_vbo = 0;
	loc1 = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2 = glGetAttribLocation(shaderProgramID, "vertex_normal");
	loc3 = glGetAttribLocation(shaderProgramID, "vertex_texture");

	glGenBuffers(1, &vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mVertices[0], GL_STATIC_DRAW);
	unsigned int vn_vbo = 0;
	glGenBuffers(1, &vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mNormals[0], GL_STATIC_DRAW);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	unsigned int vt_vbo = 0;
	//	glGenBuffers (1, &vt_vbo);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glBufferData (GL_ARRAY_BUFFER, monkey_head_data.mTextureCoords * sizeof (vec2), &monkey_head_data.mTextureCoords[0], GL_STATIC_DRAW);

	unsigned int vao = 0;
	glBindVertexArray(vao);

	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	glEnableVertexAttribArray (loc3);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	
	//// 解绑 VAO
	//glBindVertexArray(0);

	//// 生成并绑定法线 VAO 和 VBO
	//glGenVertexArrays(1, &normalVAO);
	//glGenBuffers(1, &normalVBO);

	//glBindVertexArray(normalVAO);
	//glBindBuffer(GL_ARRAY_BUFFER, normalVBO);
	//glBufferData(GL_ARRAY_BUFFER, mesh_data.normalLines.size() * sizeof(float), mesh_data.normalLines.data(), GL_STATIC_DRAW);
	//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	//glEnableVertexAttribArray(0);

	//// 解绑 VAO
	//glBindVertexArray(0);
}
#pragma endregion VBO_FUNCTIONS


void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_BACK);
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.0f, 0.0f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	glUseProgram(shaderProgramID);

	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(shaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(shaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(shaderProgramID, "proj");


	// Root of the Hierarchy
	mat4 view = identity_mat4();
	mat4 persp_proj = perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
	mat4 model = identity_mat4();
	//model = rotate_z_deg(model, rotate_y);
	view = translate(view, vec3(0.0, -2.0, -25.0));
	view = translate(view, translation);

	// update uniforms & draw
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model.m);
	glDrawArrays(GL_TRIANGLES, 0, mesh_data.mPointCount);

	//// Set up the child matrix
	//mat4 modelChild = identity_mat4();
	////modelChild = rotate_y_deg(modelChild, rotate_y);
	//modelChild = translate(modelChild, vec3(0.0f, 1.9f, 0.0f));

	//// Apply the root matrix to the child matrix
	//modelChild = model * modelChild;

	//// Update the appropriate uniform and draw the mesh again
	//glUniformMatrix4fv(matrix_location, 1, GL_FALSE, modelChild.m);
	//glDrawArrays(GL_TRIANGLES, 0, mesh_data.mPointCount);

	//// 使用法线的着色器
	//glUseProgram(normalShaderID);
	//// 禁用深度测试，以防止法线被遮挡
	//glDisable(GL_DEPTH_TEST);
	//glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	//glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	//glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model.m);
	//glBindVertexArray(normalVAO); // 绑定法线的 VAO
	//glDrawArrays(GL_LINES, 0, mesh_data.normalLines.size() / 3);
	//glBindVertexArray(0);
	//// 重新启用深度测试
	//glEnable(GL_DEPTH_TEST);

	glutSwapBuffers();
}


void updateScene() {

	static DWORD last_time = 0;
	DWORD curr_time = timeGetTime();
	if (last_time == 0)
		last_time = curr_time;
	float delta = (curr_time - last_time) * 0.001f;
	last_time = curr_time;

	// Rotate the model slowly around the y axis at 20 degrees per second
	//rotate_y += 20.0f * delta;
	//rotate_y = fmodf(rotate_y, 360.0f);

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

float speed = 0.5f;
// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {
	if (key == 'a') {
		//Translate the base, etc.
		translation.v[0] += speed;
	}
	else if (key == 'd')
	{
		translation.v[0] -= speed;
	}
	else if (key == 'w')
	{
		translation.v[2] += speed;
	}
	else if (key == 's')
	{
		translation.v[2] -= speed;
	}
	else if (key == 'q')
	{
		translation.v[1] -= speed;
	}
	else if (key == 'e')
	{
		translation.v[1] += speed;
	}
	else if (key == 'r')
	{
		translation = vec3(0.0f, 0.0f, 0.0f);
	}
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
	glutKeyboardFunc(keypress);

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
