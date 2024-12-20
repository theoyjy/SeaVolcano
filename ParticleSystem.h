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

// Random number generator for particles
std::mt19937 rng(std::random_device{}());
std::uniform_real_distribution<float> horizontalDist(-5.f, 5.f); // Small horizontal spread
std::uniform_real_distribution<float> verticalDist(12.f, 25.f);    // Upward velocity


// Particle structure
struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    float lifetime;
    Particle() : position(0.0f), velocity(0.0f), lifetime(0.0f) {}
};

class ParticleSystem {

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


    GLuint particleVAO, particleVBO, offsetVBO, lifeTimeVBO;
    GLuint shaderProgram;
    GLuint particleTexture;
    glm::vec3 startPosition{ 4.3, 40, -0.5 }; // crater of the volcano


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
        // track which particle to reset
        static int particleIndex = 0; 
        int searched = 0;
        for (int i = 0; i < NewParticlePerFrame; ++i) {

            searched++;
            // if particle is still alive, skip and --i to try again
            if (particles[particleIndex].lifetime > 0.0f)
            {
                if (searched > NUM_PARTICLES)
                {
                    std::cout << "No more particles to reset" << std::endl;
                    break;
                }
                --i;
                particleIndex = (particleIndex + 1) % NUM_PARTICLES; 
                continue;
            }

            // particle is dead, reset it
            if (particles[particleIndex].lifetime <= 0.0f)
            {
                // Reset a particle
                Particle& p = particles[particleIndex];
                // start from crater of the volcano
                p.position = startPosition + glm::vec3(horizontalDist(rng), 0.0f, horizontalDist(rng));
                p.velocity = glm::vec3(horizontalDist(rng), verticalDist(rng), horizontalDist(rng)); // Mostly upward velocity
                p.lifetime = PARTICLE_LIFETIME;
            }
            particleIndex = (particleIndex + 1) % NUM_PARTICLES;
        }

        // Update all particles
        const float dampingFactor = 0.75f; // Slow down speed
        for (auto& p : particles) {
            if (p.lifetime > 0.0f) {

                // randomize the velocity a bit
                float randomX = ((rand() % 100) / 100.0f - 0.5f) * 0.01f;
                float randomZ = ((rand() % 100) / 100.0f - 0.5f) * 0.01f;
                p.velocity.x += randomX;
                p.velocity.z += randomZ;

                p.position += p.velocity * deltaTime * dampingFactor;
                p.lifetime -= deltaTime;
            }
        }
    }

    void initOpenGL() {

        particleTexture = TextureManager::Instance()->LoadTexture("Assets/texture.png");
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

        // Position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Texture coordinate
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);

        // Particle world position
        glBindBuffer(GL_ARRAY_BUFFER, offsetVBO);
        glBufferData(GL_ARRAY_BUFFER, NUM_PARTICLES * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribDivisor(1, 1);

        // Particle lifetime
        glBindBuffer(GL_ARRAY_BUFFER, lifeTimeVBO);
        glBufferData(GL_ARRAY_BUFFER, NUM_PARTICLES * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(3, 1);

        
        // load and compile particle shader
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

        // only render alive particles
        std::vector<glm::vec3> offsets;
        std::vector<float> lifeTimes;
        for (const auto& p : particles) {
            if (p.lifetime > 0.0f) {
                offsets.push_back(p.position);
                lifeTimes.push_back((PARTICLE_LIFETIME - p.lifetime)/ PARTICLE_LIFETIME);
            }
        }

        // No particles to render yet
        if (offsets.empty()) return; 

        glUseProgram(shaderProgram);
        glBindVertexArray(particleVAO);

        // Update particle position buffer
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
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(persp_proj));
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

