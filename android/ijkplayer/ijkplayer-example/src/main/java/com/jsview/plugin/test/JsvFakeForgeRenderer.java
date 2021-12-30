package com.jsview.plugin.test;

import android.content.Context;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

class JsvFakeForgeRenderer implements GLSurfaceView.Renderer {
    interface OnDrawFrameListener {
        void onDrawFrame(final float[] mvpMatrix);
    }

    public JsvFakeForgeRenderer(Context context) {
        mContext = context;
    }

    public void setOnDrawFrameListener(OnDrawFrameListener listener) {
        drawFrameListener = listener;
    }

    @Override
    public void onSurfaceCreated(GL10 glUnused, EGLConfig config) {
        GLES20.glClearColor(0.0f, 0.0f, 1.0f, 1.0f);

        mTriangleForeground = new Triangle(mContext);
        mTriangleBackground = new Triangle(mContext);
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

        if(drawFrameListener != null) {
            drawFrameListener.onDrawFrame(mvpMatrix);
        }

        Matrix.rotateM(mvpMatrix, 0, 180, 0, 0, 1.0f);
        mTriangleForeground.onDrawFrame(mvpMatrix);
    }

    private float[] mProjectionMatrix = new float[16];
    private float[] mViewMatrix = new float[16];

    private Context mContext;
    private OnDrawFrameListener drawFrameListener;

    private Triangle mTriangleForeground;
    private Triangle mTriangleBackground;
}
