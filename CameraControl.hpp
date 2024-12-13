#pragma once
#include <unordered_map>
#include <iostream>
#include <GL/glut.h>
#include <GL/freeglut.h>

#include <glm.hpp>
#include <gtc/type_ptr.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/quaternion.hpp>

#include "ProgramSetting.h"
#include "ModelStructure.h"

using namespace std;

const glm::vec3 cameraDefaultPosition(0, 48, 216);
//const glm::vec3 cameraDefaultPosition(0, 0,10);
glm::vec3 cameraPosition = cameraDefaultPosition; // Camera position in world space

const float defaultYaw = -90;
const float defaultPitch = 0;
float yaw = defaultYaw;  // Horizontal rotation (around Y-axis)
float pitch = defaultPitch; // Vertical rotation (around X-axis)

// camera and keyboard, mouse input
int lastMouseX = 0, lastMouseY = 0;
float angleX = 0.0f, angleY = 0.0f;

// camera projection matrix used in shader
const glm::mat4 persp_proj = glm::perspective(45.0f, (float)width / (float)height, 0.1f, 1000.0f);
glm::mat4 view;


namespace keyControl
{
	// record the state of the keyboard
	std::unordered_map<unsigned char, bool> keyState;
	bool isShiftPressed = false; // record is shift key pressed
	bool isTranslationTriggered = false; // record is translation triggered

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

	const float normalSpeed = 0.15f;
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

	void calculateRayFromScreen(float mouseX, float mouseY, glm::vec3& rayOrigin, glm::vec3& rayDirection) {
		// Normalize mouse coordinates
		float normalizedX = (2.0f * mouseX) / width - 1.0f;
		float normalizedY = 1.0f - (2.0f * mouseY) / height;

		// Create clip space coordinates
		glm::vec4 clipCoords = glm::vec4(normalizedX, normalizedY, -1.0f, 1.0f);

		// Convert to view space
		glm::vec4 eyeCoords = glm::inverse(persp_proj) * clipCoords;
		eyeCoords = glm::vec4(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f);

		// Convert to world space
		glm::vec3 worldCoords = glm::vec3(glm::inverse(view) * eyeCoords);
		rayDirection = glm::normalize(worldCoords);
		rayOrigin = cameraPosition; // Set the origin of the ray at the camera position
	}

	bool checkRayIntersect(const glm::vec3& sphereCenter, float sphereRadius, const glm::vec3& rayOrigin, const glm::vec3& rayDirection) {
		glm::vec3 toSphere = sphereCenter - rayOrigin;
		float tClosest = glm::dot(toSphere, rayDirection);
		float distanceSquared = glm::dot(toSphere, toSphere) - tClosest * tClosest;
		return distanceSquared <= (sphereRadius * sphereRadius);
	}


	void processMouseClick(float mouseX, float mouseY) {
		// Convert screen coordinates to world space (raycast setup)
		glm::vec3 rayOrigin, rayDirection;
		calculateRayFromScreen(mouseX, mouseY, rayOrigin, rayDirection);

		// Check for intersection with crabs
		for (auto& crab : CrabModels) {
			if (checkRayIntersect(crab.CurrentPosition(), crab.radius, rayOrigin, rayDirection)) {
				crab.StartRun();
			}
		}
	}

	// record the location of just pressed mouse
	void mouseButton(int button, int state, int x, int y) {
		if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
			lastMouseX = x;
			lastMouseY = y;
			processMouseClick(x, y);
		}
		else if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
			yaw = defaultYaw;
			pitch = defaultPitch;
		}
	}

	// update camera rotation angle accroding to mouse movement compared to last time
	const float sensitivity = 0.3f;
	void mouseMotion(int x, int y) {
		int deltaX = x - lastMouseX;
		int deltaY = y - lastMouseY;

		lastMouseX = x;
		lastMouseY = y;

		yaw += deltaX * sensitivity;
		pitch -= deltaY * sensitivity;

		cout << "deltaX: " << deltaX << " deltaY: " << deltaY << endl;
		cout << "yaw: " << yaw << " pitch: " << pitch << endl;

		// Constrain the pitch to avoid gimbal lock
		if (pitch > 87.0f) pitch = 87.0f;
		if (pitch < -87.0f) pitch = -87.0f;

		glutPostRedisplay();
	}




};