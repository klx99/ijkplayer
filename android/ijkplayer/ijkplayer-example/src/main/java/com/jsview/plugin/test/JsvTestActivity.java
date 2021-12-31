package com.jsview.plugin.test;

import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.opengl.GLSurfaceView;
import android.opengl.Matrix;
import android.os.Bundle;
import android.util.Log;

import androidx.appcompat.app.AppCompatActivity;

import java.util.ArrayList;
import java.util.Arrays;

import tv.danmaku.ijk.media.example.R;
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
            IjkMediaPlayer mp = startPlayerByIndex(VideoUrlList.get(idx), idx != 0);
            if(mp == null) {
                Log.e(TAG, "Failed to start IjkMediaPlayer: " + idx);
            }

            mediaPlayerList.add(mp);

            IjkMediaPlayer.OnVideoSyncListener videoSyncListener = new IjkMediaPlayer.OnVideoSyncListener () {
                @Override
                public void onVideoSync(IMediaPlayer mp) {
                    fakeForgeView.requestRender();
                }
            };
            mp.setOnVideoSyncListener(videoSyncListener);

            JsvFakeForgeRenderer.OnDrawFrameListener drawFrameListener = new JsvFakeForgeRenderer.OnDrawFrameListener() {
                @Override
                public void onDrawFrame(Object key, final float[] mvpMatrix) {
                    Integer idx = (Integer) key;
                    IjkMediaPlayer mp = mediaPlayerList.get(idx);

                    float offset = 2.0f / MediaPlayerCount;
                    float center = MediaPlayerCount / 2.0f;
                    float[] matrix = mvpMatrix.clone();
                    Matrix.scaleM(matrix, 0, 0.5f, 0.5f, 1);
                    Matrix.translateM(matrix, 0, offset * (center - idx), offset * (center - idx), 0);

                    mp.native_jsvDrawFrame(matrix);
                }
            };
            fakeForgeRenderer.appendOnDrawFrameListener(idx, drawFrameListener);
        }
    }

    private IjkMediaPlayer startPlayerByIndex(String url, boolean mute) {
        IjkMediaPlayer mp = new IjkMediaPlayer();

        mp.native_setLogLevel(IjkMediaPlayer.IJK_LOG_DEBUG);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "mediacodec", 0);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "opensles", 1);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "overlay-format", OverlayFormat);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "framedrop", 1);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "start-on-prepared", 0);

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

    private GLSurfaceView fakeForgeView;
    private JsvFakeForgeRenderer fakeForgeRenderer;

    private ArrayList<IjkMediaPlayer> mediaPlayerList = new ArrayList();

//    private static final String OverlayFormat = "fcc-jsv0";
    private static final String OverlayFormat = "fcc-jsv1";
    private static final int MediaPlayerCount = 1;
    private static final ArrayList<String> VideoUrlList = new ArrayList(Arrays.asList(
        "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_16x9/gear1/prog_index.m3u8",
        "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_16x9/gear2/prog_index.m3u8",
        "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_16x9/gear3/prog_index.m3u8",
        "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_16x9/gear4/prog_index.m3u8",
        "http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_16x9/gear5/prog_index.m3u8"));
}
