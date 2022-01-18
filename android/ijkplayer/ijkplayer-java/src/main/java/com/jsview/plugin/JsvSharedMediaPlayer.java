package com.jsview.plugin;

import android.opengl.Matrix;
import android.text.TextUtils;

import java.nio.ByteBuffer;
import java.nio.FloatBuffer;

import tv.danmaku.ijk.media.player.IMediaPlayer;
import tv.danmaku.ijk.media.player.IjkMediaPlayer;

public class JsvSharedMediaPlayer extends IjkMediaPlayer {
    public interface OnVideoSyncListener extends IjkMediaPlayer.OnVideoSyncListener{
    }

    public void setOnVideoSyncListener(OnVideoSyncListener listener) {
        super.videoSyncListener = listener;
    }

    public void setMatrix4ByDirectBuffer(FloatBuffer mat4Buf) {
        super.native_jsvSetMatrix4ByDirectBuffer(mat4Buf);
    }

    public void setMatrix4(long mat4NativeAddr) {
        super.native_jsvSetMatrix4(mat4NativeAddr);
    }

    public int drawFrame() {
        return super.native_jsvDrawFrame();
    }

    public int getColorFormat() {
        return super.native_jsvGetColorFormat();
    }

    @Override
    public void setOption(int category, String name, String value) {
        super.setOption(category, name, value);

        if(TextUtils.equals(value, OverlayFormatFccJSVH)) {
            super.setOption(category, "opensles", 1);
            super.setOption(category, "mediacodec", 1);
        }
    }

    public static final String OverlayFormatFccJSVH = "fcc-jsvh"; // JsView Hardware MediaCodec
}
