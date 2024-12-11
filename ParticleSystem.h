#pragma once
#include <random>
#include <GL/glew.h>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>
#include <gtc/type_ptr.hpp>

#include "CameraControl.hpp"
#include "TextureManager.h"
#include "ShaderUtility.h"

// Update particles
std::mt19937 rng(std::random_device{}());
std::uniform_real_distribution<float> horizontalDist(-3.f, 3.f); // Small horizontal spread
std::uniform_real_distribution<float> verticalDist(5.f, 15.f);    // Upward velocity


// Particle structure
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime;
    Particle() : position(0.0f), velocity(0.0f), lifetime(0.0f) {}
};

class ParticleSystem {

    // singleton
private:
    ParticleSystem() {}

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;
public:

    static ParticleSystem* Instance()
    {
        static ParticleSystem* instance = new ParticleSystem();
        return instance;
    }

    const int NUM_PARTICLES = 3500;
    const float PARTICLE_LIFETIME = 7.0f;
    std::vector<Particle> particles;


    // OpenGL data
    GLuint particleVAO, particleVBO, offsetVBO, lifeTimeVBO;
    GLuint shaderProgram;
    GLuint particleTexture;
    glm::vec3 startPosition{ 4.3, 40, -0.5 };


    void Init() {
		initParticles();
		initOpenGL();
	}

    // Initialize particles
    void initParticles()
    {
        for (int i = 0; i < NUM_PARTICLES; ++i) {
            particles.emplace_back();
        }
    }


    void updateParticles(float deltaTime, int NewParticlePerFrame) {


        static int particleIndex = 0; // To track which particle to reset
        int searched = 0;
        for (int i = 0; i < NewParticlePerFrame; ++i) {
            searched++;
            if (particles[particleIndex].lifetime > 0.0f)
            {
                if (searched > NUM_PARTICLES)
                {
                    std::cout << "No more particles to reset" << std::endl;
                    break;
                }
                --i;
                particleIndex = (particleIndex + 1) % NUM_PARTICLES; // Wrap around
                continue;
            }
            if (particles[particleIndex].lifetime <= 0.0f)
            {
                // Reset a particle
                Particle& p = particles[particleIndex];
                p.position = startPosition + glm::vec3(horizontalDist(rng), 0.0f, horizontalDist(rng));; // Start at the origin
                p.velocity = glm::vec3(horizontalDist(rng), verticalDist(rng), horizontalDist(rng)); // Mostly upward velocity
                p.lifetime = PARTICLE_LIFETIME;
            }
            particleIndex = (particleIndex + 1) % NUM_PARTICLES; // Wrap around
        }

        // Update all particles
        const float dampingFactor = 0.75f; // Slow down speed
        for (auto& p : particles) {
            if (p.lifetime > 0.0f) {

                // 加入随机扰动
                float randomX = ((rand() % 100) / 100.0f - 0.5f) * 0.01f;
                float randomZ = ((rand() % 100) / 100.0f - 0.5f) * 0.01f;
                p.velocity.x += randomX;
                p.velocity.z += randomZ;

                p.position += p.velocity * deltaTime * dampingFactor;
                p.lifetime -= deltaTime;
            }
        }
    }

    // Initialize OpenGL
    void initOpenGL() {

        particleTexture = TextureManager::Instance()->LoadTexture("texture.png");
        // Enable blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Particle quad vertices
        float quadVertices[] = {
            // Positions      // Texture Coords
            -0.05f, -0.05f, 0.0f, 0.0f, 0.0f,
             0.05f, -0.05f, 0.0f, 1.0f, 0.0f,
            -0.05f,  0.05f, 0.0f, 0.0f, 1.0f,

            -0.05f,  0.05f, 0.0f, 0.0f, 1.0f,
             0.05f, -0.05f, 0.0f, 1.0f, 0.0f,
             0.05f,  0.05f, 0.0f, 1.0f, 1.0f,
        };

        // Create VAO and VBOs
        glGenVertexArrays(1, &particleVAO);
        glGenBuffers(1, &particleVBO);
        glGenBuffers(1, &offsetVBO);
        glGenBuffers(1, &lifeTimeVBO);


        glBindVertexArray(particleVAO);

        // Vertex data
        glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Texture coordinate attribute
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER, offsetVBO);
        glBufferData(GL_ARRAY_BUFFER, NUM_PARTICLES * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribDivisor(1, 1); // This is per-instance data

        glBindBuffer(GL_ARRAY_BUFFER, lifeTimeVBO);
        glBufferData(GL_ARRAY_BUFFER, NUM_PARTICLES * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(3, 1); // This is per-instance data


        // load vertex shader by path to const char * 
        const char* vertexSource = readShaderSource("particleVertexSharder.txt");
        const char* fragmentSource = readShaderSource("particleFragmentShader.txt");

        GLuint vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
        GLuint fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        GLint success;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            std::cerr << "Shader Linking Error: " << infoLog << std::endl;
            exit(1);
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void renderParticles() {
        // Cull invisible particles
        std::vector<glm::vec3> offsets;
        std::vector<float> lifeTimes;
        for (const auto& p : particles) {
            if (p.lifetime > 0.0f) { // Example culling
                offsets.push_back(p.position);
                lifeTimes.push_back(p.lifetime / PARTICLE_LIFETIME);
            }
        }

        if (offsets.empty()) return; // No particles to render

        glUseProgram(shaderProgram);
        glBindVertexArray(particleVAO);

        // Update offsets using persistent mapping
        glBindBuffer(GL_ARRAY_BUFFER, offsetVBO);
        glm::vec3* mappedBuffer = static_cast<glm::vec3*>(
            glMapBufferRange(GL_ARRAY_BUFFER, 0, offsets.size() * sizeof(glm::vec3),
                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
        std::memcpy(mappedBuffer, offsets.data(), offsets.size() * sizeof(glm::vec3));
        glUnmapBuffer(GL_ARRAY_BUFFER);

        // Update lifetime buffer
        glBindBuffer(GL_ARRAY_BUFFER, lifeTimeVBO);
        float* lifetimeBuffer = static_cast<float*>(
            glMapBufferRange(GL_ARRAY_BUFFER, 0, lifeTimes.size() * sizeof(float),
                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
        std::memcpy(lifetimeBuffer, lifeTimes.data(), lifeTimes.size() * sizeof(float));
        glUnmapBuffer(GL_ARRAY_BUFFER);

        // Set uniforms
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, persp_proj.m);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniform4f(glGetUniformLocation(shaderProgram, "particleColor"), 1.0f, 1.0f, 1.0f, 1.0f);

        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, particleTexture);
        glUniform1i(glGetUniformLocation(shaderProgram, "particleTexture"), 0);



        // Draw instanced particles
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, offsets.size());

        glBindVertexArray(0);
    }
};

