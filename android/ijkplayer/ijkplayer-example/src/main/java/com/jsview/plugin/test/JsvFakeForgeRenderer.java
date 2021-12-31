package com.jsview.plugin.test;

import android.content.Context;
import android.graphics.Color;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.os.SystemClock;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

class JsvFakeForgeRenderer implements GLSurfaceView.Renderer {
    interface OnDrawFrameListener {
        void onDrawFrame(Object key, final float[] mvpMatrix);
    }

    public JsvFakeForgeRenderer(Context context) {
        mContext = context;
    }

    public void appendOnDrawFrameListener(Object key, OnDrawFrameListener listener) {
        if(listener == null) {
            throw new RuntimeException("Append OnDrawFrameListener is null.");
        }

        drawFrameListenerMap.put(key, listener);
    }

    public void removeOnDrawFrameListener(Object key) {
        drawFrameListenerMap.remove(key);
    }

    @Override
    public void onSurfaceCreated(GL10 glUnused, EGLConfig config) {
        GLES20.glClearColor(0.0f, 0.0f, 1.0f, 1.0f);

        mTriangleForeground = new Triangle(mContext, Color.GREEN);
        mTriangleBackground = new Triangle(mContext, Color.RED);
        Matrix.setLookAtM(mViewMatrix, 0, 0, 0, -5, 0f, 0f, 0f, 0f, 1.0f, 0.0f);
    }

    @Override
    public void onSurfaceChanged(GL10 glUnused, int width, int height) {
        GLES20.glViewport(0, 0, width, height);
        float ratio = (float) width / height;
        Matrix.frustumM(mProjectionMatrix, 0, -ratio, ratio, -1, 1, 3, 7);
    }

    @Override
    public void onDrawFrame(GL10 glUnused) {
        GLES20.glClear( GLES20.GL_DEPTH_BUFFER_BIT | GLES20.GL_COLOR_BUFFER_BIT);

        float[] mvpMatrix = new float[16];
        Matrix.multiplyMM(mvpMatrix, 0, mProjectionMatrix, 0, mViewMatrix, 0);

        mTriangleBackground.onDrawFrame(mvpMatrix);

        Matrix.setIdentityM(mvpMatrix, 0);
        for (Map.Entry<Object, OnDrawFrameListener> entry : drawFrameListenerMap.entrySet()) {
            Object key = entry.getKey();
            OnDrawFrameListener listener = entry.getValue();
            listener.onDrawFrame(key, mvpMatrix);
        }

        Matrix.scaleM(mvpMatrix, 0, 0.2f, 0.2f, 1.0f);
        Matrix.rotateM(mvpMatrix, 0, 180, 0, 0, 1.0f);
        mTriangleForeground.onDrawFrame(mvpMatrix);

        updateFps();
    }

    public float getFps() {
        long currTime = SystemClock.uptimeMillis();
        long deltaTime = currTime - prevTime;
        prevTime = currTime;

        long count = fpsCount.getAndSet(0);
        float fps = count * 1000f / deltaTime;
        return fps;
    }
    private void updateFps() {
        fpsCount.incrementAndGet();
    }
    public AtomicInteger fpsCount = new AtomicInteger(0);
    private long prevTime = 0;

    private float[] mProjectionMatrix = new float[16];
    private float[] mViewMatrix = new float[16];

    private Context mContext;
    // LinkedHashMap是一个有序map
    private LinkedHashMap<Object, OnDrawFrameListener> drawFrameListenerMap = new LinkedHashMap();

    private Triangle mTriangleForeground;
    private Triangle mTriangleBackground;
}
