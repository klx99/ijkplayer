package com.jsview.plugin.test;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.opengl.Matrix;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.FrameLayout;
import android.widget.TextView;

import com.jsview.plugin.JsvSharedMediaPlayer;
import com.jsview.plugin.JsvSharedSurfaceView;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Arrays;

import tv.danmaku.ijk.media.example.R;
import tv.danmaku.ijk.media.example.widget.media.InfoHudViewHolder;
import tv.danmaku.ijk.media.player.IjkMediaPlayer;

public class JsvTestActivity extends BaseTestActivity {
    public static void intentTo(Context context) {
        Intent intent = new Intent(context, JsvTestActivity.class);
        context.startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mainHandler = new Handler(Looper.getMainLooper());

        FrameLayout rootView = findViewById(R.id.root_view);

        JsvSharedSurfaceView.SetParentView(this, rootView);
        jsvSharedSurfaceView = JsvSharedSurfaceView.GetInstance();

        logView = findViewById(R.id.log_view);
    }

    @Override
    protected void onStart() {
        super.onStart();

        jsvSharedSurfaceView.appendRenderer("triangle", (key) -> {
            if(triangle == null) {
                triangle = new Triangle(this, Color.GREEN);
            }
            triangle.onDrawFrame();
        });
    }

    @Override
    protected void onStop() {
        super.onStop();
    }

    @Override
    protected IjkMediaPlayer onMakeMediaPlayer(int idx) {
        JsvSharedMediaPlayer mp = new JsvSharedMediaPlayer();

        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_PLAYER, "overlay-format", JsvSharedMediaPlayer.OverlayFormatFccJSVH);

        float[] matrix4 = new float[16];
        Matrix.setIdentityM(matrix4, 0);
        if(idx > 0) { // 第0个视频全屏播放
            float offset = 2.0f / MediaPlayerCount * idx - 1;
            Matrix.scaleM(matrix4, 0, 0.5f, 0.5f, 1);
            Matrix.translateM(matrix4, 0, offset, offset, 0);
        }
        // 将第4象限（Forge用）转为全屏
        Matrix.translateM(matrix4, 0, -1, 1, 0);
        Matrix.scaleM(matrix4, 0, 2, 2, 1);
        ByteBuffer buf = ByteBuffer.allocateDirect(16 * 4).order(ByteOrder.nativeOrder());
        FloatBuffer mat4Buf = buf.asFloatBuffer().put(matrix4);
        mp.setMatrix4ByDirectBuffer(mat4Buf);

        mp.setOnVideoSyncListener((player) -> {
            if(idx != 0) {
                return;
            }
            jsvSharedSurfaceView.requestRender();

            mainHandler.post(() -> {
                String log = "video fmt:" + ((JsvSharedMediaPlayer)player).getColorFormat();
                log += ", w/h:" + player.getVideoWidth() + "/" + player.getVideoHeight();
                logView.setText(log);
            });
        });

        jsvSharedSurfaceView.appendRenderer(mp, (key) -> {
            JsvSharedMediaPlayer player = (JsvSharedMediaPlayer) key;
            player.drawFrame();
        });

        return mp;
    }

    private Handler mainHandler;
    private TextView logView;
    private JsvSharedSurfaceView jsvSharedSurfaceView;
    private Triangle triangle;
}
