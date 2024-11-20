**Demonstration video:** https://www.youtube.com/watch?v=53ZmWnFnnbc

### Hierarchical Animation
1. Data structure and global variables:
```cpp
typedef struct ModelData
{
	size_t mPointCount = 0;
	GLuint mVao = 0;
	vector<glm::vec3> mVertices;
	vector<glm::vec3> mNormals;
	vector<glm::vec4> mColors;
	vector<vec2> mTextureCoords;
	glm::mat4 mLocalTransform;
	// internal array to store hierarchical meshes for animation
	vector<ModelData> mChildMeshes;

} ModelData;

// global variables:

// time to update rotate degree
auto startTime = std::chrono::high_resolution_clock::now();

// only program
GLuint terrianShaderProgramID;

// all the meshes in the scene
ModelData volcano_terrian_mesh;
ModelData smoke_mesh;
// hierarchical meshes for animation
vector<ModelData> animation_meshes;

int width = 1440;
int height = 720;

// fish hierarchical mesh rotation for each fish part
GLfloat rotate_head = 0.0f, rotate_fin = 0.0f, rotate_body = 0.f;

// whole translation for each fish model
vector<glm::vec3> fish_translations(9, glm::vec3(0.0f));
```

2. `load_mesh()` method would load up one mesh, and its own children meshes, as well as build hierarchical relationships. Because until now, the only hierarchical animation contained is the fish which is quite simple, it only has two layer: one for parent mesh (fish body), and one layer for all the other meshes(fin, head). That is why the code only map the hierarchy to two layer relationship.
```cpp
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

```

3. `generateObjectBufferMesh()` method would:
	1. Find all the hierarchical animation model paths
	2. Load up all the meshes: terrain, smoke, animations
	3. Set up buffers of all the models needed, and it would call a internal lambda function `SetUpModelBuffers()` to set up buffers for each mesh and recursively set up buffers for its children meshes.
```cpp
// find all the animation mmodel file paths first
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

	// Set up buffers for each mesh and its children.
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

		// set up buffers for its children
		for (int j = 0; j < model.mChildMeshes.size(); ++j)
		{
			SetUpModelBuffers(model.mChildMeshes[j]);
		}
	};

	glUseProgram(terrianShaderProgramID);

	// set up buffers for terrain
	SetUpModelBuffers(volcano_terrian_mesh);

	// set up buffers for smoke
	SetUpModelBuffers(smoke_mesh);
	
	// set up buffers for each animation mesh
	for (int i = 0; i < animationModelPaths.size(); ++i) {
		SetUpModelBuffers(animation_meshes[i]);
	}

	cout << "finish generate object buffer mesh\n";
}
```

4. `updateScene()` would :
	- update the rotation values of different meshes along the hierarchy, so they could look more realistically
	- update each fish's translation heading towards its own direction.
	- update camera position by checking if listened key being pressed, in this way, different key can be pressed together to produce a combined direction.

```cpp
const float PI = 3.14159265358979323846f;

// the heading direction of each fish array
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
	// calculate time elapsed in seconds
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsedTime = currentTime - startTime;
    float timeInSeconds = elapsedTime.count(); // Get elapsed time in seconds

	// Update roatetion of fin and head for fishes  in a sinusoidal pattern
    rotate_fin = 0.4f * sin(timeInSeconds);
	rotate_body = 0.4f * sin(timeInSeconds + PI / 4.0f);
    rotate_head = 0.4f * sin(timeInSeconds + PI / 2.0f);

	// update translation for each fish
	for(int i = 0; i < fish_translations.size(); ++i)
	{
		fish_translations[i] += fish_translate_directions[i] * 0.01f;
	}
	
	// update camera poistion and location based on key status
    keyControl::updateCameraPosition();

    // Draw the next frame
    glutPostRedisplay();
}
```

5. `display()` function would pass the `view, model, proj` for each mesh to the shader, and then:
	- For the hierarchical models, it would iterate all the animation models and recursively call inside function `UpdateNormalMeshUniforms()` to update its own mesh and children meshes uniform variables.
	- The terrain mesh doesn't have hierarchy, so the internal recursion would not happen.
	- And if it's the smoke mesh, then it would pass more variables to shader such as smoke color and camera position to adjust the transparency of the smoke.


```cpp
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


	// update mesh uniforms & draw function
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

	// Draw animation meshes with its children meshes
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

	// display key features of the current state of the work on the screen
	renderBitmapText(-1.0, 0.9, GLUT_BITMAP_HELVETICA_18, "The scene consists of a shining volcano, swiming fishes, smoke out of volcano, rocks, coral");
	renderBitmapText(-1.0, 0.85, GLUT_BITMAP_HELVETICA_18, "All the fishes are moving towards its heading direction");
	renderBitmapText(-1.0, 0.8, GLUT_BITMAP_HELVETICA_18, "Each Fish contains hierarchical meshes that fin and tail would move relatively to its body");
	renderBitmapText(-1.0, 0.75, GLUT_BITMAP_HELVETICA_18, "Key Control : W(forward) S(backward) A(left) D(right) Q(upward) E(downward). Shift + Key Control : Speed up");
	renderBitmapText(-1.0, 0.7, GLUT_BITMAP_HELVETICA_18, "Mouse Control : Left click and drag to control camera's yaw and pitch");

	glutSwapBuffers();
}
```

