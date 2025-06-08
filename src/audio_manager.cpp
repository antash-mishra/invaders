#include "audio_manager.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>  // Add this for uint32_t, uint16_t types

// Simple WAV loader (you can use libraries like libsndfile for more formats)
struct WAVHeader {
    char riff[4];
    uint32_t fileSize;
    char wave[4];
    char fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4];
    uint32_t dataSize;
};

AudioManager::AudioManager(int maxConcurrentSounds) : maxSources(maxConcurrentSounds) {
    device = nullptr;
    context = nullptr;
}

AudioManager::~AudioManager() {
    cleanup();
}

bool AudioManager::initialize() {
    // Open audio device
    device = alcOpenDevice(nullptr); // Default device
    if (!device) {
        std::cerr << "Failed to open audio device" << std::endl;
        return false;
    }

    // Create audio context
    context = alcCreateContext(device, nullptr);
    if (!context) {
        std::cerr << "Failed to create audio context" << std::endl;
        alcCloseDevice(device);
        return false;
    }

    // Make context current
    if (!alcMakeContextCurrent(context)) {
        std::cerr << "Failed to make audio context current" << std::endl;
        alcDestroyContext(context);
        alcCloseDevice(device);
        return false;
    }

    // Generate audio sources
    sources.resize(maxSources);
    alGenSources(maxSources, sources.data());
    
    if (alGetError() != AL_NO_ERROR) {
        std::cerr << "Failed to generate audio sources" << std::endl;
        return false;
    }

    std::cout << "Audio system initialized successfully" << std::endl;
    return true;
}

bool AudioManager::loadSound(const std::string& name, const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open audio file: " << filepath << std::endl;
        return false;
    }

    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    // Simple validation
    if (std::string(header.riff, 4) != "RIFF" || 
        std::string(header.wave, 4) != "WAVE" ||
        std::string(header.data, 4) != "data") {
        std::cerr << "Invalid WAV file: " << filepath << std::endl;
        return false;
    }

    // Read audio data
    std::vector<char> audioData(header.dataSize);
    file.read(audioData.data(), header.dataSize);
    file.close();

    // Determine OpenAL format
    ALenum format;
    if (header.numChannels == 1) {
        format = (header.bitsPerSample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
    } else {
        format = (header.bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
    }

    // Generate and fill buffer
    ALuint buffer;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, format, audioData.data(), header.dataSize, header.sampleRate);

    if (alGetError() != AL_NO_ERROR) {
        std::cerr << "Failed to load sound: " << name << std::endl;
        alDeleteBuffers(1, &buffer);
        return false;
    }

    soundBuffers[name] = buffer;
    std::cout << "Loaded sound: " << name << std::endl;
    return true;
}

void AudioManager::playSound(const std::string& name, float volume, float pitch) {
    auto it = soundBuffers.find(name);
    if (it == soundBuffers.end()) {
        std::cerr << "Sound not found: " << name << std::endl;
        return;
    }

    // Find available source
    for (ALuint source : sources) {
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        
        if (state != AL_PLAYING) {
            alSourcei(source, AL_BUFFER, it->second);
            alSourcef(source, AL_GAIN, volume);
            alSourcef(source, AL_PITCH, pitch);
            alSourcei(source, AL_LOOPING, AL_FALSE);
            alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
            alSourcePlay(source);  // Fix: was alPlaySource, should be alSourcePlay
            return;
        }
    }
    
    std::cerr << "No available audio source for: " << name << std::endl;
}

void AudioManager::play3DSound(const std::string& name, float x, float y, float z, float volume) {
    auto it = soundBuffers.find(name);
    if (it == soundBuffers.end()) {
        std::cerr << "Sound not found: " << name << std::endl;
        return;
    }

    // Find available source
    for (ALuint source : sources) {
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        
        if (state != AL_PLAYING) {
            alSourcei(source, AL_BUFFER, it->second);
            alSourcef(source, AL_GAIN, volume);
            alSourcef(source, AL_PITCH, 1.0f);
            alSourcei(source, AL_LOOPING, AL_FALSE);
            alSource3f(source, AL_POSITION, x, y, z);
            alSourcePlay(source);  // Fix: was alPlaySource, should be alSourcePlay
            return;
        }
    }
}

void AudioManager::setListenerPosition(float x, float y, float z) {
    alListener3f(AL_POSITION, x, y, z);
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    
    // Set orientation (forward and up vectors)
    ALfloat orientation[] = {0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f};
    alListenerfv(AL_ORIENTATION, orientation);
}

void AudioManager::cleanup() {
    // Delete sources
    if (!sources.empty()) {
        alDeleteSources(sources.size(), sources.data());
        sources.clear();
    }

    // Delete buffers
    for (auto& pair : soundBuffers) {
        alDeleteBuffers(1, &pair.second);
    }
    soundBuffers.clear();

    // Clean up context and device
    if (context) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        context = nullptr;
    }
    
    if (device) {
        alcCloseDevice(device);
        device = nullptr;
    }
}