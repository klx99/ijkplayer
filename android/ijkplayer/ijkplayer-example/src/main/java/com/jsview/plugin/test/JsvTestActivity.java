package com.jsview.plugin.test;

import android.content.Context;
import android.content.Intent;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.view.ViewGroup;

import androidx.appcompat.app.AppCompatActivity;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import tv.danmaku.ijk.media.example.R;
import tv.danmaku.ijk.media.player.IjkMediaPlayer;

public class JsvTestActivity extends AppCompatActivity {
    public static final String TAG = "JsvPlug";

    private String mVideoPath;

    private GLSurfaceView fakeForgeView;

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


        fakeForgeView = findViewById(R.id.fake_forge_view);
        fakeForgeView.setEGLContextClientVersion(2);
        fakeForgeView.setRenderer(new JsvFakeForgeRenderer(this));
        fakeForgeView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);

        // init player
        IjkMediaPlayer.loadLibrariesOnce(null);
        IjkMediaPlayer.native_profileBegin("libijkplayer.so");

        // handle arguments
        mVideoPath = getIntent().getStringExtra("videoPath");
    }
}