### Camera Control

1. Default values of camera position and rotations to initialize the camera and also support to reset the camera position(Key: R) and rotation(click Right mouse) 
```cpp
// camera default values
const glm::vec3 cameraDefaultPosition(-4.0f, 8.0f, 30.0f);
const float defaultYaw = -80.f;
const float defaultPitch = -5.f;

// camera realtime position
glm::vec3 cameraPosition = cameraDefaultPosition; // Camera position in world space

// camera rotation
float yaw = defaultYaw;  // Horizontal rotation (around Y-axis)
float pitch = defaultPitch; // Vertical rotation (around X-axis)

// last pressed mouse location
int lastMouseX = 0, lastMouseY = 0;
```

2.  A map that records the keys being pressed, so it can combine all the inputs when updating the camera position.  And also record the special key `shift` status
```cpp
namespace keyControl
{
std::unordered_map<unsigned char, bool> keyState;
bool isShiftPressed = false; // record is shift key pressed
bool isTranslationTriggered = false; // record is translation triggered
}
```

3. Listen to key events and mouse events to update the map and other status.
```cpp
namespace keyControl
{
	// record the state of the keyboard

	// update kep pressed
	void keypress(unsigned char key, int x, int y) {
		key = std::tolower(key); // only record lower case
		keyState[key] = true;
		isTranslationTriggered = true;
	}

	// update kep released
	void keyRelease(unsigned char key, int x, int y) {
		key = std::tolower(key);
		keyState[key] = false;
		isTranslationTriggered = false;
	}

	// update shift statue
	void specialKeypress(int key, int x, int y) {
		if (key == GLUT_KEY_SHIFT_L || key == GLUT_KEY_SHIFT_R) {
			isShiftPressed = true;
		}
	}

	// update shift statue
	void specialKeyRelease(int key, int x, int y) {
		if (key == GLUT_KEY_SHIFT_L || key == GLUT_KEY_SHIFT_R) {
			isShiftPressed = false;
		}
	}
	// record the location of just pressed mouse
	void mouseButton(int button, int state, int x, int y) {
		if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
			lastMouseX = x;
			lastMouseY = y;
		}
		else if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
			yaw = defaultYaw;
			pitch = defaultPitch;
		}
	}
}
```


4. Updating the camera position:
	1. Retrieving the forward, up, right directions of current camera, so that the position can be updating accordingly
	2. Fetching the key pressed status and calculating the camera position
```cpp
namespace keyControl
{
const float normalSpeed = 0.05f;
	// update camera position
void updateCameraPosition() {

	if (!isTranslationTriggered)
		return;
	// update speed based on shift key
	float currentSpeed = isShiftPressed ? (normalSpeed * 2) : normalSpeed;

	// calculate the forward, right, up vectors of camera
	glm::vec3 forward(0.0);
	forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward.y = sin(glm::radians(pitch));
	forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	forward = glm::normalize(forward);
	glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

	// update camera position
	if (keyState['w']) {
		cameraPosition += forward * currentSpeed;
	}
	if (keyState['s']) {
		cameraPosition -= forward * currentSpeed;
	}
	if (keyState['a']) {
		cameraPosition -= right * currentSpeed;
	}
	if (keyState['d']) {
		cameraPosition += right * currentSpeed;
	}
	if (keyState['q']) {
		cameraPosition += up * currentSpeed;
	}
	if (keyState['e']) {
		cameraPosition -= up * currentSpeed;
	}
	if (keyState['r']) { // reset camera position
		cameraPosition = cameraDefaultPosition;
	}

	cout << "cameraPosition: " << cameraPosition.x << " " << cameraPosition.y << " " << cameraPosition.z << endl;
}
}
```

5. Update camera rotation according to last pressed mouse location  
```cpp
namespace keyControl
{
// update camera rotation angle accroding to mouse movement compared to last time
const float sensitivity = 0.3f;
void mouseMotion(int x, int y) {

	// calculate delta movement
	int deltaX = x - lastMouseX;
	int deltaY = y - lastMouseY;

	// update last pressed location
	lastMouseX = x;
	lastMouseY = y;

	// update rotation angle
	yaw += deltaX * sensitivity;
	pitch -= deltaY * sensitivity;

	cout << "deltaX: " << deltaX << " deltaY: " << deltaY << endl;
	cout << "yaw: " << yaw << " pitch: " << pitch << endl;

	// Constrain the pitch to avoid gimbal lock
	if (pitch > 87.0f) pitch = 87.0f;
	if (pitch < -87.0f) pitch = -87.0f;

	glutPostRedisplay();
}
}
```

### Vertex shader

