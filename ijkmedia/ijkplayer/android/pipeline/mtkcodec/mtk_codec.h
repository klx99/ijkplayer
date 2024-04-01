#ifndef MTK_CODEC_H
#define MTK_CODEC_H
// #include "Surface.h"
#include <stdint.h>
#include <jni.h>
#ifdef __cplusplus
extern "C"
{
#endif
#include "vformat.h"
#include "aformat.h"

// namespace android
// {
#define MTK_SUCCESS   0
#define MTK_FAIL      (-1)
#define MTK_NODATA    (-2)

#define MTK_HANDLER_INVALID      (0xffffffff)

struct CTC_SUBT_PARM_S{
    bool mIsEnable;
    int  mPids[10];
};

typedef enum
{
    STREAMTYPE_NULL = -1,
    STREAMTYPE_TS,
    STREAMTYPE_VIDEO,
    STREAMTYPE_AUDIO,
    STREAMTYPE_MAX,
}STREAMTYPE_E;

typedef struct {
    //pid
    unsigned short  pid;
    //video width
    int             nVideoWidth;
    //video height
    int             nVideoHeight;
    //video frame rate
    uint32_t             nFrameRate;
    //video format
    vformat_t       vFmt;
    //video codec format
    long   cFmt;
} VIDEO_PARA_T, *PVIDEO_PARA_T;

typedef struct {
    unsigned short  pid;
    //numbers of audio channel
    int             nChannels;
    int             nSampleRate;
    uint32_t        bit_rate;
    uint16_t        block_align;
    uint16_t        sample_depth;
    //audio format
    aformat_t       aFmt;
    int             nExtraSize;
    unsigned char*  pExtraData;
} AUDIO_PARA_T, *PAUDIO_PARA_T;

typedef struct{
	unsigned short	pid;//pid
	int sub_type; 
	unsigned short  pgno;//ttx sub pgno
	int stream_index;
	
}SUBTITLE_PARA_T, *PSUBTITLE_PARA_T;

typedef enum {
	/* subtitle codecs */
    CODEC_ID_DVD_SUBTITLE= 0x17000,
    CODEC_ID_DVB_SUBTITLE,
    CODEC_ID_TEXT,  ///< raw UTF-8 text
    CODEC_ID_XSUB,
    CODEC_ID_SSA,
    CODEC_ID_MOV_TEXT,
    CODEC_ID_HDMV_PGS_SUBTITLE,
    CODEC_ID_DVB_TELETEXT,
    CODEC_ID_SRT,
    CODEC_ID_MICRODVD,
}SUB_TYPE;

typedef struct {
   int abuf_size;
   int abuf_data_len;
   int abuf_free_len;
   int vbuf_size;
   int vbuf_data_len;
   int vbuf_free_len;
}AVBUF_STATUS, *PAVBUF_STATUS;

typedef enum {
    IPTV_PLAYER_CONTENTMODE_AUTO = 0,
    IPTV_PLAYER_CONTENTMODE_FULL = 1,
    IPTV_PLAYER_CONTENTMODE_LETTERBOX_BY_WIDTH = 2,
    IPTV_PLAYER_CONTENTMODE_LETTERBOX_BY_HEIGHT = 3,
    IPTV_PLAYER_CONTENTMODE_NULL = 255,
} IPTV_PLAYER_CONTENTMODE_e;

typedef enum {
    IPTV_PLAYER_EVT_STREAM_VALID = 0,
    //first frame decoded event
    IPTV_PLAYER_EVT_FIRST_PTS,
    //VOD EOS event
    IPTV_PLAYER_EVT_VOD_EOS,
    //underflow event
    IPTV_PLAYER_EVT_UNDERFLOW,

    IPTV_PLAYER_EVENT_STUTTER = 0x100,
       //Unload event
    IPTV_PLAYER_EVT_UNLOAD_START,
    IPTV_PLAYER_EVT_UNLOAD_END,
    //BLURREDSCREEN event
    IPTV_PLAYER_EVT_BLURREDSCREEN_START ,
    IPTV_PLAYER_EVT_BLURREDSCREEN_END,
    //playback error event
    IPTV_PLAYER_EVT_PLAYBACK_ERROR,
    //video decode error
    IPTV_PLAYER_EVT_VID_FRAME_ERROR = 0x200,
    //video decode drop frame
    IPTV_PLAYER_EVT_VID_DISCARD_FRAME,
    //video decode out of memory
    IPTV_PLAYER_EVT_VID_DEC_UNDERFLOW,
    //video decode pts error
    IPTV_PLAYER_EVT_VID_PTS_ERROR,
    //audio decode error
    IPTV_PLAYER_EVT_AUD_FRAME_ERROR,
    //audio decode drop frame
    IPTV_PLAYER_EVT_AUD_DISCARD_FRAME,
    //audio decode out of memory
    IPTV_PLAYER_EVT_AUD_DEC_UNDERFLOW,
    //audio decode pts error
    IPTV_PLAYER_EVT_AUD_PTS_ERROR,
    IPTV_PLAYER_EVT_BUTT
} IPTV_PLAYER_EVT_e;

typedef enum {
    CTC_PLAYER_EVT_STREAM_VALID = 0,
    CTC_PLAYER_EVT_FIRST_PTS,
    CTC_PLAYER_EVT_VOD_EOS,
    CTC_PLAYER_EVT_UNDERFLOW,
    CTC_PLAYER_EVT_PLAYBACK_ERROR,
    CTC_PLAYER_EVT_PLAYBACK_LOSTFRAME = 51,
    CTC_PLAYER_EVT_AUDIOTRACK_NOSUPPORT = 101,

    CTC_PLAYER_EVENT_STUTTER =0x100,
    //Unload event
    CTC_PLAYER_EVT_UNLOAD_START,
    CTC_PLAYER_EVT_UNLOAD_END,
    //BLURREDSCREEN event
    CTC_PLAYER_EVT_BLURREDSCREEN_START,
    CTC_PLAYER_EVT_BLURREDSCREEN_END
} CTC_PLAYER_EVT_e;

typedef enum {
    // video resolution: 0--640*480, 1--720*576, 2--1280*720, 3--1920*1080, 4--3840*2160, 5--others
    IPTV_PLAYER_ATTR_VID_ASPECT = 0,
    //video ratio: 0--4:3, 1--16:9
    IPTV_PLAYER_ATTR_VID_RATIO,
    //1:progressive 0:interlace
    IPTV_PLAYER_ATTR_VID_SAMPLETYPE,
    //audio video pts diff
    IPTV_PLAYER_ATTR_VIDAUDDIFF,
    //size of video buffer
    IPTV_PLAYER_ATTR_VID_BUF_SIZE,
    //size of used video buffer
    IPTV_PLAYER_ATTR_VID_USED_SIZE,
    //size of audio buffer
    IPTV_PLAYER_ATTR_AUD_BUF_SIZE,
    //size of used audio buffer
    IPTV_PLAYER_ATTR_AUD_USED_SIZE,
    IPTV_PLAYER_ATTR_AUD_BITRATE,
    IPTV_PLAYER_ATTR_VID_BITRATE,
    IPTV_PLAYER_ATTR_AUD_SAMPLERATE,
    IPTV_PLAYER_ATTR_AUD_CHANNEL_NUM = 11,
    //video frame rate
    IPTV_PLAYER_ATTR_VID_FRAMERATE = 18,
    IPTV_PLAYER_ATTR_BUTT
} IPTV_ATTR_TYPE_e;

typedef enum {
    VID_FRAME_TYPE_UNKNOWN = 0,
    VID_FRAME_TYPE_I,
    VID_FRAME_TYPE_P,
    VID_FRAME_TYPE_B,
    VID_FRAME_TYPE_IDR,
    VID_FRAME_TYPE_BUTT,
} VID_FRAME_TYPE_e;

typedef struct {
    VID_FRAME_TYPE_e enVidFrmType;
    int nVidFrmSize;
    int nVidFrmQPMin;
    int nVidFrmQPMax;
    int nVidFrmQPAvg;
    int nMaxMV;
    int nMinMV;
    int nAvgMV;
    int SkipRatio;
    int decoder_buffer;
} VIDEO_FRM_STATUS_INFO_T;

typedef enum {
    IPTV_PLAYER_PARAM_EVT_VIDFRM_STATUS_REPORT = 0,
    IPTV_PLAYER_PARAM_EVT_BUTT
} IPTV_PLAYER_PARAM_Evt_e;

enum {
    IPTV_PLAYER_VIDEO_RESOLUTION_640x480 = 0,
    IPTV_PLAYER_VIDEO_RESOLUTION_720x576,
    IPTV_PLAYER_VIDEO_RESOLUTION_1280x720,
    IPTV_PLAYER_VIDEO_RESOLUTION_1920x1080,
    IPTV_PLAYER_VIDEO_RESOLUTION_3840x2160,
    IPTV_PLAYER_VIDEO_RESOLUTION_OTHERS,
};

enum {
    IPTV_PLAYER_VIDEO_ASPECT_4x3 = 0,
    IPTV_PLAYER_VIDEO_ASPECT_16x9,
};

enum {
    AUDIO_BALANCE_NONE = 0,
    AUDIO_BALANCE_LEFT = 1,
    AUDIO_BALANCE_RIGHT = 2,
    AUDIO_BALANCE_STEREO = 3,
    AUDIO_BALANCE_JOINTSTEREO = 4,
};

typedef enum{
    CTC_PARAMETER_KEY_SET_SPEED = 18,
    CTC_PARAMETER_KEY_GET_SPEED = 19,
    CTC_PARAMETER_KEY_SMALL_WINDOW = 20,
}CTC_PARAMETER_KEY_E;

typedef void (*CTC_PLAYER_EVT_CB)(CTC_PLAYER_EVT_e evt, int32_t ext,void *handler);
typedef void (*IPTV_PLAYER_PARAM_EVENT_CB)(void *handler, IPTV_PLAYER_PARAM_Evt_e enEvt, void *pParam);

/**
 * @brief 初始化工作，调用其它接口前，必须先调用此接口。
 * 
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 * /
int32_t mtk_player_init();

/**
 * @brief 释放接口，确定不再使用时，调用此接口进行释放。
 * 
 */
void mtk_player_deinit();

/**
 * @brief 创建播放器，返回播放器句柄handler
 * 
 * @param pip 是否PIP模式，true为PIP小窗，否则正常模式。
 * @return int32_t handler or MTK_FAIL
 */
uint32_t mtk_player_create(int pip);

/**
 * @brief 释放播放器
 * 
 * @param handler 播放器句柄
 */
void mtk_player_destroy(const uint32_t handler);

/**
 * @brief 初始化video
 * 
 * @param handler 播放器句柄
 * @param pVideoPara 初始化参数
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_video_init(const uint32_t handler, const PVIDEO_PARA_T pVideoPara);

/**
 * @brief 初始化audio
 * 
 * @param handler 播放器句柄
 * @param pAudioPara 初始化参数
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_audio_init(const uint32_t handler, const PAUDIO_PARA_T pAudioPara);

/**
 * @brief 设置显示位置
 * 
 * @param handler 播放器句柄
 * @param x 
 * @param y 
 * @param width 
 * @param height 
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_set_video_window(const uint32_t handler, int32_t x,int32_t y,int32_t width,int32_t height);

/**
 * @brief 设置 surface
 * 
 * @param handler 播放器句柄
 * @param pSurface surface
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_set_surface(const uint32_t handler, JNIEnv* env, jobject jSurface);

/**
 * @brief 显示视频窗口
 * 
 * @param handler 播放器句柄
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_video_show(const uint32_t handler);

/**
 * @brief 隐藏视频窗口
 * 
 * @param handler 播放器句柄
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_video_hide(const uint32_t handler);

/**
 * @brief 开始播放
 * 
 * @param handler 播放器句柄
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_start(const uint32_t handler);

/**
 * @brief pause
 * 
 * @param handler 播放器句柄
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_pause(const uint32_t handler);

/**
 * @brief resume
 * 
 * @param handler 播放器句柄
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_resume(const uint32_t handler);

/**
 * @brief reset
 * 
 * @param handler 播放器句柄
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_reset(const uint32_t handler);

/**
 * @brief stop
 * 
 * @param handler 播放器句柄
 * @param freeze 是否保留最后一帧画面
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_stop(const uint32_t handler, int freeze);
/**
 * @brief 倍速设置
 * 
 * @param handler 播放器句柄
 * @param speed 速率
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_set_speed(const uint32_t handler, float speed);

/**
 * @brief 获取当前倍速
 * 
 * @param handler 播放器句柄
 * @return float 倍速值
 */
float mtk_player_get_speed(const uint32_t handler);

/**
 * @brief 设置音量
 * 
 * @param handler 播放器句柄
 * @param volume 音量值，0-100
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_set_volume(const uint32_t handler, int32_t volume);

/**
 * @brief 获取当前音量
 * 
 * @param handler 播放器句柄
 * @return int32_t 当前音量 or MTK_FAIL
 */
int32_t mtk_player_get_volume(const uint32_t handler);

/**
 * @brief 设置显示比例
 * 
 * @param handler 播放器句柄
 * @param nRatio IPTV_PLAYER_VIDEO_ASPECT_4x3 or IPTV_PLAYER_VIDEO_ASPECT_16x9
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_set_ratio(const uint32_t handler, int32_t nRatio);

/**
 * @brief 设置音频通道
 * 
 * @param handler 播放器句柄
 * @param nAudioBalance
 * enum {
 *     AUDIO_BALANCE_NONE = 0,
 *     AUDIO_BALANCE_LEFT = 1,
 *     AUDIO_BALANCE_RIGHT = 2,
 *     AUDIO_BALANCE_STEREO = 3,
 *     AUDIO_BALANCE_JOINTSTEREO = 4,
 * };
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_set_audio_balance(const uint32_t handler, int32_t nAudioBalance);

/**
 * @brief 获取当前使用音频通道
 * 
 * @param handler 播放器句柄
 * @return int32_t 
 * enum {
 *     AUDIO_BALANCE_NONE = 0,
 *     AUDIO_BALANCE_LEFT = 1,
 *     AUDIO_BALANCE_RIGHT = 2,
 *     AUDIO_BALANCE_STEREO = 3,
 *     AUDIO_BALANCE_JOINTSTEREO = 4,
 * };
 */
int32_t mtk_player_get_audio_balance(const uint32_t handler);

/**
 * @brief 设置属性
 * 
 * @param handler 播放器句柄
 * @param nType 类型
 * @param nSub sub type
 * @param nValue 值
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_set_property(const uint32_t handler, int nType, int nSub, int nValue);

/**
 * @brief 设置参数
 * 
 * @param handler 播放器句柄
 * @param key key
 * @param request value
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_set_parameter(const uint32_t handler, int32_t key, void* request);

/**
 * @brief 获取当前状态
 * 
 * @param handler 播放器句柄
 * @param eAttrType IPTV_ATTR_TYPE_e
 * @param value 当前值
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_get_status_info(const uint32_t handler, IPTV_ATTR_TYPE_e eAttrType, int *value);

/**
 * @brief 获取当前时间
 * 
 * @param handler 播放器句柄
 * @return int64_t 当前时间
 */
int64_t mtk_player_get_current_playtime(const uint32_t handler);

/**
 * @brief 写数据
 * 
 * @param handler 播放器句柄
 * @param type 见STREAMTYPE_E定义
 * @param pBuffer 数据缓存地址
 * @param size 数据字节数
 * @param timestamp pts
 * @return int32_t MTK_SUCCESS or MTK_FAIL
 */
int32_t mtk_player_write_data(const uint32_t handler, STREAMTYPE_E type, const uint8_t *pBuffer, uint32_t size, uint64_t timestamp);

/**
 * @brief 设置事件监听接口
 * 
 * @param handler 播放器句柄
 * @param pfunc 监听接口
 * @param evt_handler 主体标识
 */
void mtk_player_register_evt_cb(const uint32_t handler, CTC_PLAYER_EVT_CB pfunc, void *evt_handler);
#ifdef __cplusplus
}
#endif
#endif