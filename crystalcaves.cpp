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

class Model3DS {
public:
    std::vector<Vector3> vertices;
    std::vector<std::vector<int>> faces;
    
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    
    GLuint displayList;
    bool hasDisplayList;
    bool isLoaded;
    std::string name;
    
    Model3DS() : hasDisplayList(false), isLoaded(false), displayList(0) {
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
                glVertex3f(v0.x, v0.y, v0.z);
                glVertex3f(v1.x, v1.y, v1.z);
                glVertex3f(v2.x, v2.y, v2.z);
                
                glEnd();
            }
        }
        
        glEndList();
        hasDisplayList = true;
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
// COLLECTIBLE CLASS - For game collectibles
// ============================================================================

struct Collectible {
    OBJModel* model;
    Vector3 position;
    float rotationY;
    float bobOffset;
    bool collected;
    int points;
    std::string type; // "crystal", "gem", "coin", etc.
    float glowColor[3];
    
    Collectible() : model(nullptr), rotationY(0), bobOffset(0), collected(false), points(10) {
        glowColor[0] = 1.0f; glowColor[1] = 1.0f; glowColor[2] = 0.0f;
    }
    
    void update(float deltaTime, float gameTime) {
        if (collected) return;
        rotationY += deltaTime * 90.0f; // Rotate 90 degrees per second
        bobOffset = sin(gameTime * 3.0f) * 0.2f; // Bob up and down
    }
    
    void render() {
        if (collected) return;
        
        glPushMatrix();
        glTranslatef(position.x, position.y + bobOffset + 0.5f, position.z);
        glRotatef(rotationY, 0.0f, 1.0f, 0.0f);
        
        // Glow effect
        GLfloat emission[] = { glowColor[0] * 0.3f, glowColor[1] * 0.3f, glowColor[2] * 0.3f, 1.0f };
        glMaterialfv(GL_FRONT, GL_EMISSION, emission);
        
        if (model && model->isLoaded) {
            model->render();
        } else {
            // Fallback: draw a sphere if no model
            glColor3f(glowColor[0], glowColor[1], glowColor[2]);
            glutSolidSphere(0.3, 16, 16);
        }
        
        // Reset emission
        GLfloat noEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glMaterialfv(GL_FRONT, GL_EMISSION, noEmission);
        
        glPopMatrix();
    }
    
    // Check collision with player
    bool checkCollision(float playerX, float playerY, float playerZ, float radius = 1.0f) {
        if (collected) return false;
        float dx = position.x - playerX;
        float dy = position.y - playerY;
        float dz = position.z - playerZ;
        float dist = sqrt(dx*dx + dy*dy + dz*dz);
        return dist < radius;
    }
};

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
float mouseSensitivity = 0.1f;

// Key states for continuous movement
bool keyW = false;
bool keyA = false;
bool keyS = false;
bool keyD = false;

