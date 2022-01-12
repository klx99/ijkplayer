package com.jsview.plugin.test;

import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.media.MediaCodec;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.TableLayout;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

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

        fakeForgeRenderer = new JsvFakeForgeRenderer(this);

        TableLayout hudView = findViewById(R.id.hud_view);
        hudViewHolder = new InfoHudViewHolder(this, hudView);
        fakeForgeView = findViewById(R.id.fake_forge_view);
        fakeForgeView.setEGLContextClientVersion(2);
        fakeForgeView.setRenderer(fakeForgeRenderer);
//        fakeForgeView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        fakeForgeView.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);

        // init player
        IjkMediaPlayer.loadLibrariesOnce(null);
        IjkMediaPlayer.native_profileBegin("libijkplayer.so");
    }

    @Override
    protected void onStart() {
        super.onStart();

        startPlayers(MediaPlayerCount < VideoUrlList.size() ? MediaPlayerCount : VideoUrlList.size());
    }

    @Override
    protected void onStop() {
        super.onStop();

        for(IjkMediaPlayer mp : mediaPlayerList) {
            mp.stop();
            mp.release();
        }
        mediaPlayerList.clear();
    }

    private void startPlayers(int count) {
        for(int idx = 0; idx < count; idx++) {
            IjkMediaPlayer mp = startPlayerWithRenderer(idx);
            if(mp == null) {
                Log.e(TAG, "Failed to start IjkMediaPlayer: " + idx);
            }

            mediaPlayerList.add(mp);
            fakeVideoRendererList.add(new JsvFakeVideoRenderer(mp, fakeForgeRenderer, idx));
        }
    }

    private IjkMediaPlayer startPlayerWithRenderer(int idx) {
        IjkMediaPlayer mp = startPlayerByIndex(VideoUrlList.get(idx), idx != 0);
        if(mp == null) {
            Log.e(TAG, "Failed to start IjkMediaPlayer: " + idx);
        }

        IjkMediaPlayer.OnVideoSyncListener videoSyncListener = new IjkMediaPlayer.OnVideoSyncListener () {
            @Override
            public void onVideoSync(IMediaPlayer mp) {
                fakeForgeView.requestRender();
            }
        };
        mp.setOnVideoSyncListener(videoSyncListener);

        if(idx == 0) {
            hudViewHolder.setMediaPlayer(mp);
        }

        return mp;
    }

    private IjkMediaPlayer startPlayerByIndex(String url, boolean mute) {
        IjkMediaPlayer mp = new IjkMediaPlayer();

        mp.native_setLogLevel(IjkMediaPlayer.IJK_LOG_DEBUG);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "mediacodec", UseMediaCodec);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "opensles", 1);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "overlay-format", OverlayFormat);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "framedrop", 1);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "start-on-prepared", 0);

        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_FORMAT, "http-detect-range-support", 0);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_CODEC, "skip_loop_filter", 48);

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
    private GLSurfaceView fakeForgeView;
    private JsvFakeForgeRenderer fakeForgeRenderer;
    private JsvFakeVideoRenderer fakeVideoRenderer;

    private ArrayList<IjkMediaPlayer> mediaPlayerList = new ArrayList();
    private ArrayList<JsvFakeVideoRenderer> fakeVideoRendererList = new ArrayList();

    private static final int UseMediaCodec = 1;
//     private static final String OverlayFormat = "fcc-jsv0";
//    private static final String OverlayFormat = "fcc-jsv1";
    private static final String OverlayFormat = "fcc-jsv2";
    public static final int MediaPlayerCount = 2;
    private static final ArrayList<String> VideoUrlList = new ArrayList(Arrays.asList(
//          "/data/local/tmp/test.mp4",
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
