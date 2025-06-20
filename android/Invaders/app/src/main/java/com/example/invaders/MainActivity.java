package com.example.invaders;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.content.res.AssetManager;
import android.opengl.GLSurfaceView;

// Add Google Play Games
import com.google.android.gms.games.PlayGamesSdk;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'invaders' library on application startup.
    static {
        System.loadLibrary("invaders");
    }

    private GameServicesManager gameServicesManager;
    private GLGameView glGameView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Initialize Play Games SDK first
        PlayGamesSdk.initialize(this);
        
        // Initialize Play Games sign-in helper
        gameServicesManager = new GameServicesManager(this);
        
        // Check sign-in status when the activity starts
        gameServicesManager.checkSignInStatus();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (gameServicesManager != null) {
            gameServicesManager.onActivityResult(requestCode, resultCode, data);
        }
    }

    /**
     * JNI method called from C++ to submit score to Play Games
     */
    public void submitScoreJNI(long score) {
        if (gameServicesManager != null) {
            gameServicesManager.submitScore(score);
        }
    }

    /**
     * JNI method called from C++ to show leaderboard
     */
    public void showLeaderboardJNI() {
        if (gameServicesManager != null) {
            gameServicesManager.showLeaderboard();
        }
    }

    /**
     * Called by {@link GameServicesManager} once the Google Play Games sign-in succeeds.
     * Replaces the current layout with an OpenGL surface backed by the native engine.
     */
    public void startGame() {
        if (glGameView == null) {
            glGameView = new GLGameView(this);
        }
        setContentView(glGameView);
    }

    /* ===================== JNI BRIDGE ===================== */
    public native void nativeSetAssetManager(AssetManager mgr);
    public native void nativeOnSurfaceCreated();
    public native void nativeOnSurfaceChanged(int width, int height);
    public native void nativeOnDrawFrame();

    public native void nativeOnTouchDown(float ndcX, float ndcY);
    public native void nativeOnTouchMove(float ndcX, float ndcY);
    public native void nativeOnTouchUp(float ndcX, float ndcY);

    /**
     * A native method that is implemented by the 'invaders' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
}