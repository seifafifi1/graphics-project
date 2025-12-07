/*
 * Crystal Caves - OpenGL Graphics Project
 * 
 * Controls:
 * - Press '1' to switch to Scene 1 (Crystal Cave Entrance)
 * - Press '2' to switch to Scene 2 (Deep Crystal Cavern)
 * - Press 'ESC' to exit
 * - Use WASD for movement (to be implemented)
 * - Mouse for camera look (to be implemented)
 */

// Silence OpenGL deprecation warnings on macOS
#define GL_SILENCE_DEPRECATION

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>
#include <thread>
#include <chrono>
#include <mutex>

// Windows multimedia for sound
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

// STB Image for texture loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ============================================================================
// TEXTURE LOADER FUNCTION
// ============================================================================

GLuint loadTexture(const std::string& filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 0);
    
    if (!data) {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        return 0;
    }
    
    std::cout << "Loaded texture: " << filename << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;
    
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload texture data
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);
    
    return textureID;
}

// ============================================================================
// BACKGROUND MUSIC VARIABLES
// ============================================================================

bool backgroundMusicPlaying = false;  // Track if background music is playing
std::string currentBackgroundMusic = "";  // Current background music file
std::mutex musicMutex;  // Mutex to ensure only one music plays at a time

// ============================================================================
// SOUND FUNCTION
// ============================================================================

void playDamageSound() {
#ifdef _WIN32
    // Play obstacle.wav asynchronously (non-blocking)
    PlaySound(TEXT("obstacle.wav"), NULL, SND_FILENAME | SND_ASYNC);
#endif
}

void playKeySound() {
#ifdef _WIN32
    // Play keys.wav asynchronously (non-blocking)
    PlaySound(TEXT("keys.wav"), NULL, SND_FILENAME | SND_ASYNC);
#endif
}

void playExplosionSound() {
#ifdef _WIN32
    // Play explosion.wav asynchronously (non-blocking)
    PlaySound(TEXT("explosion.wav"), NULL, SND_FILENAME | SND_ASYNC);
#endif
}

void playCrystalSound() {
#ifdef _WIN32
    // Play crystal.wav asynchronously (non-blocking)
    PlaySound(TEXT("crystal.wav"), NULL, SND_FILENAME | SND_ASYNC);
#endif
}

void playGameWinSound() {
#ifdef _WIN32
    // Play game win.wav asynchronously (non-blocking)
    PlaySound(TEXT("game win.wav"), NULL, SND_FILENAME | SND_ASYNC);
#endif
}

void playGameOverSound() {
#ifdef _WIN32
    // Play game over.wav asynchronously (non-blocking)
    PlaySound(TEXT("game over.wav"), NULL, SND_FILENAME | SND_ASYNC);
#endif
}

void playJumpSound() {
#ifdef _WIN32
    // Play jump.wav asynchronously (non-blocking)
    PlaySound(TEXT("jump.wav"), NULL, SND_FILENAME | SND_ASYNC);
#endif
}

// Forward declaration
void stopBackgroundMusic();

void playBackgroundMusic(const char* filename) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(musicMutex);
    
    // Stop any current background music completely
    backgroundMusicPlaying = false;
    currentBackgroundMusic = "";
    
    // Force close all MCI devices that might be playing
    mciSendString("close all", NULL, 0, NULL);
    
    // Give it a moment to stop completely
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Store the new filename
    currentBackgroundMusic = filename;
    backgroundMusicPlaying = true;
    
    std::cout << "Starting background music: " << filename << std::endl;
    
    // Create a thread to handle looping background music
    std::thread([filename]() {  // Capture filename by value
        std::string localFilename = filename;
        std::string musicAlias = "bgm";  // Use simple fixed alias
        
        while (backgroundMusicPlaying && currentBackgroundMusic == localFilename) {
            char errorMsg[256];
            char returnString[128];
            MCIERROR error;
            
            // Force close any existing bgm
            mciSendString("close bgm", NULL, 0, NULL);
            
            // Open the file
            std::string command = "open \"" + localFilename + "\" type waveaudio alias bgm";
            error = mciSendString(command.c_str(), NULL, 0, NULL);
            if (error != 0) {
                mciGetErrorString(error, errorMsg, 256);
                std::cout << "Error opening background music: " << errorMsg << std::endl;
                break;
            }
            
            // Play the music (non-blocking)
            error = mciSendString("play bgm", NULL, 0, NULL);
            if (error != 0) {
                mciGetErrorString(error, errorMsg, 256);
                std::cout << "Error playing background music: " << errorMsg << std::endl;
                mciSendString("close bgm", NULL, 0, NULL);
                break;
            }
            
            // Wait for the music to finish playing, checking status every 100ms
            while (backgroundMusicPlaying && currentBackgroundMusic == localFilename) {
                mciSendString("status bgm mode", returnString, sizeof(returnString), NULL);
                if (strcmp(returnString, "stopped") == 0 || strcmp(returnString, "") == 0) {
                    break;  // Song finished, loop to play again
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Close after playing
            mciSendString("close bgm", NULL, 0, NULL);
        }
    }).detach();
#endif
}

void stopBackgroundMusic() {
#ifdef _WIN32
    backgroundMusicPlaying = false;
    currentBackgroundMusic = "";
    // Force close all MCI devices
    mciSendString("close all", NULL, 0, NULL);
    std::cout << "Stopped background music" << std::endl;
#endif
}

void playExplosionThenDamageSound() {
#ifdef _WIN32
    // Create a thread to play sounds sequentially without blocking the game
    std::thread([]() {
        // Play explosion sound and wait for it to finish
        PlaySound(TEXT("explosion.wav"), NULL, SND_FILENAME | SND_SYNC);
        // Then play damage sound
        PlaySound(TEXT("obstacle.wav"), NULL, SND_FILENAME | SND_ASYNC);
    }).detach();
#endif
}

void playExplosionThenGameOverSound() {
#ifdef _WIN32
    // Create a thread to play sounds sequentially without blocking the game
    std::thread([]() {
        // Play explosion sound and wait for it to finish
        PlaySound(TEXT("explosion.wav"), NULL, SND_FILENAME | SND_SYNC);
        // Then play game over sound
        PlaySound(TEXT("game over.wav"), NULL, SND_FILENAME | SND_ASYNC);
    }).detach();
#endif
}

// ============================================================================
// VECTOR3 STRUCT - Basic 3D vector operations
// ============================================================================

struct Vector3 {
    float x, y, z;
    
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
    Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
    Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
    
    Vector3 cross(const Vector3& v) const {
        return Vector3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }
    
    float length() const { return sqrt(x * x + y * y + z * z); }
    
    Vector3 normalized() const {
        float len = length();
        if (len > 0) return Vector3(x / len, y / len, z / len);
        return Vector3(0, 0, 0);
    }
};

struct Vector2 {
    float u, v;
    Vector2() : u(0), v(0) {}
    Vector2(float u, float v) : u(u), v(v) {}
};

// ============================================================================
// MATERIAL STRUCT - For MTL file support
// ============================================================================

struct Material {
    std::string name;
    float ambient[4];
    float diffuse[4];
    float specular[4];
    float emission[4];
    float shininess;
    float transparency;
    std::string textureFile;
    GLuint textureId;
    
    Material() : shininess(32.0f), transparency(1.0f), textureId(0) {
        ambient[0] = 0.2f; ambient[1] = 0.2f; ambient[2] = 0.2f; ambient[3] = 1.0f;
        diffuse[0] = 0.8f; diffuse[1] = 0.8f; diffuse[2] = 0.8f; diffuse[3] = 1.0f;
        specular[0] = 1.0f; specular[1] = 1.0f; specular[2] = 1.0f; specular[3] = 1.0f;
        emission[0] = 0.0f; emission[1] = 0.0f; emission[2] = 0.0f; emission[3] = 1.0f;
    }
    
    void apply() const {
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
        glColor4f(diffuse[0], diffuse[1], diffuse[2], diffuse[3]);
        
        // Bind texture if available
        if (textureId != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureId);
        } else {
            glDisable(GL_TEXTURE_2D);
        }
    }
};

// ============================================================================
// FACE STRUCT - Represents a polygon face
// ============================================================================

struct Face {
    std::vector<int> vertexIndices;
    std::vector<int> texCoordIndices;
    std::vector<int> normalIndices;
    std::string materialName;
};

// ============================================================================
// OBJ MODEL CLASS - Complete OBJ file loader
// ============================================================================

class OBJModel {
public:
    std::vector<Vector3> vertices;
    std::vector<Vector3> normals;
    std::vector<Vector2> texCoords;
    std::vector<Face> faces;
    std::map<std::string, Material> materials;
    
    // Bounding box
    Vector3 minBounds, maxBounds;
    Vector3 center;
    float boundingRadius;
    
    // Display list for faster rendering
    GLuint displayList;
    bool hasDisplayList;
    bool hasTextures;  // Flag to indicate if model uses textures
    
    // Transform properties
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    
    std::string name;
    bool isLoaded;
    
    OBJModel() : hasDisplayList(false), isLoaded(false), displayList(0), hasTextures(false) {
        position = Vector3(0, 0, 0);
        rotation = Vector3(0, 0, 0);
        scale = Vector3(1, 1, 1);
    }
    
    ~OBJModel() {
        if (hasDisplayList) {
            glDeleteLists(displayList, 1);
        }
    }
    
    // Load OBJ file
    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open OBJ file: " << filename << std::endl;
            return false;
        }
        
        std::cout << "Loading OBJ model: " << filename << std::endl;
        
        // Extract directory path for MTL file loading
        std::string directory = "";
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            directory = filename.substr(0, lastSlash + 1);
        }
        
        name = filename;
        std::string currentMaterial = "";
        std::string line;
        
        while (std::getline(file, line)) {
            // Remove carriage return if present (Windows line endings)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;
            
            if (prefix == "v") {
                // Vertex position
                Vector3 v;
                iss >> v.x >> v.y >> v.z;
                vertices.push_back(v);
            }
            else if (prefix == "vn") {
                // Vertex normal
                Vector3 n;
                iss >> n.x >> n.y >> n.z;
                normals.push_back(n);
            }
            else if (prefix == "vt") {
                // Texture coordinate
                Vector2 t;
                iss >> t.u >> t.v;
                texCoords.push_back(t);
            }
            else if (prefix == "f") {
                // Face
                Face face;
                face.materialName = currentMaterial;
                std::string vertexData;
                
                while (iss >> vertexData) {
                    int vIdx = 0, vtIdx = 0, vnIdx = 0;
                    
                    // Parse face vertex data (formats: v, v/vt, v/vt/vn, v//vn)
                    size_t pos1 = vertexData.find('/');
                    if (pos1 == std::string::npos) {
                        // Format: v
                        vIdx = std::stoi(vertexData);
                    } else {
                        vIdx = std::stoi(vertexData.substr(0, pos1));
                        size_t pos2 = vertexData.find('/', pos1 + 1);
                        if (pos2 == std::string::npos) {
                            // Format: v/vt
                            vtIdx = std::stoi(vertexData.substr(pos1 + 1));
                        } else {
                            // Format: v/vt/vn or v//vn
                            std::string vtStr = vertexData.substr(pos1 + 1, pos2 - pos1 - 1);
                            if (!vtStr.empty()) {
                                vtIdx = std::stoi(vtStr);
                            }
                            vnIdx = std::stoi(vertexData.substr(pos2 + 1));
                        }
                    }
                    
                    // OBJ indices are 1-based, convert to 0-based
                    // Negative indices are relative to current count
                    if (vIdx < 0) vIdx = vertices.size() + vIdx + 1;
                    if (vtIdx < 0) vtIdx = texCoords.size() + vtIdx + 1;
                    if (vnIdx < 0) vnIdx = normals.size() + vnIdx + 1;
                    
                    face.vertexIndices.push_back(vIdx - 1);
                    face.texCoordIndices.push_back(vtIdx - 1);
                    face.normalIndices.push_back(vnIdx - 1);
                }
                
                if (face.vertexIndices.size() >= 3) {
                    faces.push_back(face);
                }
            }
            else if (prefix == "mtllib") {
                // Material library - read rest of line to handle filenames with spaces
                std::string mtlFile;
                std::getline(iss >> std::ws, mtlFile);
                loadMTL(directory + mtlFile);
            }
            else if (prefix == "usemtl") {
                // Use material - read rest of line to handle material names with spaces
                std::getline(iss >> std::ws, currentMaterial);
            }
        }
        
        file.close();
        
        // Calculate bounds
        calculateBounds();
        
        // Generate normals if not provided
        if (normals.empty()) {
            generateNormals();
        }
        
        // Create display list for faster rendering
        // Check if any materials have textures
        hasTextures = false;
        for (const auto& mat : materials) {
            if (mat.second.textureId != 0) {
                hasTextures = true;
                break;
            }
        }
        
        // Create display list for all models (including textured ones)
        createDisplayList();
        
        isLoaded = true;
        std::cout << "Loaded OBJ: " << vertices.size() << " vertices, " 
                  << faces.size() << " faces, " 
                  << materials.size() << " materials" << std::endl;
        
        return true;
    }
    
    // Load MTL material file
    bool loadMTL(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open MTL file: " << filename << std::endl;
            return false;
        }
        
        std::cout << "Loading MTL file: " << filename << std::endl;
        
        Material* currentMat = nullptr;
        std::string line;
        
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;
            
            if (prefix == "newmtl") {
                std::string matName;
                iss >> matName;
                materials[matName] = Material();
                materials[matName].name = matName;
                currentMat = &materials[matName];
            }
            else if (currentMat != nullptr) {
                if (prefix == "Ka") {
                    iss >> currentMat->ambient[0] >> currentMat->ambient[1] >> currentMat->ambient[2];
                }
                else if (prefix == "Kd") {
                    iss >> currentMat->diffuse[0] >> currentMat->diffuse[1] >> currentMat->diffuse[2];
                }
                else if (prefix == "Ks") {
                    iss >> currentMat->specular[0] >> currentMat->specular[1] >> currentMat->specular[2];
                }
                else if (prefix == "Ke") {
                    iss >> currentMat->emission[0] >> currentMat->emission[1] >> currentMat->emission[2];
                }
                else if (prefix == "Ns") {
                    iss >> currentMat->shininess;
                    // OBJ shininess is 0-1000, OpenGL is 0-128
                    currentMat->shininess = std::min(currentMat->shininess * 128.0f / 1000.0f, 128.0f);
                }
                else if (prefix == "d" || prefix == "Tr") {
                    iss >> currentMat->transparency;
                    if (prefix == "Tr") currentMat->transparency = 1.0f - currentMat->transparency;
                    currentMat->diffuse[3] = currentMat->transparency;
                    currentMat->ambient[3] = currentMat->transparency;
                }
                else if (prefix == "map_Kd") {
                    std::string texFile;
                    std::getline(iss >> std::ws, texFile);
                    currentMat->textureFile = texFile;
                    
                    // Extract directory from MTL filename and load texture
                    std::string directory = "";
                    size_t lastSlash = filename.find_last_of("/\\");
                    if (lastSlash != std::string::npos) {
                        directory = filename.substr(0, lastSlash + 1);
                    }
                    
                    std::string fullTexPath = directory + texFile;
                    currentMat->textureId = loadTexture(fullTexPath);
                }
            }
        }
        
        file.close();
        return true;
    }
    
    // Calculate bounding box
    void calculateBounds() {
        if (vertices.empty()) return;
        
        minBounds = maxBounds = vertices[0];
        
        for (const auto& v : vertices) {
            minBounds.x = std::min(minBounds.x, v.x);
            minBounds.y = std::min(minBounds.y, v.y);
            minBounds.z = std::min(minBounds.z, v.z);
            maxBounds.x = std::max(maxBounds.x, v.x);
            maxBounds.y = std::max(maxBounds.y, v.y);
            maxBounds.z = std::max(maxBounds.z, v.z);
        }
        
        center = Vector3(
            (minBounds.x + maxBounds.x) / 2.0f,
            (minBounds.y + maxBounds.y) / 2.0f,
            (minBounds.z + maxBounds.z) / 2.0f
        );
        
        boundingRadius = (maxBounds - minBounds).length() / 2.0f;
    }
    
    // Generate normals if not present in OBJ file
    void generateNormals() {
        normals.resize(vertices.size(), Vector3(0, 0, 0));
        
        for (const auto& face : faces) {
            if (face.vertexIndices.size() < 3) continue;
            
            // Calculate face normal
            Vector3 v0 = vertices[face.vertexIndices[0]];
            Vector3 v1 = vertices[face.vertexIndices[1]];
            Vector3 v2 = vertices[face.vertexIndices[2]];
            
            Vector3 edge1 = v1 - v0;
            Vector3 edge2 = v2 - v0;
            Vector3 faceNormal = edge1.cross(edge2).normalized();
            
            // Add face normal to all vertices of the face
            for (int idx : face.vertexIndices) {
                normals[idx] = normals[idx] + faceNormal;
            }
        }
        
        // Normalize all vertex normals
        for (auto& n : normals) {
            n = n.normalized();
        }
        
        // Update face normal indices
        for (auto& face : faces) {
            face.normalIndices = face.vertexIndices;
        }
    }
    
    // Create OpenGL display list for faster rendering (batched by material)
    void createDisplayList() {
        displayList = glGenLists(1);
        glNewList(displayList, GL_COMPILE);
        
        // Group faces by material for batch rendering
        std::map<std::string, std::vector<const Face*>> facesByMaterial;
        for (const auto& face : faces) {
            facesByMaterial[face.materialName].push_back(&face);
        }
        
        // Render each material group
        for (const auto& group : facesByMaterial) {
            // Apply material once per group
            if (!group.first.empty()) {
                auto it = materials.find(group.first);
                if (it != materials.end()) {
                    it->second.apply();
                }
            }
            
            // Batch all triangles for this material
            glBegin(GL_TRIANGLES);
            for (const Face* face : group.second) {
                if (face->vertexIndices.size() < 3) continue;
                
                // Triangulate quads and polygons
                for (size_t i = 1; i < face->vertexIndices.size() - 1; i++) {
                    // First vertex of triangle fan
                    int vIdx = face->vertexIndices[0];
                    int nIdx = face->normalIndices[0];
                    int tIdx = face->texCoordIndices[0];
                    if (tIdx >= 0 && tIdx < (int)texCoords.size()) {
                        glTexCoord2f(texCoords[tIdx].u, texCoords[tIdx].v);
                    }
                    if (nIdx >= 0 && nIdx < (int)normals.size()) {
                        glNormal3f(normals[nIdx].x, normals[nIdx].y, normals[nIdx].z);
                    }
                    if (vIdx >= 0 && vIdx < (int)vertices.size()) {
                        glVertex3f(vertices[vIdx].x, vertices[vIdx].y, vertices[vIdx].z);
                    }
                    
                    // Second vertex
                    vIdx = face->vertexIndices[i];
                    nIdx = face->normalIndices[i];
                    tIdx = face->texCoordIndices[i];
                    if (tIdx >= 0 && tIdx < (int)texCoords.size()) {
                        glTexCoord2f(texCoords[tIdx].u, texCoords[tIdx].v);
                    }
                    if (nIdx >= 0 && nIdx < (int)normals.size()) {
                        glNormal3f(normals[nIdx].x, normals[nIdx].y, normals[nIdx].z);
                    }
                    if (vIdx >= 0 && vIdx < (int)vertices.size()) {
                        glVertex3f(vertices[vIdx].x, vertices[vIdx].y, vertices[vIdx].z);
                    }
                    
                    // Third vertex
                    vIdx = face->vertexIndices[i + 1];
                    nIdx = face->normalIndices[i + 1];
                    tIdx = face->texCoordIndices[i + 1];
                    if (tIdx >= 0 && tIdx < (int)texCoords.size()) {
                        glTexCoord2f(texCoords[tIdx].u, texCoords[tIdx].v);
                    }
                    if (nIdx >= 0 && nIdx < (int)normals.size()) {
                        glNormal3f(normals[nIdx].x, normals[nIdx].y, normals[nIdx].z);
                    }
                    if (vIdx >= 0 && vIdx < (int)vertices.size()) {
                        glVertex3f(vertices[vIdx].x, vertices[vIdx].y, vertices[vIdx].z);
                    }
                }
            }
            glEnd();
        }
        
        // Disable textures at end of display list
        glDisable(GL_TEXTURE_2D);
        
        glEndList();
        hasDisplayList = true;
    }
    
    // Render the model
    void render() const {
        if (!isLoaded) return;
        
        glPushMatrix();
        
        // Apply transformations
        glTranslatef(position.x, position.y, position.z);
        glRotatef(rotation.x, 1.0f, 0.0f, 0.0f);
        glRotatef(rotation.y, 0.0f, 1.0f, 0.0f);
        glRotatef(rotation.z, 0.0f, 0.0f, 1.0f);
        glScalef(scale.x, scale.y, scale.z);
        
        // Use display list for all models (pre-compiled for performance)
        if (hasDisplayList) {
            glCallList(displayList);
        } else {
            // Fallback to direct rendering
            renderDirect();
        }
        
        // Ensure textures are disabled after rendering
        glDisable(GL_TEXTURE_2D);
        
        glPopMatrix();
    }
    
    // Direct rendering for textured models (batched for performance)
    void renderDirect() const {
        // Group faces by material for batch rendering
        std::map<std::string, std::vector<const Face*>> facesByMaterial;
        for (const auto& face : faces) {
            facesByMaterial[face.materialName].push_back(&face);
        }
        
        // Render each material group
        for (const auto& group : facesByMaterial) {
            // Apply material once per group
            if (!group.first.empty()) {
                auto it = materials.find(group.first);
                if (it != materials.end()) {
                    it->second.apply();
                }
            }
            
            // Batch triangles together
            glBegin(GL_TRIANGLES);
            for (const Face* face : group.second) {
                if (face->vertexIndices.size() < 3) continue;
                
                // Triangulate if needed (for quads and polygons)
                for (size_t i = 1; i < face->vertexIndices.size() - 1; i++) {
                    // First vertex of triangle fan
                    int vIdx = face->vertexIndices[0];
                    int nIdx = face->normalIndices[0];
                    int tIdx = face->texCoordIndices[0];
                    if (tIdx >= 0 && tIdx < (int)texCoords.size()) {
                        glTexCoord2f(texCoords[tIdx].u, texCoords[tIdx].v);
                    }
                    if (nIdx >= 0 && nIdx < (int)normals.size()) {
                        glNormal3f(normals[nIdx].x, normals[nIdx].y, normals[nIdx].z);
                    }
                    if (vIdx >= 0 && vIdx < (int)vertices.size()) {
                        glVertex3f(vertices[vIdx].x, vertices[vIdx].y, vertices[vIdx].z);
                    }
                    
                    // Second vertex
                    vIdx = face->vertexIndices[i];
                    nIdx = face->normalIndices[i];
                    tIdx = face->texCoordIndices[i];
                    if (tIdx >= 0 && tIdx < (int)texCoords.size()) {
                        glTexCoord2f(texCoords[tIdx].u, texCoords[tIdx].v);
                    }
                    if (nIdx >= 0 && nIdx < (int)normals.size()) {
                        glNormal3f(normals[nIdx].x, normals[nIdx].y, normals[nIdx].z);
                    }
                    if (vIdx >= 0 && vIdx < (int)vertices.size()) {
                        glVertex3f(vertices[vIdx].x, vertices[vIdx].y, vertices[vIdx].z);
                    }
                    
                    // Third vertex
                    vIdx = face->vertexIndices[i + 1];
                    nIdx = face->normalIndices[i + 1];
                    tIdx = face->texCoordIndices[i + 1];
                    if (tIdx >= 0 && tIdx < (int)texCoords.size()) {
                        glTexCoord2f(texCoords[tIdx].u, texCoords[tIdx].v);
                    }
                    if (nIdx >= 0 && nIdx < (int)normals.size()) {
                        glNormal3f(normals[nIdx].x, normals[nIdx].y, normals[nIdx].z);
                    }
                    if (vIdx >= 0 && vIdx < (int)vertices.size()) {
                        glVertex3f(vertices[vIdx].x, vertices[vIdx].y, vertices[vIdx].z);
                    }
                }
            }
            glEnd();
        }
    }
    
    // Render with external texture (bypasses display list to use caller's bound texture)
    void renderWithTexture() const {
        if (!isLoaded) return;
        
        glPushMatrix();
        
        glTranslatef(position.x, position.y, position.z);
        glRotatef(rotation.x, 1.0f, 0.0f, 0.0f);
        glRotatef(rotation.y, 0.0f, 1.0f, 0.0f);
        glRotatef(rotation.z, 0.0f, 0.0f, 1.0f);
        glScalef(scale.x, scale.y, scale.z);
        
        // Render directly without display list to use caller's bound texture
        glBegin(GL_TRIANGLES);
        for (const auto& face : faces) {
            if (face.vertexIndices.size() < 3) continue;
            
            for (size_t i = 1; i < face.vertexIndices.size() - 1; i++) {
                // First vertex
                int vIdx = face.vertexIndices[0];
                int nIdx = face.normalIndices[0];
                int tIdx = face.texCoordIndices[0];
                if (tIdx >= 0 && tIdx < (int)texCoords.size()) {
                    glTexCoord2f(texCoords[tIdx].u, texCoords[tIdx].v);
                }
                if (nIdx >= 0 && nIdx < (int)normals.size()) {
                    glNormal3f(normals[nIdx].x, normals[nIdx].y, normals[nIdx].z);
                }
                if (vIdx >= 0 && vIdx < (int)vertices.size()) {
                    glVertex3f(vertices[vIdx].x, vertices[vIdx].y, vertices[vIdx].z);
                }
                
                // Second vertex
                vIdx = face.vertexIndices[i];
                nIdx = face.normalIndices[i];
                tIdx = face.texCoordIndices[i];
                if (tIdx >= 0 && tIdx < (int)texCoords.size()) {
                    glTexCoord2f(texCoords[tIdx].u, texCoords[tIdx].v);
                }
                if (nIdx >= 0 && nIdx < (int)normals.size()) {
                    glNormal3f(normals[nIdx].x, normals[nIdx].y, normals[nIdx].z);
                }
                if (vIdx >= 0 && vIdx < (int)vertices.size()) {
                    glVertex3f(vertices[vIdx].x, vertices[vIdx].y, vertices[vIdx].z);
                }
                
                // Third vertex
                vIdx = face.vertexIndices[i + 1];
                nIdx = face.normalIndices[i + 1];
                tIdx = face.texCoordIndices[i + 1];
                if (tIdx >= 0 && tIdx < (int)texCoords.size()) {
                    glTexCoord2f(texCoords[tIdx].u, texCoords[tIdx].v);
                }
                if (nIdx >= 0 && nIdx < (int)normals.size()) {
                    glNormal3f(normals[nIdx].x, normals[nIdx].y, normals[nIdx].z);
                }
                if (vIdx >= 0 && vIdx < (int)vertices.size()) {
                    glVertex3f(vertices[vIdx].x, vertices[vIdx].y, vertices[vIdx].z);
                }
            }
        }
        glEnd();
        
        glPopMatrix();
    }
    
    // Render with custom color (ignores material)
    void renderWithColor(float r, float g, float b, float a = 1.0f) const {
        if (!isLoaded) return;
        
        glPushMatrix();
        
        glTranslatef(position.x, position.y, position.z);
        glRotatef(rotation.x, 1.0f, 0.0f, 0.0f);
        glRotatef(rotation.y, 0.0f, 1.0f, 0.0f);
        glRotatef(rotation.z, 0.0f, 0.0f, 1.0f);
        glScalef(scale.x, scale.y, scale.z);
        
        glColor4f(r, g, b, a);
        
        GLfloat matDiffuse[] = { r, g, b, a };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDiffuse);
        
        if (hasDisplayList) {
            glCallList(displayList);
        }
        
        glPopMatrix();
    }
    
    // Set position
    void setPosition(float x, float y, float z) {
        position = Vector3(x, y, z);
    }
    
    // Set rotation (degrees)
    void setRotation(float rx, float ry, float rz) {
        rotation = Vector3(rx, ry, rz);
    }
    
    // Set scale
    void setScale(float sx, float sy, float sz) {
        scale = Vector3(sx, sy, sz);
    }
    
    void setUniformScale(float s) {
        scale = Vector3(s, s, s);
    }
};

