#include "model.h"
#include "mesh.h"
#include "shader.h"
#include "stb_image.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

unsigned int TextureFromFile(const char *path, const std::string &directory);

Model::Model(std::string const &path, bool gamma) : gammaCorrection(gamma) {
  std::cout << "Model constructor called with path: " << path << std::endl;
  try {
    loadModel(path);
    std::cout << "Model loading completed successfully" << std::endl;
  }
  catch (const std::exception& e) { 
    std::cerr << "Exception in Model constructor: " << e.what() << std::endl;
    throw; // Re-throw to allow calling code to handle it
  }
  catch (...) {
    std::cerr << "Unknown exception in Model constructor" << std::endl;
    throw; // Re-throw to allow calling code to handle it
  }
}


void Model::Draw(Shader &shader){
  if (meshes.empty()) {
    std::cerr << "WARNING::MODEL::No meshes to draw" << std::endl;
    return;
  }
  
  try {
    for (unsigned int i=0; i< meshes.size(); i++) {
      meshes[i].Draw(shader);
    }
  }
  catch (const std::exception& e) {
    std::cerr << "ERROR::MODEL::Exception in Draw: " << e.what() << std::endl;
    throw; // Re-throw the exception for higher-level handling
  }
}

void Model::loadModel(std::string const &path){
  std::cout << "loadModel called with path: " << path << std::endl;
  
  // Check if the path is empty
  if (path.empty()) {
    std::string error = "ERROR::MODEL::Empty path provided";
    std::cerr << error << std::endl;
    throw std::runtime_error(error);
  }
  
  std::cout << "Creating Assimp importer..." << std::endl;
  Assimp::Importer importer;
  std::cout << "Reading file with Assimp..." << std::endl;
  const aiScene *scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
  std::cout << "Assimp ReadFile completed" << std::endl;

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode){
    std::string error = "ERROR::ASSIMP::" + std::string(importer.GetErrorString());
    std::cerr << error << std::endl;
    throw std::runtime_error(error);
  }
  
  std::cout << "Extracting directory from path: " << path << std::endl;
  directory = path.substr(0, path.find_last_of('/'));
  std::cout << "Directory extracted: " << directory << std::endl;
  
  // Check if directory was successfully extracted
  if (directory.empty() && path.find('/') != std::string::npos) {
    std::string error = "ERROR::MODEL::Failed to extract directory from path: " + path;
    std::cerr << error << std::endl;
    throw std::runtime_error(error);
  }
  
  try {
    std::cout << "Processing root node..." << std::endl;
    processNode(scene->mRootNode, scene);
    std::cout << "Node processing completed" << std::endl;
  }
  catch (const std::exception& e) {
    std::string error = "ERROR::MODEL::Failed to process nodes: " + std::string(e.what());
    std::cerr << error << std::endl;
    throw std::runtime_error(error);
  }
}