// Current scene (1 or 2)
int currentScene = 1;

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
    
    Player() : position(0.0f, 0.0f, 5.0f), yaw(0.0f), pitch(0.0f), isFirstPerson(false), radius(0.3f) {}
    
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
    
    // This will be overridden by checking scene objects
    bool hasCollision(float targetX, float targetZ) {
        // Pig collision (dynamic position)
        // Note: We'll need to access the pig position from Scene1
        // For now, disable pig collision since it moves
        // if (checkCollision(targetX, targetZ, pigPosition.x, pigPosition.z, 0.8f)) {
        //     return true;
        // }
        
        // Procedural trees collision
        float treePositions[][2] = {
            {-8.0f, -8.0f}, {0.0f, -12.0f}, {8.0f, -8.0f},
            {12.0f, 0.0f}, {8.0f, 8.0f},
            {0.0f, 12.0f}, {-8.0f, 8.0f}, {-12.0f, 0.0f},
            {-15.0f, -15.0f}, {15.0f, -15.0f}, {15.0f, 15.0f}, {-15.0f, 15.0f}
        };
        
        for (int i = 0; i < 12; i++) {
            if (checkCollision(targetX, targetZ, treePositions[i][0], treePositions[i][1], 0.5f)) {
                return true;
            }
        }
        
        return false;
    }
    
    void rotate(float dYaw, float dPitch) {
        yaw += dYaw;
        pitch += dPitch;
        
        // Clamp pitch
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
        glRotatef(yaw, 0.0f, 1.0f, 0.0f);
        
        // Draw simple player model (capsule/cylinder)
        // Body
        glColor3f(0.2f, 0.2f, 0.8f); // Blue player
        glPushMatrix();
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        GLUquadric* quad = gluNewQuadric();
        gluCylinder(quad, 0.3f, 0.3f, 1.5f, 16, 4);
        gluDeleteQuadric(quad);
        glPopMatrix();
        
        // Head
        glColor3f(0.8f, 0.6f, 0.5f); // Skin tone
        glPushMatrix();
        glTranslatef(0.0f, 1.7f, 0.0f);
        glutSolidSphere(0.25f, 16, 16);
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
            // Third person camera orbits around player
            float distance = 5.0f;
            float heightOffset = 2.5f;
            
            // Position camera behind and above the player based on yaw and pitch
            eye.x = position.x - forward.x * distance;
            eye.y = position.y + heightOffset - forward.y * distance;
            eye.z = position.z - forward.z * distance;
            
            // Look at a point in front of the player
            center.x = position.x + forward.x * 2.0f;
            center.y = position.y + 1.5f + forward.y * 2.0f;
            center.z = position.z + forward.z * 2.0f;
        }
    }
};

Player player;

// Game state
int score = 0;
int health = 100;
bool gameRunning = true;

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
    std::vector<Collectible> collectibles;    // Collectibles in this scene
    
    Scene(const std::string& sceneName) : name(sceneName) {
        ambientLight[0] = 0.2f;
        ambientLight[1] = 0.2f;
        ambientLight[2] = 0.3f;
        ambientLight[3] = 1.0f;
    }
    
    virtual ~Scene() {
        // Models are managed by ModelManager, don't delete here
        sceneModels.clear();
        collectibles.clear();
    }
    
    virtual void init() = 0;
    virtual void render() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void cleanup() = 0;
    
    // Helper to add a model to the scene
    void addModel(OBJModel* model) {
        if (model) sceneModels.push_back(model);
    }
    
    // Helper to add a collectible
    void addCollectible(const Collectible& c) {
        collectibles.push_back(c);
    }
    
    // Render all collectibles
    void renderCollectibles() {
        for (auto& c : collectibles) {
            c.render();
        }
    }
    
    // Update all collectibles
    void updateCollectibles(float deltaTime, float gameTime) {
        for (auto& c : collectibles) {
            c.update(deltaTime, gameTime);
        }
    }
};

// ============================================================================
// SCENE 1: Crystal Cave Entrance
// ============================================================================

class Scene1_CaveEntrance : public Scene {
private:
    // Scene-specific model pointers for easy access
    OBJModel* caveModel;
    OBJModel* crystalModel;
    OBJModel* entranceRocksModel;
    OBJModel* pigModel;  // Pink pig model
    OBJModel* minecraftTree;  // Minecraft tree model
    OBJModel* hedgeModel;  // Hedge model for border
    Model3DS* model3ds;  // 3DS model
    
    // Pig AI variables
    Vector3 pigPosition;
    float pigRotation;
    float pigFollowDistance;
    float pigMoveSpeed;
    
    // Minecraft tree instances for the forest
    struct MinecraftTreeInstance {
        float x, z;
        float scale;
        float yOffset;  // Height offset based on scale
    };
    std::vector<MinecraftTreeInstance> minecraftTrees;
    