// ============================================================================
// 3DS MODEL CLASS - Basic 3DS file loader
// ============================================================================

// 3DS file chunk IDs
#define MAIN3DS       0x4D4D
#define EDIT3DS       0x3D3D
#define EDIT_OBJECT   0x4000
#define OBJ_TRIMESH   0x4100
#define TRI_VERTEXL   0x4110
#define TRI_FACEL     0x4120
#define TRI_MAPCOORD  0x4140

class Model3DS {
public:
    std::vector<Vector3> vertices;
    std::vector<std::pair<float, float>> texCoords;  // UV coordinates
    std::vector<std::vector<int>> faces;
    float minY, maxY, minZ, maxZ;  // For positioning on ground
    
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    
    GLuint displayList;
    bool hasDisplayList;
    bool isLoaded;
    std::string name;
    
    Model3DS() : hasDisplayList(false), isLoaded(false), displayList(0), minY(0), maxY(0), minZ(0), maxZ(0) {
        position = Vector3(0, 0, 0);
        rotation = Vector3(0, 0, 0);
        scale = Vector3(1, 1, 1);
    }
    
    ~Model3DS() {
        if (hasDisplayList) {
            glDeleteLists(displayList, 1);
        }
    }
    
    unsigned short readShort(std::ifstream& file) {
        unsigned short value;
        file.read(reinterpret_cast<char*>(&value), sizeof(unsigned short));
        return value;
    }
    
    unsigned int readInt(std::ifstream& file) {
        unsigned int value;
        file.read(reinterpret_cast<char*>(&value), sizeof(unsigned int));
        return value;
    }
    
    float readFloat(std::ifstream& file) {
        float value;
        file.read(reinterpret_cast<char*>(&value), sizeof(float));
        return value;
    }
    
    std::string readString(std::ifstream& file) {
        std::string str;
        char c;
        while (file.read(&c, 1) && c != '\0') {
            str += c;
        }
        return str;
    }
    
    void processChunk(std::ifstream& file, unsigned short chunkID, unsigned int chunkLength) {
        unsigned int currentPos = file.tellg();
        unsigned int endPos = currentPos + chunkLength - 6;
        
        switch(chunkID) {
            case MAIN3DS:
            case EDIT3DS:
                // Container chunks - read sub-chunks
                while (file.tellg() < endPos) {
                    unsigned short subChunkID = readShort(file);
                    unsigned int subChunkLength = readInt(file);
                    processChunk(file, subChunkID, subChunkLength);
                }
                break;
                
            case EDIT_OBJECT:
                {
                    std::string objectName = readString(file);
                    // Read sub-chunks for this object
                    while (file.tellg() < endPos) {
                        unsigned short subChunkID = readShort(file);
                        unsigned int subChunkLength = readInt(file);
                        processChunk(file, subChunkID, subChunkLength);
                    }
                }
                break;
                
            case OBJ_TRIMESH:
                // Triangle mesh - read sub-chunks
                while (file.tellg() < endPos) {
                    unsigned short subChunkID = readShort(file);
                    unsigned int subChunkLength = readInt(file);
                    processChunk(file, subChunkID, subChunkLength);
                }
                break;
                
            case TRI_VERTEXL:
                {
                    unsigned short numVertices = readShort(file);
                    for (int i = 0; i < numVertices; i++) {
                        float x = readFloat(file);
                        float y = readFloat(file);
                        float z = readFloat(file);
                        vertices.push_back(Vector3(x, y, z));
                        // Track min/max Y and Z for ground positioning
                        if (vertices.size() == 1) {
                            minY = maxY = y;
                            minZ = maxZ = z;
                        } else {
                            if (y < minY) minY = y;
                            if (y > maxY) maxY = y;
                            if (z < minZ) minZ = z;
                            if (z > maxZ) maxZ = z;
                        }
                    }
                }
                break;
            
            case TRI_MAPCOORD:
                {
                    unsigned short numCoords = readShort(file);
                    for (int i = 0; i < numCoords; i++) {
                        float u = readFloat(file);
                        float v = readFloat(file);
                        texCoords.push_back(std::make_pair(u, v));
                    }
                }
                break;
                
            case TRI_FACEL:
                {
                    unsigned short numFaces = readShort(file);
                    for (int i = 0; i < numFaces; i++) {
                        unsigned short v1 = readShort(file);
                        unsigned short v2 = readShort(file);
                        unsigned short v3 = readShort(file);
                        unsigned short flags = readShort(file);
                        
                        std::vector<int> face;
                        face.push_back(v1);
                        face.push_back(v2);
                        face.push_back(v3);
                        faces.push_back(face);
                    }
                }
                break;
                
            default:
                // Skip unknown chunks
                file.seekg(endPos);
                break;
        }
    }
    
    bool load(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open 3DS file: " << filename << std::endl;
            return false;
        }
        
        std::cout << "Loading 3DS model: " << filename << std::endl;
        name = filename;
        
        // Read main chunk
        unsigned short chunkID = readShort(file);
        unsigned int chunkLength = readInt(file);
        
        if (chunkID != MAIN3DS) {
            std::cerr << "Error: Not a valid 3DS file!" << std::endl;
            file.close();
            return false;
        }
        
        // Process all chunks
        processChunk(file, chunkID, chunkLength);
        
        file.close();
        
        std::cout << "Loaded 3DS model with " << vertices.size() << " vertices and " 
                  << faces.size() << " faces" << std::endl;
        
        isLoaded = true;
        buildDisplayList();
        return true;
    }
    
    void buildDisplayList() {
        if (!isLoaded || vertices.empty() || faces.empty()) return;
        
        bool hasTexCoords = !texCoords.empty() && texCoords.size() >= vertices.size();
        
        displayList = glGenLists(1);
        glNewList(displayList, GL_COMPILE);
        
        for (const auto& face : faces) {
            if (face.size() == 3) {
                glBegin(GL_TRIANGLES);
                
                // Calculate face normal
                Vector3 v0 = vertices[face[0]];
                Vector3 v1 = vertices[face[1]];
                Vector3 v2 = vertices[face[2]];
                
                Vector3 edge1 = v1 - v0;
                Vector3 edge2 = v2 - v0;
                Vector3 normal = edge1.cross(edge2).normalized();
                
                glNormal3f(normal.x, normal.y, normal.z);
                
                if (hasTexCoords) {
                    glTexCoord2f(texCoords[face[0]].first, texCoords[face[0]].second);
                }
                glVertex3f(v0.x, v0.y, v0.z);
                
                if (hasTexCoords) {
                    glTexCoord2f(texCoords[face[1]].first, texCoords[face[1]].second);
                }
                glVertex3f(v1.x, v1.y, v1.z);
                
                if (hasTexCoords) {
                    glTexCoord2f(texCoords[face[2]].first, texCoords[face[2]].second);
                }
                glVertex3f(v2.x, v2.y, v2.z);
                
                glEnd();
            }
        }
        
        glEndList();
        hasDisplayList = true;
        
        std::cout << "Model has " << texCoords.size() << " texture coordinates" << std::endl;
    }
    
    void render() const {
        if (!isLoaded) return;
        
        glPushMatrix();
        
        glTranslatef(position.x, position.y, position.z);
        glRotatef(rotation.x, 1.0f, 0.0f, 0.0f);
        glRotatef(rotation.y, 0.0f, 1.0f, 0.0f);
        glRotatef(rotation.z, 0.0f, 0.0f, 1.0f);
        glScalef(scale.x, scale.y, scale.z);
        
        if (hasDisplayList) {
            glCallList(displayList);
        }
        
        glPopMatrix();
    }
    
    void setPosition(float x, float y, float z) {
        position = Vector3(x, y, z);
    }
    
    void setRotation(float rx, float ry, float rz) {
        rotation = Vector3(rx, ry, rz);
    }
    
    void setScale(float sx, float sy, float sz) {
        scale = Vector3(sx, sy, sz);
    }
    
    void setUniformScale(float s) {
        scale = Vector3(s, s, s);
    }
};

// ============================================================================
// MODEL MANAGER - Manages all loaded models
// ============================================================================

class ModelManager {
private:
    std::map<std::string, OBJModel*> models;
    
public:
    ~ModelManager() {
        for (auto& pair : models) {
            delete pair.second;
        }
        models.clear();
    }
    
    // Load a model and store it with a name
    OBJModel* loadModel(const std::string& name, const std::string& filename) {
        OBJModel* model = new OBJModel();
        if (model->load(filename)) {
            models[name] = model;
            return model;
        }
        delete model;
        return nullptr;
    }
    
