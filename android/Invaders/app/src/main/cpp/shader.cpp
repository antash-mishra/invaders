#include "shader.h"
#include "glm/detail/type_vec.hpp"

#include <GLES3/gl3.h>  // Use OpenGL ES 3.0 for Android
#include <glm/glm.hpp>
#include <string>
#include <iostream>
#include <cstring>
#include <android/log.h>  // For Android logging
#include <android/asset_manager.h>

#define LOG_TAG "Shader"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

Shader::Shader(AAssetManager* assetManager, const char* vertexAssetPath, const char* fragmentAssetPath) {
  // Load shader sources from Android assets
  std::string vertexCode = loadShaderFromAssets(assetManager, vertexAssetPath);
  std::string fragmentCode = loadShaderFromAssets(assetManager, fragmentAssetPath);
  
  if (vertexCode.empty() || fragmentCode.empty()) {
    LOGE("Failed to load shader assets: %s, %s", vertexAssetPath, fragmentAssetPath);
    ID = 0;
    return;
  }
  
  LOGI("Loaded shaders from assets: %s + %s", vertexAssetPath, fragmentAssetPath);
  
  // Compile and link the shaders
  compileAndLinkShaders(vertexCode, fragmentCode);
}

std::string Shader::loadShaderFromAssets(AAssetManager* assetManager, const char* filename) {
  if (!assetManager) {
    LOGE("Asset manager not initialized");
    return "";
  }
  
  AAsset* asset = AAssetManager_open(assetManager, filename, AASSET_MODE_BUFFER);
  if (!asset) {
    LOGE("Failed to open shader asset: %s", filename);
    return "";
  }
  
  off_t length = AAsset_getLength(asset);
  const char* buffer = (const char*)AAsset_getBuffer(asset);
  
  if (!buffer || length <= 0) {
    LOGE("Failed to read shader asset: %s", filename);
    AAsset_close(asset);
    return "";
  }
  
  std::string shaderSource(buffer, length);
  AAsset_close(asset);
  
  LOGI("Loaded shader: %s (%d bytes)", filename, (int)length);
  return shaderSource;
}

void Shader::compileAndLinkShaders(const std::string& vertexCode, const std::string& fragmentCode) {
  const char* vShaderCode = vertexCode.c_str();
  const char* fShaderCode = fragmentCode.c_str();

  // compile shaders
  unsigned int vertex, fragment;
  int success;
  char infoLog[512];

  // vertex shader
  vertex = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex, 1, &vShaderCode, NULL);
  glCompileShader(vertex);

  // error checking
  glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertex, 512, NULL, infoLog);
    LOGE("SHADER VERTEX COMPILATION_FAILED: %s", infoLog);
  }

  // fragment shader
  fragment = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment, 1, &fShaderCode, NULL);
  glCompileShader(fragment);

  // error checking
  glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragment, 512, NULL, infoLog);
    LOGE("SHADER FRAGMENT COMPILATION_FAILED: %s", infoLog);
  }

  // shader program
  ID = glCreateProgram();
  glAttachShader(ID, vertex);
  glAttachShader(ID, fragment);
  glLinkProgram(ID);

  // linking error checking
  glGetProgramiv(ID, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(ID, 512, NULL, infoLog);
    LOGE("SHADER PROGRAM LINKING_FAILED: %s", infoLog);
  } else {
    LOGI("Shader program created successfully with ID: %u", ID);
  }
  
  glDeleteShader(vertex);
  glDeleteShader(fragment);
}

void Shader::use() {
  glUseProgram(ID);
}

void Shader::setBool(const std::string &name, bool value) const {
  glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string &name, int value) const {
  glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string &name, float value) const {
  glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec2(const std::string &name,const glm::vec2 &value) const {
  glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}

void Shader::setVec2(const std::string &name, float x, float y) const {
  glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
}

void Shader::setVec3(const std::string &name,const glm::vec3 &value) const {
  glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}

void Shader::setVec3(const std::string &name, float x, float y, float z) const {
  glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
}

void Shader::setVec4(const std::string &name,const glm::vec4 &value) const {
  glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
}

void Shader::setVec4(const std::string &name, float x, float y, float z, float w) const {
  glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
}

void Shader::setMat2(const std::string &name, const glm::mat2 &mat) const {
  glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setMat3(const std::string &name, const glm::mat3 &mat) const {
  glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::setMat4(const std::string &name, const glm::mat4 &mat) const {
  glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}
