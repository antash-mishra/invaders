#include "audio_manager.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>  // Add this for uint32_t, uint16_t types
#include <filesystem>

// Simple WAV loader (you can use libraries like libsndfile for more formats)
struct WAVHeader {
    char riff[4];
    uint32_t fileSize;
    char wave[4];
};

struct WAVChunkHeader {
    char id[4];
    uint32_t size;
};

struct WAVFmtChunk {
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
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
    std::cout << "Attempting to load sound: " << filepath << std::endl;
    
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "ERROR: Failed to open audio file: " << filepath << std::endl;
        std::cerr << "Current working directory: " << std::filesystem::current_path() << std::endl;
        return false;
    }

    std::cout << "File opened successfully. Reading WAV header..." << std::endl;

    // Read RIFF header
    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (file.gcount() != sizeof(header)) {
        std::cerr << "ERROR: Failed to read complete WAV header" << std::endl;
        file.close();
        return false;
    }

    if (std::string(header.riff, 4) != "RIFF") {
        std::cerr << "ERROR: Invalid WAV file - RIFF header not found" << std::endl;
        file.close();
        return false;
    }
    
    if (std::string(header.wave, 4) != "WAVE") {
        std::cerr << "ERROR: Invalid WAV file - WAVE format not found" << std::endl;
        file.close();
        return false;
    }

    // Variables to store format info
    WAVFmtChunk fmtChunk;
    std::vector<char> audioData;
    bool foundFmt = false;
    bool foundData = false;

    // Read chunks until we find both fmt and data
    while (!file.eof() && (!foundFmt || !foundData)) {
        WAVChunkHeader chunkHeader;
        file.read(reinterpret_cast<char*>(&chunkHeader), sizeof(chunkHeader));
        
        if (file.gcount() != sizeof(chunkHeader)) break;

        std::string chunkId(chunkHeader.id, 4);
        std::cout << "Found chunk: " << chunkId << " with size: " << chunkHeader.size << std::endl;

        if (chunkId == "fmt ") {
            file.read(reinterpret_cast<char*>(&fmtChunk), sizeof(fmtChunk));
            foundFmt = true;
            std::cout << "Format chunk found:" << std::endl
                      << "Audio format: " << fmtChunk.audioFormat << std::endl
                      << "Channels: " << fmtChunk.numChannels << std::endl
                      << "Sample rate: " << fmtChunk.sampleRate << std::endl
                      << "Bits per sample: " << fmtChunk.bitsPerSample << std::endl;
        }
        else if (chunkId == "data") {
            audioData.resize(chunkHeader.size);
            file.read(audioData.data(), chunkHeader.size);
            foundData = true;
            std::cout << "Data chunk found, size: " << chunkHeader.size << " bytes" << std::endl;
        }
        else {
            // Skip other chunks (like LIST)
            file.seekg(chunkHeader.size, std::ios::cur);
        }
    }

    file.close();

    if (!foundFmt || !foundData) {
        std::cerr << "ERROR: Failed to find required chunks in WAV file" << std::endl;
        return false;
    }

    // Determine OpenAL format
    ALenum format;
    if (fmtChunk.numChannels == 1) {
        format = (fmtChunk.bitsPerSample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
    } else {
        format = (fmtChunk.bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
    }

    std::cout << "Using OpenAL format: " << format << std::endl;

    // Generate and fill buffer
    ALuint buffer;
    alGenBuffers(1, &buffer);
    
    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cerr << "ERROR: Failed to generate OpenAL buffer: " << error << std::endl;
        return false;
    }

    alBufferData(buffer, format, audioData.data(), audioData.size(), fmtChunk.sampleRate);
    
    error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cerr << "ERROR: Failed to fill OpenAL buffer: " << error << std::endl;
        std::cerr << "Format: " << format << ", Data size: " << audioData.size() << ", Sample rate: " << fmtChunk.sampleRate << std::endl;
        alDeleteBuffers(1, &buffer);
        return false;
    }

    soundBuffers[name] = buffer;
    std::cout << "Successfully loaded sound: " << name << std::endl;
    return true;
}

void AudioManager::playSound(const std::string& name, float volume, float pitch, bool loop) {
    auto it = soundBuffers.find(name);
    if (it == soundBuffers.end()) {
        std::cerr << "Sound not found: " << name << std::endl;
        return;
    }

    // Find an available (non-playing) source
    for (ALuint source : sources) {
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);

        if (state != AL_PLAYING) {
            alSourcei(source, AL_BUFFER, it->second);
            alSourcef(source, AL_GAIN, volume);
            alSourcef(source, AL_PITCH, pitch);
            alSourcei(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
            alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
            alSourcePlay(source);
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

// Stop playback of a particular sound buffer across all sources
void AudioManager::stopSound(const std::string& name) {
    auto it = soundBuffers.find(name);
    if (it == soundBuffers.end()) {
        // Nothing to stop
        return;
    }

    ALuint buffer = it->second;
    for (ALuint source : sources) {
        ALint boundBuffer = 0;
        alGetSourcei(source, AL_BUFFER, &boundBuffer);
        if (static_cast<ALuint>(boundBuffer) == buffer) {
            alSourceStop(source);
        }
    }
}

// Stop all currently playing sources
void AudioManager::stopAllSounds() {
    for (ALuint source : sources) {
        alSourceStop(source);
    }
}

// Update the gain for all sources currently playing the specified sound
void AudioManager::setSoundVolume(const std::string& name, float volume) {
    auto it = soundBuffers.find(name);
    if (it == soundBuffers.end()) {
        return;
    }

    ALuint buffer = it->second;
    for (ALuint source : sources) {
        ALint boundBuffer = 0;
        alGetSourcei(source, AL_BUFFER, &boundBuffer);
        if (static_cast<ALuint>(boundBuffer) == buffer) {
            alSourcef(source, AL_GAIN, volume);
        }
    }
}