    // Get a loaded model by name
    OBJModel* getModel(const std::string& name) {
        auto it = models.find(name);
        if (it != models.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    // Check if model exists
    bool hasModel(const std::string& name) {
        return models.find(name) != models.end();
    }
    
    // Unload a specific model
    void unloadModel(const std::string& name) {
        auto it = models.find(name);
        if (it != models.end()) {
            delete it->second;
            models.erase(it);
        }
    }
    
    // Get all model names
    std::vector<std::string> getModelNames() {
        std::vector<std::string> names;
        for (const auto& pair : models) {
            names.push_back(pair.first);
        }
        return names;
    }
};

// Global model manager
ModelManager modelManager;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Window dimensions
int windowWidth = 1280;
int windowHeight = 720;

// Mouse control
int lastMouseX = windowWidth / 2;
int lastMouseY = windowHeight / 2;
bool mouseInitialized = false;
float mouseSensitivity = 0.2f;

// Key states for continuous movement
bool keyW = false;
bool keyA = false;
bool keyS = false;
bool keyD = false;

// Current scene (1 or 2)
int currentScene = 1;

// Forward declaration for scene collision checking
typedef bool (*CollisionCheckFunc)(float x, float z, float radius);
CollisionCheckFunc sceneCollisionCheck = nullptr;

// ============================================================================
// PLAYER CLASS
// ============================================================================

class Player {
public:
    Vector3 position;
    float yaw;   // Rotation around Y axis (left/right)
    float pitch; // Rotation around X axis (up/down)
    bool isFirstPerson;
    float radius; // Collision radius
    
    // Jump mechanics
    float velocityY;      // Vertical velocity
    bool isJumping;       // Is player in the air?
    bool isOnGround;      // Is player on ground?
    float playerHeight;   // Player height for camera
    float groundLevel;    // Current ground level (can be negative for lava pits)
    
    // Animation variables
    float walkAnimation;  // Walking animation timer
    bool isMoving;        // Is player currently moving
    float bodyYaw;        // Character body rotation (separate from camera yaw)
    
    Player() : position(0.0f, 0.0f, 5.0f), yaw(0.0f), pitch(0.0f), isFirstPerson(false), radius(0.3f),
               velocityY(0.0f), isJumping(false), isOnGround(true), playerHeight(1.7f), groundLevel(0.0f),
               walkAnimation(0.0f), isMoving(false), bodyYaw(180.0f) {}
    
    void jump() {
        if (isOnGround && !isJumping) {
            velocityY = 6.0f;  // Jump strength
            isJumping = true;
            isOnGround = false;
            playJumpSound();  // Play jump sound effect
        }
    }
    
    void updatePhysics(float deltaTime) {
        // Apply gravity
        float gravity = -15.0f;
        velocityY += gravity * deltaTime;
        position.y += velocityY * deltaTime;
        
        // Check ground collision
        if (position.y <= groundLevel) {
            position.y = groundLevel;
            velocityY = 0.0f;
            isJumping = false;
            isOnGround = true;
        }
    }
    
    bool checkCollision(float targetX, float targetZ, float objX, float objZ, float objRadius) {
        float dx = targetX - objX;
        float dz = targetZ - objZ;
        float distance = sqrt(dx * dx + dz * dz);
        return distance < (radius + objRadius);
    }
    
    void move(float forward, float right) {
        // Calculate movement direction based on yaw
        float radYaw = yaw * M_PI / 180.0f;
        
        // Calculate target position
        float targetX = position.x + sin(radYaw) * forward + cos(radYaw) * right;
        float targetZ = position.z - (cos(radYaw) * forward - sin(radYaw) * right);
        
        // Check collision before moving
        if (!hasCollision(targetX, targetZ)) {
            position.x = targetX;
            position.z = targetZ;
        }
    }
    
    // Check collision using scene's collision detection
    bool hasCollision(float targetX, float targetZ) {
        // Use scene collision check if available
        if (sceneCollisionCheck != nullptr) {
            return sceneCollisionCheck(targetX, targetZ, radius);
        }
        return false;
    }
    
    void rotate(float dYaw, float dPitch) {
        yaw += dYaw;
        pitch += dPitch;
        

        // Clamp pitch to prevent camera flipping
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }
    
    void toggleView() {
        isFirstPerson = !isFirstPerson;
    }
    
    void render() {
        if (isFirstPerson) return;
        
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        // Character body uses bodyYaw (updated only when moving forward)
        // Character faces the direction of movement, camera sees the back
        glRotatef(bodyYaw, 0.0f, 1.0f, 0.0f);
        
        // Minecraft-style blocky character with animations
        
        // Calculate animation angles
        float legSwing = 0.0f;
        float armSwing = 0.0f;
        float bodyBounce = 0.0f;
        
        if (isMoving) {
            // Walking animation - swing legs and arms back and forth
            legSwing = sin(walkAnimation * 5.0f) * 30.0f; // Swing legs ±30 degrees
            armSwing = sin(walkAnimation * 5.0f) * 25.0f; // Swing arms ±25 degrees
            bodyBounce = abs(sin(walkAnimation * 10.0f)) * 0.05f; // Slight bounce
        }
        
        float jumpSquash = 0.0f;
        if (isJumping && !isOnGround) {
            // Jump animation - stretch body slightly when in air
            jumpSquash = 0.1f;
        }
        
        // Head (cube) - bobs when walking and tilts with camera pitch
        glColor3f(0.8f, 0.6f, 0.5f); // Skin tone
        glPushMatrix();
        glTranslatef(0.0f, 1.5f + bodyBounce + jumpSquash * 0.5f, 0.0f);
        glRotatef(-pitch, 1.0f, 0.0f, 0.0f); // Head tilts up/down with camera pitch
        glScalef(0.5f, 0.5f, 0.5f);
        glutSolidCube(1.0f);
        
        // Draw face on the front of the head
        glDisable(GL_LIGHTING); // Disable lighting for face features
        
        // Left Eye (on front side, z = 0.51)
        glColor3f(0.0f, 0.0f, 0.0f); // Black
        glPushMatrix();
        glTranslatef(-0.2f, 0.15f, 0.51f); // Left side of face
        glScalef(0.15f, 0.2f, 0.05f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Right Eye (on front side)
        glPushMatrix();
        glTranslatef(0.2f, 0.15f, 0.51f); // Right side of face
        glScalef(0.15f, 0.2f, 0.05f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Mouth (simple rectangle on front side)
        glPushMatrix();
        glTranslatef(0.0f, -0.2f, 0.51f); // Center, below eyes, front of head
        glScalef(0.4f, 0.1f, 0.05f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        glEnable(GL_LIGHTING); // Re-enable lighting
        
        glPopMatrix();
        
        // Body (cube) - bounces when walking, stretches when jumping
        glColor3f(0.2f, 0.2f, 0.8f); // Blue shirt
        glPushMatrix();
        glTranslatef(0.0f, 0.9f + bodyBounce, 0.0f);
        glScalef(0.5f, 0.75f + jumpSquash, 0.25f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Left Arm - swings when walking
        glColor3f(0.2f, 0.2f, 0.8f); // Blue shirt (same as body)
        glPushMatrix();
        glTranslatef(-0.375f, 1.15f + bodyBounce, 0.0f); // Move to shoulder
        glRotatef(-armSwing, 1.0f, 0.0f, 0.0f); // Rotate from shoulder
        glTranslatef(0.0f, -0.25f, 0.0f); // Offset to center of arm
        glScalef(0.25f, 0.75f, 0.25f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Right Arm - swings opposite to left arm
        glPushMatrix();
        glTranslatef(0.375f, 1.15f + bodyBounce, 0.0f); // Move to shoulder
        glRotatef(armSwing, 1.0f, 0.0f, 0.0f); // Rotate from shoulder (opposite direction)
        glTranslatef(0.0f, -0.25f, 0.0f); // Offset to center of arm
        glScalef(0.25f, 0.75f, 0.25f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Left Leg - swings when walking
        glColor3f(0.1f, 0.1f, 0.4f); // Dark blue pants
        glPushMatrix();
        glTranslatef(-0.125f, 0.6f + bodyBounce, 0.0f); // Move to hip
        glRotatef(legSwing, 1.0f, 0.0f, 0.0f); // Rotate from hip
        glTranslatef(0.0f, -0.3f, 0.0f); // Offset to center of leg
        glScalef(0.25f, 0.6f, 0.25f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Right Leg - swings opposite to left leg
        glPushMatrix();
        glTranslatef(0.125f, 0.6f + bodyBounce, 0.0f); // Move to hip
        glRotatef(-legSwing, 1.0f, 0.0f, 0.0f); // Rotate from hip (opposite direction)
        glTranslatef(0.0f, -0.3f, 0.0f); // Offset to center of leg
        glScalef(0.25f, 0.6f, 0.25f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        glPopMatrix();
    }
    
    void getCameraTransform(Vector3& eye, Vector3& center) {
        float radYaw = yaw * M_PI / 180.0f;
        float radPitch = pitch * M_PI / 180.0f;
        
        Vector3 forward;
        forward.x = sin(radYaw) * cos(radPitch);
        forward.y = sin(radPitch);
        forward.z = -cos(radYaw) * cos(radPitch);
        
        if (isFirstPerson) {
            eye = position + Vector3(0.0f, 1.6f, 0.0f); // Eye level
            center = eye + forward;
        } else {
            // Third person camera orbits around player - player always centered
            float distance = 6.0f;
            float playerHeight = 1.0f;  // Height of player center
            
            // Camera orbits around the player based on yaw and pitch
            // Camera is positioned behind and above/below based on pitch
            eye.x = position.x - forward.x * distance;
            eye.y = position.y + playerHeight - forward.y * distance;
            eye.z = position.z - forward.z * distance;
            
            // Always look at the player's center (keeps character centered on screen)
            center.x = position.x;
            center.y = position.y + playerHeight;
            center.z = position.z;
        }
    }
};

Player player;

// Game state
int score = 0;
float lives = 5.0f;  // Float to allow fractional damage (lava does 0.5 per second)
float trapDamageCooldown = 0.0f;  // Cooldown to prevent rapid damage
bool gameRunning = true;
bool hasKey = false;  // Whether player has collected the key

// Chest state
Vector3 chestPosition(15.0f, 0.0f, 15.0f);  // Chest location in Scene 1 - open area
bool chestOpened = false;  // Whether chest has been opened

// Portal state
Vector3 portalPosition(-23.0f, 0.0f, -23.0f);  // Portal location in Scene 1 - near the edge/border
float portalTime = 0.0f;  // For portal animation
float portalCooldown = 0.0f; // Cooldown to prevent instant re-teleport
bool portalOpened = false;  // Whether portal has been opened by player click

// Portal position for Scene 2
Vector3 portalPositionScene2(0.0f, 0.0f, -45.0f);  // Portal location in Scene 2 - at the far wall

// Crystal collection state
int crystalsCollected = 0;  // Number of purple crystals collected (need 10 to win)
bool gameWon = false;  // Whether player has won the game
bool gameWonSoundPlayed = false;  // Track if win sound has been played
bool gameOverSoundPlayed = false;  // Track if game over sound has been played

// Sparkle effect for crystal collection
struct Sparkle {
    Vector3 position;
    float lifetime;
    float velocityY;
    float size;
};
std::vector<Sparkle> sparkles;

// Flame particle for burning effect
struct Flame {
    Vector3 position;
    float lifetime;
    Vector3 velocity;
    float size;
};
std::vector<Flame> flames;
bool isPlayerBurning = false;  // Track if player is currently on lava

// Animation timer
float animationTime = 0.0f;

// ============================================================================
// SCENE CLASS - Base class for all scenes
// ============================================================================

class Scene {
public:
    std::string name;
    float ambientLight[4];
    std::vector<OBJModel*> sceneModels;      // Models specific to this scene
    
    Scene(const std::string& sceneName) : name(sceneName) {
        ambientLight[0] = 0.2f;
        ambientLight[1] = 0.2f;
        ambientLight[2] = 0.3f;
        ambientLight[3] = 1.0f;
    }
    
    virtual ~Scene() {
        // Models are managed by ModelManager, don't delete here
        sceneModels.clear();
    }
    
    virtual void init() = 0;
    virtual void render() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void cleanup() = 0;
    
    // Helper to add a model to the scene
    void addModel(OBJModel* model) {
        if (model) sceneModels.push_back(model);
    }
};

// ============================================================================
// SCENE 1: Crystal Cave Entrance
// ============================================================================

// Forward declaration for collision callback
class Scene1_CaveEntrance;
Scene1_CaveEntrance* scene1Instance = nullptr;
class Scene2_DeepCavern;
Scene2_DeepCavern* scene2Instance = nullptr;

class Scene1_CaveEntrance : public Scene {
private:
    // Scene-specific model pointers for easy access
    OBJModel* caveModel;
    OBJModel* crystalModel;
    OBJModel* entranceRocksModel;
    OBJModel* pigModel;  // Pink pig model
    OBJModel* minecraftTree;  // Minecraft tree model
    GLuint wallTexture;  // Wall texture for border
    OBJModel* wolfModel;  // Wolf Minecraft OBJ model
    GLuint wolfTexture;  // Wolf texture
    OBJModel* cowModel;  // Cow Minecraft OBJ model
    GLuint cowTexture;  // Cow texture
    OBJModel* creeperModel;  // Creeper OBJ model
    GLuint creeperTexture;  // Creeper texture
    Model3DS* flockModel;  // Flock 3DS model
    GLuint grassTexture;  // Floor grass texture
    GLuint flockTexture;  // Flock/bird texture
    
    // Wolf position and AI
    Vector3 wolfPosition;
    float wolfRotation;
    float wolfWanderTime;
    Vector3 wolfTargetPosition;
    float wolfMoveSpeed;
    
    // Cow position and AI
    Vector3 cowPosition;
    float cowRotation;
    float cowWanderTime;
    Vector3 cowTargetPosition;
    float cowMoveSpeed;
    
    // Creeper position and AI (4 creepers total)
    struct CreeperData {
        Vector3 position;
        float rotation;
        float wanderTime;
        Vector3 targetPosition;
        bool alive;
        bool chasing;
        float fuseTime;
        bool exploding;
        float explosionTime;
        Vector3 explosionPosition;
    };
    CreeperData creepers[4];
    float creeperDetectRadius;   // Distance at which creeper detects player
    float creeperExplodeRadius;  // Distance at which creeper explodes
    
    // Flock position (birds flying)
    Vector3 flockPosition;
    float flockRotation;
    float flockTime;  // For animation
    
    // Pig AI variables
    Vector3 pigPosition;
    float pigRotation;
    float pigWanderTime;
    Vector3 pigTargetPosition;
    float pigMoveSpeed;
    
    // Minecraft tree instances for the forest
    struct MinecraftTreeInstance {
        float x, z;
        float scale;
        float yOffset;  // Height offset based on scale
    };
    std::vector<MinecraftTreeInstance> minecraftTrees;
    
    // Boulder instances
    struct BoulderInstance {
        float x, y, z;
        float scale;
        float rotationY;
    };
    std::vector<BoulderInstance> boulders;
    GLuint stoneTexture;  // Stone texture for boulders
    
    // Flower instances for forest floor
    struct Flower {
        float x, z;
        float scale;
        int colorType;  // 0=red, 1=yellow, 2=blue, 3=white, 4=pink, 5=purple
        float swayPhase;
    };
    std::vector<Flower> flowers;
    
public:
    Scene1_CaveEntrance() : Scene("Enchanted Forest"), 
                            caveModel(nullptr), entranceRocksModel(nullptr), 
                            pigModel(nullptr), minecraftTree(nullptr),
                            wolfModel(nullptr), wolfTexture(0), cowModel(nullptr), cowTexture(0),
                            creeperModel(nullptr), creeperTexture(0), flockModel(nullptr),
                            grassTexture(0), stoneTexture(0), flockTexture(0), wallTexture(0),
                            wolfPosition(-10.0f, 0.0f, 10.0f), wolfRotation(0.0f),
                            wolfWanderTime(0.0f), wolfTargetPosition(-10.0f, 0.0f, 10.0f), wolfMoveSpeed(0.03f),
                            cowPosition(-15.0f, 0.0f, -15.0f), cowRotation(0.0f),
                            cowWanderTime(0.0f), cowTargetPosition(-15.0f, 0.0f, -15.0f), cowMoveSpeed(0.02f),
                            creeperDetectRadius(15.0f), creeperExplodeRadius(2.0f),
                            flockPosition(0.0f, 15.0f, 0.0f), flockRotation(0.0f), flockTime(0.0f),
                            pigPosition(0.0f, 0.0f, -5.0f), pigRotation(0.0f),
                            pigWanderTime(0.0f), pigTargetPosition(0.0f, 0.0f, -5.0f), pigMoveSpeed(0.02f) {
        // Initialize 4 creepers at different positions
        creepers[0] = {Vector3(15.0f, 0.0f, -10.0f), 0.0f, 0.0f, Vector3(15.0f, 0.0f, -10.0f), true, false, 0.0f, false, 0.0f, Vector3(0.0f, 0.0f, 0.0f)};
        creepers[1] = {Vector3(-20.0f, 0.0f, 15.0f), 0.0f, 0.0f, Vector3(-20.0f, 0.0f, 15.0f), true, false, 0.0f, false, 0.0f, Vector3(0.0f, 0.0f, 0.0f)};
        creepers[2] = {Vector3(20.0f, 0.0f, 20.0f), 0.0f, 0.0f, Vector3(20.0f, 0.0f, 20.0f), true, false, 0.0f, false, 0.0f, Vector3(0.0f, 0.0f, 0.0f)};
        creepers[3] = {Vector3(-10.0f, 0.0f, -20.0f), 0.0f, 0.0f, Vector3(-10.0f, 0.0f, -20.0f), true, false, 0.0f, false, 0.0f, Vector3(0.0f, 0.0f, 0.0f)};
        // Bright outdoor daytime lighting
        ambientLight[0] = 0.5f;
        ambientLight[1] = 0.6f;
        ambientLight[2] = 0.65f;
    }
    
    void init() override {
        std::cout << "Initializing Scene 1: " << name << std::endl;
        
        // =====================================================
        // LOAD YOUR 3D MODELS HERE
        // =====================================================
        // Example: Load models from the "models" folder
        // Uncomment and modify paths when you have the models:
        
        // caveModel = modelManager.loadModel("cave_entrance", "models/cave_entrance.obj");
        // if (caveModel) {
        //     caveModel->setPosition(0, 0, 0);
        //     caveModel->setUniformScale(1.0f);
        //     addModel(caveModel);
        // }
        
        // Load the pink pig model
        pigModel = modelManager.loadModel("pink_pig", "models/16433_Pig.obj");
        if (pigModel) {
            std::cout << "Pig model loaded successfully!" << std::endl;
        } else {
            std::cout << "Failed to load pig model!" << std::endl;
        }
        
        // Load the Minecraft tree model
        minecraftTree = modelManager.loadModel("minecraft_tree", "models/Minecraft Tree.obj");
        if (minecraftTree) {
            std::cout << "Minecraft tree loaded successfully!" << std::endl;
            minecraftTree->setPosition(-5.0f, 3.85f, -5.0f);  // Raised to put base on ground (lowest Y is -425 * 0.009 = -3.83)
            minecraftTree->setUniformScale(0.009f);  // User requested scale
            addModel(minecraftTree);
        } else {
            std::cout << "Failed to load Minecraft tree!" << std::endl;
        }
        
        // Load wall texture for the border walls
        wallTexture = loadTexture("models/hedge2.jpeg");
        if (wallTexture) {
            std::cout << "Wall texture loaded successfully!" << std::endl;
        } else {
            std::cout << "Failed to load wall texture!" << std::endl;
        }
        
        // Load grass texture for the floor
        grassTexture = loadTexture("models/herbe 2.jpg");
        if (grassTexture) {
            std::cout << "Grass texture loaded successfully!" << std::endl;
        }
        
        // Load stone texture for boulders
        stoneTexture = loadTexture("models/minecraft_stone.jpg");
        if (stoneTexture) {
            std::cout << "Stone texture loaded successfully!" << std::endl;
        }
        
        // Load wolf model and texture (replaces dog)
        wolfModel = new OBJModel();
        if (wolfModel->load("models/wolf_minecraft.obj")) {
            std::cout << "Wolf model loaded successfully!" << std::endl;
        } else {
            std::cout << "Failed to load wolf model!" << std::endl;
            delete wolfModel;
            wolfModel = nullptr;
        }
        wolfTexture = loadTexture("models/HD_wolf.png");
        if (wolfTexture) {
            std::cout << "Wolf texture loaded successfully!" << std::endl;
        }
        
        // Load cow model and texture
        cowModel = new OBJModel();
        if (cowModel->load("models/Cow Minecraft.obj")) {
            std::cout << "Cow model loaded successfully!" << std::endl;
        } else {
            std::cout << "Failed to load cow model!" << std::endl;
            delete cowModel;
            cowModel = nullptr;
        }
        cowTexture = loadTexture("models/cow2.png");
        if (cowTexture) {
            std::cout << "Cow texture loaded successfully!" << std::endl;
        }
        
        // Load Creeper model and texture
        creeperModel = new OBJModel();
        if (creeperModel->load("models/Creeper.obj")) {
            std::cout << "Creeper model loaded successfully!" << std::endl;
        } else {
            std::cout << "Failed to load Creeper model!" << std::endl;
            delete creeperModel;
            creeperModel = nullptr;
        }
        creeperTexture = loadTexture("models/creeper.png");
        if (creeperTexture) {
            std::cout << "Creeper texture loaded successfully!" << std::endl;
        }
        
        // Load flock texture and model
        flockTexture = loadTexture("models/swallowt.jpg");
        if (flockTexture) {
            std::cout << "Flock texture loaded successfully!" << std::endl;
        }
        flockModel = new Model3DS();
        if (flockModel->load("models/Flock N190413.3ds")) {
            std::cout << "Flock model loaded successfully!" << std::endl;
            flockModel->setPosition(flockPosition.x, flockPosition.y, flockPosition.z);
            flockModel->setUniformScale(0.01f);
        } else {
            std::cout << "Failed to load flock model!" << std::endl;
            delete flockModel;
            flockModel = nullptr;
        }
        
        // Note: Creeper.fbx cannot be loaded (FBX format not supported)
        // Convert to OBJ format using Blender to use it
        
        // Generate forest trees
        generateForest();
        
        // Generate boulders
        generateBoulders();
        
        // Set up collision callback for this scene
        scene1Instance = this;
        
        std::cout << "Scene 1 initialized" << std::endl;
    }
    
    void render() override {
        // Set scene-specific lighting for forest
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientLight);
        
        // Setup sunlight - positioned at the sun's location (30, 35, -80)
        GLfloat lightPos[] = { 30.0f, 35.0f, -80.0f, 1.0f }; // Sun position
        GLfloat lightDiffuse[] = { 1.0f, 1.0f, 0.95f, 1.0f };  // Bright white sunlight
        GLfloat lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
        
        // Draw sky dome (simple gradient effect using a large sphere)
        drawSky();
        
        // Draw grass ground with texture
        glPushMatrix();
        glDisable(GL_LIGHTING);  // Disable lighting for flat texture
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, grassTexture);
        glColor3f(1.0f, 1.0f, 1.0f);  // White to show texture at full brightness
        glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(-50.0f, 0.0f, -50.0f);
            glTexCoord2f(0.0f, 50.0f); glVertex3f(-50.0f, 0.0f, 50.0f);
            glTexCoord2f(50.0f, 50.0f); glVertex3f(50.0f, 0.0f, 50.0f);
            glTexCoord2f(50.0f, 0.0f); glVertex3f(50.0f, 0.0f, -50.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_LIGHTING);  // Re-enable lighting for other objects
        glPopMatrix();
        
        // Render border walls
        renderBorderWalls();
        
        // Render boulders
        renderBoulders();
        
        // Render flowers on the forest floor
        renderFlowers();
        
        // Render all Minecraft tree instances using display list
        if (minecraftTree && minecraftTree->hasDisplayList) {
            for (const auto& treeInst : minecraftTrees) {
                glPushMatrix();
                // Position and scale the tree
                glTranslatef(treeInst.x, treeInst.yOffset, treeInst.z);
                glScalef(treeInst.scale, treeInst.scale, treeInst.scale);
                
                // Use display list for performance
                glCallList(minecraftTree->displayList);
                
                glPopMatrix();
            }
            glDisable(GL_TEXTURE_2D);
        }
        
        // Render all loaded OBJ models (excluding trees, we handle them separately)
        for (auto* model : sceneModels) {
            if (model && model != minecraftTree) model->render();
        }
        
        // Render the pink pig
        if (pigModel) {
            glPushMatrix();
            
            // Disable textures for the pig - it should be solid pink
            glDisable(GL_TEXTURE_2D);
            
            glTranslatef(pigPosition.x, pigPosition.y, pigPosition.z);
            glRotatef(pigRotation, 0.0f, 1.0f, 0.0f);  // Rotate to face direction of movement
            glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
            glRotatef(180.0f, 0.0f, 0.0f, 1.0f);
            glScalef(0.03f, 0.03f, 0.03f);  // Half player size
            
            // Set pink material
            GLfloat pinkDiffuse[] = { 1.0f, 0.6f, 0.7f, 1.0f };
            GLfloat pinkAmbient[] = { 0.4f, 0.2f, 0.25f, 1.0f };
            GLfloat pinkSpecular[] = { 0.5f, 0.4f, 0.4f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, pinkDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, pinkAmbient);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, pinkSpecular);
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 30.0f);
            glColor3f(1.0f, 0.6f, 0.7f);
            
            pigModel->render();
            glPopMatrix();
        }
        
        // Render the wolf (dog) - with HD_wolf.png texture
        if (wolfModel) {
            glPushMatrix();
            
            // Position wolf on the ground - rotate to stand upright
            float wolfScale = 0.025f;  // Half player size
            float wolfYOffset = 0.4f;  // Raise slightly above ground
            glTranslatef(wolfPosition.x, wolfYOffset, wolfPosition.z);
            glRotatef(wolfRotation, 0.0f, 1.0f, 0.0f);
            glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);  // Rotate to stand upright
            glScalef(wolfScale, wolfScale, wolfScale);
            
            // Enable texture and bind wolf texture
            glEnable(GL_TEXTURE_2D);
            if (wolfTexture) {
                glBindTexture(GL_TEXTURE_2D, wolfTexture);
            }
            
            // White material to allow texture colors to show
            GLfloat wolfDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            GLfloat wolfAmbient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, wolfDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, wolfAmbient);
            glColor3f(1.0f, 1.0f, 1.0f);
            
            wolfModel->renderWithTexture();
            
            // Disable texture after rendering
            glDisable(GL_TEXTURE_2D);
            
            glPopMatrix();
        }
        
        // Render the cow - with Cow Minecraft.jpg texture
        if (cowModel) {
            glPushMatrix();
            
            // Position cow on the ground - rotate to stand upright
            float cowScale = 0.04f;
            float cowYOffset = 0.4f;  // Raise slightly above ground
            glTranslatef(cowPosition.x, cowYOffset, cowPosition.z);
            glRotatef(cowRotation, 0.0f, 1.0f, 0.0f);
            glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);  // Rotate to stand upright
            glScalef(cowScale, cowScale, cowScale);
            
            // Enable texture and bind cow texture
            glEnable(GL_TEXTURE_2D);
            if (cowTexture) {
                glBindTexture(GL_TEXTURE_2D, cowTexture);
            }
            
            // White material to allow texture colors to show
            GLfloat cowDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            GLfloat cowAmbient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, cowDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, cowAmbient);
            glColor3f(1.0f, 1.0f, 1.0f);
            
            cowModel->renderWithTexture();
            
            // Disable texture after rendering
            glDisable(GL_TEXTURE_2D);
            
            glPopMatrix();
        }
        
        // Render all Creepers (alive ones)
        // Render creepers using original model - dark green with black eyes
        if (creeperModel) {
            for (int i = 0; i < 4; i++) {
                if (!creepers[i].alive) continue;
                glPushMatrix();
                
                // Position Creeper on the ground
                float creeperScale = 0.008f;
                float creeperYOffset = 0.8f;
                glTranslatef(creepers[i].position.x, creeperYOffset, creepers[i].position.z);
                glRotatef(creepers[i].rotation, 0.0f, 1.0f, 0.0f);
                glScalef(creeperScale, creeperScale, creeperScale);
                
                glDisable(GL_TEXTURE_2D);
                
                // Flash when about to explode
                float flashIntensity = 0.0f;
                if (creepers[i].chasing && creepers[i].fuseTime > 0.0f) {
                    flashIntensity = (sin(creepers[i].fuseTime * 15.0f) + 1.0f) * 0.5f;
                }
                
                // Dark green color for creeper body
                float greenR = 0.1f + flashIntensity * 0.9f;
                float greenG = 0.5f + flashIntensity * 0.5f;
                float greenB = 0.1f + flashIntensity * 0.9f;
                GLfloat creeperDiffuse[] = { greenR, greenG, greenB, 1.0f };
                GLfloat creeperAmbient[] = { greenR * 0.5f, greenG * 0.5f, greenB * 0.5f, 1.0f };
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, creeperDiffuse);
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, creeperAmbient);
                glColor3f(greenR, greenG, greenB);
                
                creeperModel->render();
                
                // Draw creeper face - model coords: X=side(±33), Y=up(-260 to 115), Z=front/back(±160)
                // Head is at Y=50 to 115, face should be on +Z side (around Z=34)
                GLfloat blackDiffuse[] = { 0.0f, 0.0f, 0.0f, 1.0f };
                GLfloat blackAmbient[] = { 0.0f, 0.0f, 0.0f, 1.0f };
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, blackDiffuse);
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, blackAmbient);
                glColor3f(0.0f, 0.0f, 0.0f);
                
                // Left eye (square) - on head front face
                glPushMatrix();
                glTranslatef(-12.0f, 90.0f, 34.0f);  // X=left, Y=up on head, Z=front
                glScalef(8.0f, 12.0f, 2.0f);
                glutSolidCube(1.0f);
                glPopMatrix();
                
                // Right eye (square)
                glPushMatrix();
                glTranslatef(12.0f, 90.0f, 34.0f);
                glScalef(8.0f, 12.0f, 2.0f);
                glutSolidCube(1.0f);
                glPopMatrix();
                
                // Mouth (sad frown shape - 3 parts)
                glPushMatrix();
                glTranslatef(0.0f, 65.0f, 34.0f);
                glScalef(16.0f, 5.0f, 2.0f);
                glutSolidCube(1.0f);
                glPopMatrix();
                
                // Left corner of frown
                glPushMatrix();
                glTranslatef(-12.0f, 72.0f, 34.0f);
                glScalef(5.0f, 5.0f, 2.0f);
                glutSolidCube(1.0f);
                glPopMatrix();
                
                // Right corner of frown
                glPushMatrix();
                glTranslatef(12.0f, 72.0f, 34.0f);
                glScalef(5.0f, 5.0f, 2.0f);
                glutSolidCube(1.0f);
                glPopMatrix();
                
                glPopMatrix();
            }
        }

        // Render explosion animations for creepers that exploded
        for (int i = 0; i < 4; i++) {
            if (creepers[i].exploding) {
                renderExplosion(creepers[i].explosionPosition, creepers[i].explosionTime);
            }
        }
        
        // Render the flock (birds flying high in the sky) - 3x bigger
        if (flockModel) {
            glPushMatrix();
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, flockTexture);
            
            // Flock circles high in the sky
            glTranslatef(flockPosition.x, flockPosition.y, flockPosition.z);
            glRotatef(flockRotation, 0.0f, 1.0f, 0.0f);
            glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
            glScalef(0.045f, 0.045f, 0.045f);  // 3x bigger (0.015 * 3 = 0.045)
            
            GLfloat birdDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            GLfloat birdAmbient[] = { 0.7f, 0.7f, 0.7f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, birdDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, birdAmbient);
            glColor3f(1.0f, 1.0f, 1.0f);
            
            flockModel->render();
            glDisable(GL_TEXTURE_2D);
            glPopMatrix();
        }
        
        // Draw the treasure chest
        drawChest();
        
        // Draw the portal
        drawPortal();
        
        // Draw scene label
        drawSceneLabel();
    }
    
    void update(float deltaTime) override {
        // Update portal animation
        portalTime += deltaTime;
        
        // Update pig AI - wander randomly
        updatePigAI(deltaTime);
        
        // Update flock - circle around the scene
        flockTime += deltaTime;
        flockRotation += 0.5f;  // Rotate slowly
        if (flockRotation > 360.0f) flockRotation -= 360.0f;
        
        // Flock flies in a circle high in the sky
        float radius = 25.0f;
        flockPosition.x = radius * cos(flockTime * 0.3f);
        flockPosition.z = radius * sin(flockTime * 0.3f);
        flockPosition.y = 25.0f + 3.0f * sin(flockTime * 0.5f);  // High in sky with up/down motion
        
        // Update wolf - wander around randomly
        updateWolfAI(deltaTime);
        
        // Update cow - wander around randomly
        updateCowAI(deltaTime);
        
        // Update Creeper - wander around randomly
        updateCreeperAI(deltaTime);
        
        // Update explosion animations for all creepers
        for (int i = 0; i < 4; i++) {
            if (creepers[i].exploding) {
                creepers[i].explosionTime += deltaTime;
                if (creepers[i].explosionTime > 2.0f) {
                    creepers[i].exploding = false;  // End explosion after 2 seconds
                }
            }
        }
    }
    
    void cleanup() override {
        std::cout << "Cleaning up Scene 1" << std::endl;
        minecraftTrees.clear();
        boulders.clear();
        if (wallTexture) {
            glDeleteTextures(1, &wallTexture);
            wallTexture = 0;
        }
        if (wolfModel) {
            delete wolfModel;
            wolfModel = nullptr;
        }
        if (wolfTexture) {
            glDeleteTextures(1, &wolfTexture);
            wolfTexture = 0;
        }
        if (cowModel) {
            delete cowModel;
            cowModel = nullptr;
        }
        if (cowTexture) {
            glDeleteTextures(1, &cowTexture);
            cowTexture = 0;
        }
        if (creeperModel) {
            delete creeperModel;
            creeperModel = nullptr;
        }
        if (creeperTexture) {
            glDeleteTextures(1, &creeperTexture);
            creeperTexture = 0;
        }
        if (flockModel) {
            delete flockModel;
            flockModel = nullptr;
        }
    }
    