    // Hedge instances for border
    struct HedgeInstance {
        float x, z;
        float rotation;  // Rotation angle in degrees
    };
    std::vector<HedgeInstance> hedgeInstances;
    
public:
    Scene1_CaveEntrance() : Scene("Enchanted Forest"), 
                            caveModel(nullptr), crystalModel(nullptr), entranceRocksModel(nullptr), 
                            pigModel(nullptr), minecraftTree(nullptr), hedgeModel(nullptr), model3ds(nullptr),
                            pigPosition(0.0f, 0.0f, -5.0f), pigRotation(0.0f),
                            pigFollowDistance(2.0f), pigMoveSpeed(0.05f) {
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
        
        // crystalModel = modelManager.loadModel("crystal_blue", "models/crystal.obj");
        // if (crystalModel) {
        //     crystalModel->setPosition(3, 0, -5);
        //     crystalModel->setUniformScale(0.5f);
        //     addModel(crystalModel);
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
        
        // Load the 3DS model
        model3ds = new Model3DS();
        if (model3ds->load("models/3ds.3ds")) {
            std::cout << "3DS model loaded successfully!" << std::endl;
            model3ds->setPosition(5.0f, 1.5f, 0.0f);  // Position above the floor
            model3ds->setUniformScale(0.0005f);  // 100x smaller (0.05 / 100)
        } else {
            std::cout << "Failed to load 3DS model!" << std::endl;
            delete model3ds;
            model3ds = nullptr;
        }
        
        // Load the hedge model for the border
        hedgeModel = modelManager.loadModel("hedge", "models/Hedge.obj");
        if (hedgeModel) {
            std::cout << "Hedge model loaded successfully!" << std::endl;
        } else {
            std::cout << "Failed to load hedge model!" << std::endl;
        }
        
        // Note: Creeper.fbx cannot be loaded (FBX format not supported)
        // Convert to OBJ format using Blender to use it
        
        // Generate forest trees
        generateForest();
        
        // Generate hedge border
        generateHedgeBorder();
        
        // =====================================================
        // ADD COLLECTIBLES
        // =====================================================
        // Example: Add gem collectibles
        
        // Create some placeholder collectibles
        Collectible gem1;
        gem1.position = Vector3(2.0f, 0.5f, -3.0f);
        gem1.points = 100;
        gem1.type = "gem";
        gem1.glowColor[0] = 0.0f; gem1.glowColor[1] = 1.0f; gem1.glowColor[2] = 0.5f; // Green
        addCollectible(gem1);
        
        Collectible gem2;
        gem2.position = Vector3(-3.0f, 0.5f, -1.0f);
        gem2.points = 50;
        gem2.type = "crystal";
        gem2.glowColor[0] = 0.3f; gem2.glowColor[1] = 0.5f; gem2.glowColor[2] = 1.0f; // Blue
        addCollectible(gem2);
        
        Collectible coin1;
        coin1.position = Vector3(0.0f, 0.5f, -6.0f);
        coin1.points = 25;
        coin1.type = "coin";
        coin1.glowColor[0] = 1.0f; coin1.glowColor[1] = 0.85f; coin1.glowColor[2] = 0.0f; // Gold
        addCollectible(coin1);
        
        std::cout << "Scene 1 initialized with " << collectibles.size() << " collectibles" << std::endl;
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
        
        // Draw grass ground
        glPushMatrix();
        GLfloat grassDiffuse[] = { 0.2f, 0.5f, 0.15f, 1.0f };  // Forest green
        GLfloat grassAmbient[] = { 0.1f, 0.25f, 0.08f, 1.0f };
        glMaterialfv(GL_FRONT, GL_DIFFUSE, grassDiffuse);
        glMaterialfv(GL_FRONT, GL_AMBIENT, grassAmbient);
        glColor3f(0.2f, 0.5f, 0.15f);
        glBegin(GL_QUADS);
            glNormal3f(0.0f, 1.0f, 0.0f);
            glVertex3f(-50.0f, 0.0f, -50.0f);
            glVertex3f(-50.0f, 0.0f, 50.0f);
            glVertex3f(50.0f, 0.0f, 50.0f);
            glVertex3f(50.0f, 0.0f, -50.0f);
        glEnd();
        glPopMatrix();
        
        // Render hedge border
        renderHedgeBorder();
        
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
            glScalef(0.5f, 0.5f, 0.5f);
            
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
        
        // Render the 3DS model
        if (model3ds) {
            GLfloat modelDiffuse[] = { 0.7f, 0.5f, 0.3f, 1.0f };  // Brown/tan color
            GLfloat modelAmbient[] = { 0.3f, 0.2f, 0.15f, 1.0f };
            GLfloat modelSpecular[] = { 0.5f, 0.5f, 0.5f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, modelDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, modelAmbient);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, modelSpecular);
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0f);
            glColor3f(0.7f, 0.5f, 0.3f);
            
