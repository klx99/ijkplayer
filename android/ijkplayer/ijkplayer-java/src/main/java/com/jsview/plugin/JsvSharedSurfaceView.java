package com.jsview.plugin;

import android.content.Context;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.os.SystemClock;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class JsvSharedSurfaceView extends GLSurfaceView
                                  implements GLSurfaceView.Renderer {
    public interface Renderer {
        void onDrawFrame(Object key);
    }

    public static void SetParentView(Context context, ViewGroup parentView) {
        ViewContext = context;
        ParentView = parentView;
    }

    public static JsvSharedSurfaceView GetInstance() {
        synchronized (JsvSharedSurfaceView.class) {
            if (ViewInstance == null) {
                ViewInstance = new JsvSharedSurfaceView(ViewContext);
            }
        }

        return ViewInstance;
    }

    public void appendRenderer(Object key, Renderer renderer) {
        if(renderer == null) {
            removeRenderer(key);
        }
        synchronized (JsvSharedSurfaceView.class) {
            if(ViewInstance.getParent() == null) {
                ParentView.addView(ViewInstance, 0, new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        400));
            }

            rendererMap.put(key, renderer);
        }
    }

    public void removeRenderer(Object key) {
        synchronized (JsvSharedSurfaceView.class) {
            rendererMap.remove(key);

            if (rendererMap.isEmpty()) {
                ParentView.removeView(ViewInstance);
                ViewInstance = null;
            }
        }
    }

    @Override
    public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {
        GLES20.glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        Matrix.setLookAtM(viewMatrix, 0, 0, 0, -5, 0f, 0f, 0f, 0f, 1.0f, 0.0f);
    }

    @Override
    public void onSurfaceChanged(GL10 glUnused, int width, int height) {
        GLES20.glViewport(0, 0, width, height);
        float ratio = (float) width / height;
        Matrix.frustumM(projectionMatrix, 0, -ratio, ratio, -1, 1, 3, 7);
        Matrix.multiplyMM(mvpMatrix, 0, projectionMatrix, 0, viewMatrix, 0);
    }

    @Override
    public void onDrawFrame(GL10 gl10) {
        GLES20.glClear( GLES20.GL_DEPTH_BUFFER_BIT | GLES20.GL_COLOR_BUFFER_BIT);

        for (Map.Entry<Object, Renderer> entry : rendererMap.entrySet()) {
            Object key = entry.getKey();
            Renderer renderer = entry.getValue();
            renderer.onDrawFrame(key);
        }

        fpsCount.incrementAndGet();
    }

    public String getFps() {
        long currTime = SystemClock.uptimeMillis();
        long deltaTime = currTime - prevTime;
        prevTime = currTime;

        long count = fpsCount.getAndSet(0);
        float fps = count * 1000f / deltaTime;
        return String.format("%.02f", fps);
    }

    public JsvSharedSurfaceView(Context context) {
        super(context);

        this.setZOrderOnTop(false);
        this.setZOrderMediaOverlay(false);

        this.setEGLContextClientVersion(2);

        this.setRenderer(this);
        this.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
//        this.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
    }

    private float[] projectionMatrix = new float[16];
    private float[] viewMatrix = new float[16];
    private float[] mvpMatrix = new float[16];

    private Map<Object, Renderer> rendererMap = new LinkedHashMap();

    private AtomicInteger fpsCount = new AtomicInteger(0);
    private long prevTime = 0;

    private static Context ViewContext;
    private static ViewGroup ParentView;
    private static JsvSharedSurfaceView ViewInstance;
}