void Model::processNode(aiNode *node, const aiScene *scene)
{
    // process all the node's meshes (if any)
    for(unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]]; 
        meshes.push_back(processMesh(mesh, scene));			
    }
    // then do the same for each of its children
    for(unsigned int i = 0; i < node->mNumChildren; i++)
    {
        Model::processNode(node->mChildren[i], scene);
    }
}  


Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene) {
  std::cout << "processMesh: Starting..." << std::endl;
  
  if (!mesh) {
    throw std::runtime_error("ERROR::MODEL::Null mesh pointer");
  }

  std::cout << "processMesh: Mesh pointer is valid" << std::endl;
  std::cout << "processMesh: Mesh has " << mesh->mNumVertices << " vertices" << std::endl;
  
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Texture> textures;

  // Check if mesh has vertices
  if (mesh->mNumVertices == 0) {
    std::string warning = "WARNING::MODEL::Mesh contains no vertices";
    std::cout << warning << std::endl;
  }

  std::cout << "processMesh: Processing vertices..." << std::endl;
  
  // process vertices from assimp to openGL
  for (unsigned int i=0; i< mesh->mNumVertices; i++) {
    if (i % 1000 == 0) {
      std::cout << "processMesh: Processing vertex " << i << " of " << mesh->mNumVertices << std::endl;
    }
    
    Vertex vertex;
    glm::vec3 vector;
    
    // Check if mesh vertices are valid
    if (mesh->mVertices == nullptr) {
      throw std::runtime_error("ERROR::MODEL::Null mVertices pointer in mesh");
    }
    
    vector.x = mesh->mVertices[i].x;
    vector.y = mesh->mVertices[i].y;
    vector.z = mesh->mVertices[i].z;
    vertex.Position = vector;

    if (mesh->HasNormals())
    {
        if (mesh->mNormals == nullptr) {
          throw std::runtime_error("ERROR::MODEL::Null mNormals pointer in mesh");
        }
        
        vector.x = mesh->mNormals[i].x;
        vector.y = mesh->mNormals[i].y;
        vector.z = mesh->mNormals[i].z;
        vertex.Normal = vector;
    }
    else {
        // Default normal if none provided
        vertex.Normal = glm::vec3(0.0f, 0.0f, 0.0f);
    }

    if(mesh->mTextureCoords[0]){
      // Check that texture coordinates are valid
      if (mesh->mTextureCoords[0] == nullptr) {
        throw std::runtime_error("ERROR::MODEL::Null mTextureCoords pointer in mesh");
      }
      
      glm::vec2 vec;
      vec.x = mesh->mTextureCoords[0][i].x;
      vec.y = mesh->mTextureCoords[0][i].y;
      vertex.TexCoords = vec;
      
      // Only set tangent and bitangent if they exist
      if (mesh->mTangents && mesh->mBitangents) {
        // Check that tangents and bitangents are valid
        if (mesh->mTangents == nullptr) {
          throw std::runtime_error("ERROR::MODEL::Null mTangents pointer in mesh");
        }
        
        if (mesh->mBitangents == nullptr) {
          throw std::runtime_error("ERROR::MODEL::Null mBitangents pointer in mesh");
        }
        
        // tangent
        vector.x = mesh->mTangents[i].x;
        vector.y = mesh->mTangents[i].y;
        vector.z = mesh->mTangents[i].z;
        vertex.Tangent = vector;
        
        // bitangent
        vector.x = mesh->mBitangents[i].x;
        vector.y = mesh->mBitangents[i].y;
        vector.z = mesh->mBitangents[i].z;
        vertex.Bitangent = vector;
      }
    }
    else {
      vertex.TexCoords = glm::vec2(0.0, 0.0);
    }
    vertices.push_back(vertex);
  }

  std::cout << "processMesh: Finished processing vertices" << std::endl;
  std::cout << "processMesh: Processing indices..." << std::endl;

  // Check if the mesh has any faces
  if (mesh->mNumFaces == 0) {
    std::string warning = "WARNING::MODEL::Mesh contains no faces";
    std::cout << warning << std::endl;
  }

  // Check if faces are valid
  if (mesh->mFaces == nullptr && mesh->mNumFaces > 0) {
    throw std::runtime_error("ERROR::MODEL::Null mFaces pointer in mesh");
  }

  // process indices from assimp to openGL format
  for (unsigned int i=0; i<mesh->mNumFaces; i++) {
    if (i % 1000 == 0) {
      std::cout << "processMesh: Processing face " << i << " of " << mesh->mNumFaces << std::endl;
    }
    
    aiFace face = mesh->mFaces[i];
    if (face.mNumIndices != 3) {
      std::cout << "WARNING::MODEL::Face is not a triangle. Indices: " << face.mNumIndices << std::endl;
      continue; // Skip non-triangular faces
    }
    
    // Check if indices are valid
    if (face.mIndices == nullptr) {
      throw std::runtime_error("ERROR::MODEL::Null mIndices pointer in face");
    }
    
    for (unsigned int j=0; j< face.mNumIndices; j++){
      indices.push_back(face.mIndices[j]);
    }
  }

  std::cout << "processMesh: Finished processing indices" << std::endl;
  std::cout << "processMesh: Processing materials..." << std::endl;

  // process material from assimp data struct to our defined OpenGL data structure
  if(mesh->mMaterialIndex >= 0){
    std::cout << "processMesh: Material index: " << mesh->mMaterialIndex << std::endl;
    
    try {
      // Check if materials are valid
      if (scene->mMaterials == nullptr) {
        throw std::runtime_error("ERROR::MODEL::Null mMaterials pointer in scene");
      }
      
      if (mesh->mMaterialIndex >= scene->mNumMaterials) {
        throw std::runtime_error("ERROR::MODEL::Invalid material index: " + 
                                std::to_string(mesh->mMaterialIndex) + 
                                ", max is " + std::to_string(scene->mNumMaterials - 1));
      }
      
      aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
      if (!material) {
        throw std::runtime_error("ERROR::MODEL::Invalid material pointer");
      }
      
      std::cout << "processMesh: Loading diffuse textures..." << std::endl;
      // load diffuse map texture
      std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE,"texture_diffuse");
      textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
      
      std::cout << "processMesh: Loading specular textures..." << std::endl;
      // load specular map texture
      std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
      textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    
    }
    catch (const std::exception& e) {
      std::cout << "WARNING::MODEL::Error loading textures: " << e.what() << std::endl;
    }
  }

  std::cout << "processMesh: Creating and returning mesh..." << std::endl;
  std::cout << "processMesh: Vertices: " << vertices.size() << ", Indices: " << indices.size() << ", Textures: " << textures.size() << std::endl;
  
  return Mesh(vertices, indices, textures);
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName) {
  std::cout << "loadMaterialTextures: starting for type " << typeName << std::endl;
  
  if (!mat) {
    throw std::runtime_error("ERROR::MODEL::Invalid material pointer in loadMaterialTextures");
  }

  // loads and generates the texture and store info in Vertex struct
  std::vector<Texture> textures;
  unsigned int textureCount = mat->GetTextureCount(type);
  
  for (unsigned int i=0; i<textureCount; i++){
    aiString str;
    if (mat->GetTexture(type, i, &str) != AI_SUCCESS) {
      std::cout << "WARNING::MODEL::Failed to get texture " << i << " of type " << typeName << std::endl;
      continue;
    }
    
    std::cout << "loadMaterialTextures: Processing texture " << i << ": " << str.C_Str() << std::endl;
    bool skip = false;

    for (unsigned int j=0; j<textures_loaded.size(); j++) {
      if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
        std::cout << "loadMaterialTextures: Reusing already loaded texture" << std::endl;
        textures.push_back(textures_loaded[j]);
        skip=true;
        break;
      }
    }
    if (!skip) {
      try {
        std::cout << "loadMaterialTextures: Loading new texture from " << directory << "/" << str.C_Str() << std::endl;
        Texture texture;
        texture.id = TextureFromFile(str.C_Str(), this->directory);
        texture.type = typeName;
        std::cout << "loadMaterialTextures: Texture ID: " << texture.id << std::endl;
        texture.path = str.C_Str();

        if (!texture.type.empty()) {
          std::cout << "loadMaterialTextures: Found " << textureCount << " textures of type " << texture.type << std::endl;
        }
        
        textures.push_back(texture);
        textures_loaded.push_back(texture);
      }
      catch (const std::exception& e) {
        std::cout << "WARNING::MODEL::Failed to load texture: " << str.C_Str() << " - " << e.what() << std::endl;
      }
    }
  }
  
  std::cout << "loadMaterialTextures: Completed for type " << typeName << std::endl;
  return textures; 
}

