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

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import tv.danmaku.ijk.media.player.IjkMediaPlayer;

class JsvFakeVideoRenderer {
    public JsvFakeVideoRenderer(IjkMediaPlayer mp, JsvFakeForgeRenderer forgeRenderer) {
        playerHandler = mp.native_lockPlayerHandler();
        long getFrameFormatHandler = mp.native_getFrameFormatHandler();
        long lockFrameBufferHandler = mp.native_lockFrameBufferHandler();
        long unlockFrameBufferHandler = mp.native_unlockFrameBufferHandler();

        IjkMediaPlayer.native_testPlayerNativeHandlers(playerHandler,
                                                       getFrameFormatHandler,
                                                       lockFrameBufferHandler,
                                                       unlockFrameBufferHandler);

        forgeRenderer.appendOnDrawFrameListener(this, drawFrameListener);
    }

    JsvFakeForgeRenderer.OnDrawFrameListener drawFrameListener = new JsvFakeForgeRenderer.OnDrawFrameListener() {
        @Override
        public void onDrawFrame(Object key, final float[] mvpMatrix) {
            float[] matrix = mvpMatrix.clone();

            IjkMediaPlayer.native_testDrawFrame(playerHandler, matrix);
        }
    };

    long playerHandler;
}