private:
    void generateForest() {
        minecraftTrees.clear();
        
        // Define tree positions spread throughout the scene with varying scales for different heights
        // Format: {x, z, scale}
        float treeData[][3] = {
            // Near the center clearing (smaller trees)
            {-8.0f, -8.0f, 0.007f},
            {8.0f, -6.0f, 0.008f},
            {-6.0f, 7.0f, 0.009f},
            {7.0f, 9.0f, 0.0075f},
            
            // Middle ring (medium trees)
            {-12.0f, 0.0f, 0.010f},
            {12.0f, -3.0f, 0.009f},
            {0.0f, -14.0f, 0.011f},
            {-3.0f, 13.0f, 0.0085f},
            {14.0f, 5.0f, 0.010f},
            {-14.0f, -6.0f, 0.0095f},
            
            // Outer area (taller trees)
            {-18.0f, -15.0f, 0.012f},
            {18.0f, -12.0f, 0.011f},
            {-15.0f, 18.0f, 0.013f},
            {16.0f, 16.0f, 0.0105f},
            {-20.0f, 5.0f, 0.012f},
            {20.0f, 0.0f, 0.0115f},
            
            // Far corners (largest trees)
            {-22.0f, -22.0f, 0.014f},
            {22.0f, -20.0f, 0.013f},
            {-20.0f, 22.0f, 0.0125f},
            {23.0f, 21.0f, 0.014f},
        };
        
        int numTrees = sizeof(treeData) / sizeof(treeData[0]);
        
        // Base Y offset calculation: lowest vertex is -425.757576
        // So yOffset = 425.757576 * scale to put base on ground
        const float baseVertexY = 425.757576f;
        
        for (int i = 0; i < numTrees; i++) {
            MinecraftTreeInstance tree;
            tree.x = treeData[i][0];
            tree.z = treeData[i][1];
            tree.scale = treeData[i][2];
            tree.yOffset = baseVertexY * tree.scale;  // Adjust Y to put base on ground
            
            minecraftTrees.push_back(tree);
        }
        
        std::cout << "Generated " << minecraftTrees.size() << " Minecraft trees for the forest" << std::endl;
    }
    
    void renderBorderWalls() {
        // Border walls - 4 simple textured quads at the edges of the floor
        const float borderDistance = 50.0f;  // Match the floor size (-50 to 50)
        const float wallHeight = 5.0f;       // Height of walls
        const float wallLength = 100.0f;     // Full length of wall (2 * borderDistance)
        
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, wallTexture);
        
        // Disable back-face culling so walls are visible from both sides
        glDisable(GL_CULL_FACE);
        
        glColor3f(1.0f, 1.0f, 1.0f);  // White to show true texture colors
        
        // Calculate texture repeat based on wall length
        float texRepeatX = wallLength / 4.0f;  // Repeat texture every 4 units
        float texRepeatY = wallHeight / 2.0f;  // Repeat texture every 2 units in height
        
        glBegin(GL_QUADS);
        
        // North wall (positive Z) - facing inward (toward -Z)
        glNormal3f(0.0f, 0.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f);      glVertex3f(-borderDistance, 0.0f, borderDistance);
        glTexCoord2f(texRepeatX, 0.0f); glVertex3f(borderDistance, 0.0f, borderDistance);
        glTexCoord2f(texRepeatX, texRepeatY); glVertex3f(borderDistance, wallHeight, borderDistance);
        glTexCoord2f(0.0f, texRepeatY); glVertex3f(-borderDistance, wallHeight, borderDistance);
        
        // South wall (negative Z) - facing inward (toward +Z)
        glNormal3f(0.0f, 0.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f);      glVertex3f(-borderDistance, 0.0f, -borderDistance);
        glTexCoord2f(texRepeatX, 0.0f); glVertex3f(borderDistance, 0.0f, -borderDistance);
        glTexCoord2f(texRepeatX, texRepeatY); glVertex3f(borderDistance, wallHeight, -borderDistance);
        glTexCoord2f(0.0f, texRepeatY); glVertex3f(-borderDistance, wallHeight, -borderDistance);
        
        // East wall (positive X) - facing inward (toward -X)
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f);      glVertex3f(borderDistance, 0.0f, -borderDistance);
        glTexCoord2f(texRepeatX, 0.0f); glVertex3f(borderDistance, 0.0f, borderDistance);
        glTexCoord2f(texRepeatX, texRepeatY); glVertex3f(borderDistance, wallHeight, borderDistance);
        glTexCoord2f(0.0f, texRepeatY); glVertex3f(borderDistance, wallHeight, -borderDistance);
        
        // West wall (negative X) - facing inward (toward +X)
        glNormal3f(1.0f, 0.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f);      glVertex3f(-borderDistance, 0.0f, -borderDistance);
        glTexCoord2f(texRepeatX, 0.0f); glVertex3f(-borderDistance, 0.0f, borderDistance);
        glTexCoord2f(texRepeatX, texRepeatY); glVertex3f(-borderDistance, wallHeight, borderDistance);
        glTexCoord2f(0.0f, texRepeatY); glVertex3f(-borderDistance, wallHeight, -borderDistance);
        
        glEnd();
        
        glEnable(GL_CULL_FACE);  // Re-enable culling
        glDisable(GL_TEXTURE_2D);
    }
    
    void generateBoulders() {
        boulders.clear();
        
        // Create boulder positions scattered around the scene
        // Avoid the center area where player spawns
        float boulderData[][4] = {
            // {x, z, scale, rotation}
            {-15.0f, -10.0f, 0.8f, 45.0f},
            {12.0f, -15.0f, 1.2f, 120.0f},
            {-20.0f, 5.0f, 0.6f, 200.0f},
            {18.0f, 8.0f, 1.0f, 75.0f},
            {-8.0f, 18.0f, 0.9f, 30.0f},
            {5.0f, -20.0f, 1.1f, 160.0f},
            {-22.0f, -18.0f, 0.7f, 90.0f},
            {20.0f, -22.0f, 1.3f, 15.0f},
            {-25.0f, 15.0f, 0.5f, 270.0f},
            {25.0f, 20.0f, 0.8f, 180.0f},
            {-10.0f, -22.0f, 1.0f, 60.0f},
            {15.0f, 25.0f, 0.9f, 135.0f},
            {-5.0f, 12.0f, 0.6f, 220.0f},
            {8.0f, -8.0f, 0.7f, 300.0f},
            {-18.0f, -5.0f, 1.1f, 45.0f},
        };
        
        int numBoulders = sizeof(boulderData) / sizeof(boulderData[0]);
        
        for (int i = 0; i < numBoulders; i++) {
            BoulderInstance b;
            b.x = boulderData[i][0];
            b.z = boulderData[i][1];
            b.scale = boulderData[i][2];
            b.y = b.scale * 0.3f;  // Slightly sink into ground based on size
            b.rotationY = boulderData[i][3];
            boulders.push_back(b);
        }
        
        std::cout << "Generated " << boulders.size() << " boulders" << std::endl;
        
        // Generate flowers scattered across the forest floor
        srand(11111);  // Fixed seed for consistent flower placement
        for (int i = 0; i < 80; i++) {  // 80 flowers
            Flower f;
            f.x = -45.0f + (rand() % 9000) / 100.0f;  // -45 to 45
            f.z = -45.0f + (rand() % 9000) / 100.0f;  // -45 to 45
            f.scale = 0.15f + (rand() % 15) / 100.0f;  // 0.15 to 0.30
            f.colorType = rand() % 6;  // 0-5 for different colors
            f.swayPhase = (rand() % 628) / 100.0f;  // Random phase for swaying
            
            // Don't place flowers too close to center (player spawn) or trees
            float distFromCenter = sqrt(f.x * f.x + f.z * f.z);
            if (distFromCenter < 3.0f) continue;
            
            // Check distance from trees
            bool tooCloseToTree = false;
            for (const auto& tree : minecraftTrees) {
                float dx = f.x - tree.x;
                float dz = f.z - tree.z;
                if (sqrt(dx*dx + dz*dz) < 2.0f) {
                    tooCloseToTree = true;
                    break;
                }
            }
            if (tooCloseToTree) continue;
            
            flowers.push_back(f);
        }
        
        std::cout << "Generated " << flowers.size() << " flowers" << std::endl;
    }
    
    void renderExplosion(const Vector3& explosionPosition, float explosionTime) {
        // Render explosion animation with expanding particles
        glPushMatrix();
        glTranslatef(explosionPosition.x, 1.0f, explosionPosition.z);
        
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for fire effect
        
        // Explosion parameters based on time
        float progress = explosionTime / 2.0f;  // 0 to 1 over 2 seconds
        float alpha = 1.0f - progress;  // Fade out
        float size = 1.0f + progress * 8.0f;  // Expand from 1 to 9 units
        
        // Draw expanding fireball core
        int numParticles = 30;
        for (int i = 0; i < numParticles; i++) {
            float angle = (float)i / numParticles * M_PI * 2.0f;
            float vertAngle = ((float)(i % 10) / 10.0f - 0.5f) * M_PI;
            
            float particleX = cos(angle) * cos(vertAngle) * size * (0.5f + 0.5f * sin(explosionTime * 10 + i));
            float particleY = sin(vertAngle) * size * 0.7f + progress * 2.0f;  // Rise up
            float particleZ = sin(angle) * cos(vertAngle) * size * (0.5f + 0.5f * cos(explosionTime * 8 + i));
            
            float particleSize = 0.3f + 0.5f * (1.0f - progress);
            
            // Color gradient from white/yellow to orange to red
            float r = 1.0f;
            float g = 0.8f - progress * 0.6f;
            float b = 0.2f - progress * 0.2f;
            
            glColor4f(r, g, b, alpha * 0.8f);
            
            glPushMatrix();
            glTranslatef(particleX, particleY, particleZ);
            
            // Draw particle as a small sphere approximation (cube for simplicity)
            glBegin(GL_QUADS);
            // Front
            glVertex3f(-particleSize, -particleSize, particleSize);
            glVertex3f(particleSize, -particleSize, particleSize);
            glVertex3f(particleSize, particleSize, particleSize);
            glVertex3f(-particleSize, particleSize, particleSize);
            // Back
            glVertex3f(-particleSize, -particleSize, -particleSize);
            glVertex3f(-particleSize, particleSize, -particleSize);
            glVertex3f(particleSize, particleSize, -particleSize);
            glVertex3f(particleSize, -particleSize, -particleSize);
            // Top
            glVertex3f(-particleSize, particleSize, -particleSize);
            glVertex3f(-particleSize, particleSize, particleSize);
            glVertex3f(particleSize, particleSize, particleSize);
            glVertex3f(particleSize, particleSize, -particleSize);
            // Bottom
            glVertex3f(-particleSize, -particleSize, -particleSize);
            glVertex3f(particleSize, -particleSize, -particleSize);
            glVertex3f(particleSize, -particleSize, particleSize);
            glVertex3f(-particleSize, -particleSize, particleSize);
            glEnd();
            
            glPopMatrix();
        }
        
        // Draw central flash (bright white sphere that fades quickly)
        if (explosionTime < 0.5f) {
            float flashAlpha = (0.5f - explosionTime) * 2.0f;
            float flashSize = 0.5f + explosionTime * 4.0f;
            glColor4f(1.0f, 1.0f, 0.9f, flashAlpha);
            
            // Simple flash sphere
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(0.0f, 0.0f, 0.0f);
            for (int i = 0; i <= 16; i++) {
                float a = (float)i / 16.0f * M_PI * 2.0f;
                glVertex3f(cos(a) * flashSize, sin(a) * flashSize, 0.0f);
            }
            glEnd();
        }
        
        // Draw smoke particles (darker, rise slower, last longer)
        if (explosionTime > 0.3f) {
            float smokeProgress = (explosionTime - 0.3f) / 1.7f;
            int numSmoke = 15;
            for (int i = 0; i < numSmoke; i++) {
                float angle = (float)i / numSmoke * M_PI * 2.0f + explosionTime;
                float smokeX = cos(angle) * size * 0.4f;
                float smokeY = smokeProgress * 5.0f + sin(i) * 0.5f;
                float smokeZ = sin(angle) * size * 0.4f;
                
                float smokeSize = 0.5f + smokeProgress * 0.3f;
                float smokeAlpha = (1.0f - smokeProgress) * 0.5f;
                
                glColor4f(0.3f, 0.3f, 0.3f, smokeAlpha);
                
                glPushMatrix();
                glTranslatef(smokeX, smokeY, smokeZ);
                glBegin(GL_QUADS);
                glVertex3f(-smokeSize, -smokeSize, 0);
                glVertex3f(smokeSize, -smokeSize, 0);
                glVertex3f(smokeSize, smokeSize, 0);
                glVertex3f(-smokeSize, smokeSize, 0);
                glEnd();
                glPopMatrix();
            }
        }
        
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
    
    void renderBoulders() {
        if (stoneTexture == 0) return;
        
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, stoneTexture);
        
        // Set stone material
        GLfloat stoneDiffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
        GLfloat stoneAmbient[] = { 0.4f, 0.4f, 0.4f, 1.0f };
        GLfloat stoneSpecular[] = { 0.2f, 0.2f, 0.2f, 1.0f };
        glMaterialfv(GL_FRONT, GL_DIFFUSE, stoneDiffuse);
        glMaterialfv(GL_FRONT, GL_AMBIENT, stoneAmbient);
        glMaterialfv(GL_FRONT, GL_SPECULAR, stoneSpecular);
        glMaterialf(GL_FRONT, GL_SHININESS, 10.0f);
        glColor3f(0.8f, 0.8f, 0.8f);
        
        for (const auto& boulder : boulders) {
            glPushMatrix();
            glTranslatef(boulder.x, boulder.y, boulder.z);
            glRotatef(boulder.rotationY, 0.0f, 1.0f, 0.0f);
            glScalef(boulder.scale, boulder.scale * 0.7f, boulder.scale);  // Flatten slightly
            
            // Draw textured sphere as boulder
            GLUquadric* quad = gluNewQuadric();
            gluQuadricTexture(quad, GL_TRUE);
            gluQuadricNormals(quad, GLU_SMOOTH);
            gluSphere(quad, 1.0f, 16, 12);
            gluDeleteQuadric(quad);
            
            glPopMatrix();
        }
        
        glDisable(GL_TEXTURE_2D);
    }
    
    void renderFlowers() {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_LIGHTING);
        
        for (const auto& flower : flowers) {
            glPushMatrix();
            
            // Slight swaying animation
            float sway = sin(animationTime * 2.0f + flower.swayPhase) * 3.0f;
            
            glTranslatef(flower.x, 0.0f, flower.z);
            glRotatef(sway, 0.0f, 0.0f, 1.0f);
            glScalef(flower.scale, flower.scale, flower.scale);
            
            // Draw stem (green)
            GLfloat stemDiffuse[] = { 0.2f, 0.6f, 0.1f, 1.0f };
            GLfloat stemAmbient[] = { 0.1f, 0.3f, 0.05f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, stemDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, stemAmbient);
            glColor3f(0.2f, 0.6f, 0.1f);
            
            glPushMatrix();
            glTranslatef(0.0f, 0.5f, 0.0f);
            glScalef(0.1f, 1.0f, 0.1f);
            glutSolidCube(1.0f);
            glPopMatrix();
            
            // Draw leaves on stem
            glPushMatrix();
            glTranslatef(0.08f, 0.3f, 0.0f);
            glRotatef(30.0f, 0.0f, 0.0f, 1.0f);
            glScalef(0.3f, 0.15f, 0.08f);
            glutSolidCube(1.0f);
            glPopMatrix();
            
            glPushMatrix();
            glTranslatef(-0.08f, 0.5f, 0.0f);
            glRotatef(-30.0f, 0.0f, 0.0f, 1.0f);
            glScalef(0.3f, 0.15f, 0.08f);
            glutSolidCube(1.0f);
            glPopMatrix();
            
            // Draw flower petals based on color type
            float r, g, b;
            switch (flower.colorType) {
                case 0: r = 1.0f; g = 0.2f; b = 0.2f; break;  // Red
                case 1: r = 1.0f; g = 0.9f; b = 0.2f; break;  // Yellow
                case 2: r = 0.3f; g = 0.4f; b = 0.9f; break;  // Blue
                case 3: r = 1.0f; g = 1.0f; b = 1.0f; break;  // White
                case 4: r = 1.0f; g = 0.5f; b = 0.7f; break;  // Pink
                default: r = 0.7f; g = 0.3f; b = 0.9f; break; // Purple
            }
            
            GLfloat petalDiffuse[] = { r, g, b, 1.0f };
            GLfloat petalAmbient[] = { r * 0.4f, g * 0.4f, b * 0.4f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, petalDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, petalAmbient);
            glColor3f(r, g, b);
            
            // Draw 5 petals around center
            for (int p = 0; p < 5; p++) {
                glPushMatrix();
                glTranslatef(0.0f, 1.0f, 0.0f);
                glRotatef(p * 72.0f, 0.0f, 1.0f, 0.0f);
                glTranslatef(0.2f, 0.0f, 0.0f);
                glScalef(0.25f, 0.08f, 0.15f);
                glutSolidSphere(1.0f, 6, 4);
                glPopMatrix();
            }
            
            // Draw flower center (yellow/orange)
            GLfloat centerDiffuse[] = { 1.0f, 0.8f, 0.2f, 1.0f };
            GLfloat centerAmbient[] = { 0.5f, 0.4f, 0.1f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, centerDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, centerAmbient);
            glColor3f(1.0f, 0.8f, 0.2f);
            
            glPushMatrix();
            glTranslatef(0.0f, 1.0f, 0.0f);
            glutSolidSphere(0.12f, 8, 6);
            glPopMatrix();
            
            glPopMatrix();
        }
    }
    
    void drawSky() {
        glPushMatrix();
        glDisable(GL_LIGHTING);
        
        // Draw a simple sky gradient using a large quad behind everything
        glBegin(GL_QUADS);
        // Bottom of sky (light blue near horizon)
        glColor3f(0.7f, 0.85f, 0.95f);
        glVertex3f(-100.0f, 0.0f, -100.0f);
        glVertex3f(100.0f, 0.0f, -100.0f);
        // Top of sky (bright sky blue)
        glColor3f(0.53f, 0.81f, 0.92f);
        glVertex3f(100.0f, 50.0f, -100.0f);
        glVertex3f(-100.0f, 50.0f, -100.0f);
        glEnd();
        
        // Draw a bright sun (square shape) - this is the light source
        glPushMatrix();
        glTranslatef(30.0f, 35.0f, -80.0f);
        
        // Draw sun glow/halo
        glColor4f(1.0f, 0.95f, 0.7f, 0.3f);
        float glowSize = 8.0f;
        glBegin(GL_QUADS);
        glVertex3f(-glowSize, -glowSize, 0.0f);
        glVertex3f(glowSize, -glowSize, 0.0f);
        glVertex3f(glowSize, glowSize, 0.0f);
        glVertex3f(-glowSize, glowSize, 0.0f);
        glEnd();
        
        // Draw bright sun core
        glColor3f(1.0f, 1.0f, 0.9f);
        float sunSize = 5.0f;
        glBegin(GL_QUADS);
        glVertex3f(-sunSize, -sunSize, 0.0f);
        glVertex3f(sunSize, -sunSize, 0.0f);
        glVertex3f(sunSize, sunSize, 0.0f);
        glVertex3f(-sunSize, sunSize, 0.0f);
        glEnd();
        
        glPopMatrix();
        
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
    
    void drawChest() {
        glPushMatrix();
        glTranslatef(chestPosition.x, chestPosition.y, chestPosition.z);
        
        // Rotate slightly for visual interest
        glRotatef(25.0f, 0.0f, 1.0f, 0.0f);
        
        glDisable(GL_TEXTURE_2D);
        
        // Chest dimensions
        float chestWidth = 1.2f;
        float chestHeight = 0.8f;
        float chestDepth = 0.8f;
        
        // Base (bottom box) - brown wood
        GLfloat woodDiffuse[] = { 0.55f, 0.35f, 0.15f, 1.0f };
        GLfloat woodAmbient[] = { 0.25f, 0.15f, 0.05f, 1.0f };
        GLfloat woodSpecular[] = { 0.2f, 0.15f, 0.1f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, woodDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, woodAmbient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, woodSpecular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 10.0f);
        glColor3f(0.55f, 0.35f, 0.15f);
        
        glPushMatrix();
        glTranslatef(0.0f, chestHeight * 0.4f, 0.0f);
        glScalef(chestWidth, chestHeight * 0.8f, chestDepth);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Lid - slightly different shade if opened
        if (chestOpened) {
            // Open lid - rotated back
            GLfloat openDiffuse[] = { 0.45f, 0.28f, 0.12f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, openDiffuse);
            glColor3f(0.45f, 0.28f, 0.12f);
            
            glPushMatrix();
            glTranslatef(0.0f, chestHeight * 0.8f, -chestDepth * 0.4f);
            glRotatef(-110.0f, 1.0f, 0.0f, 0.0f);  // Lid open
            glTranslatef(0.0f, 0.0f, chestDepth * 0.2f);
            glScalef(chestWidth * 1.02f, 0.15f, chestDepth);
            glutSolidCube(1.0f);
            glPopMatrix();
            
            // Gold inside the chest (visible when open)
            GLfloat goldDiffuse[] = { 1.0f, 0.84f, 0.0f, 1.0f };
            GLfloat goldAmbient[] = { 0.4f, 0.35f, 0.0f, 1.0f };
            GLfloat goldSpecular[] = { 1.0f, 0.95f, 0.7f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, goldDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, goldAmbient);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, goldSpecular);
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 80.0f);
            glColor3f(1.0f, 0.84f, 0.0f);
            
            glPushMatrix();
            glTranslatef(0.0f, chestHeight * 0.5f, 0.0f);
            glScalef(chestWidth * 0.7f, chestHeight * 0.3f, chestDepth * 0.6f);
            glutSolidCube(1.0f);
            glPopMatrix();
        } else {
            // Closed lid
            glPushMatrix();
            glTranslatef(0.0f, chestHeight * 0.85f, 0.0f);
            glScalef(chestWidth * 1.02f, 0.15f, chestDepth * 1.02f);
            glutSolidCube(1.0f);
            glPopMatrix();
        }
        
        // Metal bands - gold trim
        GLfloat metalDiffuse[] = { 0.83f, 0.69f, 0.22f, 1.0f };
        GLfloat metalAmbient[] = { 0.4f, 0.33f, 0.1f, 1.0f };
        GLfloat metalSpecular[] = { 1.0f, 0.9f, 0.5f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, metalDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, metalAmbient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, metalSpecular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 60.0f);
        glColor3f(0.83f, 0.69f, 0.22f);
        
        // Front band
        glPushMatrix();
        glTranslatef(0.0f, chestHeight * 0.4f, chestDepth * 0.51f);
        glScalef(chestWidth * 1.05f, 0.08f, 0.05f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Lock (if not opened)
        if (!chestOpened) {
            glPushMatrix();
            glTranslatef(0.0f, chestHeight * 0.75f, chestDepth * 0.52f);
            glutSolidSphere(0.1f, 8, 8);
            glPopMatrix();
        }
        
        glPopMatrix();
    }
    
    void drawPortal() {
        glPushMatrix();
        glTranslatef(portalPosition.x, 0.0f, portalPosition.z);
        
        float portalWidth = 2.0f;
        float portalHeight = 3.0f;
        float portalDepth = 0.2f;
        
        // Enable blending for transparency
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_TEXTURE_2D);
        
        // Draw portal frame edges (always visible)
        GLfloat frameDiffuse[] = { 0.3f, 0.15f, 0.4f, 1.0f };  // Dark purple
        GLfloat frameAmbient[] = { 0.15f, 0.1f, 0.2f, 1.0f };
        GLfloat frameSpecular[] = { 0.5f, 0.3f, 0.6f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, frameDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, frameAmbient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, frameSpecular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 50.0f);
        glColor4f(0.3f, 0.15f, 0.4f, 1.0f);
        
        float frameThickness = 0.15f;
        
        // Left edge
        glPushMatrix();
        glTranslatef(-portalWidth/2.0f, portalHeight/2.0f, 0.0f);
        glScalef(frameThickness, portalHeight, frameThickness);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Right edge
        glPushMatrix();
        glTranslatef(portalWidth/2.0f, portalHeight/2.0f, 0.0f);
        glScalef(frameThickness, portalHeight, frameThickness);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Top edge
        glPushMatrix();
        glTranslatef(0.0f, portalHeight, 0.0f);
        glScalef(portalWidth + frameThickness * 2, frameThickness, frameThickness);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Bottom edge
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 0.0f);
        glScalef(portalWidth + frameThickness * 2, frameThickness, frameThickness);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Draw portal interior
        if (portalOpened) {
            // Portal is opened - dark purple glowing effect
            float glowPulse = 0.5f + 0.3f * sin(portalTime * 3.0f);  // Pulsing glow
            
            GLfloat portalDiffuse[] = { 0.3f * glowPulse, 0.1f * glowPulse, 0.4f * glowPulse, 0.9f };
            GLfloat portalAmbient[] = { 0.2f * glowPulse, 0.05f * glowPulse, 0.25f * glowPulse, 0.9f };
            GLfloat portalEmission[] = { 0.25f * glowPulse, 0.1f * glowPulse, 0.35f * glowPulse, 1.0f };
            
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, portalDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, portalAmbient);
            glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, portalEmission);
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 100.0f);
            glColor4f(0.3f * glowPulse, 0.1f * glowPulse, 0.4f * glowPulse, 0.9f);
            
            // Draw semi-transparent purple portal surface
            glPushMatrix();
            glTranslatef(0.0f, portalHeight/2.0f, 0.0f);
            glBegin(GL_QUADS);
            glVertex3f(-portalWidth/2.0f, -portalHeight/2.0f, 0.0f);
            glVertex3f(portalWidth/2.0f, -portalHeight/2.0f, 0.0f);
            glVertex3f(portalWidth/2.0f, portalHeight/2.0f, 0.0f);
            glVertex3f(-portalWidth/2.0f, portalHeight/2.0f, 0.0f);
            glEnd();
            glPopMatrix();
            
            // Add swirling particle effect
            for (int i = 0; i < 20; i++) {
                float angle = (portalTime * 2.0f + i * 18.0f) * 3.14159f / 180.0f;
                float radius = 0.3f + 0.5f * (i / 20.0f);
                float height = (portalHeight * 0.9f) * (i / 20.0f);
                
                glPushMatrix();
                glTranslatef(
                    radius * cos(angle),
                    height + 0.1f,
                    radius * sin(angle) * 0.1f
                );
                
                float particleGlow = 0.6f + 0.4f * sin(portalTime * 4.0f + i);
                glColor4f(0.4f * particleGlow, 0.15f * particleGlow, 0.5f * particleGlow, 0.9f);
                glutSolidSphere(0.05f, 6, 6);
                glPopMatrix();
            }
            
            // Reset emission
            GLfloat noEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
        } else {
            // Portal is closed - fully transparent center, only edges visible
            // No need to draw anything for the center when portal is not opened
        }
        
        glDisable(GL_BLEND);
        glPopMatrix();
    }
    

    
    void drawSceneLabel() {
        // Display scene name (for debugging)
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos3f(0.0f, 5.0f, 0.0f);
    }
    
    void updatePigAI(float deltaTime) {
        pigWanderTime += deltaTime;
        
        // Pick a new random target every 5-7 seconds
        if (pigWanderTime > 5.0f + (rand() % 20) / 10.0f) {
            pigWanderTime = 0.0f;
            // Pick random position within bounds
            pigTargetPosition.x = -20.0f + (rand() % 400) / 10.0f;
            pigTargetPosition.z = -20.0f + (rand() % 400) / 10.0f;
        }
        
        // Move towards random target
        float dx = pigTargetPosition.x - pigPosition.x;
        float dz = pigTargetPosition.z - pigPosition.z;
        float distance = sqrt(dx * dx + dz * dz);
        
        if (distance > 0.5f) {
            // Normalize direction
            float dirX = dx / distance;
            float dirZ = dz / distance;
            
            // Move pig towards target
            pigPosition.x += dirX * pigMoveSpeed;
            pigPosition.z += dirZ * pigMoveSpeed;
            
            // Calculate rotation to face movement direction
            pigRotation = atan2(dx, -dz) * 180.0f / M_PI;
        }
    }
    
    void updateWolfAI(float deltaTime) {
        wolfWanderTime += deltaTime;
        
        // Pick a new random target every 3-5 seconds
        if (wolfWanderTime > 3.0f + (rand() % 20) / 10.0f) {
            wolfWanderTime = 0.0f;
            // Pick random position within bounds (avoid edges)
            wolfTargetPosition.x = -15.0f + (rand() % 300) / 10.0f;
            wolfTargetPosition.z = -15.0f + (rand() % 300) / 10.0f;
        }
        
        // Move towards target
        float dx = wolfTargetPosition.x - wolfPosition.x;
        float dz = wolfTargetPosition.z - wolfPosition.z;
        float distance = sqrt(dx * dx + dz * dz);
        
        if (distance > 0.5f) {
            // Normalize direction
            float dirX = dx / distance;
            float dirZ = dz / distance;
            
            // Move wolf towards target
            wolfPosition.x += dirX * wolfMoveSpeed;
            wolfPosition.z += dirZ * wolfMoveSpeed;
            
            // Calculate rotation to face movement direction
            wolfRotation = atan2(dx, -dz) * 180.0f / M_PI;
        }
    }
    
    void updateCowAI(float deltaTime) {
        cowWanderTime += deltaTime;
        
        // Pick a new random target every 5-8 seconds (slower than wolf)
        if (cowWanderTime > 5.0f + (rand() % 30) / 10.0f) {
            cowWanderTime = 0.0f;
            // Pick random position within bounds (avoid edges)
            cowTargetPosition.x = -20.0f + (rand() % 400) / 10.0f;
            cowTargetPosition.z = -20.0f + (rand() % 400) / 10.0f;
        }
        
        // Move towards target
        float dx = cowTargetPosition.x - cowPosition.x;
        float dz = cowTargetPosition.z - cowPosition.z;
        float distance = sqrt(dx * dx + dz * dz);
        
        if (distance > 0.5f) {
            // Normalize direction
            float dirX = dx / distance;
            float dirZ = dz / distance;
            
            // Move cow towards target (slower)
            cowPosition.x += dirX * cowMoveSpeed;
            cowPosition.z += dirZ * cowMoveSpeed;
            
            // Calculate rotation to face movement direction
            cowRotation = atan2(dx, -dz) * 180.0f / M_PI;
        }
    }
    
    void updateCreeperAI(float deltaTime) {
        // Update all 4 creepers
        for (int i = 0; i < 4; i++) {
            CreeperData& creeper = creepers[i];
            
            // If creeper is dead, don't update
            if (!creeper.alive) continue;
            
            // Calculate distance to player
            float playerDx = player.position.x - creeper.position.x;
            float playerDz = player.position.z - creeper.position.z;
            float playerDistance = sqrt(playerDx * playerDx + playerDz * playerDz);
            
            // Check if player is within detection radius
            if (playerDistance < creeperDetectRadius) {
                creeper.chasing = true;
            }
            
            if (creeper.chasing) {
                // Chase the player!
                float creeperChaseSpeed = 0.04f;  // Faster when chasing
                
                if (playerDistance > 0.1f) {
                    // Normalize direction toward player
                    float dirX = playerDx / playerDistance;
                    float dirZ = playerDz / playerDistance;
                    
                    // Move toward player
                    creeper.position.x += dirX * creeperChaseSpeed;
                    creeper.position.z += dirZ * creeperChaseSpeed;
                    
                    // Face the player
                    creeper.rotation = atan2(playerDx, -playerDz) * 180.0f / M_PI;
                }
                
                // Check if close enough to explode
                if (playerDistance < creeperExplodeRadius) {
                    creeper.fuseTime += deltaTime;
                    
                    // Explode after 1.5 seconds of being close
                    if (creeper.fuseTime >= 1.5f) {
                        // BOOM! Creeper explodes - start explosion animation
                        creeper.explosionPosition = creeper.position;
                        creeper.exploding = true;
                        creeper.explosionTime = 0.0f;
                        creeper.alive = false;
                        lives -= 4.0f;  // Lose 4 lives
                        if (lives < 0) lives = 0;
                        
                        // Check if player died and play appropriate sound
                        if (lives <= 0 && !gameOverSoundPlayed) {
                            playExplosionThenGameOverSound();  // Play explosion then game over sound
                            gameOverSoundPlayed = true;
                            std::cout << "CREEPER " << (i+1) << " EXPLOSION! GAME OVER!" << std::endl;
                        } else {
                            playExplosionThenDamageSound();  // Play explosion then damage sound
                            std::cout << "CREEPER " << (i+1) << " EXPLOSION! Lost 4 lives. Remaining: " << lives << std::endl;
                        }
                    }
                } else {
                    // Reset fuse if player moves away
                    creeper.fuseTime = 0.0f;
                }
            } else {
                // Normal wandering behavior when not chasing
                creeper.wanderTime += deltaTime;
                
                // Pick a new random target every 4-6 seconds
                if (creeper.wanderTime > 4.0f + (rand() % 20) / 10.0f) {
                    creeper.wanderTime = 0.0f;
                    creeper.targetPosition.x = -20.0f + (rand() % 400) / 10.0f;
                    creeper.targetPosition.z = -20.0f + (rand() % 400) / 10.0f;
                }
                
                // Move towards random target
                float dx = creeper.targetPosition.x - creeper.position.x;
                float dz = creeper.targetPosition.z - creeper.position.z;
                float distance = sqrt(dx * dx + dz * dz);
                
                if (distance > 0.5f) {
                    float dirX = dx / distance;
                    float dirZ = dz / distance;
                    
                    float creeperMoveSpeed = 0.02f;
                    creeper.position.x += dirX * creeperMoveSpeed;
                    creeper.position.z += dirZ * creeperMoveSpeed;
                    
                    creeper.rotation = atan2(dx, -dz) * 180.0f / M_PI;
                }
            }
        }
    }
    