unsigned int TextureFromFile(const char *path, const std::string &directory){
  std::cout << "TextureFromFile: Starting for " << path << std::endl;
  
  if (!path) {
    throw std::runtime_error("ERROR::TEXTURE::Null path in TextureFromFile");
  }

  std::string fileName = std::string(path);
  fileName = directory + '/' + fileName;
  std::cout << "TextureFromFile: Full path: " << fileName << std::endl;

  // Check if the file exists
  std::ifstream f(fileName.c_str());
  bool exists = f.good();
  f.close();
  std::cout << "TextureFromFile: File exists: " << (exists ? "Yes" : "No") << std::endl;
  
  if (!exists) {
    std::string error = "Texture file does not exist: " + fileName;
    std::cerr << error << std::endl;
    throw std::runtime_error(error);
  }

  unsigned int textureID;
  glGenTextures(1, &textureID);
  std::cout << "TextureFromFile: Generated texture ID: " << textureID << std::endl;

  int width, height, nrComponents;
  
  // Load image using stb_image
  std::cout << "TextureFromFile: Loading with stbi_load..." << std::endl;
  stbi_set_flip_vertically_on_load(true);

  std::cout << "TextureFromFile: Loading image: " << fileName << std::endl;
    
  unsigned char* data = stbi_load(fileName.c_str(), &width, &height, &nrComponents, 0);
  std::cout << "TextureFromFile: stbi_load completed" << std::endl;
    
  if (data) {
    std::cout << "TextureFromFile: Successfully loaded image: " << width << "x" << height << " with " << nrComponents << " components" << std::endl;
    
    GLenum format;
    
    // Set format based on channels
    if (nrComponents == 1)
      format = GL_RED;
    else if (nrComponents == 3)
      format = GL_RGB;
    else if (nrComponents == 4)
      format = GL_RGBA;
    else {
      stbi_image_free(data);
      throw std::runtime_error("ERROR::TEXTURE::Unsupported number of components: " + std::to_string(nrComponents));
    }

    std::cout << "TextureFromFile: Uploading to GPU..." << std::endl;
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Set the texture wrapping/filtering options (on the currently bound texture object)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    std::cout << "TextureFromFile: Texture uploaded successfully" << std::endl;

    stbi_image_free(data);
  } else {
    std::string error = "Texture failed to load at path: " + std::string(path) + " - " + std::string(stbi_failure_reason());
    std::cerr << error << std::endl;
    stbi_image_free(data);
    throw std::runtime_error(error);
  }

  std::cout << "TextureFromFile: Returning texture ID: " << textureID << std::endl;
  return textureID;
};