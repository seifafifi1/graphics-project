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
int lives = 5;
float trapDamageCooldown = 0.0f;  // Cooldown to prevent rapid damage
bool gameRunning = true;
bool hasKey = false;  // Whether player has collected the key

// Chest state
Vector3 chestPosition(8.0f, 0.0f, -8.0f);  // Chest location in Scene 1
bool chestOpened = false;  // Whether chest has been opened

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
    OBJModel* hedgeModel;  // Hedge model for border
    OBJModel* dogModel;  // Dog OBJ model
    Model3DS* flockModel;  // Flock 3DS model
    GLuint grassTexture;  // Floor grass texture
    GLuint flockTexture;  // Flock/bird texture
    
    // Dog position and AI
    Vector3 dogPosition;
    float dogRotation;
    float dogWanderTime;
    Vector3 dogTargetPosition;
    float dogMoveSpeed;
    
    // Flock position (birds flying)
    Vector3 flockPosition;
    float flockRotation;
    float flockTime;  // For animation
    
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
    
    // Boulder instances
    struct BoulderInstance {
        float x, y, z;
        float scale;
        float rotationY;
    };
    std::vector<BoulderInstance> boulders;
    GLuint stoneTexture;  // Stone texture for boulders
    
public:
    Scene1_CaveEntrance() : Scene("Enchanted Forest"), 
                            caveModel(nullptr), crystalModel(nullptr), entranceRocksModel(nullptr), 
                            pigModel(nullptr), minecraftTree(nullptr), hedgeModel(nullptr), 
                            dogModel(nullptr), flockModel(nullptr),
                            grassTexture(0), stoneTexture(0), flockTexture(0),
                            dogPosition(-10.0f, 0.0f, 10.0f), dogRotation(0.0f),
                            dogWanderTime(0.0f), dogTargetPosition(-10.0f, 0.0f, 10.0f), dogMoveSpeed(0.03f),
                            flockPosition(0.0f, 15.0f, 0.0f), flockRotation(0.0f), flockTime(0.0f),
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
        
        // Load the hedge model for the border
        hedgeModel = modelManager.loadModel("hedge", "models/Hedge.obj");
        if (hedgeModel) {
            std::cout << "Hedge model loaded successfully!" << std::endl;
        } else {
            std::cout << "Failed to load hedge model!" << std::endl;
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
        
        // Load dog model (OBJ with texture)
        dogModel = new OBJModel();
        if (dogModel->load("models/dog.obj")) {
            std::cout << "Dog model loaded successfully!" << std::endl;
        } else {
            std::cout << "Failed to load dog model!" << std::endl;
            delete dogModel;
            dogModel = nullptr;
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
        
        // Generate hedge border
        generateHedgeBorder();
        
        // Generate boulders
        generateBoulders();
        
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
        
        // Set up collision callback for this scene
        scene1Instance = this;
        
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
        
        // Render hedge border
        renderHedgeBorder();
        
        // Render boulders
        renderBoulders();
        
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
            glScalef(1.5f, 1.5f, 1.5f);  // Tripled size
            
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
        
        // Render the dog (standing on ground)
        if (dogModel) {
            glPushMatrix();
            
            // Position dog on the ground
            float dogScale = 0.04f;  // Halved size
            glTranslatef(dogPosition.x, 0.0f, dogPosition.z);
            glRotatef(dogRotation, 0.0f, 1.0f, 0.0f);
            glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);  // Stand upright
            glScalef(dogScale, dogScale, dogScale);
            
            // White material so texture shows properly
            GLfloat dogDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            GLfloat dogAmbient[] = { 0.8f, 0.8f, 0.8f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dogDiffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, dogAmbient);
            glColor3f(1.0f, 1.0f, 1.0f);
            
            dogModel->render();
            glPopMatrix();
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
        
        // Update flock - circle around the scene
        flockTime += deltaTime;
        flockRotation += 0.5f;  // Rotate slowly
        if (flockRotation > 360.0f) flockRotation -= 360.0f;
        
        // Flock flies in a circle high in the sky
        float radius = 25.0f;
        flockPosition.x = radius * cos(flockTime * 0.3f);
        flockPosition.z = radius * sin(flockTime * 0.3f);
        flockPosition.y = 25.0f + 3.0f * sin(flockTime * 0.5f);  // High in sky with up/down motion
        
        // Update dog - wander around randomly
        updateDogAI(deltaTime);
    }
    
    void cleanup() override {
        std::cout << "Cleaning up Scene 1" << std::endl;
        minecraftTrees.clear();
        hedgeInstances.clear();
        boulders.clear();
        if (dogModel) {
            delete dogModel;
            dogModel = nullptr;
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
    
    void updateDogAI(float deltaTime) {
        dogWanderTime += deltaTime;
        
        // Pick a new random target every 3-5 seconds
        if (dogWanderTime > 3.0f + (rand() % 20) / 10.0f) {
            dogWanderTime = 0.0f;
            // Pick random position within bounds (avoid edges)
            dogTargetPosition.x = -15.0f + (rand() % 300) / 10.0f;
            dogTargetPosition.z = -15.0f + (rand() % 300) / 10.0f;
        }
        
        // Move towards target
        float dx = dogTargetPosition.x - dogPosition.x;
        float dz = dogTargetPosition.z - dogPosition.z;
        float distance = sqrt(dx * dx + dz * dz);
        
        if (distance > 0.5f) {
            // Normalize direction
            float dirX = dx / distance;
            float dirZ = dz / distance;
            
            // Move dog towards target
            dogPosition.x += dirX * dogMoveSpeed;
            dogPosition.z += dirZ * dogMoveSpeed;
            
            // Calculate rotation to face movement direction
            dogRotation = atan2(dx, -dz) * 180.0f / M_PI;
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
        
        // Check collision with hedges (border)
        float borderLimit = 38.0f;  // Just inside the hedge border
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
        
        // Check collision with dog
        {
            float dx = x - dogPosition.x;
            float dz = z - dogPosition.z;
            float dist = sqrt(dx * dx + dz * dz);
            if (dist < radius + 0.5f) return true;  // Dog collision radius
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
    
    // Room dimensions
    float roomWidth = 40.0f;
    float roomHeight = 15.0f;
    float roomDepth = 40.0f;
    
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
    Scene2_DeepCavern() : Scene("Dark Stone Dungeon"), stoneTexture(0) {
        // Very dark ambient for dungeon atmosphere
        ambientLight[0] = 0.05f;
        ambientLight[1] = 0.04f;
        ambientLight[2] = 0.03f;
        scene2Instance = this;  // Set global instance for collision callback
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
        for (const auto& stone : stones) {
            float dx = x - stone.position.x;
            float dz = z - stone.position.z;
            float dist = sqrt(dx*dx + dz*dz);
            if (dist < radius + stone.scale * 1.2f) {
                return 1;
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
        
        // Place stones around the dungeon - some small, some 2x, some 4x scale
        stones.push_back({Vector3(-12.0f, 0.0f, -12.0f), 45.0f, 4.0f});   // 4x scale - big boulder
        stones.push_back({Vector3(10.0f, 0.0f, -8.0f), 120.0f, 2.0f});    // 2x scale
        stones.push_back({Vector3(-8.0f, 0.0f, 6.0f), 200.0f, 1.0f});     // normal
        stones.push_back({Vector3(14.0f, 0.0f, 10.0f), 75.0f, 4.0f});     // 4x scale - big boulder
        stones.push_back({Vector3(-5.0f, 0.0f, -15.0f), 300.0f, 2.0f});   // 2x scale
        stones.push_back({Vector3(6.0f, 0.0f, 12.0f), 160.0f, 1.0f});     // normal
        stones.push_back({Vector3(-15.0f, 0.0f, 0.0f), 30.0f, 2.0f});     // 2x scale
        stones.push_back({Vector3(0.0f, 0.0f, -10.0f), 90.0f, 4.0f});     // 4x scale - big boulder
        
        // Place traps in the dungeon (dangerous!)
        traps.push_back({Vector3(-6.0f, 0.0f, -6.0f), 0.0f, 1.5f});
        traps.push_back({Vector3(8.0f, 0.0f, 4.0f), 45.0f, 1.5f});
        traps.push_back({Vector3(-10.0f, 0.0f, 10.0f), 90.0f, 1.5f});
        traps.push_back({Vector3(12.0f, 0.0f, -10.0f), 135.0f, 1.5f});
        traps.push_back({Vector3(0.0f, 0.0f, 8.0f), 180.0f, 1.5f});
        
        // Create torches on the walls
        float torchHeight = 5.0f;
        float halfWidth = roomWidth / 2.0f - 0.5f;
        float halfDepth = roomDepth / 2.0f - 0.5f;
        
        // Torches on north wall (negative Z)
        torches.push_back({Vector3(-10.0f, torchHeight, -halfDepth), 0.0f, 3.5f, 1.0f});
        torches.push_back({Vector3(10.0f, torchHeight, -halfDepth), 1.5f, 4.0f, 1.0f});
        
        // Torches on south wall (positive Z)
        torches.push_back({Vector3(-10.0f, torchHeight, halfDepth), 0.7f, 3.8f, 1.0f});
        torches.push_back({Vector3(10.0f, torchHeight, halfDepth), 2.1f, 3.2f, 1.0f});
        
        // Torches on west wall (negative X)
        torches.push_back({Vector3(-halfWidth, torchHeight, -10.0f), 1.2f, 4.2f, 1.0f});
        torches.push_back({Vector3(-halfWidth, torchHeight, 10.0f), 0.3f, 3.6f, 1.0f});
        
        // Torches on east wall (positive X)
        torches.push_back({Vector3(halfWidth, torchHeight, -10.0f), 1.8f, 3.4f, 1.0f});
        torches.push_back({Vector3(halfWidth, torchHeight, 10.0f), 2.5f, 4.5f, 1.0f});
        
        std::cout << "Scene 2 initialized with " << torches.size() << " torches, " 
                  << stones.size() << " stones, and " << traps.size() << " traps" << std::endl;
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
        
        // Draw torches
        for (const auto& torch : torches) {
            drawTorch(torch);
        }
        
        // Disable extra lights
        for (int i = 0; i < 8; i++) {
            if (i > 0) glDisable(GL_LIGHT0 + i);
        }
    }
    
    void update(float deltaTime) override {
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
    }
    
    void cleanup() override {
        std::cout << "Cleaning up Scene 2" << std::endl;
        if (stoneTexture) {
            glDeleteTextures(1, &stoneTexture);
            stoneTexture = 0;
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
    }
    
private:
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
}

void switchScene(int sceneNumber) {
    if (sceneNumber == currentScene) return;
    
    std::cout << "Switching to Scene " << sceneNumber << std::endl;
    
    if (sceneNumber == 1) {
        currentScenePtr = scene1;
        currentScene = 1;
        sceneCollisionCheck = scene1CollisionCheck;
    } else if (sceneNumber == 2) {
        currentScenePtr = scene2;
        currentScene = 2;
        sceneCollisionCheck = scene2CollisionCheck;  // Collision with stones, traps, walls
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
    
    // Draw lives (hearts) in top right corner
    glColor3f(1.0f, 0.2f, 0.2f);  // Red color for hearts
    std::string livesText = "Lives: ";
    for (int i = 0; i < lives; i++) {
        livesText += "\x03 ";  // Heart symbol (works on some systems)
    }
    // Fallback to numeric display
    livesText = "Lives: " + std::to_string(lives) + "/5";
    glRasterPos2f(windowWidth - 120, windowHeight - 30);
    for (char c : livesText) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    }
    
    // Draw key indicator if player has key
    if (hasKey) {
        glColor3f(1.0f, 0.84f, 0.0f);  // Gold color for key
        std::string keyText = "[KEY]";
        glRasterPos2f(windowWidth - 120, windowHeight - 55);
        for (char c : keyText) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
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
                lives = 5;
                player.position = Vector3(0.0f, 0.0f, 5.0f);
                player.yaw = 0.0f;
                player.pitch = 0.0f;
                trapDamageCooldown = 0.0f;
                hasKey = false;
                chestOpened = false;
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
    
    // Only check for chest interaction in Scene 1
    if (currentScene != 1) return;
    
    // Check if chest is already opened
    if (chestOpened) return;
    
    // Check if player is close enough to the chest
    float dx = player.position.x - chestPosition.x;
    float dz = player.position.z - chestPosition.z;
    float distToChest = sqrt(dx*dx + dz*dz);
    
    // Must be within 4 units of the chest
    if (distToChest > 4.0f) return;
    
    // Check if player is looking at the chest (simple angle check)
    float radYaw = player.yaw * M_PI / 180.0f;
    float lookX = sin(radYaw);
    float lookZ = -cos(radYaw);
    
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
        std::cout << "*** CHEST OPENED! You found a KEY! ***" << std::endl;
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
    
    glutPostRedisplay();
}

void mousePassiveMotion(int x, int y) {
    // Same as mouseMotion for passive movement
    mouseMotion(x, y);
}

void timer(int value) {
    // Update animation time
    animationTime += 0.016f; // Approximately 60 FPS
    
    // Update trap damage cooldown
    if (trapDamageCooldown > 0) {
        trapDamageCooldown -= 0.016f;
    }
    
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
    
    // Check for trap damage in Scene 2 (traps don't block, but damage on contact)
    if (currentScene == 2 && trapDamageCooldown <= 0) {
        if (scene2Instance) {
            if (scene2Instance->checkTrapCollision(player.position.x, player.position.z, 0.3f)) {
                lives--;
                trapDamageCooldown = 1.5f;  // 1.5 second cooldown before taking damage again
                std::cout << "OUCH! Trap damage! Lives remaining: " << lives << std::endl;
                if (lives <= 0) {
                    std::cout << "GAME OVER! You ran out of lives!" << std::endl;
                    lives = 0;  // Clamp to 0
                }
            }
        }
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
