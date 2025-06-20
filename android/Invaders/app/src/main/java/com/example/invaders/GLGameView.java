package com.example.invaders;

import android.content.Context;
import android.content.res.AssetManager;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * GLSurfaceView that bridges the native C++ renderer.
 * It forwards all GL lifecycle and touch events to the JNI functions
 * declared on {@link MainActivity} (which are implemented in C++).
 */
public class GLGameView extends GLSurfaceView {

    private final NativeRenderer renderer;

    public GLGameView(Context context) {
        super(context);
        // Request an OpenGL ES 3.0 context (as per manifest requirement)
        setEGLContextClientVersion(3);
        renderer = new NativeRenderer((MainActivity) context);
        setRenderer(renderer);
        setRenderMode(RENDERMODE_CONTINUOUSLY);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // Convert raw screen coordinates to Normalised Device Coordinates (-1..1)
        float ndcX = (event.getX() / getWidth()) * 2.0f - 1.0f;
        float ndcY = 1.0f - (event.getY() / getHeight()) * 2.0f; // flip Y

        MainActivity activity = (MainActivity) getContext();
        switch (event.getAction()) {
            case MotionEvent.ACTION_DOWN:
                activity.nativeOnTouchDown(ndcX, ndcY);
                break;
            case MotionEvent.ACTION_MOVE:
                activity.nativeOnTouchMove(ndcX, ndcY);
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                activity.nativeOnTouchUp(ndcX, ndcY);
                break;
        }
        return true; // consume event
    }

    /**
     * Inner GLSurfaceView.Renderer that forwards callbacks to native code.
     */
    private static class NativeRenderer implements Renderer {
        private final MainActivity activity;

        NativeRenderer(MainActivity activity) {
            this.activity = activity;
        }

        @Override
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            AssetManager am = activity.getAssets();
            activity.nativeSetAssetManager(am);
            activity.nativeOnSurfaceCreated();
        }

        @Override
        public void onSurfaceChanged(GL10 gl, int width, int height) {
            activity.nativeOnSurfaceChanged(width, height);
        }

        @Override
        public void onDrawFrame(GL10 gl) {
            activity.nativeOnDrawFrame();
        }
    }
} 