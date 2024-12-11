#pragma once

int width = 1440;
int height = 720;

// illumination
glm::vec3 lightPosition;
glm::vec3 lightDirection;
float depthFalloff;
float causticIntensity;

glm::vec3 lightSpecular;
glm::vec3 lightDiffuse;