public:
    // Check if a position collides with scene objects
    bool checkSceneCollision(float x, float z, float radius) {
        // Check collision with trees
        for (const auto& tree : minecraftTrees) {
            float dx = x - tree.x;
            float dz = z - tree.z;
            float dist = sqrt(dx * dx + dz * dz);
            float treeRadius = 1.0f;  // Tree trunk radius
            if (dist < radius + treeRadius) return true;
        }
        
        // Check collision with border walls (at floor edge)
        float borderLimit = 49.0f;  // At the floor edge (50 - player radius)
        if (fabs(x) > borderLimit || fabs(z) > borderLimit) return true;
        
        // Check collision with boulders
        for (const auto& boulder : boulders) {
            float dx = x - boulder.x;
            float dz = z - boulder.z;
            float dist = sqrt(dx * dx + dz * dz);
            float boulderRadius = boulder.scale * 0.8f;
            if (dist < radius + boulderRadius) return true;
        }
        
        // Check collision with pig
        {
            float dx = x - pigPosition.x;
            float dz = z - pigPosition.z;
            float dist = sqrt(dx * dx + dz * dz);
            if (dist < radius + 1.5f) return true;  // Pig collision radius
        }
        
        // Check collision with wolf
        {
            float dx = x - wolfPosition.x;
            float dz = z - wolfPosition.z;
            float dist = sqrt(dx * dx + dz * dz);
            if (dist < radius + 0.5f) return true;  // Wolf collision radius
        }
        
        // Check collision with cow
        {
            float dx = x - cowPosition.x;
            float dz = z - cowPosition.z;
            float dist = sqrt(dx * dx + dz * dz);
            if (dist < radius + 1.0f) return true;  // Cow collision radius
        }
        
        return false;
    }
};

// Static collision check function for Scene1
bool scene1CollisionCheck(float x, float z, float radius) {
    if (scene1Instance) {
        return scene1Instance->checkSceneCollision(x, z, radius);
    }
    return false;
}

// Forward declaration for Scene2 collision - defined after Scene2 class
bool scene2CollisionCheck(float x, float z, float radius);
int lastScene2CollisionType = 0;  // 0=none, 1=stone, 2=trap, 3=wall

// ============================================================================
// SCENE 2: Deep Crystal Cavern
// ============================================================================

class Scene2_DeepCavern : public Scene {
private:
    GLuint stoneTexture;  // Stone texture for walls/floor/ceiling
    GLuint lavaTexture;   // Lava texture for lava pools
    
    // Room dimensions - same as Scene 1 (100x100)
    float roomWidth = 100.0f;
    float roomHeight = 15.0f;
    float roomDepth = 100.0f;
    
    // Lava pool data
    struct LavaPool {
        float x, z;        // Center position
        float size;        // Size of the square
        float depth;       // Depth of lava pit
    };
    std::vector<LavaPool> lavaPools;
    
    // Torch data
    struct Torch {
        Vector3 position;
        float flickerPhase;
        float flickerSpeed;
        float intensity;
    };
    std::vector<Torch> torches;
    
    // Stones and traps
    OBJModel* stonesModel = nullptr;
    OBJModel* trapModel = nullptr;
    
    struct Stone {
        Vector3 position;
        float rotation;
        float scale;
    };
    std::vector<Stone> stones;
    
    struct Trap {
        Vector3 position;
        float rotation;
        float collisionRadius;
    };
    std::vector<Trap> traps;

public:
    float lavaDamageTimer;  // Timer for lava damage (public for timer access)
    
    // Purple crystals (collectibles) - public for timer access
    struct Crystal {
        Vector3 position;
        float rotation;
        float bobPhase;
        bool collected;
    };
    std::vector<Crystal> crystals;
    
    // Flying bats (harmless, atmospheric)
    struct Bat {
        Vector3 position;       // Current position
        Vector3 targetPos;      // Where bat is flying to
        float wingAngle;        // Wing flapping animation
        float wingSpeed;        // How fast wings flap
        float flySpeed;         // Movement speed
        float size;             // Scale of the bat
    };
    std::vector<Bat> bats;

    Scene2_DeepCavern() : Scene("Dark Stone Dungeon"), stoneTexture(0), lavaTexture(0), lavaDamageTimer(0.0f) {
        // Very dark ambient for dungeon atmosphere
        ambientLight[0] = 0.05f;
        ambientLight[1] = 0.04f;
        ambientLight[2] = 0.03f;
        scene2Instance = this;  // Set global instance for collision callback
    }
    
    // Check if player is in a lava pool
    bool checkLavaCollision(float x, float z, float radius) {
        for (const auto& lava : lavaPools) {
            float halfSize = lava.size / 2.0f;
            if (x > lava.x - halfSize && x < lava.x + halfSize &&
                z > lava.z - halfSize && z < lava.z + halfSize) {
                return true;
            }
        }
        return false;
    }
    
    // Get lava depth at position
    float getLavaDepth(float x, float z) {
        for (const auto& lava : lavaPools) {
            float halfSize = lava.size / 2.0f;
            if (x > lava.x - halfSize && x < lava.x + halfSize &&
                z > lava.z - halfSize && z < lava.z + halfSize) {
                return lava.depth;
            }
        }
        return 0.0f;
    }
    
    // Collision check for stones only (blocks movement)
    // Returns: 0 = no collision, 1 = stone collision, 3 = wall collision
    int checkSceneCollision(float x, float z, float radius) {
        float hw = roomWidth / 2.0f - radius;
        float hd = roomDepth / 2.0f - radius;
        
        // Wall collision
        if (x < -hw || x > hw || z < -hd || z > hd) {
            return 3;
        }
        
        // Stone collision (blocks movement)
        // Large stones (scale >= 6) cannot be jumped over
        // Smaller stones can be jumped over
        for (const auto& stone : stones) {
            float dx = x - stone.position.x;
            float dz = z - stone.position.z;
            float dist = sqrt(dx*dx + dz*dz);
            float collisionRadius = radius + stone.scale * 0.6f;
            
            if (dist < collisionRadius) {
                // Large stones always block (can't jump over)
                if (stone.scale >= 6.0f) {
                    return 1;
                }
                // Smaller stones only block when on ground
                if (player.position.y <= player.groundLevel + 0.1f) {
                    return 1;
                }
            }
        }
        
        // Traps do NOT block movement - player can walk through them
        return 0;
    }
    
    // Check if player is touching a trap (for damage, doesn't block movement)
    bool checkTrapCollision(float x, float z, float radius) {
        for (const auto& trap : traps) {
            float dx = x - trap.position.x;
            float dz = z - trap.position.z;
            float dist = sqrt(dx*dx + dz*dz);
            if (dist < radius + trap.collisionRadius) {
                return true;  // Touching a trap!
            }
        }
        return false;
    }
    
