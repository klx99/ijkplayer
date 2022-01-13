package com.jsview.plugin.test;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.media.AudioManager;
import android.opengl.Matrix;
import android.os.Bundle;
import android.util.Log;
import android.widget.FrameLayout;
import android.widget.TableLayout;

import androidx.appcompat.app.AppCompatActivity;

import com.jsview.plugin.JsvSharedMediaPlayer;
import com.jsview.plugin.JsvSharedSurfaceView;

import java.util.ArrayList;
import java.util.Arrays;

import tv.danmaku.ijk.media.example.R;
import tv.danmaku.ijk.media.example.widget.media.InfoHudViewHolder;
import tv.danmaku.ijk.media.player.IMediaPlayer;

public class JsvTestActivity extends AppCompatActivity {
    public static final String TAG = "JsvPlug";

    public static void intentTo(Context context) {
        Intent intent = new Intent(context, JsvTestActivity.class);
        context.startActivity(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.jsv_test_activity);

        FrameLayout rootView = findViewById(R.id.root_view);

        jsvSharedSurfaceView = new JsvSharedSurfaceView(this);
        rootView.addView(jsvSharedSurfaceView, 0);

        TableLayout hudView = findViewById(R.id.hud_view);
        hudViewHolder = new InfoHudViewHolder(this, hudView);

        // init player
        JsvSharedMediaPlayer.loadLibrariesOnce(null);
        JsvSharedMediaPlayer.native_profileBegin("libijkplayer.so");
    }

    @Override
    protected void onStart() {
        super.onStart();

        startPlayers(MediaPlayerCount < VideoUrlList.size() ? MediaPlayerCount : VideoUrlList.size());

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

        for(JsvSharedMediaPlayer mp : mediaPlayerList) {
            mp.stop();
            mp.release();
        }
        mediaPlayerList.clear();
    }

    private void startPlayers(int count) {
        for(int idx = 0; idx < count; idx++) {
            JsvSharedMediaPlayer mp = startPlayerWithRenderer(idx);
            if(mp == null) {
                Log.e(TAG, "Failed to start JsvSharedMediaPlayer: " + idx);
            }

            mediaPlayerList.add(mp);
        }
    }

    private JsvSharedMediaPlayer startPlayerWithRenderer(int idx) {
        JsvSharedMediaPlayer mp = startPlayerByIndex(VideoUrlList.get(idx), idx != 0);
        if(mp == null) {
            Log.e(TAG, "Failed to start JsvSharedMediaPlayer: " + idx);
        }

        float[] matrix = new float[16];
        Matrix.setIdentityM(matrix, 0);
        if(idx > 0) { // 第0个视频全屏播放
            float offset = 2.0f / MediaPlayerCount * idx - 1;
            Matrix.scaleM(matrix, 0, 0.5f, 0.5f, 1);
            Matrix.translateM(matrix, 0, offset, offset, 0);
        }
        mp.setMvpMatrix(matrix);

        mp.setOnVideoSyncListener((player) -> {
            jsvSharedSurfaceView.requestRender();
        });

        jsvSharedSurfaceView.appendRenderer(mp, (key) -> {
            JsvSharedMediaPlayer player = (JsvSharedMediaPlayer) key;
            player.drawFrame();
        });

        if(idx == 0) {
            hudViewHolder.setMediaPlayer(mp);
        }

        return mp;
    }

    private JsvSharedMediaPlayer startPlayerByIndex(String url, boolean mute) {
        JsvSharedMediaPlayer mp = new JsvSharedMediaPlayer();

        mp.native_setLogLevel(JsvSharedMediaPlayer.IJK_LOG_DEBUG);
        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_PLAYER, "overlay-format", JsvSharedMediaPlayer.OverlayFormatFccJSV2);
        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_PLAYER, "framedrop", 1);
        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_PLAYER, "start-on-prepared", 0);
        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_FORMAT, "http-detect-range-support", 0);
        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_CODEC, "skip_loop_filter", 48);

        IMediaPlayer.OnPreparedListener preparedListener = new IMediaPlayer.OnPreparedListener() {
            public void onPrepared(IMediaPlayer mp) {
                mp.start();
            }
        };
        mp.setOnPreparedListener(preparedListener);

        IMediaPlayer.OnErrorListener errorListener = new IMediaPlayer.OnErrorListener () {
            @Override
            public boolean onError(IMediaPlayer mp, int what, int extra) {
                Log.e(TAG, mp + " Error. code=" + what);
                return false;
            }
        };
        mp.setOnErrorListener(errorListener);

        try {
            mp.setDataSource(url);
            mp.setAudioStreamType(AudioManager.STREAM_MUSIC);
            mp.setScreenOnWhilePlaying(true);
            mp.prepareAsync();
            if(mute) {
                mp.setVolume(0.0f, 0.0f);
            }
        } catch (Exception ex) {
            Log.e(TAG, "Failed to start IjkMediaPlayer", ex);
            mp.release();

            return null;
        }

        return mp;
    }

    private InfoHudViewHolder hudViewHolder;
    private JsvSharedSurfaceView jsvSharedSurfaceView;
    private Triangle triangle;

    private ArrayList<JsvSharedMediaPlayer> mediaPlayerList = new ArrayList();

    private static final int UseMediaCodec = 1;
//     private static final String OverlayFormat = "fcc-jsv0";
//    private static final String OverlayFormat = "fcc-jsv1";
    public static final int MediaPlayerCount = 1;
    private static final ArrayList<String> VideoUrlList = new ArrayList(Arrays.asList(
        "/data/local/tmp/test.mp4",
        "http://39.135.138.58:18890/PLTV/88888888/224/3221225642/index.m3u8",
        "http://39.135.138.58:18890/PLTV/88888888/224/3221225633/index.m3u8",
        "http://39.135.138.58:18890/PLTV/88888888/224/3221225643/index.m3u8",
        "http://39.135.138.58:18890/PLTV/88888888/224/3221225644/index.m3u8",
//        "http://192.168.1.99/test.mp4",
//        "http://192.168.1.99/test.mp4",
//        "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_16x9/gear1/prog_index.m3u8",
//        "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_16x9/gear2/prog_index.m3u8",
//        "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_16x9/gear3/prog_index.m3u8",
//        "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_16x9/gear4/prog_index.m3u8",
        "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_16x9/gear5/prog_index.m3u8"
    ));
}