            model3ds->render();
        }
        
        // Draw some mushrooms and forest details
        drawForestDetails();
        
        // Render collectibles
        renderCollectibles();
        
        // Draw scene label
        drawSceneLabel();
    }
    
    void update(float deltaTime) override {
        // Update collectibles (rotation, bobbing)
        updateCollectibles(deltaTime, animationTime);
        
        // Update pig AI - follow player
        updatePigAI();
    }
    
    void cleanup() override {
        std::cout << "Cleaning up Scene 1" << std::endl;
        minecraftTrees.clear();
        hedgeInstances.clear();
        if (model3ds) {
            delete model3ds;
            model3ds = nullptr;
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
    
    void generateHedgeBorder() {
        hedgeInstances.clear();
        
        // Hedge model dimensions (raw):
        // X: -116 to 114 (width ~231)
        // Y: -40 to 40 (height ~80)  
        // Z: -6 to 154 (depth ~160)
        
        // OPTIMIZATION: Larger scale = fewer hedge segments needed
        const float hedgeScale = 0.05f;  // Doubled scale for fewer segments
        const float hedgeLength = 160.0f * hedgeScale;  // ~8 units per hedge segment
        const float borderDistance = 30.0f;  // Distance from center to border
        
        // Create hedge border around the playing area
        // Each hedge segment is ~8 units long now
        
        // North side (positive Z) - hedges facing inward
        for (float x = -borderDistance; x < borderDistance; x += hedgeLength) {
            HedgeInstance h;
            h.x = x + hedgeLength/2;
            h.z = borderDistance;
            h.rotation = 180.0f;  // Face inward
            hedgeInstances.push_back(h);
        }
        
        // South side (negative Z) - hedges facing inward
        for (float x = -borderDistance; x < borderDistance; x += hedgeLength) {
            HedgeInstance h;
            h.x = x + hedgeLength/2;
            h.z = -borderDistance;
            h.rotation = 0.0f;  // Face inward
            hedgeInstances.push_back(h);
        }
        
        // East side (positive X) - hedges rotated 90 degrees
        for (float z = -borderDistance; z < borderDistance; z += hedgeLength) {
            HedgeInstance h;
            h.x = borderDistance;
            h.z = z + hedgeLength/2;
            h.rotation = 90.0f;  // Rotated to face inward
            hedgeInstances.push_back(h);
        }
        
        // West side (negative X) - hedges rotated -90 degrees
        for (float z = -borderDistance; z < borderDistance; z += hedgeLength) {
            HedgeInstance h;
            h.x = -borderDistance;
            h.z = z + hedgeLength/2;
            h.rotation = -90.0f;  // Rotated to face inward
            hedgeInstances.push_back(h);
        }
        
        std::cout << "Generated " << hedgeInstances.size() << " hedge segments for border" << std::endl;
    }
    
    void renderHedgeBorder() {
        if (!hedgeModel || !hedgeModel->hasDisplayList) return;
        
        // Hedge scale and Y offset (must match generateHedgeBorder)
        const float hedgeScale = 0.05f;
        // Hedge model has Y from -40 to 40, so center is at 0
        // Bottom of hedge is at -40 * scale, so we add that to put on ground
        const float hedgeYOffset = 40.0f * hedgeScale;
        
        for (const auto& hedge : hedgeInstances) {
            glPushMatrix();
            
            glTranslatef(hedge.x, hedgeYOffset, hedge.z);
            glRotatef(hedge.rotation, 0.0f, 1.0f, 0.0f);
            glScalef(hedgeScale, hedgeScale, hedgeScale);
            
            // Use display list for performance
            glCallList(hedgeModel->displayList);
            
            glPopMatrix();
        }
        glDisable(GL_TEXTURE_2D);
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
    
    void drawForestDetails() {
        // Draw fewer mushrooms for performance
        float mushroomPositions[][2] = {
            {-3.0f, -2.0f}, {4.0f, 3.0f}, {-2.0f, 5.0f}
        };
        
        int numMushrooms = sizeof(mushroomPositions) / sizeof(mushroomPositions[0]);
        
        for (int i = 0; i < numMushrooms; i++) {
            drawMushroom(mushroomPositions[i][0], mushroomPositions[i][1], i % 2 == 0);
        }
        
        // Draw fewer rocks
        float rockPositions[][2] = {
            {-7.0f, -5.0f}, {7.0f, 2.0f}
        };
        
        for (int i = 0; i < 4; i++) {
            drawRock(rockPositions[i][0], rockPositions[i][1], 0.5f + (i % 3) * 0.3f);
        }
    }
    
    void drawMushroom(float x, float z, bool isRed) {
        glPushMatrix();
        glTranslatef(x, 0.0f, z);
        
        // Stem
        GLfloat stemDiffuse[] = { 0.9f, 0.85f, 0.75f, 1.0f };
        glMaterialfv(GL_FRONT, GL_DIFFUSE, stemDiffuse);
        glColor3f(0.9f, 0.85f, 0.75f);
        
        glPushMatrix();
        glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
        GLUquadric* quad = gluNewQuadric();
        gluCylinder(quad, 0.08f, 0.06f, 0.25f, 8, 1);
        gluDeleteQuadric(quad);
        glPopMatrix();
        
        // Cap
        glTranslatef(0.0f, 0.25f, 0.0f);
        if (isRed) {
            GLfloat capDiffuse[] = { 0.9f, 0.2f, 0.15f, 1.0f };  // Red mushroom
            glMaterialfv(GL_FRONT, GL_DIFFUSE, capDiffuse);
            glColor3f(0.9f, 0.2f, 0.15f);
        } else {
            GLfloat capDiffuse[] = { 0.7f, 0.5f, 0.3f, 1.0f };  // Brown mushroom
            glMaterialfv(GL_FRONT, GL_DIFFUSE, capDiffuse);
            glColor3f(0.7f, 0.5f, 0.3f);
        }
        
        glPushMatrix();
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
        glutSolidCone(0.2f, 0.15f, 10, 2);
        glPopMatrix();
        
        glPopMatrix();
    }
    
    void drawRock(float x, float z, float size) {
        glPushMatrix();
        glTranslatef(x, size * 0.3f, z);
        
        GLfloat rockDiffuse[] = { 0.5f, 0.48f, 0.45f, 1.0f };
        GLfloat rockAmbient[] = { 0.25f, 0.24f, 0.22f, 1.0f };
        glMaterialfv(GL_FRONT, GL_DIFFUSE, rockDiffuse);
        glMaterialfv(GL_FRONT, GL_AMBIENT, rockAmbient);
        glColor3f(0.5f, 0.48f, 0.45f);
        
        // Squashed sphere for rock
        glScalef(1.0f, 0.6f, 0.8f);
        glutSolidSphere(size, 8, 6);
        
        glPopMatrix();
    }
    
    void drawSceneLabel() {
        // Display scene name (for debugging)
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos3f(0.0f, 5.0f, 0.0f);
    }
    
    void updatePigAI() {
        // Calculate direction from pig to player
        float dx = player.position.x - pigPosition.x;
        float dz = player.position.z - pigPosition.z;
        float distance = sqrt(dx * dx + dz * dz);
        
        // Only move if pig is farther than desired follow distance
        if (distance > pigFollowDistance) {
            // Normalize direction
            float dirX = dx / distance;
            float dirZ = dz / distance;
            
            // Move pig towards player
            pigPosition.x += dirX * pigMoveSpeed;
            pigPosition.z += dirZ * pigMoveSpeed;
            
            // Calculate rotation to face player
            pigRotation = atan2(dx, -dz) * 180.0f / M_PI;
        }
    }
};

// ============================================================================
// SCENE 2: Deep Crystal Cavern
// ============================================================================

class Scene2_DeepCavern : public Scene {
private:
    OBJModel* deepCaveModel;
    OBJModel* largeCrystalModel;
    OBJModel* stalactiteModel;
    
public:
    Scene2_DeepCavern() : Scene("Deep Crystal Cavern"),
                          deepCaveModel(nullptr), largeCrystalModel(nullptr), stalactiteModel(nullptr) {
        // Darker ambient for deep cave
        ambientLight[0] = 0.1f;
        ambientLight[1] = 0.1f;
        ambientLight[2] = 0.2f;
    }
    
    void init() override {
        std::cout << "Initializing Scene 2: " << name << std::endl;
        
        // =====================================================
        // LOAD YOUR 3D MODELS FOR SCENE 2 HERE
        // =====================================================
        // Example:
        // deepCaveModel = modelManager.loadModel("deep_cave", "models/deep_cave.obj");
        // largeCrystalModel = modelManager.loadModel("large_crystal", "models/large_crystal.obj");
        
        // =====================================================
        // ADD COLLECTIBLES FOR SCENE 2
        // =====================================================
        
        // Rare purple gem
        Collectible rareGem;
        rareGem.position = Vector3(0.0f, 0.5f, -5.0f);
        rareGem.points = 500;
        rareGem.type = "rare_gem";
        rareGem.glowColor[0] = 0.8f; rareGem.glowColor[1] = 0.2f; rareGem.glowColor[2] = 0.9f;
        addCollectible(rareGem);
        
        // Blue crystals
        Collectible crystal1;
        crystal1.position = Vector3(-4.0f, 0.5f, -3.0f);
        crystal1.points = 150;
        crystal1.type = "crystal";
        crystal1.glowColor[0] = 0.2f; crystal1.glowColor[1] = 0.4f; crystal1.glowColor[2] = 1.0f;
        addCollectible(crystal1);
        
        Collectible crystal2;
        crystal2.position = Vector3(4.0f, 0.5f, 2.0f);
        crystal2.points = 150;
        crystal2.type = "crystal";
        crystal2.glowColor[0] = 0.2f; crystal2.glowColor[1] = 0.4f; crystal2.glowColor[2] = 1.0f;
        addCollectible(crystal2);
        
        std::cout << "Scene 2 initialized with " << collectibles.size() << " collectibles" << std::endl;
    }
    
    void render() override {
        // Set scene-specific lighting (darker, more mysterious)
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientLight);
        
        // Crystal glow lights (multiple point lights)
        GLfloat light1Pos[] = { -5.0f, 3.0f, -5.0f, 1.0f };
        GLfloat light1Diffuse[] = { 0.3f, 0.5f, 0.8f, 1.0f }; // Blue glow
        
        GLfloat light2Pos[] = { 5.0f, 3.0f, 5.0f, 1.0f };
        GLfloat light2Diffuse[] = { 0.8f, 0.3f, 0.8f, 1.0f }; // Purple glow
        
        glLightfv(GL_LIGHT0, GL_POSITION, light1Pos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light1Diffuse);
        
        glEnable(GL_LIGHT1);
        glLightfv(GL_LIGHT1, GL_POSITION, light2Pos);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, light2Diffuse);
        
        // Draw ground plane (darker cave floor)
        glPushMatrix();
        glColor3f(0.15f, 0.12f, 0.1f); // Dark brown cave floor
        glBegin(GL_QUADS);
            glNormal3f(0.0f, 1.0f, 0.0f);
            glVertex3f(-20.0f, 0.0f, -20.0f);
            glVertex3f(-20.0f, 0.0f, 20.0f);
            glVertex3f(20.0f, 0.0f, 20.0f);
            glVertex3f(20.0f, 0.0f, -20.0f);
        glEnd();
        glPopMatrix();
        
        // Render all loaded OBJ models
        for (auto* model : sceneModels) {
            if (model) model->render();
        }
        
        // Draw larger crystal formations (placeholders)
        drawLargeCrystalFormation(-5.0f, 0.0f, -5.0f, 0.2f, 0.4f, 0.9f); // Blue crystals
        drawLargeCrystalFormation(5.0f, 0.0f, 5.0f, 0.9f, 0.2f, 0.9f);   // Magenta crystals
        drawLargeCrystalFormation(0.0f, 0.0f, -8.0f, 0.2f, 0.9f, 0.9f);  // Cyan crystals
        
        // Render collectibles
        renderCollectibles();
        
        glDisable(GL_LIGHT1);
    }
    
    void update(float deltaTime) override {
        // Update collectibles
        updateCollectibles(deltaTime, animationTime);
    }
    
    void cleanup() override {
        std::cout << "Cleaning up Scene 2" << std::endl;
        // TODO: Unload models specific to this scene
    }
    
private:
    void drawLargeCrystalFormation(float x, float y, float z, float r, float g, float b) {
        // Draw a cluster of crystals
        for (int i = 0; i < 5; i++) {
            float offsetX = (i % 3 - 1) * 0.8f;
            float offsetZ = (i / 3 - 0.5f) * 0.8f;
            float height = 2.0f + (i % 2) * 1.5f;
            float angle = i * 15.0f;
            
            glPushMatrix();
            glTranslatef(x + offsetX, y, z + offsetZ);
            glRotatef(angle, 0.0f, 0.0f, 1.0f);
            glRotatef(animationTime * 10.0f + i * 30.0f, 0.0f, 1.0f, 0.0f);
            
            // Pulsing glow effect
            float pulse = 0.5f + 0.5f * sin(animationTime * 2.0f + i);
            
            GLfloat matEmission[] = { r * 0.4f * pulse, g * 0.4f * pulse, b * 0.4f * pulse, 1.0f };
            GLfloat matDiffuse[] = { r, g, b, 0.9f };
            
            glMaterialfv(GL_FRONT, GL_EMISSION, matEmission);
            glMaterialfv(GL_FRONT, GL_DIFFUSE, matDiffuse);
            
            glColor4f(r, g, b, 0.9f);
            glutSolidCone(0.4, height, 6, 4);
            
            GLfloat noEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
            glMaterialfv(GL_FRONT, GL_EMISSION, noEmission);
            
            glPopMatrix();
        }
    }
};

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
}

