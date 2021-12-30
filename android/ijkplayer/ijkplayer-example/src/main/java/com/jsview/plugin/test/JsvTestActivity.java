package com.jsview.plugin.test;

import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.net.Uri;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.ViewGroup;

import androidx.appcompat.app.AppCompatActivity;

import java.io.IOException;
import java.util.ArrayList;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import tv.danmaku.ijk.media.example.R;
import tv.danmaku.ijk.media.example.application.Settings;
import tv.danmaku.ijk.media.player.IMediaPlayer;
import tv.danmaku.ijk.media.player.IjkMediaPlayer;

public class JsvTestActivity extends AppCompatActivity {
    public static final String TAG = "JsvPlug";

    public static Intent newIntent(Context context, String videoPath, String videoTitle) {
        Intent intent = new Intent(context, JsvTestActivity.class);
        intent.putExtra("videoPath", videoPath);
        intent.putExtra("videoTitle", videoTitle);
        return intent;
    }

    public static void intentTo(Context context, String videoPath, String videoTitle) {
        context.startActivity(newIntent(context, videoPath, videoTitle));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.jsv_test_activity);

        fakeForgeRenderer = new JsvFakeForgeRenderer(this);

        fakeForgeView = findViewById(R.id.fake_forge_view);
        fakeForgeView.setEGLContextClientVersion(2);
        fakeForgeView.setRenderer(fakeForgeRenderer);
        fakeForgeView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);

        // init player
        IjkMediaPlayer.loadLibrariesOnce(null);
        IjkMediaPlayer.native_profileBegin("libijkplayer.so");

        // handle arguments
        videoPath = getIntent().getStringExtra("videoPath");

        settings = new Settings(getApplicationContext());
    }

    @Override
    protected void onStart() {
        super.onStart();

        startPlayers(1);
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
            IjkMediaPlayer mp = startPlayerByIndex(idx);
            if(mp == null) {
                Log.e(TAG, "Failed to start IjkMediaPlayer: " + idx);
            }

            mediaPlayerList.add(mp);

            JsvFakeForgeRenderer.OnDrawFrameListener drawFrameListener = new JsvFakeForgeRenderer.OnDrawFrameListener() {
                @Override
                public void onDrawFrame(Object key, float[] mvpMatrix) {
                    Integer idx = (Integer) key;
                    IjkMediaPlayer mp = mediaPlayerList.get(idx);

                    
                    mp.native_jsvDrawFrame(mvpMatrix);
                }
            };
            fakeForgeRenderer.appendOnDrawFrameListener(idx, drawFrameListener);

        }
    }

    private IjkMediaPlayer startPlayerByIndex(int idx) {
        IjkMediaPlayer mp = new IjkMediaPlayer();

        mp.native_setLogLevel(IjkMediaPlayer.IJK_LOG_DEBUG);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "mediacodec", 0);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "opensles", 1);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "overlay-format", settings.getPixelFormat());
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "framedrop", 1);
        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "start-on-prepared", 0);

        mp.setOnPreparedListener(preparedListener);

        try {
            String url = Uri.parse(videoPath).toString();
            mp.setDataSource(url);
            mp.setAudioStreamType(AudioManager.STREAM_MUSIC);
            mp.setScreenOnWhilePlaying(true);
            mp.prepareAsync();
        } catch (Exception ex) {
            Log.e(TAG, "Failed to start IjkMediaPlayer", ex);
            mp.release();

            return null;
        }

        return mp;
    }

    IMediaPlayer.OnPreparedListener preparedListener = new IMediaPlayer.OnPreparedListener() {
        public void onPrepared(IMediaPlayer mp) {
            mp.start();
        }
    };

    private Settings settings;

    private GLSurfaceView fakeForgeView;
    private JsvFakeForgeRenderer fakeForgeRenderer;

    private String videoPath;
    private ArrayList<IjkMediaPlayer> mediaPlayerList = new ArrayList();
}
