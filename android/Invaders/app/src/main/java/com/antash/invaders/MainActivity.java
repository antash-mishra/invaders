package com.antash.invaders;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.content.res.AssetManager;
import android.content.Context;

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

    @Override
    protected void onPause() {
        super.onPause();
        if (glGameView != null) {
            glGameView.onPause();
        }
        nativeOnPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (glGameView != null) {
            glGameView.onResume();
        }
        nativeOnResume();
    }

    /* ===================== JNI BRIDGE ===================== */
    public native void nativeSetAssetManager(AssetManager mgr);
    public native void nativeOnSurfaceCreated();
    public native void nativeOnSurfaceChanged(int width, int height);
    public native void nativeOnDrawFrame();

    public native void nativeOnTouchDown(float ndcX, float ndcY);
    public native void nativeOnTouchMove(float ndcX, float ndcY);
    public native void nativeOnTouchUp(float ndcX, float ndcY);

    public native void nativeOnPause();
    public native void nativeOnResume();

    /**
     * A native method that is implemented by the 'invaders' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    /**
     * JNI method invoked from native code to trigger a short vibration.
     * @param durationMs duration in milliseconds
     */
    public void vibrateJNI(int durationMs) {
        android.os.Vibrator vib = (android.os.Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        if (vib == null) return;

        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
            vib.vibrate(android.os.VibrationEffect.createOneShot(durationMs,
                    android.os.VibrationEffect.DEFAULT_AMPLITUDE));
        } else {
            //noinspection deprecation
            vib.vibrate(durationMs);
        }
    }

    // ---------------------------------------------------------------------
    // Disable the system BACK gesture / button while the GL game view is
    // on screen so accidental edge-swipes do not minimise the game.
    // ---------------------------------------------------------------------
    @Override
    public void onBackPressed() {
        // If the OpenGL game is active, swallow the back action.
        if (glGameView != null) {
            // Optionally show an in-game pause menu instead. For now we simply
            // ignore the back gesture to prevent unintended minimising.
            return;
        }
        super.onBackPressed();
    }
}