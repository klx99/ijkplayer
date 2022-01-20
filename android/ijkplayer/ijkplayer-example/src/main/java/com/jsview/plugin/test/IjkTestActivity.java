package com.jsview.plugin.test;

import android.content.Context;
import android.content.Intent;
import tv.danmaku.ijk.media.player.IjkMediaPlayer;

public class IjkTestActivity extends BaseTestActivity {
    public static void intentTo(Context context) {
        Intent intent = new Intent(context, IjkTestActivity.class);
        context.startActivity(intent);
    }

    @Override
    protected IjkMediaPlayer onMakeMediaPlayer(int idx) {
        IjkMediaPlayer mp = new IjkMediaPlayer();

        mp.setOption(IjkMediaPlayer.OPT_CATEGORY_PLAYER, "mediacodec", 1);

        return mp;
    }
}
