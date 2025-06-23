package com.antash.invaders;

import android.app.Activity;
import android.content.Intent;
import android.util.Log;
import android.widget.Toast;

import androidx.annotation.NonNull;

import com.google.android.gms.games.GamesSignInClient;
import com.google.android.gms.games.LeaderboardsClient;
import com.google.android.gms.games.PlayGames;
import com.google.android.gms.games.PlayGamesSdk;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;

/**
 * Helper class for Google Play Games Services v2 integration
 */
public class GameServicesManager {
    private static final String TAG = "GameServicesManager";
    private static final String LEADERBOARD_ID = "CgkIp7qi7ZUKEAIQAA";
    private static final int RC_SIGN_IN = 9001;
    
    private Activity activity;
    private boolean isSignedIn = false;
    
    public GameServicesManager(Activity activity) {
        this.activity = activity;
        // Initialize Play Games SDK
        PlayGamesSdk.initialize(activity);
    }
    
    /**
     * Check if user is authenticated and update sign-in status
     */
    public void checkSignInStatus() {
        GamesSignInClient gamesSignInClient = PlayGames.getGamesSignInClient(activity);
        
        gamesSignInClient.isAuthenticated().addOnCompleteListener(new OnCompleteListener<com.google.android.gms.games.AuthenticationResult>() {
            @Override
            public void onComplete(@NonNull Task<com.google.android.gms.games.AuthenticationResult> task) {
                if (task.isSuccessful()) {
                    isSignedIn = task.getResult().isAuthenticated();
                    if (isSignedIn) {
                        Log.d(TAG, "User is signed in to Play Games Services");
                        onSignInSucceeded();
                    } else {
                        Log.d(TAG, "User is not signed in to Play Games Services – starting sign-in flow");
                        startSignInIntent();
                    }
                } else {
                    Log.e(TAG, "Failed to check authentication status", task.getException());
                    isSignedIn = false;
                    // Attempt interactive sign-in on failure as well
                    startSignInIntent();
                }
            }
        });
    }
    
    /**
     * Initiates the Play Games sign-in flow (v2 API). This will display the Google
     * Play Games sign-in UI if required and delivers the result via the returned
     * Task – not through onActivityResult.
     */
    public void startSignInIntent() {
        if (!isSignedIn) {
            GamesSignInClient gamesSignInClient = PlayGames.getGamesSignInClient(activity);
            gamesSignInClient.signIn().addOnCompleteListener(new OnCompleteListener<com.google.android.gms.games.AuthenticationResult>() {
                @Override
                public void onComplete(@NonNull Task<com.google.android.gms.games.AuthenticationResult> task) {
                    if (task.isSuccessful() && task.getResult().isAuthenticated()) {
                        isSignedIn = true;
                        onSignInSucceeded();
                    } else {
                        isSignedIn = false;
                        Exception e = task.getException();
                        Log.e(TAG, "Interactive sign-in failed", e);
                        onSignInFailed();
                    }
                }
            });
        }
    }
    
    /**
     * Handle the result of sign-in intent
     */
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == RC_SIGN_IN) {
            // Re-check authentication status after sign-in attempt
            checkSignInStatus();
        }
    }
    
    /**
     * Submit a score to the leaderboard
     */
    public void submitScore(long score) {
        if (isSignedIn) {
            LeaderboardsClient leaderboardsClient = PlayGames.getLeaderboardsClient(activity);
            leaderboardsClient.submitScore(LEADERBOARD_ID, score);
            Log.d(TAG, "Score submitted: " + score);
        } else {
            Log.w(TAG, "Cannot submit score - user not signed in");
        }
    }
    
    /**
     * Show the leaderboard UI
     */
    public void showLeaderboard() {
        if (isSignedIn) {
            LeaderboardsClient leaderboardsClient = PlayGames.getLeaderboardsClient(activity);
            leaderboardsClient.getLeaderboardIntent(LEADERBOARD_ID)
                .addOnCompleteListener(new OnCompleteListener<Intent>() {
                    @Override
                    public void onComplete(@NonNull Task<Intent> task) {
                        if (task.isSuccessful()) {
                            Intent intent = task.getResult();
                            activity.startActivityForResult(intent, 5001);
                        } else {
                            Log.e(TAG, "Failed to get leaderboard intent", task.getException());
                        }
                    }
                });
        } else {
            Log.w(TAG, "Cannot show leaderboard - user not signed in");
        }
    }
    
    /**
     * Check if user is currently signed in
     */
    public boolean isSignedIn() {
        return isSignedIn;
    }
    
    /**
     * Called when sign-in succeeds
     */
    private void onSignInSucceeded() {
        Log.d(TAG, "Sign-in succeeded");
        Toast.makeText(activity, "Signed in to Google Play Games", Toast.LENGTH_SHORT).show();
        // Notify the activity so it can show the game view
        if (activity instanceof MainActivity) {
            activity.runOnUiThread(() -> ((MainActivity) activity).startGame());
        }
    }
    
    /**
     * Called when sign-in fails
     */
    private void onSignInFailed() {
        Log.d(TAG, "Sign-in failed or user not signed in");
        Toast.makeText(activity, "Playing as Guest", Toast.LENGTH_SHORT).show();
        // Start the game anyhow so the user isn't stuck on the placeholder layout
        if (activity instanceof MainActivity) {
            activity.runOnUiThread(() -> ((MainActivity) activity).startGame());
        }
    }
} 