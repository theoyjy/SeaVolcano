#pragma once
#include <unordered_map>
#include <string>
#include <GL/glew.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;
class TextureManager {
private:
    std::unordered_map<string, GLuint> textureCache;
    TextureManager() {}
    ~TextureManager() {}
    
public:
    // singleton
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    
    static TextureManager* Instance()
    {
        static TextureManager* singleton = new TextureManager();
        return singleton;
    }
    
    GLuint LoadTexture(const string& path) {
        // Check if texture is already loaded
        if (textureCache.find(path) != textureCache.end()) {
            return textureCache[path];
        }

        // Load texture using stb_image
        int width, height, nrChannels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

        if (!data) {
            std::cerr << "Failed to load texture: " << path << std::endl;
            return 0;
        }
        
        // Generate OpenGL texture
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        if(nrChannels == 1)
        {
            // only 1 channel, expand to 3 channels
            size_t imageSize = width * height;
            std::vector<unsigned char> rgbData(imageSize * 3); 
           
            for (size_t i = 0; i < imageSize; ++i) {
                unsigned char gray = data[i]; 
                rgbData[i * 3 + 0] = gray;   
                rgbData[i * 3 + 1] = gray;   
                rgbData[i * 3 + 2] = gray;
            }
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, rgbData.data());
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        }
        // Free image memory and store in cache
        stbi_image_free(data);
        textureCache[path] = textureID;

        glGenerateMipmap(GL_TEXTURE_2D);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


        return textureID;
    }

    void Clear() {
        for (auto& pair : textureCache) {
            glDeleteTextures(1, &pair.second);
        }
        textureCache.clear();
    }
};
