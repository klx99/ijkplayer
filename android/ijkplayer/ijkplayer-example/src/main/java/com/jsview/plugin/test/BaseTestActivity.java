package com.jsview.plugin.test;

import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.TableLayout;

import androidx.appcompat.app.AppCompatActivity;

import java.util.ArrayList;
import java.util.Arrays;

import tv.danmaku.ijk.media.example.R;
import tv.danmaku.ijk.media.example.widget.media.InfoHudViewHolder;
import tv.danmaku.ijk.media.player.IMediaPlayer;
import tv.danmaku.ijk.media.player.IjkMediaPlayer;

public abstract class BaseTestActivity extends AppCompatActivity {
    public static final String TAG = "JsvPlug";

    protected abstract IjkMediaPlayer onMakeMediaPlayer(int idx);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.jsv_test_activity);

        TableLayout hudView = findViewById(R.id.hud_view);
        hudViewHolder = new InfoHudViewHolder(this, hudView);
        TableLayout hudViewSub = findViewById(R.id.hud_view_sub);
        hudViewHolderSub = new InfoHudViewHolder(this, hudViewSub);

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
            IjkMediaPlayer mp = startPlayerByIndex(idx);
            if(mp == null) {
                Log.e(TAG, "Failed to start IjkMediaPlayer: " + idx);
            }

            mediaPlayerList.add(mp);
        }
    }

    private IjkMediaPlayer startPlayerByIndex(int idx) {
        IjkMediaPlayer mp = startMediaPlayer(idx, VideoUrlList.get(idx), idx != 0);
        if(mp == null) {
            Log.e(TAG, "Failed to start IjkMediaPlayer: " + idx);
        }

        if(idx == 0) {
            hudViewHolder.setMediaPlayer(mp);
        } else if(idx == 1) {
            hudViewHolderSub.setMediaPlayer(mp);
        }

        return mp;
    }

    private IjkMediaPlayer startMediaPlayer(int idx, String url, boolean mute) {
        IjkMediaPlayer mp = onMakeMediaPlayer(idx);

        mp.native_setLogLevel(IjkMediaPlayer.IJK_LOG_DEBUG);
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
    private InfoHudViewHolder hudViewHolderSub;

    private ArrayList<IjkMediaPlayer> mediaPlayerList = new ArrayList();

    public static final int MediaPlayerCount = 1;
    private static final ArrayList<String> VideoUrlList = new ArrayList(Arrays.asList(
//        "/data/local/tmp/1080p60.mp4",
        "/data/local/tmp/test.mp4",
//            "http://192.168.3.188:11002/1002/test.m3u8",
//            "http://192.168.3.188:11007/1007/test.m3u8",
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