```cpp
#version 440

in vec3 vertex_position;
in vec3 vertex_normal;

out vec4 EyeCoords;
out vec3 Normal;

uniform mat4 view;
uniform mat4 proj;
uniform mat4 model;

void main() {

    // Model-view matrix and normal transformation
    mat4 ModelViewMatrix = view * model;
    mat3 NormalMatrix = mat3(ModelViewMatrix);
    
    // Transform normal to view space and normalize it
    Normal = normalize(NormalMatrix * vertex_normal);
    
    // Transform vertex position to view space
    EyeCoords = ModelViewMatrix * vec4(vertex_position, 1.0);

    // Convert position to clip coordinates and pass along
    gl_Position = proj * view * model * vec4(vertex_position, 1.0);
}
```

### Fragment Shader

There are 5 lights in total, one from the center of the volcano which is red, the other four are blue lights at the corners of the terrain. For most of the models, they will be lighted with the mentioned lightings to calculate the color, but as for the smoke, all the lightings are replaced with the color of smoke.


```cpp
#version 330

in vec4 EyeCoords;
in vec3 Normal;

uniform vec3 smokeColor;           // Base color for fog
uniform float density;           // Density for fog effect
uniform vec3 viewPos;            // Camera position
uniform bool isSmoke;              // Flag to indicate fog vs. regular object

// Volcano and Sea Lights
vec4 VolcanoLightPosition = vec4(0.0, 2.0, 0.0, 1.0);
vec3 VolcanoLd = vec3(1.0, 1.0, 1.0);

vec4 SeaLightPosition1 = vec4(-25.0, -10.0, -25.0, 1.0);
vec4 SeaLightPosition2 = vec4(25.0, -10.0, 25.0, 1.0);
vec4 SeaLightPosition3 = vec4(-25.0, -10.0, 25.0, 1.0);
vec4 SeaLightPosition4 = vec4(25.0, -10.0, -25.0, 1.0);
vec3 SeaLd = vec3(1.0, 1.0, 1.0);

// Diffuse colors for volcano and sea lights
vec3 VolcanoKd = vec3(1.0, 0.0, 0.0);
vec3 SeaKd = vec3(0.004f, 0.361f, 0.588f);

// calculte light of each light setting up in the scene
vec3 CalcLightIntensity(vec4 LightPosition, vec3 normal, vec3 Kd, vec3 Ld, vec4 eyeCoords, float constant, float linear, float quadratic) {
    float distance = length(LightPosition.xyz - eyeCoords.xyz);
    float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));
    vec3 s = normalize(vec3(LightPosition - eyeCoords));
    return Ld * Kd * max(dot(s, normal), 0.0) * attenuation;
}

void main() {
    vec3 LightIntensity;

    if (isSmoke) {
        // Smoke effect calculations
        float distance = length(viewPos - EyeCoords.xyz);
        float smokeFactor = exp(-density * distance);  // Exponential falloff

        // Calculate smoke lighting using the same light sources
        LightIntensity = CalcLightIntensity(VolcanoLightPosition, Normal, smokeColor, VolcanoLd, EyeCoords, 1.0, 0.001, 0.0001);
        LightIntensity += CalcLightIntensity(SeaLightPosition1, Normal, smokeColor, SeaLd, EyeCoords, 1.0, 0.0025, 0.002);
        LightIntensity += CalcLightIntensity(SeaLightPosition2, Normal, smokeColor, SeaLd, EyeCoords, 1.0, 0.0025, 0.002);
        LightIntensity += CalcLightIntensity(SeaLightPosition3, Normal, smokeColor, SeaLd, EyeCoords, 1.0, 0.0025, 0.002);
        LightIntensity += CalcLightIntensity(SeaLightPosition4, Normal, smokeColor, SeaLd, EyeCoords, 1.0, 0.0025, 0.002);

        // Apply smoke color blending based on smokeFactor
        vec3 finalSmokeColor = mix(smokeColor, LightIntensity, smokeFactor);
        gl_FragColor = vec4(finalSmokeColor, smokeFactor);  
    } else {
        // Standard lighting for regular objects
        LightIntensity = CalcLightIntensity(VolcanoLightPosition, Normal, VolcanoKd, VolcanoLd, EyeCoords, 1.0, 0.001, 0.0001);
        LightIntensity += CalcLightIntensity(SeaLightPosition1, Normal, SeaKd, SeaLd, EyeCoords, 1.0, 0.0025, 0.002);
        LightIntensity += CalcLightIntensity(SeaLightPosition2, Normal, SeaKd, SeaLd, EyeCoords, 1.0, 0.0025, 0.002);
        LightIntensity += CalcLightIntensity(SeaLightPosition3, Normal, SeaKd, SeaLd, EyeCoords, 1.0, 0.0025, 0.002);
        LightIntensity += CalcLightIntensity(SeaLightPosition4, Normal, SeaKd, SeaLd, EyeCoords, 1.0, 0.0025, 0.002);

        gl_FragColor = vec4(LightIntensity, 1.0);  // Solid alpha for regular objects
    }
}
```