    void init() override {
        std::cout << "Initializing Scene 2: " << name << std::endl;
        
        // Load stone texture
        stoneTexture = loadTexture("models/minecraft_stone.jpg");
        if (stoneTexture) {
            std::cout << "Stone texture loaded for dungeon!" << std::endl;
        }
        
        // Load stones and trap models
        stonesModel = new OBJModel();
        if (stonesModel->load("models/stones.obj")) {
            std::cout << "Stones model loaded!" << std::endl;
        }
        
        trapModel = new OBJModel();
        if (trapModel->load("models/trap.obj")) {
            std::cout << "Trap model loaded!" << std::endl;
        }
        
        // Place stones around the dungeon - scaled for 100x100 room
        stones.push_back({Vector3(-30.0f, 0.0f, -30.0f), 45.0f, 5.0f});   // big boulder
        stones.push_back({Vector3(25.0f, 0.0f, -20.0f), 120.0f, 3.0f});   // medium
        stones.push_back({Vector3(-20.0f, 0.0f, 15.0f), 200.0f, 2.0f});   // normal
        stones.push_back({Vector3(35.0f, 0.0f, 25.0f), 75.0f, 5.0f});     // big boulder
        stones.push_back({Vector3(-12.0f, 0.0f, -38.0f), 300.0f, 3.0f});  // medium
        stones.push_back({Vector3(15.0f, 0.0f, 30.0f), 160.0f, 2.0f});    // normal
        stones.push_back({Vector3(-38.0f, 0.0f, 0.0f), 30.0f, 3.0f});     // medium
        stones.push_back({Vector3(0.0f, 0.0f, -25.0f), 90.0f, 5.0f});     // big boulder
        stones.push_back({Vector3(40.0f, 0.0f, -35.0f), 45.0f, 4.0f});    // big boulder
        stones.push_back({Vector3(-35.0f, 0.0f, 35.0f), 180.0f, 4.0f});   // big boulder
        stones.push_back({Vector3(20.0f, 0.0f, -40.0f), 270.0f, 3.0f});   // medium
        stones.push_back({Vector3(-40.0f, 0.0f, -20.0f), 135.0f, 3.0f});  // medium
        
        // Large stone structures along the edges - cave walls feel
        // North edge (negative Z)
        stones.push_back({Vector3(-45.0f, 0.0f, -46.0f), 15.0f, 8.0f});   // corner
        stones.push_back({Vector3(-35.0f, 0.0f, -47.0f), 45.0f, 7.0f});
        stones.push_back({Vector3(-22.0f, 0.0f, -46.0f), 90.0f, 9.0f});
        stones.push_back({Vector3(-8.0f, 0.0f, -47.0f), 120.0f, 7.0f});
        stones.push_back({Vector3(8.0f, 0.0f, -46.0f), 180.0f, 8.0f});
        stones.push_back({Vector3(22.0f, 0.0f, -47.0f), 210.0f, 7.0f});
        stones.push_back({Vector3(35.0f, 0.0f, -46.0f), 270.0f, 9.0f});
        stones.push_back({Vector3(45.0f, 0.0f, -47.0f), 315.0f, 8.0f});   // corner
        
        // South edge (positive Z)
        stones.push_back({Vector3(-45.0f, 0.0f, 46.0f), 30.0f, 8.0f});    // corner
        stones.push_back({Vector3(-32.0f, 0.0f, 47.0f), 75.0f, 7.0f});
        stones.push_back({Vector3(-18.0f, 0.0f, 46.0f), 135.0f, 9.0f});
        stones.push_back({Vector3(-5.0f, 0.0f, 47.0f), 160.0f, 7.0f});
        stones.push_back({Vector3(10.0f, 0.0f, 46.0f), 200.0f, 8.0f});
        stones.push_back({Vector3(25.0f, 0.0f, 47.0f), 240.0f, 7.0f});
        stones.push_back({Vector3(38.0f, 0.0f, 46.0f), 290.0f, 9.0f});
        stones.push_back({Vector3(46.0f, 0.0f, 47.0f), 330.0f, 8.0f});    // corner
        
        // West edge (negative X)
        stones.push_back({Vector3(-47.0f, 0.0f, -35.0f), 60.0f, 7.0f});
        stones.push_back({Vector3(-46.0f, 0.0f, -20.0f), 100.0f, 9.0f});
        stones.push_back({Vector3(-47.0f, 0.0f, -5.0f), 150.0f, 7.0f});
        stones.push_back({Vector3(-46.0f, 0.0f, 10.0f), 190.0f, 8.0f});
        stones.push_back({Vector3(-47.0f, 0.0f, 25.0f), 230.0f, 7.0f});
        stones.push_back({Vector3(-46.0f, 0.0f, 38.0f), 280.0f, 9.0f});
        
        // East edge (positive X)
        stones.push_back({Vector3(47.0f, 0.0f, -38.0f), 40.0f, 7.0f});
        stones.push_back({Vector3(46.0f, 0.0f, -22.0f), 85.0f, 9.0f});
        stones.push_back({Vector3(47.0f, 0.0f, -8.0f), 130.0f, 7.0f});
        stones.push_back({Vector3(46.0f, 0.0f, 8.0f), 175.0f, 8.0f});
        stones.push_back({Vector3(47.0f, 0.0f, 22.0f), 220.0f, 7.0f});
        stones.push_back({Vector3(46.0f, 0.0f, 35.0f), 265.0f, 9.0f});
        
        // Place traps in the dungeon (dangerous!) - scaled for 100x100 room
        traps.push_back({Vector3(-15.0f, 0.0f, -15.0f), 0.0f, 2.0f});
        traps.push_back({Vector3(20.0f, 0.0f, 10.0f), 45.0f, 2.0f});
        traps.push_back({Vector3(-25.0f, 0.0f, 25.0f), 90.0f, 2.0f});
        traps.push_back({Vector3(30.0f, 0.0f, -25.0f), 135.0f, 2.0f});
        traps.push_back({Vector3(0.0f, 0.0f, 20.0f), 180.0f, 2.0f});
        traps.push_back({Vector3(-30.0f, 0.0f, -5.0f), 225.0f, 2.0f});
        traps.push_back({Vector3(35.0f, 0.0f, 35.0f), 270.0f, 2.0f});
        traps.push_back({Vector3(-10.0f, 0.0f, 40.0f), 315.0f, 2.0f});
        
        // Place 10 purple crystals scattered around the dungeon (collectibles for winning)
        crystals.push_back({Vector3(-35.0f, 1.5f, -35.0f), 0.0f, 0.0f, false});
        crystals.push_back({Vector3(30.0f, 1.5f, -30.0f), 45.0f, 1.0f, false});
        crystals.push_back({Vector3(-25.0f, 1.5f, 20.0f), 90.0f, 2.0f, false});
        crystals.push_back({Vector3(35.0f, 1.5f, 15.0f), 135.0f, 3.0f, false});
        crystals.push_back({Vector3(-15.0f, 1.5f, 35.0f), 180.0f, 4.0f, false});
        crystals.push_back({Vector3(25.0f, 1.5f, 35.0f), 225.0f, 5.0f, false});
        crystals.push_back({Vector3(10.0f, 1.5f, -35.0f), 270.0f, 0.5f, false});
        crystals.push_back({Vector3(-40.0f, 1.5f, 10.0f), 315.0f, 1.5f, false});
        crystals.push_back({Vector3(40.0f, 1.5f, -10.0f), 60.0f, 2.5f, false});
        crystals.push_back({Vector3(5.0f, 1.5f, 25.0f), 150.0f, 3.5f, false});
        
        // Load lava texture
        lavaTexture = loadTexture("models/lava.jpeg");
        if (lavaTexture) {
            std::cout << "Lava texture loaded!" << std::endl;
        }
        
        // Generate random lava pools in the dungeon floor - scaled for 100x100 room
        srand(12345);  // Fixed seed for consistent layout
        float lavaDepth = 0.5f;  // Half player height (player height is 1.0f)
        for (int i = 0; i < 15; i++) {  // 15 lava pools for larger room
            bool validPosition = false;
            float lx, lz;
            float lavaSize = 2.0f + (rand() % 150) / 100.0f;  // 2.0 to 3.5 units
            
            // Try to find a valid position (not overlapping stones or traps)
            for (int attempts = 0; attempts < 50 && !validPosition; attempts++) {
                lx = -40.0f + (rand() % 8000) / 100.0f;  // -40 to 40
                lz = -40.0f + (rand() % 8000) / 100.0f;  // -40 to 40
                
                validPosition = true;
                
                // Check distance from stones
                for (const auto& stone : stones) {
                    float dx = lx - stone.position.x;
                    float dz = lz - stone.position.z;
                    if (sqrt(dx*dx + dz*dz) < stone.scale * 2.0f + lavaSize) {
                        validPosition = false;
                        break;
                    }
                }
                
                // Check distance from traps
                if (validPosition) {
                    for (const auto& trap : traps) {
                        float dx = lx - trap.position.x;
                        float dz = lz - trap.position.z;
                        if (sqrt(dx*dx + dz*dz) < trap.collisionRadius + lavaSize) {
                            validPosition = false;
                            break;
                        }
                    }
                }
                
                // Check distance from other lava pools
                if (validPosition) {
                    for (const auto& lava : lavaPools) {
                        float dx = lx - lava.x;
                        float dz = lz - lava.z;
                        if (sqrt(dx*dx + dz*dz) < lava.size + lavaSize) {
                            validPosition = false;
                            break;
                        }
                    }
                }
                
                // Don't spawn too close to spawn point (0, 0)
                if (validPosition && sqrt(lx*lx + lz*lz) < 3.0f) {
                    validPosition = false;
                }
            }
            
            if (validPosition) {
                lavaPools.push_back({lx, lz, lavaSize, lavaDepth});
                std::cout << "Lava pool at (" << lx << ", " << lz << ") size: " << lavaSize << std::endl;
            }
        }
        
        // Create torches on the walls - more torches for larger room
        float torchHeight = 5.0f;
        float halfWidth = roomWidth / 2.0f - 0.5f;
        float halfDepth = roomDepth / 2.0f - 0.5f;
        
        // Torches on north wall (negative Z) - 4 torches
        torches.push_back({Vector3(-35.0f, torchHeight, -halfDepth), 0.0f, 3.5f, 1.0f});
        torches.push_back({Vector3(-12.0f, torchHeight, -halfDepth), 1.5f, 4.0f, 1.0f});
        torches.push_back({Vector3(12.0f, torchHeight, -halfDepth), 0.8f, 3.8f, 1.0f});
        torches.push_back({Vector3(35.0f, torchHeight, -halfDepth), 2.2f, 4.2f, 1.0f});
        
        // Torches on south wall (positive Z) - 4 torches
        torches.push_back({Vector3(-35.0f, torchHeight, halfDepth), 0.7f, 3.8f, 1.0f});
        torches.push_back({Vector3(-12.0f, torchHeight, halfDepth), 2.1f, 3.2f, 1.0f});
        torches.push_back({Vector3(12.0f, torchHeight, halfDepth), 1.3f, 4.0f, 1.0f});
        torches.push_back({Vector3(35.0f, torchHeight, halfDepth), 0.5f, 3.6f, 1.0f});
        
        // Torches on west wall (negative X) - 4 torches
        torches.push_back({Vector3(-halfWidth, torchHeight, -35.0f), 1.2f, 4.2f, 1.0f});
        torches.push_back({Vector3(-halfWidth, torchHeight, -12.0f), 0.3f, 3.6f, 1.0f});
        torches.push_back({Vector3(-halfWidth, torchHeight, 12.0f), 1.9f, 3.9f, 1.0f});
        torches.push_back({Vector3(-halfWidth, torchHeight, 35.0f), 2.6f, 4.4f, 1.0f});
        
        // Torches on east wall (positive X) - 4 torches
        torches.push_back({Vector3(halfWidth, torchHeight, -35.0f), 1.8f, 3.4f, 1.0f});
        torches.push_back({Vector3(halfWidth, torchHeight, -12.0f), 2.5f, 4.5f, 1.0f});
        torches.push_back({Vector3(halfWidth, torchHeight, 12.0f), 0.9f, 3.7f, 1.0f});
        torches.push_back({Vector3(halfWidth, torchHeight, 35.0f), 1.6f, 4.1f, 1.0f});
        
        // Initialize flying bats
        srand(54321);  // Fixed seed for consistent bat positions
        for (int i = 0; i < 12; i++) {  // 12 bats flying around
            Bat bat;
            // Random starting position - fly throughout the cave
            bat.position.x = -35.0f + (rand() % 7000) / 100.0f;  // -35 to 35
            bat.position.y = 4.0f + (rand() % 800) / 100.0f;     // 4 to 12 (visible height)
            bat.position.z = -35.0f + (rand() % 7000) / 100.0f;  // -35 to 35
            
            // Random target position
            bat.targetPos.x = -35.0f + (rand() % 7000) / 100.0f;
            bat.targetPos.y = 4.0f + (rand() % 800) / 100.0f;
            bat.targetPos.z = -35.0f + (rand() % 7000) / 100.0f;
            
            bat.wingAngle = (rand() % 628) / 100.0f;  // Random starting wing phase
            bat.wingSpeed = 15.0f + (rand() % 500) / 100.0f;  // 15-20 flaps per second
            bat.flySpeed = 3.0f + (rand() % 300) / 100.0f;    // 3-6 units per second
            bat.size = 0.8f + (rand() % 40) / 100.0f;         // 0.8 to 1.2 scale (much bigger!)
            
            bats.push_back(bat);
        }
        
        std::cout << "Scene 2 initialized with " << torches.size() << " torches, " 
                  << stones.size() << " stones, " << traps.size() << " traps, and " 
                  << bats.size() << " bats" << std::endl;
    }
    
    void render() override {
        // Set very dark ambient lighting
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientLight);
        
        // Set up torch lights with flickering effect
        int lightIndex = 0;
        for (size_t i = 0; i < torches.size() && lightIndex < 8; i++) {
            GLenum light = GL_LIGHT0 + lightIndex;
            glEnable(light);
            
            GLfloat lightPos[] = { torches[i].position.x, torches[i].position.y, torches[i].position.z, 1.0f };
            
            // Flickering orange/yellow light
            float flicker = torches[i].intensity;
            GLfloat lightDiffuse[] = { 1.0f * flicker, 0.6f * flicker, 0.2f * flicker, 1.0f };
            GLfloat lightAmbient[] = { 0.1f * flicker, 0.05f * flicker, 0.02f * flicker, 1.0f };
            GLfloat lightSpecular[] = { 0.5f * flicker, 0.3f * flicker, 0.1f * flicker, 1.0f };
            
            // Attenuation for realistic light falloff
            glLightf(light, GL_CONSTANT_ATTENUATION, 0.5f);
            glLightf(light, GL_LINEAR_ATTENUATION, 0.05f);
            glLightf(light, GL_QUADRATIC_ATTENUATION, 0.01f);
            
            glLightfv(light, GL_POSITION, lightPos);
            glLightfv(light, GL_DIFFUSE, lightDiffuse);
            glLightfv(light, GL_AMBIENT, lightAmbient);
            glLightfv(light, GL_SPECULAR, lightSpecular);
            
            lightIndex++;
        }
        
        // Enable texturing
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, stoneTexture);
        
        // Set material for stone surfaces
        GLfloat stoneDiffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
        GLfloat stoneAmbient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
        GLfloat stoneSpecular[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, stoneDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, stoneAmbient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, stoneSpecular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 10.0f);
        glColor3f(1.0f, 1.0f, 1.0f);
        
        float hw = roomWidth / 2.0f;
        float hd = roomDepth / 2.0f;
        float tileSize = 4.0f;  // Texture tiling
        
        // Draw floor
        glBegin(GL_QUADS);
        glNormal3f(0.0f, 1.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-hw, 0.0f, -hd);
        glTexCoord2f(0.0f, roomDepth/tileSize); glVertex3f(-hw, 0.0f, hd);
        glTexCoord2f(roomWidth/tileSize, roomDepth/tileSize); glVertex3f(hw, 0.0f, hd);
        glTexCoord2f(roomWidth/tileSize, 0.0f); glVertex3f(hw, 0.0f, -hd);
        glEnd();
        
        // Draw ceiling
        glBegin(GL_QUADS);
        glNormal3f(0.0f, -1.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-hw, roomHeight, -hd);
        glTexCoord2f(roomWidth/tileSize, 0.0f); glVertex3f(hw, roomHeight, -hd);
        glTexCoord2f(roomWidth/tileSize, roomDepth/tileSize); glVertex3f(hw, roomHeight, hd);
        glTexCoord2f(0.0f, roomDepth/tileSize); glVertex3f(-hw, roomHeight, hd);
        glEnd();
        
        // Draw north wall (negative Z)
        glBegin(GL_QUADS);
        glNormal3f(0.0f, 0.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-hw, 0.0f, -hd);
        glTexCoord2f(roomWidth/tileSize, 0.0f); glVertex3f(hw, 0.0f, -hd);
        glTexCoord2f(roomWidth/tileSize, roomHeight/tileSize); glVertex3f(hw, roomHeight, -hd);
        glTexCoord2f(0.0f, roomHeight/tileSize); glVertex3f(-hw, roomHeight, -hd);
        glEnd();
        
        // Draw south wall (positive Z)
        glBegin(GL_QUADS);
        glNormal3f(0.0f, 0.0f, -1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(hw, 0.0f, hd);
        glTexCoord2f(roomWidth/tileSize, 0.0f); glVertex3f(-hw, 0.0f, hd);
        glTexCoord2f(roomWidth/tileSize, roomHeight/tileSize); glVertex3f(-hw, roomHeight, hd);
        glTexCoord2f(0.0f, roomHeight/tileSize); glVertex3f(hw, roomHeight, hd);
        glEnd();
        
        // Draw west wall (negative X)
        glBegin(GL_QUADS);
        glNormal3f(1.0f, 0.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-hw, 0.0f, hd);
        glTexCoord2f(roomDepth/tileSize, 0.0f); glVertex3f(-hw, 0.0f, -hd);
        glTexCoord2f(roomDepth/tileSize, roomHeight/tileSize); glVertex3f(-hw, roomHeight, -hd);
        glTexCoord2f(0.0f, roomHeight/tileSize); glVertex3f(-hw, roomHeight, hd);
        glEnd();
        
        // Draw east wall (positive X)
        glBegin(GL_QUADS);
        glNormal3f(-1.0f, 0.0f, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(hw, 0.0f, -hd);
        glTexCoord2f(roomDepth/tileSize, 0.0f); glVertex3f(hw, 0.0f, hd);
        glTexCoord2f(roomDepth/tileSize, roomHeight/tileSize); glVertex3f(hw, roomHeight, hd);
        glTexCoord2f(0.0f, roomHeight/tileSize); glVertex3f(hw, roomHeight, -hd);
        glEnd();
        
        glDisable(GL_TEXTURE_2D);
        
        // Draw stones
        if (stonesModel) {
            for (const auto& stone : stones) {
                glPushMatrix();
                glTranslatef(stone.position.x, stone.position.y, stone.position.z);
                glRotatef(stone.rotation, 0.0f, 1.0f, 0.0f);
                glScalef(stone.scale, stone.scale, stone.scale);
                stonesModel->render();
                glPopMatrix();
            }
        }
        
        // Draw traps
        if (trapModel) {
            for (const auto& trap : traps) {
                glPushMatrix();
                glTranslatef(trap.position.x, trap.position.y, trap.position.z);
                glRotatef(trap.rotation, 0.0f, 1.0f, 0.0f);
                glScalef(1.5f, 1.5f, 1.5f);  // Scale traps to be visible
                trapModel->render();
                glPopMatrix();
            }
        }
        
        // Draw lava pools
        if (lavaTexture) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, lavaTexture);
            
            // Bright emissive material for lava
            GLfloat lavaEmission[] = { 0.6f, 0.2f, 0.0f, 1.0f };
            GLfloat lavaDiffuse[] = { 1.0f, 0.5f, 0.1f, 1.0f };
            GLfloat lavaAmbient[] = { 0.8f, 0.3f, 0.1f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, lavaEmission);
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, lavaDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, lavaAmbient);
            glColor3f(1.0f, 1.0f, 1.0f);
            
            for (const auto& lava : lavaPools) {
                float hs = lava.size / 2.0f;
                float lavaY = 0.02f;  // Slightly above floor level to be visible
                
                // Draw the lava surface (no pit, just a glowing pool on the floor)
                glBindTexture(GL_TEXTURE_2D, lavaTexture);
                glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, lavaEmission);
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, lavaDiffuse);
                
                glBegin(GL_QUADS);
                glNormal3f(0.0f, 1.0f, 0.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(lava.x - hs, lavaY, lava.z - hs);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(lava.x - hs, lavaY, lava.z + hs);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(lava.x + hs, lavaY, lava.z + hs);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(lava.x + hs, lavaY, lava.z - hs);
                glEnd();
            }
            
            // Reset emission
            GLfloat noEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
            glDisable(GL_TEXTURE_2D);
        }
        
        // Draw torches
        for (const auto& torch : torches) {
            drawTorch(torch);
        }
        
        // Draw purple crystals (collectibles)
        for (auto& crystal : crystals) {
            if (!crystal.collected) {
                drawCrystal(crystal);
            }
        }
        
        // Draw flying bats
        for (auto& bat : bats) {
            drawBat(bat);
        }
        
        // Draw the portal (exit portal in Scene 2)
        drawPortalScene2();
        
        // Disable extra lights
        for (int i = 0; i < 8; i++) {
            if (i > 0) glDisable(GL_LIGHT0 + i);
        }
    }
    
    void drawPortalScene2() {
        glPushMatrix();
        glTranslatef(portalPositionScene2.x, 0.0f, portalPositionScene2.z);
        
        float portalWidth = 2.0f;
        float portalHeight = 3.0f;
        
        // Enable blending for transparency
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_TEXTURE_2D);
        
        // Draw portal frame edges (always visible)
        GLfloat frameDiffuse[] = { 0.3f, 0.15f, 0.4f, 1.0f };  // Dark purple
        GLfloat frameAmbient[] = { 0.15f, 0.1f, 0.2f, 1.0f };
        GLfloat frameSpecular[] = { 0.5f, 0.3f, 0.6f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, frameDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, frameAmbient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, frameSpecular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 50.0f);
        glColor4f(0.3f, 0.15f, 0.4f, 1.0f);
        
        float frameThickness = 0.15f;
        
        // Left edge
        glPushMatrix();
        glTranslatef(-portalWidth/2.0f, portalHeight/2.0f, 0.0f);
        glScalef(frameThickness, portalHeight, frameThickness);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Right edge
        glPushMatrix();
        glTranslatef(portalWidth/2.0f, portalHeight/2.0f, 0.0f);
        glScalef(frameThickness, portalHeight, frameThickness);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Top edge
        glPushMatrix();
        glTranslatef(0.0f, portalHeight, 0.0f);
        glScalef(portalWidth + frameThickness * 2, frameThickness, frameThickness);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Bottom edge
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 0.0f);
        glScalef(portalWidth + frameThickness * 2, frameThickness, frameThickness);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        // Portal is always active in Scene 2 (return portal)
        float glowPulse = 0.5f + 0.3f * sin(portalTime * 3.0f);
        
        GLfloat portalDiffuse[] = { 0.6f * glowPulse, 0.2f * glowPulse, 0.8f * glowPulse, 0.7f };
        GLfloat portalAmbient[] = { 0.4f * glowPulse, 0.1f * glowPulse, 0.5f * glowPulse, 0.7f };
        GLfloat portalEmission[] = { 0.4f * glowPulse, 0.15f * glowPulse, 0.6f * glowPulse, 1.0f };
        
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, portalDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, portalAmbient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, portalEmission);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 100.0f);
        glColor4f(0.6f * glowPulse, 0.2f * glowPulse, 0.8f * glowPulse, 0.7f);
        
        // Draw semi-transparent purple portal surface
        glPushMatrix();
        glTranslatef(0.0f, portalHeight/2.0f, 0.0f);
        glBegin(GL_QUADS);
        glVertex3f(-portalWidth/2.0f, -portalHeight/2.0f, 0.0f);
        glVertex3f(portalWidth/2.0f, -portalHeight/2.0f, 0.0f);
        glVertex3f(portalWidth/2.0f, portalHeight/2.0f, 0.0f);
        glVertex3f(-portalWidth/2.0f, portalHeight/2.0f, 0.0f);
        glEnd();
        glPopMatrix();
        
        // Add swirling particle effect
        for (int i = 0; i < 20; i++) {
            float angle = (portalTime * 2.0f + i * 18.0f) * 3.14159f / 180.0f;
            float radius = 0.3f + 0.5f * (i / 20.0f);
            float height = (portalHeight * 0.9f) * (i / 20.0f);
            
            glPushMatrix();
            glTranslatef(
                radius * cos(angle),
                height + 0.1f,
                radius * sin(angle) * 0.1f
            );
            
            float particleGlow = 0.6f + 0.4f * sin(portalTime * 4.0f + i);
            glColor4f(0.7f * particleGlow, 0.3f * particleGlow, 1.0f * particleGlow, 0.8f);
            glutSolidSphere(0.05f, 6, 6);
            glPopMatrix();
        }
        
        // Reset emission
        GLfloat noEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
        
        glDisable(GL_BLEND);
        glPopMatrix();
    }
    
    void update(float deltaTime) override {
        // Update portal animation for Scene 2 as well
        portalTime += deltaTime;

        // Update torch flickering
        for (auto& torch : torches) {
            torch.flickerPhase += deltaTime * torch.flickerSpeed;
            // Random-ish flickering using multiple sine waves
            float flicker1 = sin(torch.flickerPhase * 7.3f) * 0.1f;
            float flicker2 = sin(torch.flickerPhase * 11.7f) * 0.05f;
            float flicker3 = sin(torch.flickerPhase * 23.1f) * 0.03f;
            torch.intensity = 0.85f + flicker1 + flicker2 + flicker3;
            torch.intensity = std::max(0.6f, std::min(1.0f, torch.intensity));
        }
        
        // Update flying bats
        for (auto& bat : bats) {
            // Animate wing flapping
            bat.wingAngle += deltaTime * bat.wingSpeed;
            
            // Move bat towards target
            float dx = bat.targetPos.x - bat.position.x;
            float dy = bat.targetPos.y - bat.position.y;
            float dz = bat.targetPos.z - bat.position.z;
            float dist = sqrt(dx*dx + dy*dy + dz*dz);
            
            if (dist > 0.5f) {
                // Normalize and move towards target
                float moveSpeed = bat.flySpeed * deltaTime;
                bat.position.x += (dx / dist) * moveSpeed;
                bat.position.y += (dy / dist) * moveSpeed;
                bat.position.z += (dz / dist) * moveSpeed;
            } else {
                // Reached target, pick new random target
                bat.targetPos.x = -35.0f + (rand() % 7000) / 100.0f;
                bat.targetPos.y = 4.0f + (rand() % 800) / 100.0f;  // 4-12 height range
                bat.targetPos.z = -35.0f + (rand() % 7000) / 100.0f;
            }
            
            // Keep bats within bounds
            bat.position.x = std::max(-45.0f, std::min(45.0f, bat.position.x));
            bat.position.y = std::max(3.0f, std::min(12.0f, bat.position.y));  // Lower minimum
            bat.position.z = std::max(-45.0f, std::min(45.0f, bat.position.z));
        }
    }
    
    void cleanup() override {
        std::cout << "Cleaning up Scene 2" << std::endl;
        if (stoneTexture) {
            glDeleteTextures(1, &stoneTexture);
            stoneTexture = 0;
        }
        if (lavaTexture) {
            glDeleteTextures(1, &lavaTexture);
            lavaTexture = 0;
        }
        if (stonesModel) {
            delete stonesModel;
            stonesModel = nullptr;
        }
        if (trapModel) {
            delete trapModel;
            trapModel = nullptr;
        }
        torches.clear();
        stones.clear();
        traps.clear();
        lavaPools.clear();
        bats.clear();
    }
    
