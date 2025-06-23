#include "include/audio_manager.h"
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AudioManager", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "AudioManager", __VA_ARGS__)

AudioManager::AudioManager(int maxConcurrentSounds) : maxSources(maxConcurrentSounds) {
    javaVM = nullptr;
    audioManagerObject = nullptr;
    loadSoundMethod = nullptr;
    playSoundMethod = nullptr;
    play3DSoundMethod = nullptr;
    LOGI("AudioManager created");
}

AudioManager::~AudioManager() {
    cleanup();
}

bool AudioManager::initialize(JNIEnv* env, jobject context) {
    // Get the JavaVM
    if (env->GetJavaVM(&javaVM) != JNI_OK) {
        LOGE("Failed to get JavaVM");
        return false;
    }

    // Find the AudioManagerJNI class
    jclass audioManagerClass = env->FindClass("com/antash/invaders/AudioManagerJNI");
    if (!audioManagerClass) {
        LOGE("Failed to find AudioManagerJNI class");
        return false;
    }

    // Get the constructor
    jmethodID constructor = env->GetMethodID(audioManagerClass, "<init>", "(Landroid/content/Context;)V");
    if (!constructor) {
        LOGE("Failed to find AudioManagerJNI constructor");
        return false;
    }

    // Create the AudioManagerJNI object
    jobject localRef = env->NewObject(audioManagerClass, constructor, context);
    if (!localRef) {
        LOGE("Failed to create AudioManagerJNI object");
        return false;
    }

    // Create global reference
    audioManagerObject = env->NewGlobalRef(localRef);
    env->DeleteLocalRef(localRef);

    // Get method IDs
    loadSoundMethod = env->GetMethodID(audioManagerClass, "loadSound", "(Ljava/lang/String;Ljava/lang/String;)Z");
    playSoundMethod = env->GetMethodID(audioManagerClass, "playSound", "(Ljava/lang/String;FF)V");
    play3DSoundMethod = env->GetMethodID(audioManagerClass, "play3DSound", "(Ljava/lang/String;FFFF)V");

    if (!loadSoundMethod || !playSoundMethod || !play3DSoundMethod) {
        LOGE("Failed to get method IDs");
        return false;
    }

    LOGI("Audio system initialized successfully");
    return true;
}

bool AudioManager::loadSound(const std::string& name, const std::string& filepath) {
    if (!audioManagerObject) {
        LOGE("AudioManager not initialized");
        return false;
    }

    JNIEnv* env;
    if (javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Failed to get JNI environment");
        return false;
    }

    jstring jName = env->NewStringUTF(name.c_str());
    jstring jFilepath = env->NewStringUTF(filepath.c_str());

    jboolean result = env->CallBooleanMethod(audioManagerObject, loadSoundMethod, jName, jFilepath);

    env->DeleteLocalRef(jName);
    env->DeleteLocalRef(jFilepath);

    LOGI("Loading sound: %s -> %s", name.c_str(), result ? "success" : "failed");
    return result;
}

void AudioManager::playSound(const std::string& name, float volume, float pitch) {
    if (!audioManagerObject) {
        LOGE("AudioManager not initialized");
        return;
    }

    JNIEnv* env;
    if (javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Failed to get JNI environment");
        return;
    }

    jstring jName = env->NewStringUTF(name.c_str());
    env->CallVoidMethod(audioManagerObject, playSoundMethod, jName, volume, pitch);
    env->DeleteLocalRef(jName);

    LOGI("Playing sound: %s", name.c_str());
}

void AudioManager::play3DSound(const std::string& name, float x, float y, float z, float volume) {
    if (!audioManagerObject) {
        LOGE("AudioManager not initialized");
        return;
    }

    JNIEnv* env;
    if (javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Failed to get JNI environment");
        return;
    }

    jstring jName = env->NewStringUTF(name.c_str());
    env->CallVoidMethod(audioManagerObject, play3DSoundMethod, jName, x, y, z, volume);
    env->DeleteLocalRef(jName);

//    LOGI("Playing 3D sound: %s at (%.2f, %.2f, %.2f)", name.c_str(), x, y, z);
}

void AudioManager::setListenerPosition(float x, float y, float z) {
    // Not needed for MediaPlayer implementation
//    LOGI("Setting listener position: (%.2f, %.2f, %.2f)", x, y, z);
}

void AudioManager::cleanup() {
    if (audioManagerObject) {
        JNIEnv* env;
        if (javaVM && javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK) {
            env->DeleteGlobalRef(audioManagerObject);
        }
        audioManagerObject = nullptr;
    }
    LOGI("Audio system cleaned up");
}

bool AudioManager::loadMusic(const std::string& name, const std::string& filepath) {
    if (!audioManagerObject) return false;
    JNIEnv* env;
    if (javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return false;
    jclass cls = env->GetObjectClass(audioManagerObject);
    jmethodID mid = env->GetMethodID(cls, "loadMusicJNI", "(Ljava/lang/String;Ljava/lang/String;)Z");
    if (!mid) return false;
    jstring jName = env->NewStringUTF(name.c_str());
    jstring jPath = env->NewStringUTF(filepath.c_str());
    jboolean result = env->CallBooleanMethod(audioManagerObject, mid, jName, jPath);
    env->DeleteLocalRef(jName);
    env->DeleteLocalRef(jPath);
    return result;
}

void AudioManager::playMusic(const std::string& name, float volume) {
    if (!audioManagerObject) return;
    JNIEnv* env;
    if (javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return;
    jclass cls = env->GetObjectClass(audioManagerObject);
    jmethodID mid = env->GetMethodID(cls, "playMusicJNI", "(Ljava/lang/String;F)V");
    if (!mid) return;
    jstring jName = env->NewStringUTF(name.c_str());
    env->CallVoidMethod(audioManagerObject, mid, jName, volume);
    env->DeleteLocalRef(jName);
}

void AudioManager::stopMusic() {
    if (!audioManagerObject) return;
    JNIEnv* env;
    if (javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return;
    jclass cls = env->GetObjectClass(audioManagerObject);
    jmethodID mid = env->GetMethodID(cls, "stopMusicJNI", "()V");
    if (!mid) return;
    env->CallVoidMethod(audioManagerObject, mid);
}

void AudioManager::setMusicVolume(float volume) {
    if (!audioManagerObject) return;
    JNIEnv* env;
    if (javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) return;
    jclass cls = env->GetObjectClass(audioManagerObject);
    jmethodID mid = env->GetMethodID(cls, "setMusicVolumeJNI", "(F)V");
    if (!mid) return;
    env->CallVoidMethod(audioManagerObject, mid, volume);
} 