#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <AL/al.h>
#include <AL/alc.h>
#include <string>
#include <unordered_map>
#include <vector>

class AudioManager {
private:
    ALCdevice* device;
    ALCcontext* context;
    std::unordered_map<std::string, ALuint> soundBuffers;
    std::vector<ALuint> sources;
    int maxSources;

public:
    AudioManager(int maxConcurrentSounds = 16);
    ~AudioManager();
    
    bool initialize();
    void cleanup();
    bool loadSound(const std::string& name, const std::string& filepath);
    void playSound(const std::string& name, float volume = 1.0f, float pitch = 1.0f, bool loop = false);
    void stopSound(const std::string& name);
    void stopAllSounds();
    void setSoundVolume(const std::string& name, float volume);
    void setListenerPosition(float x, float y, float z);
    void play3DSound(const std::string& name, float x, float y, float z, float volume = 1.0f);
};

#endif