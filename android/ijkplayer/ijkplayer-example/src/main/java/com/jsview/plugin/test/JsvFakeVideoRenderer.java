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

import static com.jsview.plugin.test.JsvTestActivity.MediaPlayerCount;

class JsvFakeVideoRenderer {
    public JsvFakeVideoRenderer(IjkMediaPlayer mp, JsvFakeForgeRenderer forgeRenderer, int idx) {
        playerHandler = mp.native_lockPlayerHandler();
        long getFrameFormatHandler = mp.native_getFrameFormatHandler();
        long lockFrameBufferHandler = mp.native_lockFrameBufferHandler();
        long unlockFrameBufferHandler = mp.native_unlockFrameBufferHandler();

        IjkMediaPlayer.native_testPlayerNativeHandlers(playerHandler,
                                                       getFrameFormatHandler,
                                                       lockFrameBufferHandler,
                                                       unlockFrameBufferHandler);

        forgeRenderer.appendOnDrawFrameListener(idx, drawFrameListener);
    }

    JsvFakeForgeRenderer.OnDrawFrameListener drawFrameListener = new JsvFakeForgeRenderer.OnDrawFrameListener() {
        @Override
        public void onDrawFrame(Object key, final float[] mvpMatrix) {
            float[] matrix = mvpMatrix.clone();

            int idx = (Integer) key;
            if(idx > 0) { // 第0个视频全屏播放
                float offset = 2.0f / MediaPlayerCount;
                float center = MediaPlayerCount / 2.0f;
                Matrix.scaleM(matrix, 0, 0.5f, 0.5f, 1);
                Matrix.translateM(matrix, 0, offset * (idx - center), offset * (idx - center), 0);
            }

            IjkMediaPlayer.native_testDrawFrame(playerHandler, matrix);
        }
    };

    long playerHandler;
}