private:
    void drawBat(Bat& bat) {
        glPushMatrix();
        glTranslatef(bat.position.x, bat.position.y, bat.position.z);
        
        // Face direction of movement
        float dx = bat.targetPos.x - bat.position.x;
        float dz = bat.targetPos.z - bat.position.z;
        float angle = atan2(dx, dz) * 180.0f / 3.14159f;
        glRotatef(angle, 0.0f, 1.0f, 0.0f);
        
        glScalef(bat.size, bat.size, bat.size);
        
        // Dark gray/brown bat material
        glDisable(GL_TEXTURE_2D);
        GLfloat batDiffuse[] = { 0.15f, 0.12f, 0.1f, 1.0f };
        GLfloat batAmbient[] = { 0.08f, 0.06f, 0.05f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, batDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, batAmbient);
        glColor3f(0.15f, 0.12f, 0.1f);
        
        // Body (elongated sphere)
        glPushMatrix();
        glScalef(0.4f, 0.3f, 0.8f);
        glutSolidSphere(1.0f, 10, 8);
        glPopMatrix();
        
        // Head (small sphere at front)
        glPushMatrix();
        glTranslatef(0.0f, 0.1f, 0.7f);
        glutSolidSphere(0.35f, 8, 6);
        
        // Ears (small cones)
        glPushMatrix();
        glTranslatef(-0.15f, 0.25f, 0.0f);
        glRotatef(-20.0f, 0.0f, 0.0f, 1.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        glutSolidCone(0.08f, 0.25f, 6, 2);
        glPopMatrix();
        
        glPushMatrix();
        glTranslatef(0.15f, 0.25f, 0.0f);
        glRotatef(20.0f, 0.0f, 0.0f, 1.0f);
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        glutSolidCone(0.08f, 0.25f, 6, 2);
        glPopMatrix();
        
        // Eyes (tiny red spheres)
        GLfloat eyeDiffuse[] = { 0.6f, 0.1f, 0.1f, 1.0f };
        GLfloat eyeEmission[] = { 0.3f, 0.05f, 0.05f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, eyeDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, eyeEmission);
        glColor3f(0.6f, 0.1f, 0.1f);
        
        glPushMatrix();
        glTranslatef(-0.12f, 0.05f, 0.25f);
        glutSolidSphere(0.06f, 6, 4);
        glPopMatrix();
        
        glPushMatrix();
        glTranslatef(0.12f, 0.05f, 0.25f);
        glutSolidSphere(0.06f, 6, 4);
        glPopMatrix();
        
        GLfloat noEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
        
        glPopMatrix();  // End head
        
        // Wings (animated triangular membranes)
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, batDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, batAmbient);
        glColor3f(0.12f, 0.1f, 0.08f);
        
        float wingFlap = sin(bat.wingAngle) * 40.0f;  // -40 to +40 degrees
        
        // Left wing
        glPushMatrix();
        glTranslatef(-0.3f, 0.0f, 0.0f);
        glRotatef(wingFlap - 10.0f, 0.0f, 0.0f, 1.0f);
        
        glBegin(GL_TRIANGLES);
        // Wing membrane - multiple triangular segments
        glNormal3f(0.0f, 1.0f, 0.0f);
        // Main wing section
        glVertex3f(0.0f, 0.0f, -0.3f);
        glVertex3f(-2.0f, 0.0f, 0.0f);
        glVertex3f(0.0f, 0.0f, 0.5f);
        // Wing tip section
        glVertex3f(-2.0f, 0.0f, 0.0f);
        glVertex3f(-2.2f, 0.0f, -0.2f);
        glVertex3f(-1.5f, 0.0f, -0.4f);
        // Inner section
        glVertex3f(0.0f, 0.0f, -0.3f);
        glVertex3f(-1.5f, 0.0f, -0.4f);
        glVertex3f(-2.0f, 0.0f, 0.0f);
        glEnd();
        
        // Wing finger bones
        glColor3f(0.2f, 0.15f, 0.12f);
        glBegin(GL_LINES);
        glVertex3f(0.0f, 0.02f, 0.0f);
        glVertex3f(-2.0f, 0.02f, 0.0f);
        glVertex3f(-0.3f, 0.02f, 0.0f);
        glVertex3f(-1.8f, 0.02f, -0.3f);
        glVertex3f(-0.5f, 0.02f, 0.0f);
        glVertex3f(-2.1f, 0.02f, -0.15f);
        glEnd();
        
        glPopMatrix();
        
        // Right wing (mirrored)
        glPushMatrix();
        glTranslatef(0.3f, 0.0f, 0.0f);
        glRotatef(-wingFlap + 10.0f, 0.0f, 0.0f, 1.0f);
        
        glColor3f(0.12f, 0.1f, 0.08f);
        glBegin(GL_TRIANGLES);
        glNormal3f(0.0f, 1.0f, 0.0f);
        // Main wing section
        glVertex3f(0.0f, 0.0f, -0.3f);
        glVertex3f(2.0f, 0.0f, 0.0f);
        glVertex3f(0.0f, 0.0f, 0.5f);
        // Wing tip section
        glVertex3f(2.0f, 0.0f, 0.0f);
        glVertex3f(2.2f, 0.0f, -0.2f);
        glVertex3f(1.5f, 0.0f, -0.4f);
        // Inner section
        glVertex3f(0.0f, 0.0f, -0.3f);
        glVertex3f(1.5f, 0.0f, -0.4f);
        glVertex3f(2.0f, 0.0f, 0.0f);
        glEnd();
        
        // Wing finger bones
        glColor3f(0.2f, 0.15f, 0.12f);
        glBegin(GL_LINES);
        glVertex3f(0.0f, 0.02f, 0.0f);
        glVertex3f(2.0f, 0.02f, 0.0f);
        glVertex3f(0.3f, 0.02f, 0.0f);
        glVertex3f(1.8f, 0.02f, -0.3f);
        glVertex3f(0.5f, 0.02f, 0.0f);
        glVertex3f(2.1f, 0.02f, -0.15f);
        glEnd();
        
        glPopMatrix();
        
        // Small legs/feet
        glColor3f(0.1f, 0.08f, 0.06f);
        glPushMatrix();
        glTranslatef(-0.1f, -0.2f, 0.0f);
        glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
        glScalef(0.05f, 0.3f, 0.05f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        glPushMatrix();
        glTranslatef(0.1f, -0.2f, 0.0f);
        glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
        glScalef(0.05f, 0.3f, 0.05f);
        glutSolidCube(1.0f);
        glPopMatrix();
        
        glPopMatrix();
    }
    
    void drawTorch(const Torch& torch) {
        glPushMatrix();
        glTranslatef(torch.position.x, torch.position.y, torch.position.z);
        
        // Rotate torch to face into room based on wall position
        float hw = roomWidth / 2.0f - 0.5f;
        float hd = roomDepth / 2.0f - 0.5f;
        if (fabs(torch.position.x) > fabs(torch.position.z)) {
            // On east/west wall
            if (torch.position.x > 0) glRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
            else glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
        } else {
            // On north/south wall
            if (torch.position.z > 0) glRotatef(180.0f, 0.0f, 1.0f, 0.0f);
        }
        
        // Draw torch handle (brown)
        glDisable(GL_TEXTURE_2D);
        GLfloat handleDiffuse[] = { 0.4f, 0.25f, 0.1f, 1.0f };
        GLfloat handleAmbient[] = { 0.2f, 0.1f, 0.05f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, handleDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, handleAmbient);
        glColor3f(0.4f, 0.25f, 0.1f);
        
        glPushMatrix();
        glRotatef(-30.0f, 1.0f, 0.0f, 0.0f);  // Angle the torch
        glTranslatef(0.0f, 0.0f, 0.3f);
        
        // Torch stick
        GLUquadric* quad = gluNewQuadric();
        gluCylinder(quad, 0.08f, 0.06f, 0.8f, 8, 1);
        
        // Torch head (fire)
        glTranslatef(0.0f, 0.0f, 0.8f);
        
        // Emissive fire glow
        float glow = torch.intensity;
        GLfloat fireEmission[] = { 1.0f * glow, 0.5f * glow, 0.1f * glow, 1.0f };
        GLfloat fireDiffuse[] = { 1.0f, 0.6f, 0.1f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, fireEmission);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, fireDiffuse);
        glColor3f(1.0f * glow, 0.5f * glow, 0.1f);
        
        // Draw flame as a cone
        glutSolidCone(0.15f, 0.4f * (0.8f + 0.2f * glow), 8, 4);
        
        // Reset emission
        GLfloat noEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
        
        gluDeleteQuadric(quad);
        glPopMatrix();
        glPopMatrix();
    }
    
    void drawCrystal(Crystal& crystal) {
        glPushMatrix();
        
        // Apply bobbing animation
        float bobOffset = sin(animationTime * 2.0f + crystal.bobPhase) * 0.2f;
        glTranslatef(crystal.position.x, crystal.position.y + bobOffset, crystal.position.z);
        
        // Rotate crystal slowly
        crystal.rotation += 1.0f;
        if (crystal.rotation > 360.0f) crystal.rotation -= 360.0f;
        glRotatef(crystal.rotation, 0.0f, 1.0f, 0.0f);
        
        // Purple glowing material
        float glowPulse = 0.7f + 0.3f * sin(animationTime * 3.0f + crystal.bobPhase);
        GLfloat crystalDiffuse[] = { 0.6f * glowPulse, 0.2f * glowPulse, 0.8f * glowPulse, 0.9f };
        GLfloat crystalAmbient[] = { 0.4f * glowPulse, 0.1f * glowPulse, 0.5f * glowPulse, 0.9f };
        GLfloat crystalEmission[] = { 0.5f * glowPulse, 0.2f * glowPulse, 0.7f * glowPulse, 1.0f };
        GLfloat crystalSpecular[] = { 0.9f, 0.7f, 1.0f, 1.0f };
        
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, crystalDiffuse);
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, crystalAmbient);
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, crystalEmission);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, crystalSpecular);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 100.0f);
        
        glDisable(GL_TEXTURE_2D);
        glColor4f(0.6f * glowPulse, 0.2f * glowPulse, 0.8f * glowPulse, 0.9f);
        
        // Draw crystal as an octahedron (8-sided diamond shape)
        float size = 0.4f;
        glBegin(GL_TRIANGLES);
        
        // Top pyramid (4 faces)
        glNormal3f(0.0f, 1.0f, 1.0f);
        glVertex3f(0.0f, size, 0.0f);
        glVertex3f(-size, 0.0f, 0.0f);
        glVertex3f(0.0f, 0.0f, size);
        
        glNormal3f(1.0f, 1.0f, 0.0f);
        glVertex3f(0.0f, size, 0.0f);
        glVertex3f(0.0f, 0.0f, size);
        glVertex3f(size, 0.0f, 0.0f);
        
        glNormal3f(0.0f, 1.0f, -1.0f);
        glVertex3f(0.0f, size, 0.0f);
        glVertex3f(size, 0.0f, 0.0f);
        glVertex3f(0.0f, 0.0f, -size);
        
        glNormal3f(-1.0f, 1.0f, 0.0f);
        glVertex3f(0.0f, size, 0.0f);
        glVertex3f(0.0f, 0.0f, -size);
        glVertex3f(-size, 0.0f, 0.0f);
        
        // Bottom pyramid (4 faces)
        glNormal3f(0.0f, -1.0f, 1.0f);
        glVertex3f(0.0f, -size, 0.0f);
        glVertex3f(0.0f, 0.0f, size);
        glVertex3f(-size, 0.0f, 0.0f);
        
        glNormal3f(1.0f, -1.0f, 0.0f);
        glVertex3f(0.0f, -size, 0.0f);
        glVertex3f(size, 0.0f, 0.0f);
        glVertex3f(0.0f, 0.0f, size);
        
        glNormal3f(0.0f, -1.0f, -1.0f);
        glVertex3f(0.0f, -size, 0.0f);
        glVertex3f(0.0f, 0.0f, -size);
        glVertex3f(size, 0.0f, 0.0f);
        
        glNormal3f(-1.0f, -1.0f, 0.0f);
        glVertex3f(0.0f, -size, 0.0f);
        glVertex3f(-size, 0.0f, 0.0f);
        glVertex3f(0.0f, 0.0f, -size);
        
        glEnd();
        
        // Reset emission
        GLfloat noEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
        
        glPopMatrix();
    }
};

// Static collision check function for Scene2 (only blocks on walls and stones)
bool scene2CollisionCheck(float x, float z, float radius) {
    if (scene2Instance) {
        lastScene2CollisionType = scene2Instance->checkSceneCollision(x, z, radius);
        return lastScene2CollisionType != 0;  // Only walls (3) and stones (1) block movement
    }
    return false;
}

// ============================================================================
// SCENE MANAGER
// ============================================================================

Scene* scene1 = nullptr;
Scene* scene2 = nullptr;
Scene* currentScenePtr = nullptr;

void initScenes() {
    scene1 = new Scene1_CaveEntrance();
    scene2 = new Scene2_DeepCavern();
    
    scene1->init();
    scene2->init();
    
    currentScenePtr = scene1;
    currentScene = 1;
    sceneCollisionCheck = scene1CollisionCheck;  // Set collision check for scene 1
    
    // Start background music for Scene 1
    playBackgroundMusic("nature.wav");
}

void switchScene(int sceneNumber) {
    if (sceneNumber == currentScene) return;
    
    std::cout << "Switching to Scene " << sceneNumber << std::endl;
    
    if (sceneNumber == 1) {
        currentScenePtr = scene1;
        currentScene = 1;
        sceneCollisionCheck = scene1CollisionCheck;
        playBackgroundMusic("nature.wav");  // Play nature background music for Scene 1
    } else if (sceneNumber == 2) {
        currentScenePtr = scene2;
        currentScene = 2;
        sceneCollisionCheck = scene2CollisionCheck;  // Collision with stones, traps, walls
        playBackgroundMusic("lava.wav");  // Play lava background music for Scene 2
    }
}

void cleanupScenes() {
    if (scene1) {
        scene1->cleanup();
        delete scene1;
    }
    if (scene2) {
        scene2->cleanup();
        delete scene2;
    }
}

// ============================================================================
// HUD RENDERING
// ============================================================================

void renderHUD() {
    // Switch to 2D orthographic projection for HUD
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, 0, windowHeight);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable lighting for HUD
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    
    // Draw scene indicator
    glColor3f(1.0f, 1.0f, 1.0f);
    
    std::string sceneText = "Scene " + std::to_string(currentScene) + ": " + currentScenePtr->name;
    glRasterPos2f(10, windowHeight - 30);
    for (char c : sceneText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
    
    // Draw crystal counter at top center
    if (currentScene == 2) {
        glColor3f(0.8f, 0.4f, 1.0f);  // Purple color for crystals
        std::string crystalText = "Crystals: " + std::to_string(crystalsCollected) + "/10";
        int textWidth = crystalText.length() * 10;  // Approximate width
        glRasterPos2f(windowWidth / 2 - textWidth / 2, windowHeight - 30);
        for (char c : crystalText) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
        
        // Draw small crystal icon next to counter
        float iconX = windowWidth / 2 - textWidth / 2 - 25.0f;
        float iconY = windowHeight - 35.0f;
        float iconSize = 8.0f;
        
        glColor3f(0.7f, 0.3f, 0.9f);
        glBegin(GL_TRIANGLES);
        // Simple diamond shape
        glVertex2f(iconX, iconY + iconSize);
        glVertex2f(iconX - iconSize, iconY);
        glVertex2f(iconX, iconY - iconSize);
        
        glVertex2f(iconX, iconY + iconSize);
        glVertex2f(iconX, iconY - iconSize);
        glVertex2f(iconX + iconSize, iconY);
        glEnd();
    }
    
    // Draw controls hint
    std::string controlsText = "1: Third Person | 2: First Person | 3/4: Switch Scenes | T: Toggle | Mouse: Look";
    glRasterPos2f(10, windowHeight - 55);
    for (char c : controlsText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }
    
    // Draw view mode
    std::string viewText = "View: " + std::string(player.isFirstPerson ? "First Person" : "Third Person");
    glRasterPos2f(10, windowHeight - 80);
    for (char c : viewText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
    }
    
    // Draw score
    std::string scoreText = "Score: " + std::to_string(score);
    glRasterPos2f(10, 30);
    for (char c : scoreText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
    
    // Draw hearts (lives) in top right corner - 5 hearts total (Minecraft style - pixelated)
    float heartSpacing = 20.0f;
    float heartStartX = windowWidth - 130.0f;
    float heartY = windowHeight - 30.0f;
    float pixelSize = 2.0f;  // Size of each "pixel" in the heart
    
    for (int i = 0; i < 5; i++) {
        float heartX = heartStartX + i * heartSpacing;
        float heartLife = lives - i;  // How much life this heart represents
        
        // Minecraft heart pixel pattern (9x9 grid)
        // 1 = pixel exists, 0 = no pixel
        // Heart shape in pixel art style
        int heartPattern[9][9] = {
            {0, 1, 1, 0, 0, 1, 1, 0, 0},
            {1, 1, 1, 1, 1, 1, 1, 1, 0},
            {1, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 1},
            {0, 1, 1, 1, 1, 1, 1, 1, 0},
            {0, 0, 1, 1, 1, 1, 1, 0, 0},
            {0, 0, 0, 1, 1, 1, 0, 0, 0},
            {0, 0, 0, 0, 1, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0}
        };
        
        // Draw the heart pixel by pixel
        for (int row = 0; row < 9; row++) {
            for (int col = 0; col < 9; col++) {
                if (heartPattern[row][col] == 1) {
                    // Determine color based on life and position
                    if (heartLife >= 1.0f) {
                        // Full heart - bright red
                        glColor3f(1.0f, 0.0f, 0.0f);
                    } else if (heartLife >= 0.5f) {
                        // Half heart - left half red, right half black
                        if (col < 4) {
                            glColor3f(1.0f, 0.0f, 0.0f);  // Left side red
                        } else {
                            glColor3f(0.2f, 0.0f, 0.0f);  // Right side dark
                        }
                    } else {
                        // Empty heart - dark gray/black
                        glColor3f(0.2f, 0.0f, 0.0f);
                    }
                    
                    // Draw the pixel
                    float px = heartX + (col - 4.5f) * pixelSize;
                    float py = heartY - (row - 4.5f) * pixelSize;
                    
                    glBegin(GL_QUADS);
                    glVertex2f(px, py);
                    glVertex2f(px + pixelSize, py);
                    glVertex2f(px + pixelSize, py + pixelSize);
                    glVertex2f(px, py + pixelSize);
                    glEnd();
                }
            }
        }
    }
    
    // Draw key indicator if player has key
    if (hasKey) {
        float keyX = windowWidth - 90.0f;
        float keyY = windowHeight - 80.0f;
        
        // Draw key icon (simple key shape)
        glColor3f(1.0f, 0.84f, 0.0f);  // Gold color
        
        // Key head (circle)
        glBegin(GL_POLYGON);
        for (int i = 0; i < 20; i++) {
            float angle = i * 2.0f * M_PI / 20.0f;
            glVertex2f(keyX + 6.0f * cos(angle), keyY + 6.0f * sin(angle));
        }
        glEnd();
        
        // Key shaft (rectangle)
        glBegin(GL_QUADS);
        glVertex2f(keyX + 6.0f, keyY - 2.0f);
        glVertex2f(keyX + 20.0f, keyY - 2.0f);
        glVertex2f(keyX + 20.0f, keyY + 2.0f);
        glVertex2f(keyX + 6.0f, keyY + 2.0f);
        glEnd();
        
        // Key teeth
        glBegin(GL_QUADS);
        glVertex2f(keyX + 15.0f, keyY - 2.0f);
        glVertex2f(keyX + 15.0f, keyY - 5.0f);
        glVertex2f(keyX + 17.0f, keyY - 5.0f);
        glVertex2f(keyX + 17.0f, keyY - 2.0f);
        glEnd();
        
        glBegin(GL_QUADS);
        glVertex2f(keyX + 19.0f, keyY - 2.0f);
        glVertex2f(keyX + 19.0f, keyY - 5.0f);
        glVertex2f(keyX + 20.0f, keyY - 5.0f);
        glVertex2f(keyX + 20.0f, keyY - 2.0f);
        glEnd();
        
        // Key text
        glColor3f(1.0f, 0.84f, 0.0f);
        std::string keyText = "Key Collected!";
        glRasterPos2f(windowWidth - 130, windowHeight - 100);
        for (char c : keyText) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, c);
        }
    }
    
    // Draw crosshair in center of screen
    float centerX = windowWidth / 2.0f;
    float centerY = windowHeight / 2.0f;
    float crosshairSize = 12.0f;
    float crosshairThickness = 2.0f;
    
    glColor3f(1.0f, 1.0f, 1.0f);  // White crosshair
    glLineWidth(crosshairThickness);
    
    glBegin(GL_LINES);
    // Horizontal line
    glVertex2f(centerX - crosshairSize, centerY);
    glVertex2f(centerX - 4, centerY);
    glVertex2f(centerX + 4, centerY);
    glVertex2f(centerX + crosshairSize, centerY);
    // Vertical line
    glVertex2f(centerX, centerY - crosshairSize);
    glVertex2f(centerX, centerY - 4);
    glVertex2f(centerX, centerY + 4);
    glVertex2f(centerX, centerY + crosshairSize);
    glEnd();
    
    // Small dot in center
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    glVertex2f(centerX, centerY);
    glEnd();
    glPointSize(1.0f);
    glLineWidth(1.0f);
    
    // Draw damage flash if recently hit
    if (trapDamageCooldown > 1.2f) {
        glColor4f(1.0f, 0.0f, 0.0f, 0.3f);  // Semi-transparent red
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(windowWidth, 0);
        glVertex2f(windowWidth, windowHeight);
        glVertex2f(0, windowHeight);
        glEnd();
        glDisable(GL_BLEND);
    }
    
    // Draw game over message if dead
    if (lives <= 0) {
        glColor3f(1.0f, 0.0f, 0.0f);
        std::string gameOverText = "GAME OVER!";
        glRasterPos2f(windowWidth / 2 - 60, windowHeight / 2);
        for (char c : gameOverText) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
        }
        glColor3f(1.0f, 1.0f, 1.0f);
        std::string restartText = "Press R to restart";
        glRasterPos2f(windowWidth / 2 - 80, windowHeight / 2 - 30);
        for (char c : restartText) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
    }
    
    // Draw YOU WIN message if all crystals collected
    if (gameWon) {
        glColor3f(0.8f, 0.4f, 1.0f);  // Purple color
        std::string winText = "YOU WIN!";
        glRasterPos2f(windowWidth / 2 - 50, windowHeight / 2 + 40);
        for (char c : winText) {
            glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, c);
        }
        glColor3f(1.0f, 1.0f, 1.0f);
        std::string winSubText = "All Crystals Collected!";
        glRasterPos2f(windowWidth / 2 - 90, windowHeight / 2 + 10);
        for (char c : winSubText) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
        std::string congratsText = "Congratulations!";
        glRasterPos2f(windowWidth / 2 - 70, windowHeight / 2 - 20);
        for (char c : congratsText) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
        }
    }
    
    // Restore matrices first
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    // Re-enable lighting and depth test AFTER restoring matrices
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
}

