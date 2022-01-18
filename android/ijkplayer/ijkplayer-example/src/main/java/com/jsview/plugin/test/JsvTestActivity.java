package com.jsview.plugin.test;

import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.media.AudioManager;
import android.opengl.Matrix;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.TableLayout;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.jsview.plugin.JsvSharedMediaPlayer;
import com.jsview.plugin.JsvSharedSurfaceView;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Arrays;

import tv.danmaku.ijk.media.example.R;
import tv.danmaku.ijk.media.example.widget.media.InfoHudViewHolder;
import tv.danmaku.ijk.media.player.IMediaPlayer;
import tv.danmaku.ijk.media.player.IjkMediaPlayer;

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

        mainHandler = new Handler(Looper.getMainLooper());

        FrameLayout rootView = findViewById(R.id.root_view);

        JsvSharedSurfaceView.SetParentView(this, rootView);
        jsvSharedSurfaceView = JsvSharedSurfaceView.GetInstance();

        jsvSharedSurfaceViewSub = new JsvSharedSurfaceView(this);
        rootView.addView(jsvSharedSurfaceViewSub, new ViewGroup.LayoutParams(
                500,
                ViewGroup.LayoutParams.MATCH_PARENT));

        logView = findViewById(R.id.log_view);

        TableLayout hudView = findViewById(R.id.hud_view_0);
        hudViewHolder0 = new InfoHudViewHolder(this, hudView);
        hudView = findViewById(R.id.hud_view_1);
        hudViewHolder1 = new InfoHudViewHolder(this, hudView);

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

        float[] matrix4 = new float[16];
        Matrix.setIdentityM(matrix4, 0);
//        if(idx > 0) { // 第0个视频全屏播放
//            float offset = 2.0f / MediaPlayerCount * idx - 1;
//            Matrix.scaleM(matrix4, 0, 0.5f, 0.5f, 1);
//            Matrix.translateM(matrix4, 0, offset, offset, 0);
//        }

        // 将第4象限（Forge用）转为全屏
        Matrix.translateM(matrix4, 0, -1, 1, 0);
        Matrix.scaleM(matrix4, 0, 2, 2, 1);

        ByteBuffer buf = ByteBuffer.allocateDirect(16 * 4).order(ByteOrder.nativeOrder());
        FloatBuffer mat4Buf = buf.asFloatBuffer().put(matrix4);
        mp.setMatrix4ByDirectBuffer(mat4Buf);

        if(idx == 0) {
            hudViewHolder0.setMediaPlayer(mp);
            mp.setOnVideoSyncListener((player) -> {
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

        } else if(idx == 1) {
            hudViewHolder1.setMediaPlayer(mp);
            mp.setOnVideoSyncListener((player) -> {
                jsvSharedSurfaceViewSub.requestRender();
            });

            jsvSharedSurfaceViewSub.appendRenderer(mp, (key) -> {
                JsvSharedMediaPlayer player = (JsvSharedMediaPlayer) key;
                player.drawFrame();
            });

        }

        return mp;
    }

    private JsvSharedMediaPlayer startPlayerByIndex(String url, boolean mute) {
        JsvSharedMediaPlayer mp = new JsvSharedMediaPlayer();

        mp.native_setLogLevel(JsvSharedMediaPlayer.IJK_LOG_DEBUG);
        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_PLAYER, "overlay-format", JsvSharedMediaPlayer.OverlayFormatFccJSVH);
        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_PLAYER, "framedrop", 1);
        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_PLAYER, "start-on-prepared", 0);
        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_FORMAT, "http-detect-range-support", 0);
        mp.setOption(JsvSharedMediaPlayer.OPT_CATEGORY_CODEC, "skip_loop_filter", 48);

        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "mediacodec-all-videos", 1);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "framedrop", 1);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_FORMAT, "analyzemaxduration", 100L);

        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_FORMAT, "flush_packets", 1L);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "min-frames", 1000);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "sync-av-start", 0);


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

    private Handler mainHandler;
    private TextView logView;
    private InfoHudViewHolder hudViewHolder0;
    private InfoHudViewHolder hudViewHolder1;
    private JsvSharedSurfaceView jsvSharedSurfaceView;
    private JsvSharedSurfaceView jsvSharedSurfaceViewSub;
    private Triangle triangle;

    private ArrayList<JsvSharedMediaPlayer> mediaPlayerList = new ArrayList();

    public static final int MediaPlayerCount = 2;
    private static final ArrayList<String> VideoUrlList = new ArrayList(Arrays.asList(
//        "/data/local/tmp/1080P60.mp4",
//        "/data/local/tmp/test.mp4",
//        "http://192.168.3.188:11105/1105/test.m3u8", // 50fps
        "http://192.168.3.188:11002/1002/test.m3u8",
        "http://192.168.3.188:11007/1007/test.m3u8",
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
