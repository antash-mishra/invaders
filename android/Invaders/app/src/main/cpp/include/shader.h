#ifndef SHADER_H
#define SHADER_H

#include <GLES3/gl3.h>  // Use OpenGL ES 3.0 for Android
#include <glm/glm.hpp>
#include <string>
#include <iostream>
#include <android/asset_manager.h>

class Shader {
  public:
  unsigned int ID;

  // Android constructor for asset loading
  Shader(AAssetManager* assetManager, const char* vertexAssetPath, const char* fragmentAssetPath);

  void use();

  void setBool(const std::string &name, bool value) const;
  void setInt(const std::string &name, int value) const;
  void setFloat(const std::string &name, float value) const;
  void setVec2(const std::string &name, const  glm::vec2 &value) const;
  void setVec2(const std::string &name, float x, float y) const;
  void setVec3(const std::string &name, const glm::vec3 &value) const;
  void setVec3(const std::string &name, float x, float y, float z) const;
  void setVec4(const std::string &name, const glm::vec4 &value) const;
  void setVec4(const std::string &name, float x, float y, float z, float w) const;
  void setMat2(const std::string &name, const glm::mat2 &mat) const;
  void setMat3(const std::string &name, const glm::mat3 &mat) const;
  void setMat4(const std::string &name, const glm::mat4 &mat) const;

private:
  std::string loadShaderFromAssets(AAssetManager* assetManager, const char* filename);
  void compileAndLinkShaders(const std::string& vertexCode, const std::string& fragmentCode);
};

#endif