void switchScene(int sceneNumber) {
    if (sceneNumber == currentScene) return;
    
    std::cout << "Switching to Scene " << sceneNumber << std::endl;
    
    if (sceneNumber == 1) {
        currentScenePtr = scene1;
        currentScene = 1;
    } else if (sceneNumber == 2) {
        currentScenePtr = scene2;
        currentScene = 2;
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
    
    // Re-enable lighting and depth test
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    
    // Restore matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ============================================================================
// OPENGL CALLBACKS
// ============================================================================

void display() {
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
    
    // Render HUD on top
    renderHUD();
    
    glutSwapBuffers();
}

void reshape(int w, int h) {
    windowWidth = w;
    windowHeight = h;
    
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, (float)w / (float)h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
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
    
    glutPostRedisplay();
}

void mousePassiveMotion(int x, int y) {
    // Same as mouseMotion for passive movement
    mouseMotion(x, y);
}

void timer(int value) {
    // Update animation time
    animationTime += 0.016f; // Approximately 60 FPS
    
    // Handle continuous movement based on key states
    float moveSpeed = 0.15f; // Slightly reduced for smoother frame-by-frame movement
    float forward = 0.0f;
    float right = 0.0f;
    
    if (keyW) forward += moveSpeed;
    if (keyS) forward -= moveSpeed;
    if (keyD) right += moveSpeed;
    if (keyA) right -= moveSpeed;
    
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
    std::cout << "  WASD - Move" << std::endl;
    std::cout << "  Mouse - Look around" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;
    std::cout << std::endl;
    
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Crystal Caves - OpenGL Graphics Project");
    
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
    glutMotionFunc(mouseMotion);
    glutPassiveMotionFunc(mousePassiveMotion);
    glutTimerFunc(0, timer, 0);
    
    // Start main loop
    glutMainLoop();
    
    return 0;
}
