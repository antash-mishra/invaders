#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <jni.h>

// Android implementation using MediaPlayer through JNI
class AudioManager {
private:
    int maxSources;
    JavaVM* javaVM;
    jobject audioManagerObject;
    jmethodID loadSoundMethod;
    jmethodID playSoundMethod;
    jmethodID play3DSoundMethod;

public:
    AudioManager(int maxConcurrentSounds = 16);
    ~AudioManager();
    
    bool initialize(JNIEnv* env, jobject context);
    void cleanup();
    bool loadSound(const std::string& name, const std::string& filepath);
    void playSound(const std::string& name, float volume = 1.0f, float pitch = 1.0f);
    void setListenerPosition(float x, float y, float z);
    void play3DSound(const std::string& name, float x, float y, float z, float volume = 1.0f);
    bool loadMusic(const std::string& name, const std::string& filepath);
    void playMusic(const std::string& name, float volume = 1.0f);
    void stopMusic();
    void setMusicVolume(float volume);
};

#endif