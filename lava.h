#pragma once
#include "ModelStructure.h"

// generate lava plane with vertices, normals, uvs
ModelData generateLavaPlane(float width, float depth, int rows, int cols) {
    ModelData lavaPlane;

    float dx = width / (cols - 1);
    float dz = depth / (rows - 1);

    for (int z = 0; z < rows; ++z) {
        for (int x = 0; x < cols; ++x) {
            float px = -width / 2.0f + x * dx; // X
            float pz = -depth / 2.0f + z * dz; // Z 
            float py = 0.0f;                  // flat at beginning

            // vertex position
            lavaPlane.mVertices.push_back(glm::vec3(px, py, pz));

            // normal
            lavaPlane.mNormals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));

            // texture coordinates
            lavaPlane.mTextureCoords.push_back(glm::vec2(float(x) / (float)(cols - 1), float(z) / (float)(rows - 1)));

            // Calculate the indices of the four corners of the current cell
            unsigned int topLeft = z * cols + x;
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = (z + 1) * cols + x;
            unsigned int bottomRight = bottomLeft + 1;

            // Add the two triangles for this cell
            lavaPlane.mIndices.push_back(topLeft);
            lavaPlane.mIndices.push_back(bottomLeft);
            lavaPlane.mIndices.push_back(topRight);

            lavaPlane.mIndices.push_back(topRight);
            lavaPlane.mIndices.push_back(bottomLeft);
            lavaPlane.mIndices.push_back(bottomRight);
        }
    }
    lavaPlane.mPointCount = lavaPlane.mVertices.size();
    return lavaPlane;
}

vector<unsigned int> generateGridIndices(int rows, int cols) {
    vector<unsigned int> indices;

    for (int z = 0; z < rows - 1; ++z) {
        for (int x = 0; x < cols - 1; ++x) {
            
        }
    }
    return indices;
}


float generateHeight(float x, float z, float time) 
{
    float frequency = 0.1f;  
    float amplitude = 2.0f;   
    float phaseShift = 0.3f;  
    // summation of two sinosoidal waves
    return amplitude * (sin(frequency * x + time) + cos(frequency * z + time * phaseShift));
}

void updatePlaneVerticesWithHeight(ModelData& lava, int rows, int cols, float time) {
    int index = 0;
    for (int z = 0; z < rows; ++z) {
        for (int x = 0; x < cols; ++x) {
            float px = lava.mVertices[index].x;
            float pz = lava.mVertices[index].z;
            lava.mVertices[index].y = generateHeight(px, pz, time);
            index++;
        }
    }

    // update lava vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, lava.mVBOs[0]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, lava.mVertices.size() * sizeof(glm::vec3), lava.mVertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

}