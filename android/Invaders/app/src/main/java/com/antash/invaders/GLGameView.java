package com.antash.invaders;

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

        // Disable system back-swipe gesture inside the view (Android 10+)
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
            post(this::updateGestureExclusion);
        }
    }

    private void updateGestureExclusion() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
            int edge = (int) (getResources().getDisplayMetrics().density * 32); // 32dp edge
            java.util.ArrayList<android.graphics.Rect> rects = new java.util.ArrayList<>();
            // Exclude left edge
            rects.add(new android.graphics.Rect(0, 0, edge, getHeight()));
            // Exclude right edge
            rects.add(new android.graphics.Rect(getWidth() - edge, 0, getWidth(), getHeight()));
            setSystemGestureExclusionRects(rects);
        }
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        // Re-apply exclusion rects on rotation / resize
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
            updateGestureExclusion();
        }
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