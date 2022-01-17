package com.jsview.plugin;

import android.content.Context;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.view.ViewGroup;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

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
        if(ViewInstance == null) {
            synchronized (JsvSharedSurfaceView.class) {
                if (ViewInstance == null) {
                    ViewInstance = new JsvSharedSurfaceView(ViewContext);
                    ParentView.addView(ViewInstance, new ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT));
                }
            }
        }

        return ViewInstance;
    }

    public void appendRenderer(Object key, Renderer renderer) {
        if(renderer == null) {
            removeRenderer(key);
        }
        rendererMap.put(key, renderer);
    }

    public void removeRenderer(Object key) {
        rendererMap.remove(key);
    }

    @Override
    public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {
        GLES20.glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
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
    }

    private JsvSharedSurfaceView(Context context) {
        super(context);

        this.setEGLContextClientVersion(2);
        this.setRenderer(this);
//        this.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        this.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
    }


    private float[] projectionMatrix = new float[16];
    private float[] viewMatrix = new float[16];
    private float[] mvpMatrix = new float[16];

    private Map<Object, Renderer> rendererMap = Collections.synchronizedMap(new LinkedHashMap()); // 线程安全的有序map

    private static Context ViewContext;
    private static ViewGroup ParentView;
    private static JsvSharedSurfaceView ViewInstance;
}