// ============================================================================
// OPENGL CALLBACKS
// ============================================================================

void display() {
    // Set clear color based on current scene
    if (currentScene == 2) {
        // Dark dungeon - nearly black background
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    } else {
        // Forest scene - light blue sky
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    }
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // Setup camera based on player view
    Vector3 eye, center;
    player.getCameraTransform(eye, center);
    
    gluLookAt(
        eye.x, eye.y, eye.z,        // Eye position
        center.x, center.y, center.z, // Look at point
        0.0f, 1.0f, 0.0f             // Up vector
    );
    
    // Render current scene
    if (currentScenePtr) {
        currentScenePtr->render();
    }
    
    // Render player (only in third person)
    player.render();
    
    // Render sparkle particles
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    
    for (const auto& sparkle : sparkles) {
        glPushMatrix();
        glTranslatef(sparkle.position.x, sparkle.position.y, sparkle.position.z);
        
        // Billboard effect - face camera
        glRotatef(-player.yaw, 0.0f, 1.0f, 0.0f);
        glRotatef(-player.pitch, 1.0f, 0.0f, 0.0f);
        
        // Purple sparkle color with fade
        float alpha = sparkle.lifetime;
        glColor4f(0.9f, 0.5f, 1.0f, alpha);
        
        // Draw sparkle as a star
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0.0f, 0.0f, 0.0f);
        for (int i = 0; i <= 8; i++) {
            float angle = i * M_PI / 4.0f;
            float radius = (i % 2 == 0) ? sparkle.size : sparkle.size * 0.4f;
            glVertex3f(cos(angle) * radius, sin(angle) * radius, 0.0f);
        }
        glEnd();
        
        glPopMatrix();
    }
    
    // Render flame particles (burning effect)
    for (const auto& flame : flames) {
        glPushMatrix();
        glTranslatef(flame.position.x, flame.position.y, flame.position.z);
        
        // Billboard effect - face camera
        glRotatef(-player.yaw, 0.0f, 1.0f, 0.0f);
        glRotatef(-player.pitch, 1.0f, 0.0f, 0.0f);
        
        // Fire colors - transition from yellow to orange to red
        float lifeFactor = flame.lifetime / 1.0f;
        float r = 1.0f;
        float g = 0.3f + lifeFactor * 0.5f;  // Yellow when young, red when old
        float b = 0.0f;
        float alpha = lifeFactor * 0.8f;
        glColor4f(r, g, b, alpha);
        
        // Draw flame as animated triangle
        glBegin(GL_TRIANGLES);
        glVertex3f(0.0f, flame.size * 2.0f, 0.0f);  // Top point
        glVertex3f(-flame.size, -flame.size, 0.0f);  // Bottom left
        glVertex3f(flame.size, -flame.size, 0.0f);  // Bottom right
        glEnd();
        
        glPopMatrix();
    }
    
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    
    // Render HUD on top
    renderHUD();
    
    glutSwapBuffers();
}

void reshape(int w, int h) {
    // Prevent division by zero
    if (h == 0) h = 1;
    if (w == 0) w = 1;
    
    windowWidth = w;
    windowHeight = h;
    
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (float)w / (float)h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
    
    // Hide cursor - use crosshair instead
    glutSetCursor(GLUT_CURSOR_NONE);
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case '1':
            // Third person view
            player.isFirstPerson = false;
            std::cout << "Switched to Third Person view" << std::endl;
            break;
        case '2':
            // First person view
            player.isFirstPerson = true;
            std::cout << "Switched to First Person view" << std::endl;
            break;
        case '3':
            // Switch to scene 2
            switchScene(2);
            break;
        case '4':
            // Switch to scene 1
            switchScene(1);
            break;
        case 't':
        case 'T':
            // Toggle between first and third person
            player.toggleView();
            std::cout << "Switched to " << (player.isFirstPerson ? "First Person" : "Third Person") << " view" << std::endl;
            break;
        case 27: // ESC key
            cleanupScenes();
            exit(0);
            break;
        case 'f':
        case 'F':
            // Toggle fullscreen
            {
                static bool isFullscreen = false;
                static int savedX = 100, savedY = 100, savedW = 1024, savedH = 768;
                if (!isFullscreen) {
                    savedX = glutGet(GLUT_WINDOW_X);
                    savedY = glutGet(GLUT_WINDOW_Y);
                    savedW = windowWidth;
                    savedH = windowHeight;
                    glutFullScreen();
                    isFullscreen = true;
                } else {
                    glutReshapeWindow(savedW, savedH);
                    glutPositionWindow(savedX, savedY);
                    isFullscreen = false;
                }
                glutSetCursor(GLUT_CURSOR_NONE);  // Keep cursor hidden
            }
            break;
        case 'r':
        case 'R':
            // Restart game (reset lives and position)
            if (lives <= 0) {
                lives = 5.0f;
                player.position = Vector3(0.0f, 0.0f, 5.0f);
                player.position.y = 0.0f;
                player.groundLevel = 0.0f;
                player.yaw = 0.0f;
                player.pitch = 0.0f;
                player.velocityY = 0.0f;
                player.isJumping = false;
                player.isOnGround = true;
                trapDamageCooldown = 0.0f;
                hasKey = false;
                chestOpened = false;
                portalOpened = false;
                crystalsCollected = 0;
                gameWon = false;
                gameWonSoundPlayed = false;  // Reset win sound flag
                gameOverSoundPlayed = false;  // Reset game over sound flag
                sparkles.clear(); // Clear all sparkle particles
                flames.clear(); // Clear all flame particles
                isPlayerBurning = false; // Reset burning state
                // Reset all crystals in Scene 2
                if (scene2Instance) {
                    for (auto& crystal : scene2Instance->crystals) {
                        crystal.collected = false;
                    }
                }
                // Return to forest scene (Scene 1)
                switchScene(1);
                std::cout << "Game restarted!" << std::endl;
            }
            break;
        case 'w':
        case 'W':
            keyW = true;
            break;
        case 's':
        case 'S':
            keyS = true;
            break;
        case 'a':
        case 'A':
            keyA = true;
            break;
        case 'd':
        case 'D':
            keyD = true;
            break;
        case ' ':  // Space bar for jumping
            player.jump();
            break;
    }
    glutPostRedisplay();
}

void keyboardUp(unsigned char key, int x, int y) {
    switch (key) {
        case 'w':
        case 'W':
            keyW = false;
            break;
        case 's':
        case 'S':
            keyS = false;
            break;
        case 'a':
        case 'A':
            keyA = false;
            break;
        case 'd':
        case 'D':
            keyD = false;
            break;
    }
}

void specialKeys(int key, int x, int y) {
    float rotateSpeed = 3.0f;
    
    switch (key) {
        case GLUT_KEY_UP:
            player.rotate(0.0f, rotateSpeed);
            break;
        case GLUT_KEY_DOWN:
            player.rotate(0.0f, -rotateSpeed);
            break;
        case GLUT_KEY_LEFT:
            player.rotate(-rotateSpeed, 0.0f);
            break;
        case GLUT_KEY_RIGHT:
            player.rotate(rotateSpeed, 0.0f);
            break;
    }
    glutPostRedisplay();
}

void mouseClick(int button, int state, int x, int y) {
    // Only handle left click when pressed (not released)
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;
    
    // Only check for interactions in Scene 1
    if (currentScene != 1) return;
    
    // Get player look direction
    float radYaw = player.yaw * M_PI / 180.0f;
    float lookX = sin(radYaw);
    float lookZ = -cos(radYaw);
    
    // Check chest interaction if not already opened
    if (!chestOpened) {
        // Check if player is close enough to the chest
        float dx = player.position.x - chestPosition.x;
        float dz = player.position.z - chestPosition.z;
        float distToChest = sqrt(dx*dx + dz*dz);
        
        // Must be within 4 units of the chest
        if (distToChest <= 4.0f) {
            // Direction to chest
            float toChestX = chestPosition.x - player.position.x;
            float toChestZ = chestPosition.z - player.position.z;
            float toChestLen = sqrt(toChestX*toChestX + toChestZ*toChestZ);
            if (toChestLen > 0) {
                toChestX /= toChestLen;
                toChestZ /= toChestLen;
            }
            
            // Dot product to check if looking roughly at chest
            float dot = lookX * toChestX + lookZ * toChestZ;
            
            // If dot product > 0.7, player is looking at the chest (within ~45 degrees)
            if (dot > 0.7f) {
                chestOpened = true;
                hasKey = true;
                score += 100;
                playKeySound();  // Play key collection sound
                std::cout << "*** CHEST OPENED! You found a KEY! ***" << std::endl;
                return;  // Exit after chest interaction
            }
        }
    }
    
    // Check portal interaction if player has key but portal not opened yet
    if (hasKey && !portalOpened) {
        // Check if player is close enough to the portal
        float dx = player.position.x - portalPosition.x;
        float dz = player.position.z - portalPosition.z;
        float distToPortal = sqrt(dx*dx + dz*dz);
        
        // Must be within 4 units of the portal
        if (distToPortal <= 4.0f) {
            // Direction to portal
            float toPortalX = portalPosition.x - player.position.x;
            float toPortalZ = portalPosition.z - player.position.z;
            float toPortalLen = sqrt(toPortalX*toPortalX + toPortalZ*toPortalZ);
            if (toPortalLen > 0) {
                toPortalX /= toPortalLen;
                toPortalZ /= toPortalLen;
            }
            
            // Dot product to check if looking roughly at portal
            float dot = lookX * toPortalX + lookZ * toPortalZ;
            
            // If dot product > 0.7, player is looking at the portal (within ~45 degrees)
            if (dot > 0.7f) {
                portalOpened = true;
                std::cout << "*** PORTAL OPENED! Step inside to travel to Scene 2! ***" << std::endl;
            }
        }
    }
}

void mouseMotion(int x, int y) {
    if (!mouseInitialized) {
        lastMouseX = x;
        lastMouseY = y;
        mouseInitialized = true;
        return;
    }
    
    // Calculate mouse movement
    int deltaX = x - lastMouseX;
    int deltaY = y - lastMouseY;
    
    // Update camera rotation based on mouse movement
    player.rotate(deltaX * mouseSensitivity, -deltaY * mouseSensitivity);
    
    // Update last mouse position
    lastMouseX = x;
    lastMouseY = y;
    
    // Wrap mouse horizontally for infinite 360 rotation
    int margin = 10; // Pixels from edge to trigger wrap
    if (x <= margin) {
        // Mouse at left edge, warp to right side
        lastMouseX = windowWidth - margin - 1;
        glutWarpPointer(lastMouseX, y);
    } else if (x >= windowWidth - margin) {
        // Mouse at right edge, warp to left side
        lastMouseX = margin + 1;
        glutWarpPointer(lastMouseX, y);
    }
    
    glutPostRedisplay();
}

void mousePassiveMotion(int x, int y) {
    // Same as mouseMotion for passive movement
    mouseMotion(x, y);
}

// Handle portal collision and teleport between scenes
void handlePortalTeleport() {
    if (portalCooldown > 0.0f) return;

    // Scene 1 -> Scene 2 (requires portal to be opened)
    if (currentScene == 1) {
        float dx = player.position.x - portalPosition.x;
        float dz = player.position.z - portalPosition.z;
        float dist = sqrtf(dx*dx + dz*dz);
        // Require player to walk into the portal center (closer than 0.8 units)
        if (portalOpened && dist < 0.8f) {
            // Teleport to Scene 2 near its portal
            switchScene(2);
            player.position = Vector3(portalPositionScene2.x, 0.0f, portalPositionScene2.z + 3.0f);
            player.groundLevel = 0.0f;
            player.yaw = 180.0f; // Face into the scene
            portalCooldown = 1.0f;
            std::cout << "Teleported to Scene 2!" << std::endl;
            return;
        }
    }

    // Scene 2 -> Scene 1 (return portal, no key required)
    if (currentScene == 2) {
        float dx = player.position.x - portalPositionScene2.x;
        float dz = player.position.z - portalPositionScene2.z;
        float dist = sqrtf(dx*dx + dz*dz);
        // Require player to walk into the portal center (closer than 0.8 units)
        if (dist < 0.8f) {
            switchScene(1);
            player.position = Vector3(portalPosition.x, 0.0f, portalPosition.z + 3.0f);
            player.groundLevel = 0.0f;
            player.yaw = 180.0f;
            portalCooldown = 1.0f;
            std::cout << "Teleported to Scene 1!" << std::endl;
            return;
        }
    }
}

void timer(int value) {
    // Update animation time
    animationTime += 0.016f; // Approximately 60 FPS
    float deltaTime = 0.016f;
    
    // Update player physics (jumping and gravity)
    player.updatePhysics(deltaTime);
    
    // Update trap damage cooldown
    if (trapDamageCooldown > 0) {
        trapDamageCooldown -= deltaTime;
    }
    if (portalCooldown > 0.0f) {
        portalCooldown -= deltaTime;
        if (portalCooldown < 0.0f) portalCooldown = 0.0f;
    }
    
    // Handle continuous movement based on key states
    float moveSpeed = 0.15f; // Slightly reduced for smoother frame-by-frame movement
    
    // Stop movement if game is over or won
    if (lives <= 0 || gameWon) {
        moveSpeed = 0.0f;
    }
    
    // Check if player is on lava in Scene 2 and apply slowdown
    if (currentScene == 2 && scene2Instance) {
        if (scene2Instance->checkLavaCollision(player.position.x, player.position.z, 0.2f)) {
            moveSpeed *= 0.2f; // Slow down to 20% speed when on lava
        }
    }
    
    float forward = 0.0f;
    float right = 0.0f;
    
    if (keyW) forward += moveSpeed;
    if (keyS) forward -= moveSpeed;
    if (keyD) right += moveSpeed;
    if (keyA) right -= moveSpeed;
    
    // Check if player is moving and update animation
    if (forward != 0.0f || right != 0.0f) {
        player.isMoving = true;
        player.walkAnimation += deltaTime;
        
        // Update body rotation ONLY when moving forward (W key pressed)
        // Smooth rotation using lerp
        if (keyW) {
            float rotationSpeed = 10.0f; // How fast to rotate (higher = faster)
            
            // Calculate the shortest rotation direction
            // Add 180 so character faces forward (back to camera)
            float targetYaw = player.yaw + 180.0f;
            float diff = targetYaw - player.bodyYaw;
            
            // Normalize the difference to be between -180 and 180
            while (diff > 180.0f) diff -= 360.0f;
            while (diff < -180.0f) diff += 360.0f;
            
            // Smoothly interpolate towards target
            player.bodyYaw += diff * rotationSpeed * deltaTime;
            
            // Keep bodyYaw in reasonable range
            while (player.bodyYaw > 360.0f) player.bodyYaw -= 360.0f;
            while (player.bodyYaw < 0.0f) player.bodyYaw += 360.0f;
        }
    } else {
        player.isMoving = false;
    }
    
    // Apply movement if any keys are pressed
    if (forward != 0.0f || right != 0.0f) {
        // Normalize diagonal movement to prevent faster movement when moving diagonally
        if (forward != 0.0f && right != 0.0f) {
            float length = sqrt(forward * forward + right * right);
            forward /= length;
            right /= length;
            forward *= moveSpeed;
            right *= moveSpeed;
        }
        player.move(forward, right);
    }
    
    // Check for lava damage in Scene 2
    if (currentScene == 2 && scene2Instance) {
        if (scene2Instance->checkLavaCollision(player.position.x, player.position.z, 0.2f)) {
            // Player is walking on lava (no falling, lava is at ground level)
            isPlayerBurning = true;
            
            // Create flame particles around player
            static float flameSpawnTimer = 0.0f;
            flameSpawnTimer += deltaTime;
            if (flameSpawnTimer >= 0.05f) {  // Spawn flames frequently
                for (int i = 0; i < 3; i++) {
                    Flame flame;
                    float angle = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
                    float radius = ((float)rand() / RAND_MAX) * 0.3f;
                    flame.position = Vector3(
                        player.position.x + cos(angle) * radius,
                        player.position.y + ((float)rand() / RAND_MAX) * 0.5f,
                        player.position.z + sin(angle) * radius
                    );
                    flame.lifetime = 0.5f + ((float)rand() / RAND_MAX) * 0.5f;
                    flame.velocity = Vector3(
                        (((float)rand() / RAND_MAX) - 0.5f) * 0.3f,
                        1.0f + ((float)rand() / RAND_MAX) * 1.0f,
                        (((float)rand() / RAND_MAX) - 0.5f) * 0.3f
                    );
                    flame.size = 0.1f + ((float)rand() / RAND_MAX) * 0.1f;
                    flames.push_back(flame);
                }
                flameSpawnTimer = 0.0f;
            }
            
            // Apply lava damage (0.5 life per second)
            scene2Instance->lavaDamageTimer += deltaTime;
            if (scene2Instance->lavaDamageTimer >= 1.0f) {
                lives -= 0.5f;
                scene2Instance->lavaDamageTimer = 0.0f;
                trapDamageCooldown = 1.5f;  // Trigger damage flash
                playDamageSound();  // Play damage sound
                std::cout << "BURNING! Lava damage! Lives remaining: " << lives << std::endl;
                if (lives <= 0) {
                    std::cout << "GAME OVER! You burned in lava!" << std::endl;
                    lives = 0;
                    if (!gameOverSoundPlayed) {
                        playGameOverSound();  // Play game over sound
                        gameOverSoundPlayed = true;
                    }
                }
            }
        } else {
            // Not in lava - reset damage timer
            isPlayerBurning = false;
            scene2Instance->lavaDamageTimer = 0.0f;
        }
    }
    
    // Check for trap damage in Scene 2 (traps don't block, but damage on contact)
    if (currentScene == 2 && trapDamageCooldown <= 0) {
        if (scene2Instance) {
            if (scene2Instance->checkTrapCollision(player.position.x, player.position.z, 0.3f)) {
                lives -= 1.0f;
                trapDamageCooldown = 1.5f;  // 1.5 second cooldown before taking damage again
                playDamageSound();  // Play damage sound
                std::cout << "OUCH! Trap damage! Lives remaining: " << lives << std::endl;
                if (lives <= 0) {
                    std::cout << "GAME OVER! You ran out of lives!" << std::endl;
                    lives = 0.0f;  // Clamp to 0
                    if (!gameOverSoundPlayed) {
                        playGameOverSound();  // Play game over sound
                        gameOverSoundPlayed = true;
                    }
                }
            }
        }
    }
    
    // Check for crystal collection in Scene 2
    if (currentScene == 2 && scene2Instance && !gameWon) {
        for (auto& crystal : scene2Instance->crystals) {
            if (!crystal.collected) {
                float dx = player.position.x - crystal.position.x;
                float dz = player.position.z - crystal.position.z;
                float dist = sqrt(dx*dx + dz*dz);
                if (dist < 1.0f) {  // Collection radius
                    crystal.collected = true;
                    crystalsCollected++;
                    score += 50;
                    playCrystalSound();  // Play crystal collection sound
                    std::cout << "*** CRYSTAL COLLECTED! (" << crystalsCollected << "/10) ***" << std::endl;
                    
                    // Create sparkle effect
                    for (int i = 0; i < 20; i++) {
                        Sparkle sparkle;
                        sparkle.position = crystal.position;
                        sparkle.lifetime = 1.0f + (rand() % 100) / 100.0f;
                        sparkle.velocityY = 2.0f + (rand() % 100) / 50.0f;
                        sparkle.size = 0.1f + (rand() % 50) / 100.0f;
                        sparkles.push_back(sparkle);
                    }
                    
                    if (crystalsCollected >= 10) {
                        gameWon = true;
                        if (!gameWonSoundPlayed) {
                            playGameWinSound();  // Play game win sound
                            gameWonSoundPlayed = true;
                        }
                        std::cout << "\\n\\n*** YOU WIN! ALL CRYSTALS COLLECTED! ***\\n\\n" << std::endl;
                    }
                }
            }
        }
    }
    
    // Update sparkle particles
    for (auto it = sparkles.begin(); it != sparkles.end(); ) {
        it->lifetime -= deltaTime;
        it->position.y += it->velocityY * deltaTime;
        it->velocityY -= 5.0f * deltaTime;  // Gravity
        if (it->lifetime <= 0.0f) {
            it = sparkles.erase(it);
        } else {
            ++it;
        }
    }
    
    // Update flame particles
    for (auto it = flames.begin(); it != flames.end(); ) {
        it->lifetime -= deltaTime;
        it->position.x += it->velocity.x * deltaTime;
        it->position.y += it->velocity.y * deltaTime;
        it->position.z += it->velocity.z * deltaTime;
        it->velocity.y -= 0.5f * deltaTime;  // Light gravity for flames
        if (it->lifetime <= 0.0f) {
            it = flames.erase(it);
        } else {
            ++it;
        }
    }
    
    // Handle portal teleportation
    handlePortalTeleport();

    // Update current scene
    if (currentScenePtr) {
        currentScenePtr->update(0.016f);
    }
    
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0); // ~60 FPS
}

void initOpenGL() {
    // Set background color to light blue sky
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    
    // Enable backface culling for performance
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // Enable lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    
    // Enable color material
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    
    // Enable smooth shading
    glShadeModel(GL_SMOOTH);
    
    // Performance optimization hints
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_FASTEST);
    glHint(GL_FOG_HINT, GL_FASTEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Normalize normals (required when scaling)
    glEnable(GL_NORMALIZE);
    
    // Disable expensive features we don't need
    glDisable(GL_FOG);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POLYGON_SMOOTH);
    glDisable(GL_DITHER);
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "==================================" << std::endl;
    std::cout << "  Crystal Caves - OpenGL Project  " << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  1 - Third Person View" << std::endl;
    std::cout << "  2 - First Person View" << std::endl;
    std::cout << "  3 - Scene 2 (Cave)" << std::endl;
    std::cout << "  4 - Scene 1 (Forest)" << std::endl;
    std::cout << "  T - Toggle View" << std::endl;
    std::cout << "  F - Toggle Fullscreen" << std::endl;
    std::cout << "  WASD - Move" << std::endl;
    std::cout << "  Mouse - Look around" << std::endl;
    std::cout << "  Left Click - Interact (chest)" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;
    std::cout << std::endl;
    
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Crystal Caves - OpenGL Graphics Project");
    
    // Hide the mouse cursor - use crosshair instead
    glutSetCursor(GLUT_CURSOR_NONE);
    
    // Initialize OpenGL settings
    initOpenGL();
    
    // Initialize scenes
    initScenes();
    
    // Register callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeys);
    glutMouseFunc(mouseClick);  // Mouse click for chest interaction
    glutMotionFunc(mouseMotion);
    glutPassiveMotionFunc(mousePassiveMotion);
    glutTimerFunc(0, timer, 0);
    
    // Start main loop
    glutMainLoop();
    
    return 0;
}
