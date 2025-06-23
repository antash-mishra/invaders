package com.antash.invaders;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.media.SoundPool;
import android.util.Log;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

public class AudioManagerJNI {
    private static final String TAG = "AudioManagerJNI";
    private SoundPool soundPool;
    private Context context;
    private Map<String, Integer> soundMap;
    private Map<String, Integer> streamMap;
    private MediaPlayer musicPlayer;
    private String currentMusicName = null;
    private HashMap<String, String> musicFiles = new HashMap<>();
    
    public AudioManagerJNI(Context context) {
        this.context = context;
        this.soundMap = new HashMap<>();
        this.streamMap = new HashMap<>();
        
        // Create SoundPool with modern API
        AudioAttributes audioAttributes = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build();
        
        soundPool = new SoundPool.Builder()
                .setMaxStreams(16)
                .setAudioAttributes(audioAttributes)
                .build();
        
        Log.i(TAG, "AudioManagerJNI created");
    }
    
    public boolean loadSound(String name, String filename) {
        try {
            // Load from assets folder
            AssetFileDescriptor afd = context.getAssets().openFd(filename);
            int soundId = soundPool.load(afd, 1);
            soundMap.put(name, soundId);
            afd.close();
            
            Log.i(TAG, "Loaded sound: " + name + " from " + filename);
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Failed to load sound: " + name + " - " + e.getMessage());
            return false;
        }
    }

    public boolean loadMusic(String name, String assetPath) {
        musicFiles.put(name, assetPath);
        return true;
    }

    public void playMusic(String name, float volume) {
        try {
            if(musicPlayer != null) {
                musicPlayer.stop();
                musicPlayer.release();
            }
            String assetPath = musicFiles.get(name);
            if(assetPath == null)  return;
            AssetFileDescriptor afd = context.getAssets().openFd(assetPath);
            musicPlayer = new MediaPlayer();
            musicPlayer.setDataSource(afd.getFileDescriptor(), afd.getStartOffset(), afd.getLength());
            afd.close();
            musicPlayer.setLooping(true);
            musicPlayer.setVolume(volume, volume);
            musicPlayer.prepare();
            musicPlayer.start();
            currentMusicName = name;
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
    
    public void playSound(String name, float volume, float pitch) {
        Integer soundId = soundMap.get(name);
        if (soundId != null) {
            int streamId = soundPool.play(soundId, volume, volume, 1, 0, pitch);
            streamMap.put(name, streamId);
            Log.i(TAG, "Playing sound: " + name);
        } else {
            Log.w(TAG, "Sound not found: " + name);
        }
    }
    
    public void play3DSound(String name, float x, float y, float z, float volume) {
        // For simplicity, 3D sound just plays as regular sound with adjusted volume
        // Real 3D would require calculating distance and panning
        float adjustedVolume = Math.max(0.1f, volume / (1.0f + Math.abs(x) + Math.abs(z)));
        playSound(name, adjustedVolume, 1.0f);
    }

    public void stopMusic() {
        if (musicPlayer != null){
            musicPlayer.stop();
            musicPlayer.release();
            musicPlayer = null;
            currentMusicName = null;
        }
    }

    public void setMusicVolume(float volume){
        if (musicPlayer != null) {
            musicPlayer.setVolume(volume,volume);
        }
    }
    
    public void cleanup() {
        if (soundPool != null) {
            soundPool.release();
            soundPool = null;
        }
        soundMap.clear();
        streamMap.clear();
        Log.i(TAG, "AudioManagerJNI cleaned up");
    }

    public boolean loadMusicJNI(String name, String assetPath) { return loadMusic(name, assetPath); }
    public void playMusicJNI(String name, float volume) { playMusic(name, volume); }
    public void stopMusicJNI() { stopMusic(); }
    public void setMusicVolumeJNI(float volume) { setMusicVolume(volume); }
} 