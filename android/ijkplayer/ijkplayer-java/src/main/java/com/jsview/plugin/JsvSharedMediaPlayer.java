package com.jsview.plugin;

import android.opengl.Matrix;
import android.text.TextUtils;

import tv.danmaku.ijk.media.player.IjkMediaPlayer;

public class JsvSharedMediaPlayer extends IjkMediaPlayer {
    public interface OnVideoSyncListener extends IjkMediaPlayer.OnVideoSyncListener{
    }

    public void setOnVideoSyncListener(OnVideoSyncListener listener) {
        super.videoSyncListener = listener;
    }

    public void setMvpMatrix(float[] matrix) {
        mvpMatrix = matrix;
    }

    public int drawFrame() {
        if(mvpMatrix == null) {
            // 设置默认matrix
            mvpMatrix = new float[16];
            Matrix.setIdentityM(mvpMatrix, 0);
        }

        return super.native_jsvDrawFrame(mvpMatrix);
    }

    @Override
    public void setOption(int category, String name, String value) {
        super.setOption(category, name, value);

        if(TextUtils.equals(value, OverlayFormatFccJSV2)) {
            super.setOption(category, "opensles", 1);
            super.setOption(category, "mediacodec", 1);
        }
    }

    public static final String OverlayFormatFccJSV2 = "fcc-jsv2";

    private float[] mvpMatrix;
}
