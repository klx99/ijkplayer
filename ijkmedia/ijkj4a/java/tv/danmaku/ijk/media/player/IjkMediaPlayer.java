package tv.danmaku.ijk.media.player;

import android.media.MediaCodec;
import android.os.Bundle;

@SimpleCClassName
public class IjkMediaPlayer {
    private long mNativeMediaPlayer;
    private long mNativeMediaDataSource;
    private long mNativeAndroidIO;

    private static void postEventFromNative(Object weakThiz, int what, int arg1, int arg2, Object obj);
    private static String onSelectCodec(Object weakThiz, String mimeType, int profile, int level);
    private static boolean onNativeInvoke(Object weakThiz, int what, Bundle args);
    private static boolean JsvOnVideoSync(Object weakThiz, MediaCodec mediaCodec, int bufferIndex, int bufferOffset, int bufferSize);
}
