
#include "ffpipenode_android_mtkcodec_vdec.h"
#include "ijksdl/android/ijksdl_android_jni.h"
#include "ijksdl/android/ijksdl_vout_android_nativewindow.h"
#include "ijkplayer/ff_ffpipenode.h"
#include "ijkplayer/ff_ffplay.h"
#include "ijkplayer/ff_ffplay_debug.h"
#include "ijksdl/ijksdl_aout_internal.h"
#include "ffpipeline_android.h"
#include "h264_nal.h"
#include "hevc_nal.h"
#include "mtkcodec/mtk_codec.h"
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <jni.h>

#define MAX_LOG_LEN           (512)
#define HDR_BUF_SIZE          1024
#define AUDIO_BUFF_SIZE         (1024 * 256)
#define AUDIO_CALLBACK_BUFF_SIZE (1024 * 1024)

#define VIDEO_BUFF_SIZE       (1024 * 512)


// missing tags
#define CODEC_TAG_VC_1            (0x312D4356)
#define CODEC_TAG_RV30            (0x30335652)
#define CODEC_TAG_RV40            (0x30345652)
#define CODEC_TAG_hvc1            (0x31637668)
#define CODEC_TAG_hev1            (0x31766568)
#define CODEC_TAG_DIVX            (0x58564944)

// convert to ms.
#define AM_TIME_BASE              1000
#define AM_TIME_BASE_Q            (AVRational){1, AM_TIME_BASE}

// amcodec only tracks time using a 32 bit mask
#define AM_TIME_BITS              (32ULL)
#define AM_TIME_MASK              (0xffffffffULL)

#define PLAYER_CHK_PRINTF(val, ret, printstr) /*lint -save -e506 -e774*/ \
    do \
{ \
    if ((val)) \
    { \
        ALOGE("### [TsPlayer]: [%s:%d], %s, ret %d \n", __FILE__, __LINE__, printstr, (int32_t)ret); \
    } \
} while (0) /*lint -restore */

static void IPTV_ADAPTER_PLAYER_LOG(char *pFuncName, uint32_t u32LineNum, const char *format, ...);

#define PLAYER_LOGE(fmt...) \
do \
{\
    IPTV_ADAPTER_PLAYER_LOG((char *)__FUNCTION__, (uint32_t)__LINE__, fmt);\
}while(0)

#define PLAYER_LOGI(fmt...) \
do \
{\
    IPTV_ADAPTER_PLAYER_LOG((char *)__FUNCTION__, (uint32_t)__LINE__, fmt);\
}while(0)

#define MAX(a, b) \
((a)>(b)?(a):(b))

typedef struct hdr_buf {
    char *data;
    int size;
} hdr_buf_t;

typedef struct audio_buf{
    uint8_t     *buffer;
    int32_t     buffer_size;
    int32_t     write_size;
    int         valid;
    SDL_mutex   *mutex;
}audio_buf;

typedef struct video_buf{
    uint8_t     *buffer;
    int32_t     buffer_size;
    int32_t     write_pos;
}video_buf;

typedef struct mtk_packet {
    AVPacket      avpkt;
    int64_t       avpts;
    int64_t       avdts;
    int           avduration;
    int           isvalid;
    int           newflag;
    int64_t       lastpts;
    unsigned char *data;
    unsigned char *buf;
    int           data_size;
    int           buf_size;
    hdr_buf_t     *hdr;
} mtk_packet_t;

typedef struct SHIJIU_MTK_Pipenode_Opaque {
    FFPlayer            *ffp;
    Decoder             *decoder;
    vformat_t           video_format;
    vdec_type_t         mtk_video_codec;

    unsigned int        video_codec_id;
    unsigned int        video_codec_tag;
    unsigned int        video_codec_type;
    int                 video_width;
    int                 video_height;
    size_t              nal_size;
    int                 extrasize;
    uint8_t             *extradata;
    uint32_t            player_handler;
    mtk_packet_t        mtk_pkt;
    float               fps;
    int64_t             decode_dts;
    int64_t             decode_pts;
    int                 discard_b_frame;

    int64_t             last_checkstatus_time;
    video_buf           video_buffer;

    int64_t             video_write_pts;
    int64_t             audio_write_pts;

    int64_t             first_audio_pts;//seek完以后第一个视频帧的pts,因为GetCurrentPlayTime()是video和video里大的那个的数据（MTK的答复），这里取audio的，first_audio_pts+GetCurrentPlayTime()为实际的current time。
    int64_t             first_video_pts;


    //音频相关
    SDL_AudioSpec spec;
    
    audio_buf audio_buffer;
    volatile bool need_flush;
    bool pause_on;
    bool pause_changed;

    volatile bool need_set_volume;
    volatile float left_volume;
    volatile float right_volume;

    int audio_session_id;

    volatile float speed;
    volatile bool speed_changed;
    int audio_inited;
    SDL_cond *wakeup_cond;
    SDL_mutex *wakeup_mutex;
} SHIJIU_MTK_Pipenode_Opaque;

typedef struct SHiJIU_MTK_Aout_Opaque {
    SHIJIU_MTK_Pipenode_Opaque *mtk_opaque;

    SDL_Thread *audio_tid;
    SDL_Thread _audio_tid;

    volatile bool abort_request;
    int finished;
}SHiJIU_MTK_Aout_Opaque;

//static int32_t (*mtk_codec_init)();
//static uint32_t (*mtk_codec_create)(int pip);
//static void (*mtk_codec_player_destroy)(const uint32_t handler);
static int32_t (*mtk_codec_video_init)(const uint32_t handler, const PVIDEO_PARA_T pVideoPara);
static int32_t (*mtk_codec_audio_init)(const uint32_t handler, const PAUDIO_PARA_T pAudioPara);
static int32_t (*mtk_codec_write_data)(const uint32_t handler, STREAMTYPE_E type, const uint8_t *pBuffer, uint32_t size, uint64_t timestamp);
static int32_t (*mtk_codec_start)(const uint32_t handler);
static int32_t (*mtk_codec_stop)(const uint32_t handler, int freeze);
static int32_t (*mtk_codec_pause)(const uint32_t handler);
static int32_t (*mtk_codec_resume)(const uint32_t handler);
static int64_t (*mtk_codec_get_current_playtime)(const uint32_t handler);
static void (*mtk_codec_register_evt_cb)(const uint32_t handler, CTC_PLAYER_EVT_CB pfunc, void *evt_handler);
static int32_t (*mtk_codec_set_speed)(const uint32_t handler, float speed);
static int32_t (*mtk_codec_get_status_info)(const uint32_t handler, IPTV_ATTR_TYPE_e eAttrType, int *value);
static int32_t (*mtk_codec_reset)(const uint32_t handler);
static int32_t (*mtk_codec_set_volume)(const uint32_t handler, int32_t volume);
static int32_t (*mtk_codec_set_audio_balance)(const uint32_t handler, int32_t nAudioBalance);
static int32_t (*mtk_codec_set_video_window)(const uint32_t handler, int32_t x,int32_t y,int32_t width,int32_t height);
static int32_t (*mtk_codec_set_surface)(const uint32_t handler, JNIEnv* env, jobject jSurface);

// forward decls
static void func_destroy(IJKFF_Pipenode *node);
static int func_flush(IJKFF_Pipenode *node);
static int func_run_sync(IJKFF_Pipenode *node);
// static void syncToMaster(SHIJIU_MTK_Pipenode_Opaque *opaque);
static vformat_t codecid_to_vformat(enum AVCodecID id);
static vdec_type_t codecid_to_mtkcodec(enum AVCodecID id);
static vdec_type_t codec_tag_to_vdec_type(uint32_t codec_tag);
static int feed_video_buffer(JNIEnv *env, IJKFF_Pipenode *node, int *enqueue_count);
static int feed_audio_buffer(SHIJIU_MTK_Pipenode_Opaque *opaque);
static int reset(SHIJIU_MTK_Pipenode_Opaque *opaque);
static void setVolume(SHIJIU_MTK_Pipenode_Opaque *opaque, int32_t left_volume, int32_t right_volume);
static int pre_header_feeding(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt);
static int mtk_video_init(SHIJIU_MTK_Pipenode_Opaque *opaque);
static int mtk_audio_init(SHIJIU_MTK_Pipenode_Opaque *opaque, SDL_AudioSpec *param);

//视频协议相关header添加，和芯片平台无关，只和编解码协议有关。
static int write_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt);
static int mtk_write_video_data(SHIJIU_MTK_Pipenode_Opaque *opaque, uint8_t * buffer, uint64_t pts, int32_t size);
static int write_av_packet(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt);
static int h264_write_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt);
static int set_header_info(SHIJIU_MTK_Pipenode_Opaque *para);


//音视频同步相关
// static int64_t getAudioPts(FFPlayer *ffp, VideoState  *is);
// static int64_t mtk_time_sub(int64_t a, int64_t b);
// static int64_t mtk_time_sub_32(int64_t a, int64_t b);
static int64_t get_current_time(SHIJIU_MTK_Pipenode_Opaque *opaque);

static int mtk_not_present = 0;
static int mtk_so_loaded = 0;
// static int debug_mode = 0;
static int mtk_discard_b_count = 0;

#ifdef HI_HDCP_SUPPORT
const HI_CHAR * pstencryptedHdcpKey = "EncryptedKey_332bytes.bin";
#endif
// static android::OsdManager *g_om = HI_NULL;

static void IPTV_ADAPTER_PLAYER_LOG(char *pFuncName, uint32_t u32LineNum, const char *format, ...)
{
    char        LogStr[MAX_LOG_LEN] = {0};
    va_list     args;

    va_start(args, format);
    vsnprintf(LogStr, MAX_LOG_LEN, format, args);
    va_end(args);
    LogStr[MAX_LOG_LEN-1] = '\0';

    ALOGE("%s[%d]:%s", pFuncName, u32LineNum, LogStr);

    return;
}

static void set_clock_at(Clock *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

static void mtk_packet_init(SHIJIU_MTK_Pipenode_Opaque *opaque)
{
    mtk_packet_t *pkt = &(opaque->mtk_pkt);

    memset(&pkt->avpkt, 0, sizeof(AVPacket));
    pkt->avpts      = 0;
    pkt->avdts      = 0;
    pkt->avduration = 0;
    pkt->isvalid    = 0;
    pkt->newflag    = 0;
    pkt->lastpts    = 0;
    pkt->data       = NULL;
    pkt->buf        = NULL;
    pkt->data_size  = 0;
    pkt->buf_size   = 0;
    pkt->hdr        = NULL;
}

void mtk_packet_release(mtk_packet_t *pkt)
{
  if (pkt->buf != NULL)
    free(pkt->buf), pkt->buf= NULL;
  if (pkt->hdr != NULL)
  {
    if (pkt->hdr->data != NULL)
      free(pkt->hdr->data), pkt->hdr->data = NULL;
    free(pkt->hdr), pkt->hdr = NULL;
  }
}

static void * find_symbol(void *libHandler, const char * sym)
{
  void * addr = dlsym(libHandler, sym);
  if(!addr){
    ALOGE("find_symbol error: %s\n", sym);
    mtk_not_present = 1;
  }
  return addr;
}


static void loadLibrary()
{
    // We use Java to call System.loadLibrary as dlopen is weird on Android.
    if(mtk_so_loaded){
        return;
    }

    void * lib = dlopen("libMtkBridge.so", RTLD_NOW);
    if(!lib) {
      ALOGE("mtk bridge library did not load: %s\n", dlerror());
      mtk_not_present = 1;
      return;
    }

//    mtk_codec_init                   = find_symbol(lib, "mtk_player_init");
//    mtk_codec_create                 = find_symbol(lib, "mtk_player_create");
//    mtk_codec_player_destroy         = find_symbol(lib, "mtk_player_destroy");
    mtk_codec_video_init             = find_symbol(lib, "mtk_player_video_init");
    mtk_codec_audio_init             = find_symbol(lib, "mtk_player_audio_init");
    mtk_codec_write_data             = find_symbol(lib, "mtk_player_write_data");
    mtk_codec_start                  = find_symbol(lib, "mtk_player_start");
    mtk_codec_stop                   = find_symbol(lib, "mtk_player_stop");
    mtk_codec_pause                  = find_symbol(lib, "mtk_player_pause");
    mtk_codec_resume                 = find_symbol(lib, "mtk_player_resume");
    mtk_codec_get_current_playtime   = find_symbol(lib, "mtk_player_get_current_playtime");
    mtk_codec_register_evt_cb        = find_symbol(lib, "mtk_player_register_evt_cb");
    mtk_codec_set_speed              = find_symbol(lib, "mtk_player_set_speed");
    mtk_codec_get_status_info        = find_symbol(lib, "mtk_player_get_status_info");
    mtk_codec_reset                  = find_symbol(lib, "mtk_player_reset");
    mtk_codec_set_volume             = find_symbol(lib, "mtk_player_set_volume");
    mtk_codec_set_audio_balance      = find_symbol(lib, "mtk_player_set_audio_balance");
    mtk_codec_set_video_window       = find_symbol(lib, "mtk_player_set_video_window");
    mtk_codec_set_surface            = find_symbol(lib, "mtk_player_set_surface");
    

    if(mtk_not_present){
        return;
    }

//    if(mtk_codec_init() == MTK_FAIL){
//        ALOGD("mtkcodec init failed.\n");
//        mtk_not_present = 1;
//        return;
//    }
    
    mtk_so_loaded = 1;
}

IJKFF_Pipenode *ffpipenode_create_video_decoder_from_android_mtkcodec(FFPlayer *ffp)
{
    if (!ffp || !ffp->is)
        return NULL;

    ALOGI("mtkcodec initializing.\n");

    loadLibrary();

    if(mtk_not_present) {
        return NULL;
    }

    ALOGI("mtk library loads successfully\n");

    IJKFF_Pipenode *node = ffpipenode_alloc(sizeof(SHIJIU_MTK_Pipenode_Opaque));
    if (!node)
        return NULL;

    VideoState            *is     = ffp->is;

    SHIJIU_MTK_Pipenode_Opaque *opaque = node->opaque;
    SHiJIU_MTK_Aout_Opaque *audio_opaque = NULL;
    node->func_destroy  = func_destroy;
    node->func_run_sync = func_run_sync;
    node->func_flush    = func_flush;

    if(ffp && ffp->aout){
        audio_opaque = (SHiJIU_MTK_Aout_Opaque *)ffp->aout->opaque;
        audio_opaque->mtk_opaque = opaque;
    }

    //opaque->pipeline    = pipeline;
    opaque->ffp         = ffp;
    opaque->decoder     = &is->viddec;

    opaque->discard_b_frame = false;

    AVCodecContext * avctx = opaque->decoder->avctx;
    opaque->video_codec_id = avctx->codec_id; //AV_CODEC_ID_H264;
    opaque->video_codec_tag = avctx->codec_tag; //CODEC_TAG_AVC1;

    opaque->video_format = codecid_to_vformat(avctx->codec_id); //VFORMAT_H264
    opaque->mtk_video_codec = codecid_to_mtkcodec(avctx->codec_id); 
    if(opaque->video_format < 0)
        return NULL;

    opaque->video_codec_type = codec_tag_to_vdec_type(avctx->codec_tag);  //VIDEO_DEC_FORMAT_H264;

    opaque->extrasize = avctx->extradata_size;
    opaque->extradata = (uint8_t*)malloc(opaque->extrasize);
    memcpy(opaque->extradata, avctx->extradata, opaque->extrasize);

    opaque->video_width = avctx->width;
    opaque->video_height = avctx->height;

    opaque->decode_dts = 0;
    opaque->decode_pts = 0;
    if(opaque->ffp->is->video_st->avg_frame_rate.den == 0 || opaque->ffp->is->video_st->avg_frame_rate.num == 0){
        opaque->fps = 25.0; //如果没有获取到fps，用默认的25fps。
    }else{
        opaque->fps = av_q2d(opaque->ffp->is->video_st->avg_frame_rate);
    }

    opaque->player_handler = ffp->window_handle;
    opaque->pause_on = 1;
    opaque->pause_changed = false;
    opaque->need_set_volume = 0;
    opaque->left_volume = 1.0f;
    opaque->right_volume = 1.0f;
    opaque->speed = 1.0f;
    opaque->speed_changed = 0;
    opaque->video_write_pts = 0;
    opaque->audio_write_pts = 0;
    opaque->first_video_pts = -1;
    opaque->first_audio_pts = -1;

    opaque->wakeup_cond  = SDL_CreateCond();
    opaque->wakeup_mutex = SDL_CreateMutex();

    // avc1 is the codec tag when h264 is embedded in an mp4 and needs the stupid
    // nal_size and extradata stuff processed.
    if(avctx->codec_tag == CODEC_TAG_AVC1 || avctx->codec_tag == CODEC_TAG_avc1 ||
       avctx->codec_tag == CODEC_TAG_hvc1 || avctx->codec_tag == CODEC_TAG_hev1) {

        ALOGE("stream is avc1/hvc1, fixing sps/pps. extrasize:%d\n", avctx->extradata_size);

        size_t   sps_pps_size   = 0;
        size_t   convert_size   = avctx->extradata_size + 200;
        uint8_t *convert_buffer = (uint8_t *)calloc(1, convert_size);
        if (!convert_buffer) {
            ALOGE("%s:sps_pps_buffer: alloc failed\n", __func__);
            return NULL;
        }

        if(avctx->codec_tag == CODEC_TAG_AVC1 || avctx->codec_tag == CODEC_TAG_avc1) {
            if (0 != convert_sps_pps(avctx->extradata, avctx->extradata_size,
                                     convert_buffer, convert_size,
                                     &sps_pps_size, &opaque->nal_size)) {
                ALOGE("%s:convert_sps_pps: failed\n", __func__);
                return NULL;
            }
        } else {
            if (0 != convert_hevc_nal_units(avctx->extradata, avctx->extradata_size,
                                     convert_buffer, convert_size,
                                     &sps_pps_size, &opaque->nal_size)) {
                ALOGE("%s:convert_hevc_nal_units: failed\n", __func__);
                return NULL;
            }
        }
        free(opaque->extradata);
        opaque->extrasize = sps_pps_size;
        opaque->extradata = convert_buffer;
    }

    opaque->video_buffer.buffer_size = VIDEO_BUFF_SIZE;
    opaque->video_buffer.buffer = mallocz(VIDEO_BUFF_SIZE);
    if(opaque->video_buffer.buffer == NULL){
        return NULL;
    }
    opaque->video_buffer.write_pos = 0;

    mtk_video_init(opaque);

    mtk_packet_init(opaque);
    // pre_header_feeding(opaque, &opaque->mtk_pkt);

    ffp->stat.vdec_type = FFP_PROPV_DECODER_MEDIATEK;

    return node;
}

static void func_destroy(IJKFF_Pipenode *node)
{
    if (!node || !node->opaque)
        return;

    SHIJIU_MTK_Pipenode_Opaque *opaque   = node->opaque;

    ALOGI("mtk func_destroy in\n");

    if (opaque->player_handler != MTK_HANDLER_INVALID) {
        mtk_codec_stop(opaque->player_handler, 1);
    }

    mtk_packet_release(&opaque->mtk_pkt);

    if(opaque->audio_buffer.buffer){
        free(opaque->audio_buffer.buffer);
        opaque->audio_buffer.buffer = NULL;
        opaque->audio_buffer.buffer_size = 0;
        opaque->audio_buffer.valid = 0;
        SDL_DestroyMutex(opaque->audio_buffer.mutex);
    }

    if(opaque->wakeup_cond)
        SDL_DestroyCond(opaque->wakeup_cond);
    if(opaque->wakeup_mutex)
        SDL_DestroyMutex(opaque->wakeup_mutex);

    if(opaque->video_buffer.buffer){
        free(opaque->video_buffer.buffer);
        opaque->video_buffer.buffer_size = 0;
    }

    if (opaque->player_handler != MTK_HANDLER_INVALID) {
        mtk_codec_register_evt_cb(opaque->player_handler, NULL, NULL);
//        mtk_codec_player_destroy(opaque->player_handler);
    }

    ALOGI("mtk func_destroy exit\n");
}

static int func_flush(IJKFF_Pipenode *node)
{
    return 0;
}

static int32_t set_framerate(SHIJIU_MTK_Pipenode_Opaque *opaque){
    if(opaque->player_handler != MTK_HANDLER_INVALID){
        mtk_codec_set_speed(opaque->player_handler, opaque->speed);
    }

    return MTK_SUCCESS;
}

// static void req_reset(SHIJIU_MTK_Pipenode_Opaque *opaque) {
//     FFPlayer              *ffp      = opaque->ffp;
//     VideoState            *is       = ffp->is;

//     ALOGD("mtk codec req_reset!");

//     is->reset_req = 1;
// }

static int reset(SHIJIU_MTK_Pipenode_Opaque *opaque) 
{
    FFPlayer                    *ffp      = opaque->ffp;
    VideoState                  *is       = ffp->is;
    Decoder                     *d        = &is->viddec;
    int32_t                     s32Ret    = MTK_SUCCESS;

    ALOGI("mtkcodec reset!\n");

    s32Ret = mtk_codec_reset(opaque->player_handler);
    PLAYER_CHK_PRINTF((MTK_SUCCESS != s32Ret), s32Ret, "Call mtk_codec_reset failed");

    s32Ret = mtk_codec_resume(opaque->player_handler);
    PLAYER_CHK_PRINTF((MTK_SUCCESS != s32Ret), s32Ret, "Call mtk_codec_resume failed");

    mtk_packet_release(&opaque->mtk_pkt);
    mtk_packet_init(opaque);
    // pre_header_feeding(opaque, &opaque->mtk_pkt);

    opaque->decode_dts = 0;
    opaque->decode_pts = 0;
    opaque->last_checkstatus_time = 0;
    opaque->audio_write_pts = 0;
    opaque->video_write_pts = 0;
    opaque->first_video_pts = -1;
    opaque->first_audio_pts = -1;
    d->packet_pending = 0;

    if(opaque->audio_buffer.buffer){
        memset(opaque->audio_buffer.buffer, 0, opaque->audio_buffer.buffer_size);
        opaque->audio_buffer.valid = 0;
        opaque->audio_buffer.write_size = 0;
    }

    if(opaque->video_buffer.buffer){
        memset(opaque->video_buffer.buffer, 0, opaque->video_buffer.buffer_size);
        opaque->video_buffer.write_pos = 0;
    }

    ALOGI("mtkcodec reset exit\n");
    return s32Ret;
}

static void setVolume(SHIJIU_MTK_Pipenode_Opaque *opaque, int32_t left_volume, int32_t right_volume)
{
    int32_t s32Ret = 0;
    PLAYER_LOGI("### CTsPlayer::SetVolume(%d) \n",left_volume);

    if(left_volume != right_volume){
        if(left_volume == 0){
            mtk_codec_set_audio_balance(opaque->player_handler, AUDIO_BALANCE_RIGHT);
        }else if(right_volume == 0){
            mtk_codec_set_audio_balance(opaque->player_handler, AUDIO_BALANCE_LEFT);
        }else{
            mtk_codec_set_audio_balance(opaque->player_handler, AUDIO_BALANCE_STEREO);
        }
    }else{
        mtk_codec_set_audio_balance(opaque->player_handler, AUDIO_BALANCE_STEREO);
    }

    s32Ret = mtk_codec_set_volume(opaque->player_handler, left_volume==0?right_volume:left_volume);
    PLAYER_CHK_PRINTF((MTK_SUCCESS != s32Ret), s32Ret, "Call mtk_codec_set_volume failed");

    PLAYER_LOGI("### CTsPlayer::SetVolume(%d) quit\n",left_volume);

    return;
}

static int feed_video_buffer(JNIEnv *env, IJKFF_Pipenode *node, int *enqueue_count)
{
    SHIJIU_MTK_Pipenode_Opaque *opaque   = node->opaque;
    FFPlayer                    *ffp      = opaque->ffp;
    VideoState                  *is       = ffp->is;
    Decoder                     *d        = &is->viddec;
    AVCodecContext              *avctx    = d->avctx;
    mtk_packet_t                *mtk_pkt  = &(opaque->mtk_pkt);
    int32_t                     ret       = MTK_SUCCESS;

    //不知道为啥要在这个地方加个write header，先拿掉试试。
    // write_header(opaque, mtk_pkt);

    if (enqueue_count)
        *enqueue_count = 0;

    if (d->queue->abort_request) {
        ret = MTK_SUCCESS;
        goto fail;
    }

    // if (is->paused) {
    //     if(!opaque->paused) {
    //         ALOGD("mtkcodec pausing!\n");
    //         mtk_avplay_pause(hAvplay, NULL);
    //         opaque->paused = true;
    //     }

    //     usleep(1000000 / 30);   // Hmmm, is there a condwait for resuming?
    //     goto fail;
    // } else {
    //     if(opaque->paused) {
    //         ALOGE("mtkcodec resuming!\n");
    //         mtk_avplay_resume(hAvplay, NULL);
    //         opaque->paused = false;
    //     }
    // }


    if (!d->packet_pending || d->queue->serial != d->pkt_serial) {
        AVPacket pkt;
        do {
            if (d->queue->abort_request) {
                ret = MTK_SUCCESS;
                goto fail;
            }

            if (d->queue->nb_packets == 0){
                SDL_CondSignal(d->empty_queue_cond);
                ret = MTK_FAIL;
                goto fail;
            }
            
            if (ffp_packet_queue_get_or_buffering(ffp, d->queue, &pkt, &d->pkt_serial, &d->finished) < 0) {
                ret = MTK_FAIL;
                goto fail;
            }
            if (ffp_is_flush_packet(&pkt)) {
                ALOGD("mtkcodec flush packet\n");

                // reset(opaque);

                // request flush before lock, or never get mutex
                d->finished = 0;
                d->next_pts = d->start_pts;
                d->next_pts_tb = d->start_pts_tb;
            }
        } while (ffp_is_flush_packet(&pkt) || d->queue->serial != d->pkt_serial);
        av_packet_unref(&d->pkt);
        d->pkt_temp = d->pkt = pkt;
        d->packet_pending = 1;

        if (d->pkt_temp.data) {
            *enqueue_count += 1;
            H264ConvertState convert_state = {0, 0};

            if (opaque->nal_size > 0 && (avctx->codec_id == AV_CODEC_ID_H264 || avctx->codec_id == AV_CODEC_ID_HEVC)) {
                convert_h264_to_annexb(d->pkt_temp.data, d->pkt_temp.size, opaque->nal_size, &convert_state);
            }
        }
    }

    if (d->pkt_temp.data) {
        *enqueue_count += 1;

        int64_t dts = d->pkt_temp.dts;
        int64_t pts = d->pkt_temp.pts;

        // we need to force pts to be in the same 'phase' as dts
        //if(pts != AV_NOPTS_VALUE)
        //  pts = (((int)pts-(int)dts)&0x7fffffff) + dts;

        if (dts != AV_NOPTS_VALUE)
            dts = av_rescale_q(dts, is->video_st->time_base, AM_TIME_BASE_Q);

        if (pts != AV_NOPTS_VALUE)
            pts = av_rescale_q(pts, is->video_st->time_base, AM_TIME_BASE_Q);

        if(dts > pts)
            dts = pts;

        if(dts < opaque->decode_dts)
            dts = opaque->decode_dts + 1;

        if (pts == AV_NOPTS_VALUE)
          pts = dts;

        opaque->decode_dts = dts;
        opaque->decode_pts = pts;

#if TIMING_DEBUG
        ALOGD("queued dts:%llu pts:%llu\n", dts, pts);
#endif

        //if((pts - last_pts) > 300

        //if(pts_jump) {
        //    if (pts != AV_NOPTS_VALUE) {
        //        double time = av_gettime_relative() / 1000000.0;
        //        set_clock_at(&is->vidclk, pts / 90000.0, d->pkt_serial, time);
        //    }
        //}

        if(!mtk_pkt->isvalid){
            mtk_pkt->data       = d->pkt_temp.data;
            mtk_pkt->data_size  = d->pkt_temp.size;
            mtk_pkt->newflag    = 1;
            mtk_pkt->isvalid    = 1;
            mtk_pkt->avduration = 0;
            mtk_pkt->avdts      = dts;
            mtk_pkt->avpts      = pts;
        }

        // some formats need header/data tweaks.
        // the actual write occurs once in write_av_packet
        // and is controlled by am_pkt.newflag.
        set_header_info(opaque);

        // loop until we write all into codec, am_pkt.isvalid
        // will get set to zero once everything is consumed.
        // PLAYER_SUCCESS means all is ok, not all bytes were written.
        // int loop = 0;
        if(mtk_pkt->isvalid) {
            // abort on any errors.
            // ALOGD("write_av_packet , pkt size =%d\n", mtk_pkt->data_size);
            if(opaque->discard_b_frame && (d->pkt_temp.flags & AV_PKT_FLAG_DISPOSABLE) != 0 && mtk_discard_b_count == 0){
                ALOGD("Discard B frame\n");
                mtk_pkt->isvalid = 0;
                mtk_pkt->data_size = 0;
                // mtk_discard_b_count = 1;
                ret = MTK_SUCCESS;
            }else{
                // mtk_discard_b_count = 0;
                if ((ret = write_av_packet(opaque, mtk_pkt)) != MTK_SUCCESS){
                    // ALOGD("write_av_packet fail, ret =%d\n", ret);
                }
            }
        }

        if(ret == MTK_SUCCESS){
            if (!is->viddec.first_frame_decoded) {
                ALOGD("Video: first frame decoded\n");
                ffp_notify_msg1(ffp, FFP_MSG_VIDEO_DECODED_START);
                is->viddec.first_frame_decoded_time = SDL_GetTickHR();
                is->viddec.first_frame_decoded = 1;

                double time = av_gettime_relative() / 1000000.0;
                set_clock_at(&is->vidclk, pts / 90000.0, d->pkt_serial, time);
            }

            if (!ffp->first_video_frame_rendered && (d->pkt_temp.flags&0x1)) {
                int64_t current_t = av_gettime();
                ffp->first_video_frame_rendered = 1;
//                if(!is->ic){
//                    ALOGE("no ic\n");
//                }else if(!is->ic->pb){
//                    ALOGD("avformat bytes read: %lld\n", is->ic->bytes_read);
//                    ffp->stat.bit_rate = is->ic->bytes_read*8*1000000/(current_t-is->start_download_time);
//                }else{
//                    ALOGD("bytes read: %lld\n", is->ic->pb->bytes_read);
//                    ffp->stat.bit_rate = is->ic->pb->bytes_read*8*1000000/(current_t-is->start_download_time);
//                }
                ffp_notify_msg1(ffp, FFP_MSG_VIDEO_RENDERING_START);
                // ALOGE("bitrate: %jd\n", ffp->stat.bit_rate);
            }

            d->pkt_temp.dts = AV_NOPTS_VALUE;
            d->pkt_temp.pts = AV_NOPTS_VALUE;

            d->packet_pending = 0;
        }/*else{
            ret = HI_SUCCESS;
        }*/
    }else{
       
        ALOGD("finished: %d, current_time = %lld, duration = %lld\n", d->finished, get_current_time(opaque), fftime_to_milliseconds(is->ic->duration));
        
        if(get_current_time(opaque) >= MAX(opaque->audio_write_pts, opaque->video_write_pts)){
            d->finished = d->pkt_serial;
        }else{
            ret = MTK_NODATA;
        }
    }

 fail:
    return ret;
}

static int feed_audio_buffer(SHIJIU_MTK_Pipenode_Opaque *opaque)
{
    int32_t ret = MTK_SUCCESS; 
    FFPlayer *ffp = opaque->ffp;
    VideoState *is = ffp->is;
    AVRational tb = (AVRational){1, opaque->spec.freq};
//    uint64_t pts = (uint64_t)av_rescale_q(is->reading_audio_pts, tb, AM_TIME_BASE_Q);
    uint64_t pts = 0;

    ALOGI("feed_audio_buffer: size=%d, pts=%lld, audio-video=%lld\n", opaque->audio_buffer.write_size, pts, opaque->audio_write_pts-opaque->video_write_pts);

    if(opaque->video_write_pts != 0 && opaque->audio_write_pts != 0 && opaque->audio_write_pts - opaque->video_write_pts > 1000){
        return MTK_FAIL;
    }

    ret = mtk_codec_write_data(opaque->player_handler, STREAMTYPE_AUDIO, opaque->audio_buffer.buffer, opaque->audio_buffer.write_size, pts);
    ALOGI("feed_audio_buffer exit: ret = %d\n", ret);
    if(ret > 0){
        opaque->audio_write_pts = pts;
        if(opaque->first_audio_pts == -1){
            opaque->first_audio_pts = pts;
        }
        return MTK_SUCCESS;
    }else{
        return MTK_FAIL;
    }
    // return ret;
}

static void modifyPlayBackRate(SHIJIU_MTK_Pipenode_Opaque *opaque){
    if(opaque->speed_changed){
        set_framerate(opaque);
        opaque->speed_changed = 0;

        opaque->last_checkstatus_time = 0;
    }
}

static void getFps(SHIJIU_MTK_Pipenode_Opaque *opaque){
    FFPlayer              *ffp      = opaque->ffp;
    int32_t               current_fps = 0;

    ALOGI("getFps\n");
    if(opaque->last_checkstatus_time == 0){
        opaque->last_checkstatus_time = av_gettime();
    }else{
        opaque->last_checkstatus_time = av_gettime();
        if(mtk_codec_get_status_info(opaque->player_handler,IPTV_PLAYER_ATTR_VID_FRAMERATE, &current_fps) >= 0){
            ffp->stat.vfps = (float)current_fps;
        }
    }
}

static int func_run_sync(IJKFF_Pipenode *node)
{
    JNIEnv                *env      = NULL;
    SHIJIU_MTK_Pipenode_Opaque *opaque   = node->opaque;
    FFPlayer              *ffp      = opaque->ffp;
    VideoState            *is       = ffp->is;
    Decoder               *d        = &is->viddec;
    PacketQueue           *q        = d->queue;
    int                    ret1     = 0;
    int                    ret2     =0;
    int64_t                last_time = av_gettime();
    int64_t                current_time = 0;  
    int                    dequeue_count = 0;


    while (!q->abort_request) {
        // ALOGD("%s: pause_on = %d\n", __FUNCTION__, opaque->pause_on);
        SDL_LockMutex(opaque->wakeup_mutex);
        if(opaque->pause_changed){
            opaque->pause_changed = false;
            if(opaque->pause_on){
                ALOGD("mtkcodec pausing!\n");
                mtk_codec_pause(opaque->player_handler);
            }else{
                ALOGD("mtkcodec resuming!\n");
                mtk_codec_resume(opaque->player_handler);
            }
        }

        if (opaque->need_set_volume) {
            opaque->need_set_volume = false;
            setVolume(opaque, (int)(opaque->left_volume*100), (int)(opaque->right_volume*100));
            // setVolume(opaque, 100, 100);
        }

        modifyPlayBackRate(opaque);

        if (opaque->need_flush) {
            opaque->need_flush = 0;
            reset(opaque);
        }
        
        SDL_UnlockMutex(opaque->wakeup_mutex);

        if(!opaque->pause_on && opaque->audio_inited){
            dequeue_count = 0;
            // ALOGD("%s: pause_on = %d\n", __FUNCTION__, opaque->pause_on);
            if(opaque->audio_buffer.valid){
                SDL_LockMutex(opaque->audio_buffer.mutex);
                ret1 = feed_audio_buffer(opaque);
                if(ret1 == MTK_SUCCESS){
                    opaque->audio_buffer.valid = 0;
                    opaque->audio_buffer.write_size = 0;
                    memset(opaque->audio_buffer.buffer, 0, opaque->audio_buffer.buffer_size);
                }
                SDL_UnlockMutex(opaque->audio_buffer.mutex);
            }else{
                ret1 = -1;
            }
            ret2 = feed_video_buffer(env, node, &dequeue_count);
            // ALOGD("%s: after send buffer\n", __FUNCTION__);
            if (ret1 != MTK_SUCCESS && ret2 != MTK_SUCCESS/* || dequeue_count == 0*/) {
                // SDL_CondWaitTimeout(opaque->wakeup_cond, opaque->wakeup_mutex, 50);
                if(ret2 == MTK_NODATA){
                    usleep(10000); //sleep 5ms
                }else{
                    usleep(5000); //sleep 5ms
                }
                //goto fail;
            }else if(d->finished){
                usleep(10000); //sleep 10ms
            }
        }else{
            usleep(10000); //sleep 10ms
        }
        // ALOGD("%s: after send buffer\n", __FUNCTION__);

        // current_time = av_gettime();
        // // ALOGD("mtkcodec: getFPs, current=%lld, last=%lld\n", current_time, last_time);
        // //每1秒钟更新一次fps
        // if(current_time - last_time >= 1000000){
        //     // ALOGD("mtkcodec: getFPs, current=%lld, last=%lld\n", current_time, last_time);
        //     last_time = current_time;
        //     getFps(opaque);
        // }


        // if(opaque->syn_start_time > 0 && (current_time - opaque->syn_start_time > opaque->syn_time)){
        //     syn_process_off(opaque);
        // }

        // if(debug_mode){
        //     modifyRect(opaque);
        // }

        // modifyPlayBackRate(opaque);
        //
        // If we couldn't read a packet, it's probably because we're paused or at
        // the end of the stream, so sleep for a bit and resume.
        //
        // if(dequeue_count == 0 || opaque->pause_on) {
        //     // usleep(1000000/100);
        //     SDL_CondWaitTimeout(opaque->wakeup_cond, opaque->wakeup_mutex, 10);
        //     //ALOGD("no dequeue");
        //     continue;
        // }

        //
        // Synchronize the video to the master clock
        //
        // syncToMaster(opaque);
    }
// fail:
    ALOGD("mtkcodec: func_run_sync goto end\n");
    return 0;
}

// static void syncToMaster(SHIJIU_MTK_Pipenode_Opaque *opaque)
// {
//     FFPlayer     *ffp = opaque->ffp;
//     VideoState   *is  = ffp->is;
//     Decoder      *d   = &is->viddec;
//     PacketQueue  *q        = d->queue;

//     //
//     //
//     // We have our amlogic video running. It presents frames (that we can't access) according
//     // to pts values. It uses its own clock, pcrscr, that we can read and modify.
//     //
//     // ijkplayer is then presenting audio according to its own audio clock, which has some pts
//     // time associated with that too.
//     //
//     // is->audclk gives the audio pts at the last packet played (or as close as possible).
//     //
//     // We adjust pcrscr so that it matches audclk, but we don't want to do that instantaneously
//     // every frame because then the video jitters.
//     //
//     //

//     int64_t pts_audio = getAudioPts(ffp, is); // current pcr-scr base from ptr-audio

//     int64_t last_pts = opaque->decode_pts;

//     // prevent decoder/demux from getting too far ahead of video
//     // int slept = 0;
//     // while(last_pts > 0 && (mtk_time_sub_32(last_pts, pts_audio) > 90000*2)) {
//     //     if(q->abort_request){
//     //         return;
//     //     }
        
//     //     usleep(1000000/100);
//     //     slept += 1;
//     //     if(slept >= 100) {
//     //         ALOGD("slept:%d pts_audio:%jd decode_dts:%jd", slept, pts_audio, last_pts);
//     //         slept = 0;
//     //         break;
//     //     }
//     //     pts_audio = getAudioPts(ffp, is);
//     // }

//     // pts_audio = getAudioPts(ffp, is);

//     long     pts_pcrscr = get_current_time(opaque);  // think this the master clock time for amcodec output
//     int64_t delta = mtk_time_sub(pts_audio, pts_pcrscr);

//     ALOGD("pts_pcrscr: %jd, pts_audio: %jd, delta: %jd!", pts_pcrscr, pts_audio, delta);


//     // // // modify pcrscr so that the frame presentation times are adjusted 'instantly'
//     // // if(is->audclk.serial == d->pkt_serial && d->first_frame_decoded) {
//     //     int threshhold = 9000;
//     //     if (ffp->pf_playback_rate != 1.0) {
//     //         threshhold = 1000;
//     //     }
//     //     if(abs(delta) > threshhold) {
//     //         opaque->pcrscr_base = pts_pcrscr + delta;
//     //         set_pts_pcrscr(opaque, pts_pcrscr + delta);
//     //         ALOGD("set pcr %jd, %jd, %jd!", pts_pcrscr, delta, pts_pcrscr + delta);
//     //     }

//     //     opaque->do_discontinuity_check = 1;
//     // }

// // #if TIMING_DEBUG
// //     ALOGD("pts_audio:%jd pts_pcrscr:%jd delta:%jd offset:%jd last_pts:%jd slept:%d sync_master:%d",
// //         pts_audio, pts_pcrscr, delta, offset, last_pts, slept, get_master_sync_type(is));
// // #endif
// }

// static int64_t getAudioPts(FFPlayer *ffp, VideoState  *is) {


//     if (*(is->audclk.queue_serial) != is->audclk.serial)
//         return NAN;

//     int64_t pts_audio = is->audclk.pts * 90000.0;

//     // since audclk.pts was recorded time has advanced so take that into account.
//     int64_t offset = av_gettime_relative() - ffp->audio_callback_time;
//     offset = offset * 9/100;    // convert 1000KHz counter to 90KHz
//     pts_audio += offset;

//     pts_audio += 10000;    // Magic!

//     return pts_audio;
// }

// static int64_t mtk_time_sub(int64_t a, int64_t b)
// {
//     // int64_t shift = 64 - AM_TIME_BITS;
//     // return ((a - b) << shift) >> shift;
//     return a - b;
// }

// // 这个sub只拿后32位
// static int64_t mtk_time_sub_32(int64_t a, int64_t b)
// {
//     int64_t shift = 64 - AM_TIME_BITS;
//     return ((a - b) << shift) >> shift;
// }

// 拿origin_time的后32位
// static int64_t get_32bit_time(int64_t origin_time) {
//     return origin_time & 0x00000000ffffffff;
// }

// 获取当前时间
static int64_t get_current_time(SHIJIU_MTK_Pipenode_Opaque *opaque) {
    FFPlayer                    *ffp      = opaque->ffp;
    VideoState                  *is       = ffp->is;

    if(opaque->first_audio_pts == -1 || opaque->first_video_pts == -1){
        return is->seek_pos;
    }

    

    ALOGD("get_current_time, first_video_pts = %lld, current_playtime = %lld\n", opaque->first_video_pts, mtk_codec_get_current_playtime(opaque->player_handler));

    if(opaque->player_handler != MTK_HANDLER_INVALID){
        return MAX(opaque->first_video_pts, opaque->first_audio_pts)+mtk_codec_get_current_playtime(opaque->player_handler);
    }

    return (int64_t)0;
}

static void mtk_codec_evnet_handler(CTC_PLAYER_EVT_e evt, int32_t ext,void *handler)
{
    SHIJIU_MTK_Pipenode_Opaque *opaque = (SHIJIU_MTK_Pipenode_Opaque *)handler;
    FFPlayer                    *ffp      = opaque->ffp;
    VideoState                  *is       = ffp->is;
    Decoder                     *d        = &is->viddec;
    ALOGI("mtk_codec_evnet_handler: event(%d), ext(%d)\n", evt, ext);

    switch(evt){
        case CTC_PLAYER_EVT_STREAM_VALID:
            ALOGD("Mtk event: stream valid\n");
            break;
        case CTC_PLAYER_EVT_FIRST_PTS:
            ALOGD("Mtk event: first pts\n");
            break;
        case CTC_PLAYER_EVT_VOD_EOS:
            ALOGD("Mtk event: eos\n");
            d->finished = d->pkt_serial;
            break;
        case CTC_PLAYER_EVT_UNDERFLOW:
            ALOGD("Mtk event: underflow\n");
            break;
        case CTC_PLAYER_EVT_PLAYBACK_ERROR:
            ALOGD("Mtk event: play error\n");
            break;
        case CTC_PLAYER_EVT_PLAYBACK_LOSTFRAME:
            ALOGD("Mtk event: lost frame\n");
            break;
        case CTC_PLAYER_EVT_AUDIOTRACK_NOSUPPORT:
            ALOGD("Mtk event: audio track unsupport\n");
            break;
        case CTC_PLAYER_EVENT_STUTTER:
            ALOGD("Mtk event: stutter\n");
            break;
        //Unload event
        case CTC_PLAYER_EVT_UNLOAD_START:
            ALOGD("Mtk event: unload start\n");
            break;
        case CTC_PLAYER_EVT_UNLOAD_END:
            ALOGD("Mtk event: unload end\n");
            break;
        //BLURREDSCREEN event
        case CTC_PLAYER_EVT_BLURREDSCREEN_START:
            ALOGD("Mtk event: blurredscreen start\n");
            break;
        case CTC_PLAYER_EVT_BLURREDSCREEN_END:
            ALOGD("Mtk event: blurredscreen end\n");
            break;
        default:
            ALOGI("Unknown mtk codec event(%d), ext(%d)\n", evt, ext);
            break;
    }
}

static void mtk_codec_register_event(SHIJIU_MTK_Pipenode_Opaque *opaque){
    PLAYER_LOGI("### mtkcodec::register event \n");

    mtk_codec_register_evt_cb(opaque->player_handler, mtk_codec_evnet_handler, (void *)opaque);
}

static int mtk_video_init(SHIJIU_MTK_Pipenode_Opaque *opaque){
    VIDEO_PARA_T stVideoParam;
    JNIEnv *env = NULL;
    FFPlayer *ffp = opaque->ffp;
    IJKFF_Pipeline *pipeline = ffp->pipeline;
    jobject jsurface = NULL;

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        ALOGE("mtk_video_init: SDL_AndroidJni_SetupEnv: failed");
        return -1;
    }

    ALOGD("mtk_video_init: handler:0x%x\n", opaque->player_handler);
//    opaque->player_handler = mtk_codec_create(ffp->pip_mode);
//    opaque->player_handler = mtk_codec_create(0);
    if(opaque->player_handler == MTK_HANDLER_INVALID){
        return -1;
    }

    jsurface = ffpipeline_get_surface_as_global_ref(env, pipeline);
    ALOGD("mtk_video_init: jsurface:0x%x\n", jsurface);
    // mtk_codec_set_video_window(opaque->player_handler, 0, 0, 1920, 1080);
    if(jsurface != NULL)
        mtk_codec_set_surface(opaque->player_handler, env, jsurface);

    memset(&stVideoParam, 0, sizeof(VIDEO_PARA_T));
    stVideoParam.pid = 0xFFFF;
    stVideoParam.nVideoWidth = opaque->video_width;
    stVideoParam.nVideoHeight = opaque->video_height;
    stVideoParam.vFmt = opaque->video_format;
    stVideoParam.nFrameRate = (int)round(opaque->fps);
    ALOGI("mtk_video_init, video_codec_type=%d\n", opaque->video_codec_type);
    stVideoParam.cFmt = opaque->video_codec_type;
    mtk_codec_video_init(opaque->player_handler, &stVideoParam);

    return 0;
}

static int mtk_audio_init(SHIJIU_MTK_Pipenode_Opaque *opaque, SDL_AudioSpec *param){
    AUDIO_PARA_T stAudioParam;

    if(opaque->player_handler == MTK_HANDLER_INVALID){
        return -1;
    }

    memset(&stAudioParam, 0, sizeof(AUDIO_PARA_T));
    stAudioParam.pid = 0xFFFF;
    stAudioParam.nChannels = param->channels;
    stAudioParam.nSampleRate = param->freq;
    if(param->format == AUDIO_S16LSB){
        stAudioParam.aFmt = FORMAT_PCM_S16LE;
    }else{
        stAudioParam.aFmt = FORMAT_PCM_S16BE;
    }
    // stAudioParam.aFmt = FORMAT_UNKNOWN;
    stAudioParam.bit_rate = param->channels*param->freq*16;
    stAudioParam.block_align = 2;
    stAudioParam.sample_depth = 16;
    
    mtk_codec_audio_init(opaque->player_handler, &stAudioParam);

    mtk_codec_start(opaque->player_handler);

    mtk_codec_register_event(opaque);

    return 0;
}

static vformat_t codecid_to_vformat(enum AVCodecID id)
{
  vformat_t format;
  switch (id)
  {
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
//    case AV_CODEC_ID_MPEG2VIDEO_XVMC:
      format = VFORMAT_MPEG12;
      break;
    case AV_CODEC_ID_H263:
    case AV_CODEC_ID_MPEG4:
    case AV_CODEC_ID_H263P:
    case AV_CODEC_ID_H263I:
    case AV_CODEC_ID_MSMPEG4V2:
    case AV_CODEC_ID_MSMPEG4V3:
    case AV_CODEC_ID_FLV1:
      format = VFORMAT_MPEG4;
      break;
    case AV_CODEC_ID_RV10:
    case AV_CODEC_ID_RV20:
    case AV_CODEC_ID_RV30:
    case AV_CODEC_ID_RV40:
      format = VFORMAT_REAL;
      break;
    case AV_CODEC_ID_H264:
      format = VFORMAT_H264;
      break;
    /*
    case AV_CODEC_ID_H264MVC:
      // H264 Multiview Video Coding (3d blurays)
      format = VFORMAT_H264MVC;
      break;
    */
    case AV_CODEC_ID_MJPEG:
      format = VFORMAT_MJPEG;
      break;
    case AV_CODEC_ID_VC1:
    case AV_CODEC_ID_WMV3:
      format = VFORMAT_VC1;
      break;
    case AV_CODEC_ID_AVS:
    case AV_CODEC_ID_CAVS:
      format = VFORMAT_AVS;
      break;
    case AV_CODEC_ID_HEVC:
      format = VFORMAT_H265;
      break;

    default:
      format = VFORMAT_UNKNOWN;
      break;
  }

  ALOGD("codecid_to_vformat, id(%d) -> vformat(%d)", (int)id, format);
  return format;
}

static vdec_type_t codecid_to_mtkcodec(enum AVCodecID id)
{
  vdec_type_t codec;
  switch (id)
  {
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
    // case AV_CODEC_ID_MPEG2VIDEO_XVMC:
      codec = VIDEO_DEC_FORMAT_MP4;
      break;
    case AV_CODEC_ID_H263:
    case AV_CODEC_ID_H263P:
    case AV_CODEC_ID_H263I:
        codec = VIDEO_DEC_FORMAT_H263;
        break;
    case AV_CODEC_ID_MPEG4:
    case AV_CODEC_ID_FLV1:
        codec = VIDEO_DEC_FORMAT_MP4;
        break;
    case AV_CODEC_ID_MSMPEG4V2:
    case AV_CODEC_ID_MSMPEG4V3:
        codec = VIDEO_DEC_FORMAT_MPEG4_3;
        break;
    case AV_CODEC_ID_RV10:
    case AV_CODEC_ID_RV20:
    case AV_CODEC_ID_RV30:
    case AV_CODEC_ID_RV40:
      codec = VIDEO_DEC_FORMAT_REAL_8;
      break;
    case AV_CODEC_ID_H264:
      codec = VIDEO_DEC_FORMAT_H264;
      break;
    /*
    case AV_CODEC_ID_H264MVC:
      // H264 Multiview Video Coding (3d blurays)
      format = VFORMAT_H264MVC;
      break;
    */
    case AV_CODEC_ID_MJPEG:
      codec = VIDEO_DEC_FORMAT_MJPEG;
      break;
    case AV_CODEC_ID_VC1:
        codec = VIDEO_DEC_FORMAT_WVC1;
      break;
    case AV_CODEC_ID_WMV3:
      codec = VIDEO_DEC_FORMAT_WMV3;
      break;
    case AV_CODEC_ID_AVS:
    case AV_CODEC_ID_CAVS:
      codec = VIDEO_DEC_FORMAT_SW;
      break;
    case AV_CODEC_ID_HEVC:
      codec = VIDEO_DEC_FORMAT_H265;
      break;
    default:
      codec = VIDEO_DEC_FORMAT_UNKNOWN;
      break;
  }

  ALOGD("codecid_to_mtkcodec, id(%d) -> vcodec(%d)", (int)id, codec);
  return codec;
}

static vdec_type_t codec_tag_to_vdec_type(uint32_t codec_tag)
{
  vdec_type_t dec_type;
  switch (codec_tag)
  {
    case CODEC_TAG_MJPEG:
    case CODEC_TAG_mjpeg:
    case CODEC_TAG_jpeg:
    case CODEC_TAG_mjpa:
      // mjpeg
      dec_type = VIDEO_DEC_FORMAT_MJPEG;
      break;
    case CODEC_TAG_XVID:
    case CODEC_TAG_xvid:
    case CODEC_TAG_XVIX:
      // xvid
      dec_type = VIDEO_DEC_FORMAT_MPEG4_5;
      break;
    case CODEC_TAG_COL1:
    case CODEC_TAG_DIV3:
    case CODEC_TAG_MP43:
      // divx3.11
      dec_type = VIDEO_DEC_FORMAT_MPEG4_3;
      break;
    case CODEC_TAG_DIV4:
    case CODEC_TAG_DIVX:
      // divx4
      dec_type = VIDEO_DEC_FORMAT_MPEG4_4;
      break;
    case CODEC_TAG_DIV5:
    case CODEC_TAG_DX50:
    case CODEC_TAG_M4S2:
    case CODEC_TAG_FMP4:
      // divx5
      dec_type = VIDEO_DEC_FORMAT_MPEG4_5;
      break;
    case CODEC_TAG_DIV6:
      // divx6
      dec_type = VIDEO_DEC_FORMAT_MPEG4_5;
      break;
    case CODEC_TAG_MP4V:
    case CODEC_TAG_RMP4:
    case CODEC_TAG_MPG4:
    case CODEC_TAG_mp4v:
    case AV_CODEC_ID_MPEG4:
      // mp4
      dec_type = VIDEO_DEC_FORMAT_MPEG4_5;
      break;
    case AV_CODEC_ID_H263:
    case CODEC_TAG_H263:
    case CODEC_TAG_h263:
    case CODEC_TAG_s263:
    case CODEC_TAG_F263:
      // h263
      dec_type = VIDEO_DEC_FORMAT_H263;
      break;
    case CODEC_TAG_AVC1:
    case CODEC_TAG_avc1:
    case CODEC_TAG_H264:
    case CODEC_TAG_h264:
    case AV_CODEC_ID_H264:
      // h264
      dec_type = VIDEO_DEC_FORMAT_H264;
      break;
    /*
    case AV_CODEC_ID_H264MVC:
      dec_type = VIDEO_DEC_FORMAT_H264;
      break;
    */
    case AV_CODEC_ID_RV30:
    case CODEC_TAG_RV30:
      // realmedia 3
      dec_type = VIDEO_DEC_FORMAT_REAL_8;
      break;
    case AV_CODEC_ID_RV40:
    case CODEC_TAG_RV40:
      // realmedia 4
      dec_type = VIDEO_DEC_FORMAT_REAL_9;
      break;
    case CODEC_TAG_WMV3:
      // wmv3
      dec_type = VIDEO_DEC_FORMAT_WMV3;
      break;
    case AV_CODEC_ID_VC1:
    case CODEC_TAG_VC_1:
    case CODEC_TAG_WVC1:
    case CODEC_TAG_WMVA:
      // vc1
      dec_type = VIDEO_DEC_FORMAT_WVC1;
      break;
    case AV_CODEC_ID_VP6F:
      // vp6
      dec_type = VIDEO_DEC_FORMAT_SW;
      break;
    case AV_CODEC_ID_CAVS:
    case AV_CODEC_ID_AVS:
      // avs
      dec_type = VIDEO_DEC_FORMAT_SW;
      break;
    case AV_CODEC_ID_HEVC:
      // h265
      dec_type = VIDEO_DEC_FORMAT_H265;
      break;
    default:
      dec_type = VIDEO_DEC_FORMAT_UNKNOWN;
      break;
  }

  ALOGD("codec_tag_to_vdec_type, codec_tag(%d) -> vdec_type(%d)", codec_tag, dec_type);
  return dec_type;
}

static int h264_add_header(unsigned char *buf, int size, mtk_packet_t *pkt)
{
    if (size > HDR_BUF_SIZE)
    {
        free(pkt->hdr->data);
        pkt->hdr->data = (char *)malloc(size);
        if (!pkt->hdr->data)
            return MTK_FAIL;
    }

    memcpy(pkt->hdr->data, buf, size);
    pkt->hdr->size = size;
    return MTK_SUCCESS;
}

static int h264_write_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt)
{
    // ALOGD("h264_write_header");
    int ret = h264_add_header(para->extradata, para->extrasize, pkt);
    if (ret == MTK_SUCCESS) {
        pkt->newflag = 1;
        ret = write_av_packet(para, pkt);
    }
    return ret;
}

/*************************************************************************/
static int m4s2_dx50_mp4v_add_header(unsigned char *buf, int size,  mtk_packet_t *pkt)
{
    if (size > pkt->hdr->size) {
        free(pkt->hdr->data), pkt->hdr->data = NULL;
        pkt->hdr->size = 0;

        pkt->hdr->data = (char*)malloc(size);
        if (!pkt->hdr->data) {
            ALOGD("[m4s2_dx50_add_header] NOMEM!");
            return MTK_FAIL;
        }
    }

    pkt->hdr->size = size;
    memcpy(pkt->hdr->data, buf, size);

    return MTK_SUCCESS;
}

static int m4s2_dx50_mp4v_write_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt)
{
    ALOGD("m4s2_dx50_mp4v_write_header");
    int ret = m4s2_dx50_mp4v_add_header(para->extradata, para->extrasize, pkt);
    if (ret == MTK_SUCCESS) {
        pkt->newflag = 1;
        ret = write_av_packet(para, pkt);
    }
    return ret;
}

static int divx3_data_prefeeding(mtk_packet_t *pkt, unsigned w, unsigned h)
{
    unsigned i = (w << 12) | (h & 0xfff);
    unsigned char divx311_add[10] = {
        0x00, 0x00, 0x00, 0x01,
        0x20, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    divx311_add[5] = (i >> 16) & 0xff;
    divx311_add[6] = (i >> 8) & 0xff;
    divx311_add[7] = i & 0xff;

    if (pkt->hdr->data) {
        memcpy(pkt->hdr->data, divx311_add, sizeof(divx311_add));
        pkt->hdr->size = sizeof(divx311_add);
    } else {
        ALOGE("[divx3_data_prefeeding]No enough memory!");
        return MTK_FAIL;
    }
    return MTK_SUCCESS;
}

static int divx3_write_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt)
{
    ALOGD("divx3_write_header");
    divx3_data_prefeeding(pkt, para->video_width, para->video_height);
    pkt->newflag = 1;
    write_av_packet(para, pkt);
    return MTK_SUCCESS;
}

static int mjpeg_data_prefeeding(mtk_packet_t *pkt)
{
    const unsigned char mjpeg_addon_data[] = {
        0xff, 0xd8, 0xff, 0xc4, 0x01, 0xa2, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x01, 0x00, 0x03, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x10,
        0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00,
        0x00, 0x01, 0x7d, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31,
        0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1,
        0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72,
        0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47,
        0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64,
        0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95,
        0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
        0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4,
        0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
        0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1,
        0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0x11, 0x00, 0x02, 0x01,
        0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77,
        0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51,
        0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1,
        0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24,
        0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a,
        0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82,
        0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa,
        0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
        0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
        0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4,
        0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa
    };

    if (pkt->hdr->data) {
        memcpy(pkt->hdr->data, &mjpeg_addon_data, sizeof(mjpeg_addon_data));
        pkt->hdr->size = sizeof(mjpeg_addon_data);
    } else {
        ALOGE("[mjpeg_data_prefeeding]No enough memory!");
        return MTK_FAIL;
    }
    return MTK_SUCCESS;
}

static int mjpeg_write_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt)
{
    mjpeg_data_prefeeding(pkt);
    pkt->newflag = 1;
    write_av_packet(para, pkt);
    return MTK_SUCCESS;
}

static int hevc_add_header(unsigned char *buf, int size,  mtk_packet_t *pkt)
{
    if (size > HDR_BUF_SIZE)
    {
        free(pkt->hdr->data);
        pkt->hdr->data = (char *)malloc(size);
        if (!pkt->hdr->data)
            return MTK_FAIL;
    }

    memcpy(pkt->hdr->data, buf, size);
    pkt->hdr->size = size;
    return MTK_SUCCESS;
}

static int hevc_write_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt)
{
    int ret = -1;

    if (para->extradata) {
      ret = hevc_add_header(para->extradata, para->extrasize, pkt);
    }
    if (ret == MTK_SUCCESS) {
      pkt->newflag = 1;
      ret = write_av_packet(para, pkt);
    }
    return ret;
}

static int wmv3_write_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt)
{
    ALOGD("wmv3_write_header");
    unsigned i, check_sum = 0;
    unsigned data_len = para->extrasize + 4;

    pkt->hdr->data[0] = 0;
    pkt->hdr->data[1] = 0;
    pkt->hdr->data[2] = 1;
    pkt->hdr->data[3] = 0x10;

    pkt->hdr->data[4] = 0;
    pkt->hdr->data[5] = (data_len >> 16) & 0xff;
    pkt->hdr->data[6] = 0x88;
    pkt->hdr->data[7] = (data_len >> 8) & 0xff;
    pkt->hdr->data[8] = data_len & 0xff;
    pkt->hdr->data[9] = 0x88;

    pkt->hdr->data[10] = 0xff;
    pkt->hdr->data[11] = 0xff;
    pkt->hdr->data[12] = 0x88;
    pkt->hdr->data[13] = 0xff;
    pkt->hdr->data[14] = 0xff;
    pkt->hdr->data[15] = 0x88;

    for (i = 4 ; i < 16 ; i++) {
        check_sum += pkt->hdr->data[i];
    }

    pkt->hdr->data[16] = (check_sum >> 8) & 0xff;
    pkt->hdr->data[17] =  check_sum & 0xff;
    pkt->hdr->data[18] = 0x88;
    pkt->hdr->data[19] = (check_sum >> 8) & 0xff;
    pkt->hdr->data[20] =  check_sum & 0xff;
    pkt->hdr->data[21] = 0x88;

    pkt->hdr->data[22] = (para->video_width >> 8) & 0xff;
    pkt->hdr->data[23] =  para->video_width & 0xff;
    pkt->hdr->data[24] = (para->video_height >> 8) & 0xff;
    pkt->hdr->data[25] =  para->video_height & 0xff;

    memcpy(pkt->hdr->data + 26, para->extradata, para->extrasize);
    pkt->hdr->size = para->extrasize + 26;
    pkt->newflag = 1;
    return write_av_packet(para, pkt);
}

static int wvc1_write_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt)
{
    ALOGD("wvc1_write_header");
    memcpy(pkt->hdr->data, para->extradata + 1, para->extrasize - 1);
    pkt->hdr->size = para->extrasize - 1;
    pkt->newflag = 1;
    return write_av_packet(para, pkt);
}

static int mpeg_add_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt)
{
    ALOGD("mpeg_add_header");
#define STUFF_BYTES_LENGTH     (256)
    int size;
    unsigned char packet_wrapper[] = {
        0x00, 0x00, 0x01, 0xe0,
        0x00, 0x00,                                /* pes packet length */
        0x81, 0xc0, 0x0d,
        0x20, 0x00, 0x00, 0x00, 0x00, /* PTS */
        0x1f, 0xff, 0xff, 0xff, 0xff, /* DTS */
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    size = para->extrasize + sizeof(packet_wrapper);
    packet_wrapper[4] = size >> 8 ;
    packet_wrapper[5] = size & 0xff ;
    memcpy(pkt->hdr->data, packet_wrapper, sizeof(packet_wrapper));
    size = sizeof(packet_wrapper);
    //ALOGD("[mpeg_add_header:%d]wrapper size=%d\n",__LINE__,size);
    memcpy(pkt->hdr->data + size, para->extradata, para->extrasize);
    size += para->extrasize;
    //ALOGD("[mpeg_add_header:%d]wrapper+seq size=%d\n",__LINE__,size);
    memset(pkt->hdr->data + size, 0xff, STUFF_BYTES_LENGTH);
    size += STUFF_BYTES_LENGTH;
    pkt->hdr->size = size;
    //ALOGD("[mpeg_add_header:%d]hdr_size=%d\n",__LINE__,size);

    pkt->newflag = 1;
    return write_av_packet(para, pkt);
}

static int write_av_packet(SHIJIU_MTK_Pipenode_Opaque *opaque, mtk_packet_t *pkt)
{
    int32_t    ret = MTK_SUCCESS;
    unsigned char *buf;
    int size;

    // ALOGD("write_av_packet in\n");
    // memset(para->video_buffer.buffer, 0, para->video_buffer.buffer_size);
    // para->video_buffer.write_pos = 0;

    // do we need to check in pts or write the header ?
    if (pkt->newflag) {
        if (write_header(opaque, pkt) == MTK_FAIL) {
            ALOGE("[%s]write header failed!", __FUNCTION__);
            return MTK_FAIL;
        }
        pkt->newflag = 0;
    }
  
    buf = pkt->data;
    size = pkt->data_size ;
    if (size == 0 && pkt->isvalid) {
        pkt->isvalid = 0;
        pkt->data_size = 0;
    }

    if (size > 0 && pkt->isvalid) {
        // memcpy(para->video_buffer.buffer+para->video_buffer.write_pos, buf, size);
        // para->video_buffer.write_pos += size;
        ret = mtk_write_video_data(opaque, buf, pkt->avpts, size);
        // ret = mtk_write_video_data(para, para->video_buffer.buffer, pkt->avpts, para->video_buffer.write_pos);
        if(ret == MTK_SUCCESS){
            pkt->isvalid = 0;
            pkt->data_size = 0;
            if(opaque->first_video_pts == -1){
                opaque->first_video_pts =  pkt->avpts;
            }
        }
    }

    // ALOGD("write_av_packet exit\n");

    return ret;
}

static int write_header(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt)
{
    int32_t ret = MTK_SUCCESS; 

    // ALOGD("write_header\n");
    if (pkt->hdr && pkt->hdr->size > 0) {
        if (/*(NULL == pkt->codec) || */(NULL == pkt->hdr->data)) {
            ALOGE("[write_header]codec null!");
            return MTK_FAIL;
        }
        //some wvc1 es data not need to add header
        if (para->video_format == VFORMAT_VC1 && para->video_codec_type == VIDEO_DEC_FORMAT_WVC1) {
            if ((pkt->data) && (pkt->data_size >= 4)
              && (pkt->data[0] == 0) && (pkt->data[1] == 0)
              && (pkt->data[2] == 1) && (pkt->data[3] == 0xd || pkt->data[3] == 0xf)) {
                return MTK_SUCCESS;
            }
        }
        
        // ALOGD("write_header, size = %d\n", pkt->hdr->size);

        // memcpy(para->video_buffer.buffer+para->video_buffer.write_pos, (uint8_t *)pkt->hdr->data, pkt->hdr->size);
        // para->video_buffer.write_pos += pkt->hdr->size;
        ret = mtk_write_video_data(para, (uint8_t *)pkt->hdr->data, pkt->avpts, pkt->hdr->size);

        if (ret != MTK_SUCCESS) {
            ALOGE("ERROR:write header failed!");
        }
    }

    return ret;
}

static int mtk_write_video_data(SHIJIU_MTK_Pipenode_Opaque *opaque, uint8_t * buffer, uint64_t pts, int32_t size){
    ALOGI("mtk_write_video_data: size=%d, pts=%lld, video-audio=%lld\n", size, pts, opaque->video_write_pts-opaque->audio_write_pts);
    int ret = MTK_SUCCESS;

    if(!opaque->audio_inited){
        return MTK_FAIL;
    }


    if(opaque->video_write_pts != 0 && opaque->audio_write_pts != 0 && opaque->video_write_pts-opaque->audio_write_pts > 1000){
        return MTK_FAIL;
    }

    ret = mtk_codec_write_data(opaque->player_handler, STREAMTYPE_VIDEO, buffer, size, pts);
    ALOGI("mtk_write_video_data: ret = %d\n", ret);

    if(ret <= 0)
        return MTK_FAIL;
    else{
        opaque->video_write_pts = pts;
        return MTK_SUCCESS;
    }
}

static int pre_header_feeding(SHIJIU_MTK_Pipenode_Opaque *para, mtk_packet_t *pkt)
{
    int ret;
    
    if (pkt->hdr == NULL) {
        pkt->hdr = (hdr_buf_t*)malloc(sizeof(hdr_buf_t));
        pkt->hdr->data = (char *)malloc(HDR_BUF_SIZE);
        if (!pkt->hdr->data) {
            ALOGE("[pre_header_feeding] NOMEM!");
            return MTK_FAIL;
        }
    }

    if (VFORMAT_H264 == para->video_format) {
        ret = h264_write_header(para, pkt);
        if (ret != MTK_SUCCESS) {
            return ret;
        }
    } else if ((VFORMAT_MPEG4 == para->video_format) && (VIDEO_DEC_FORMAT_MPEG4_3 == para->video_codec_type)) {
        ret = divx3_write_header(para, pkt);
        if (ret != MTK_SUCCESS) {
            return ret;
        }
    } else if ((CODEC_TAG_M4S2 == para->video_codec_tag)
            || (CODEC_TAG_DX50 == para->video_codec_tag)
            || (CODEC_TAG_mp4v == para->video_codec_tag)) {
        ret = m4s2_dx50_mp4v_write_header(para, pkt);
        if (ret != MTK_SUCCESS) {
            return ret;
        }
    /*
    } else if ((AVI_FILE == para->file_type)
            && (VIDEO_DEC_FORMAT_MPEG4_3 != para->vstream_info.video_codec_type)
            && (VFORMAT_H264 != para->vstream_info.video_format)
            && (VFORMAT_VC1 != para->vstream_info.video_format)) {
        ret = avi_write_header(para);
        if (ret != PLAYER_SUCCESS) {
            return ret;
        }
    */
    } else if (CODEC_TAG_WMV3 == para->video_codec_tag) {
        ALOGD("CODEC_TAG_WMV3 == para->video_codec_tag");
        ret = wmv3_write_header(para, pkt);
        if (ret != MTK_SUCCESS) {
            return ret;
        }
    } else if ((CODEC_TAG_WVC1 == para->video_codec_tag)
            || (CODEC_TAG_VC_1 == para->video_codec_tag)
            || (CODEC_TAG_WMVA == para->video_codec_tag)) {
        ALOGD("CODEC_TAG_WVC1 == para->video_codec_tag");
        ret = wvc1_write_header(para, pkt);
        if (ret != MTK_SUCCESS) {
            return ret;
        }
    /*
    } else if ((MKV_FILE == para->file_type) &&
                ((VFORMAT_MPEG4 == para->vstream_info.video_format)
            || (VFORMAT_MPEG12 == para->vstream_info.video_format))) {
        ret = mkv_write_header(para, pkt);
        if (ret != PLAYER_SUCCESS) {
            return ret;
        }
    */
    } else if (VFORMAT_MJPEG == para->video_format) {
        ret = mjpeg_write_header(para, pkt);
        if (ret != MTK_SUCCESS) {
            return ret;
        }
    } else if (VFORMAT_H265 == para->video_format) {
        ret = hevc_write_header(para, pkt);
        if (ret != MTK_SUCCESS) {
            return ret;
        }
    }

    if (pkt->hdr) {
        if (pkt->hdr->data) {
            free(pkt->hdr->data);
            pkt->hdr->data = NULL;
        }
        free(pkt->hdr);
        pkt->hdr = NULL;
    }
    
    return MTK_SUCCESS;
}

static int divx3_prefix(mtk_packet_t *pkt)
{
#define DIVX311_CHUNK_HEAD_SIZE 13
    const unsigned char divx311_chunk_prefix[DIVX311_CHUNK_HEAD_SIZE] = {
        0x00, 0x00, 0x00, 0x01, 0xb6, 'D', 'I', 'V', 'X', '3', '.', '1', '1'
    };
    if ((pkt->hdr != NULL) && (pkt->hdr->data != NULL)) {
        free(pkt->hdr->data);
        pkt->hdr->data = NULL;
    }

    if (pkt->hdr == NULL) {
        pkt->hdr = (hdr_buf_t*)malloc(sizeof(hdr_buf_t));
        if (!pkt->hdr) {
            ALOGD("[divx3_prefix] NOMEM!");
            return MTK_FAIL;
        }

        pkt->hdr->data = NULL;
        pkt->hdr->size = 0;
    }

    pkt->hdr->data = (char*)malloc(DIVX311_CHUNK_HEAD_SIZE + 4);
    if (pkt->hdr->data == NULL) {
        ALOGD("[divx3_prefix] NOMEM!");
        return MTK_FAIL;
    }

    memcpy(pkt->hdr->data, divx311_chunk_prefix, DIVX311_CHUNK_HEAD_SIZE);

    pkt->hdr->data[DIVX311_CHUNK_HEAD_SIZE + 0] = (pkt->data_size >> 24) & 0xff;
    pkt->hdr->data[DIVX311_CHUNK_HEAD_SIZE + 1] = (pkt->data_size >> 16) & 0xff;
    pkt->hdr->data[DIVX311_CHUNK_HEAD_SIZE + 2] = (pkt->data_size >>  8) & 0xff;
    pkt->hdr->data[DIVX311_CHUNK_HEAD_SIZE + 3] = pkt->data_size & 0xff;

    pkt->hdr->size = DIVX311_CHUNK_HEAD_SIZE + 4;
    pkt->newflag = 1;

    return MTK_SUCCESS;
}


static int set_header_info(SHIJIU_MTK_Pipenode_Opaque *para)
{
  mtk_packet_t *pkt = &para->mtk_pkt;

  //if (pkt->newflag)
  {
    //if (pkt->hdr)
    //  pkt->hdr->size = 0;

    if (para->video_format == VFORMAT_MPEG4)
    {
      if (para->video_codec_type == VIDEO_DEC_FORMAT_MPEG4_3)
      {
        return divx3_prefix(pkt);
      }
      else if (para->video_codec_type == VIDEO_DEC_FORMAT_H263)
      {
        return MTK_FAIL;
      }
    } else if (para->video_format == VFORMAT_VC1) {
        if (para->video_codec_type == VIDEO_DEC_FORMAT_WMV3) {
            unsigned i, check_sum = 0, data_len = 0;

            if ((pkt->hdr != NULL) && (pkt->hdr->data != NULL)) {
                free(pkt->hdr->data);
                pkt->hdr->data = NULL;
            }

            if (pkt->hdr == NULL) {
                pkt->hdr = (hdr_buf_t*)malloc(sizeof(hdr_buf_t));
                if (!pkt->hdr) {
                    return MTK_FAIL;
                }

                pkt->hdr->data = NULL;
                pkt->hdr->size = 0;
            }

            if (pkt->avpkt.flags) {
                pkt->hdr->data = (char*)malloc(para->extrasize + 26 + 22);
                if (pkt->hdr->data == NULL) {
                    return MTK_FAIL;
                }

                pkt->hdr->data[0] = 0;
                pkt->hdr->data[1] = 0;
                pkt->hdr->data[2] = 1;
                pkt->hdr->data[3] = 0x10;

                data_len = para->extrasize + 4;
                pkt->hdr->data[4] = 0;
                pkt->hdr->data[5] = (data_len >> 16) & 0xff;
                pkt->hdr->data[6] = 0x88;
                pkt->hdr->data[7] = (data_len >> 8) & 0xff;
                pkt->hdr->data[8] =  data_len & 0xff;
                pkt->hdr->data[9] = 0x88;

                pkt->hdr->data[10] = 0xff;
                pkt->hdr->data[11] = 0xff;
                pkt->hdr->data[12] = 0x88;
                pkt->hdr->data[13] = 0xff;
                pkt->hdr->data[14] = 0xff;
                pkt->hdr->data[15] = 0x88;

                for (i = 4 ; i < 16 ; i++) {
                    check_sum += pkt->hdr->data[i];
                }

                pkt->hdr->data[16] = (check_sum >> 8) & 0xff;
                pkt->hdr->data[17] =  check_sum & 0xff;
                pkt->hdr->data[18] = 0x88;
                pkt->hdr->data[19] = (check_sum >> 8) & 0xff;
                pkt->hdr->data[20] =  check_sum & 0xff;
                pkt->hdr->data[21] = 0x88;

                pkt->hdr->data[22] = (para->video_width  >> 8) & 0xff;
                pkt->hdr->data[23] =  para->video_width  & 0xff;
                pkt->hdr->data[24] = (para->video_height >> 8) & 0xff;
                pkt->hdr->data[25] =  para->video_height & 0xff;

                memcpy(pkt->hdr->data + 26, para->extradata, para->extrasize);

                check_sum = 0;
                data_len = para->extrasize + 26;
            } else {
                pkt->hdr->data = (char*)malloc(22);
                if (pkt->hdr->data == NULL) {
                    return MTK_FAIL;
                }
            }

            pkt->hdr->data[data_len + 0]  = 0;
            pkt->hdr->data[data_len + 1]  = 0;
            pkt->hdr->data[data_len + 2]  = 1;
            pkt->hdr->data[data_len + 3]  = 0xd;

            pkt->hdr->data[data_len + 4]  = 0;
            pkt->hdr->data[data_len + 5]  = (pkt->data_size >> 16) & 0xff;
            pkt->hdr->data[data_len + 6]  = 0x88;
            pkt->hdr->data[data_len + 7]  = (pkt->data_size >> 8) & 0xff;
            pkt->hdr->data[data_len + 8]  =  pkt->data_size & 0xff;
            pkt->hdr->data[data_len + 9]  = 0x88;

            pkt->hdr->data[data_len + 10] = 0xff;
            pkt->hdr->data[data_len + 11] = 0xff;
            pkt->hdr->data[data_len + 12] = 0x88;
            pkt->hdr->data[data_len + 13] = 0xff;
            pkt->hdr->data[data_len + 14] = 0xff;
            pkt->hdr->data[data_len + 15] = 0x88;

            for (i = data_len + 4 ; i < data_len + 16 ; i++) {
                check_sum += pkt->hdr->data[i];
            }

            pkt->hdr->data[data_len + 16] = (check_sum >> 8) & 0xff;
            pkt->hdr->data[data_len + 17] =  check_sum & 0xff;
            pkt->hdr->data[data_len + 18] = 0x88;
            pkt->hdr->data[data_len + 19] = (check_sum >> 8) & 0xff;
            pkt->hdr->data[data_len + 20] =  check_sum & 0xff;
            pkt->hdr->data[data_len + 21] = 0x88;

            pkt->hdr->size = data_len + 22;
            pkt->newflag = 1;
        } else if (para->video_codec_type == VIDEO_DEC_FORMAT_WVC1) {
            if ((pkt->hdr != NULL) && (pkt->hdr->data != NULL)) {
                free(pkt->hdr->data);
                pkt->hdr->data = NULL;
            }

            if (pkt->hdr == NULL) {
                pkt->hdr = (hdr_buf_t*)malloc(sizeof(hdr_buf_t));
                if (!pkt->hdr) {
                    ALOGD("[wvc1_prefix] NOMEM!");
                    return MTK_FAIL;
                }

                pkt->hdr->data = NULL;
                pkt->hdr->size = 0;
            }

            pkt->hdr->data = (char*)malloc(4);
            if (pkt->hdr->data == NULL) {
                ALOGD("[wvc1_prefix] NOMEM!");
                return MTK_FAIL;
            }

            pkt->hdr->data[0] = 0;
            pkt->hdr->data[1] = 0;
            pkt->hdr->data[2] = 1;
            pkt->hdr->data[3] = 0xd;
            pkt->hdr->size = 4;
            pkt->newflag = 1;
        }
    }
  }
  return MTK_SUCCESS;
}




/**
 * @brief 下面是audio相关代码逻辑，用于将解码后的pcm数据送到海思音频模块里播放，这样做主要是利用海思的同步逻辑来解决音画同步问题。
 * 
 */

static SDL_Class g_audiotrack_class = {
    .name = "MTKAudioDEC",
};

static int aout_open_audio(SDL_Aout *aout, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
static void aout_pause_audio(SDL_Aout *aout, int pause_on);
static void aout_flush_audio(SDL_Aout *aout);
static void aout_set_volume(SDL_Aout *aout, float left_volume, float right_volume);
static void aout_close_audio(SDL_Aout *aout);
static int aout_get_audio_session_id(SDL_Aout *aout);
static void aout_free_l(SDL_Aout *aout);
static void func_set_playback_rate(SDL_Aout *aout, float speed);
//static int64_t func_get_current_position(SDL_Aout *aout);

SDL_Aout *SDL_AoutAndroid_CreateForMtkCodec(FFPlayer *ffp)
{
    SDL_Aout *aout = SDL_Aout_CreateInternal(sizeof(SHiJIU_MTK_Aout_Opaque));
    if (!aout)
        return NULL;

    if(ffp && ffp->node_vdec && ffp->node_vdec->opaque){
        SHiJIU_MTK_Aout_Opaque *aout_opaque = (SHiJIU_MTK_Aout_Opaque *)aout->opaque;
        SHIJIU_MTK_Pipenode_Opaque *mtk_opaque = (SHIJIU_MTK_Pipenode_Opaque *)ffp->node_vdec->opaque;
        aout_opaque->mtk_opaque = mtk_opaque;
    }

//    if(ffp->is){
//        ffp->is->use_chip_time = 1;
//    }

    aout->opaque_class = &g_audiotrack_class;
    aout->free_l       = aout_free_l;
    aout->open_audio   = aout_open_audio;
    aout->pause_audio  = aout_pause_audio;
    aout->flush_audio  = aout_flush_audio;
    aout->set_volume   = aout_set_volume;
    aout->close_audio  = aout_close_audio;
    aout->func_get_audio_session_id = aout_get_audio_session_id;
    aout->func_set_playback_rate    = func_set_playback_rate;
//    aout->func_get_current_position = func_get_current_position;

    return aout;
}

static int aout_check_spec(const SDL_AudioSpec *spec){
    if(spec == NULL){
        return -1;
    }

    if(spec->channels != 1 && spec->channels != 2){
        return -1;
    }
    
    if(spec->format != AUDIO_S16SYS && spec->format != AUDIO_U8){
        return -1;
    }

    if(spec->freq <= 0){
        return -1;
    }

    return 0;
}

static int aout_thread_n(JNIEnv *env, SHiJIU_MTK_Aout_Opaque *opaque)
{
    SHIJIU_MTK_Pipenode_Opaque *mtk_opaque = opaque->mtk_opaque;
    void *userdata = mtk_opaque->spec.userdata;
    FFPlayer *ffp  = mtk_opaque->ffp;
    SDL_AudioCallback audio_cblk = mtk_opaque->spec.callback;
    VideoState *is = ffp->is;
    // int                    ret      = 0;
    uint8_t *zero_buffer = mallocz(mtk_opaque->audio_buffer.buffer_size);
    int need_wait = 0;
    int is_zero_buffer = 0;

    while (!opaque->abort_request) {
        if(mtk_opaque->player_handler == MTK_HANDLER_INVALID || !mtk_opaque->player_handler
            || mtk_opaque->pause_on || opaque->finished){
            usleep(10000);
            continue;
        }
        
        // ALOGD("%s: pause_on = %d\n", __FUNCTION__, mtk_opaque->pause_on);
        // ret = feed_audio_buffer(mtk_opaque);
        if(!mtk_opaque->audio_buffer.valid){
            need_wait = 0;
            SDL_LockMutex(mtk_opaque->audio_buffer.mutex);
//            audio_cblk(userdata, mtk_opaque->audio_buffer.buffer, mtk_opaque->audio_buffer.buffer_size, &mtk_opaque->audio_buffer.write_size, 1);
            audio_cblk(userdata, mtk_opaque->audio_buffer.buffer, mtk_opaque->audio_buffer.buffer_size);
            if(mtk_opaque->audio_buffer.write_size > 0){
                is_zero_buffer = !memcmp(mtk_opaque->audio_buffer.buffer, zero_buffer, mtk_opaque->audio_buffer.write_size);
            }
            if(is->auddec.finished && mtk_opaque->audio_buffer.write_size > 0 && is_zero_buffer){
                opaque->finished = 1;
            }else{
                if(mtk_opaque->audio_buffer.write_size > 0 && !is_zero_buffer){
                    mtk_opaque->audio_buffer.valid = 1;
                }else{
                    need_wait = 1;
                }
            }
            SDL_UnlockMutex(mtk_opaque->audio_buffer.mutex);
            if(need_wait){
                usleep(5000);//5ms
            }
        }else{
            usleep(5000);//5ms
        }
        // ALOGD("%s: after send buffer, ret=%d\n", __FUNCTION__, ret);
    }

// fail:
    ALOGD("mtkcodec: aout_thread_n goto end\n");
    return 0;
}

static int aout_thread(void *arg)
{
    SHiJIU_MTK_Aout_Opaque *opaque = (SHiJIU_MTK_Aout_Opaque *)arg;
    
    // SDL_Aout_Opaque *opaque = aout->opaque;
    JNIEnv *env = NULL;

    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        ALOGE("aout_thread: SDL_AndroidJni_SetupEnv: failed");
        return -1;
    }

    return aout_thread_n(env, opaque);
}

static int aout_open_audio(SDL_Aout *aout, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained)
{
    // SDL_Aout_Opaque *opaque = aout->opaque;
    ALOGI("Now in aout_open_audio\n");

    if(aout_check_spec(desired) < 0){
        return -1;
    }

    JNIEnv *env = NULL;
    if (JNI_OK != SDL_JNI_SetupThreadEnv(&env)) {
        ALOGE("aout_open_audio: AttachCurrentThread: failed");
        return -1;
    }

    if(!aout){
        return -1;
    }

    SHiJIU_MTK_Aout_Opaque *opaque = (SHiJIU_MTK_Aout_Opaque *)aout->opaque;
    if(!opaque || !opaque->mtk_opaque){
        return -1;
    }

    assert(desired);

    opaque->finished = 0;
    opaque->mtk_opaque->spec = *desired;
    
    opaque->mtk_opaque->audio_buffer.buffer = malloc(AUDIO_CALLBACK_BUFF_SIZE);
    if (!opaque->mtk_opaque->audio_buffer.buffer) {
        ALOGE("%s: failed to allocate buffer\n", __FUNCTION__);
        return -1;
    }
    opaque->mtk_opaque->audio_buffer.valid = 0;
    opaque->mtk_opaque->audio_buffer.buffer_size = AUDIO_CALLBACK_BUFF_SIZE;
    opaque->mtk_opaque->audio_buffer.write_size = 0;
    opaque->mtk_opaque->audio_buffer.mutex = SDL_CreateMutex();

    mtk_audio_init(opaque->mtk_opaque, &opaque->mtk_opaque->spec);
    opaque->mtk_opaque->audio_inited = 1;
    pre_header_feeding(opaque->mtk_opaque, &opaque->mtk_opaque->mtk_pkt);

    if (obtained) {
        memcpy(obtained, desired, sizeof(SDL_AudioSpec));
        obtained->size = AUDIO_CALLBACK_BUFF_SIZE;
    }

    opaque->audio_tid = SDL_CreateThreadEx(&opaque->_audio_tid, aout_thread, opaque, "ff_aout_mtk");
    if (!opaque->audio_tid) {
        ALOGE("aout_open_audio: failed to create audio thread");
        return -1;
    }

    return 0;
}

static void aout_pause_audio(SDL_Aout *aout, int pause_on)
{
    ALOGI("%s: now in(%d)\n", __FUNCTION__, pause_on);
    if(!aout){
        ALOGI("%s: no aout\n", __FUNCTION__);
        return;
    }

    SHiJIU_MTK_Aout_Opaque *opaque = (SHiJIU_MTK_Aout_Opaque *)aout->opaque;
    if(!opaque || !opaque->mtk_opaque){
        ALOGI("%s: no opaque\n", __FUNCTION__);
        return;
    }

    SDL_LockMutex(opaque->mtk_opaque->wakeup_mutex);
    if(opaque->mtk_opaque->pause_on != pause_on){
        opaque->mtk_opaque->pause_on = pause_on;
        opaque->mtk_opaque->pause_changed = true;
    }
    if (!pause_on)
        SDL_CondSignal(opaque->mtk_opaque->wakeup_cond);

    SDL_UnlockMutex(opaque->mtk_opaque->wakeup_mutex);
}

static void aout_flush_audio(SDL_Aout *aout)
{
    ALOGI("%s: now in\n", __FUNCTION__);
    if(!aout){
        return;
    }

    SHiJIU_MTK_Aout_Opaque *opaque = (SHiJIU_MTK_Aout_Opaque *)aout->opaque;
    if(!opaque || !opaque->mtk_opaque){
        return;
    }

    SDL_LockMutex(opaque->mtk_opaque->wakeup_mutex);
    opaque->mtk_opaque->need_flush = 1;
    opaque->finished = 0;
    SDL_CondSignal(opaque->mtk_opaque->wakeup_cond);
    SDL_UnlockMutex(opaque->mtk_opaque->wakeup_mutex);
}

static void aout_set_volume(SDL_Aout *aout, float left_volume, float right_volume)
{
    if(!aout){
        return;
    }

    SHiJIU_MTK_Aout_Opaque *opaque = (SHiJIU_MTK_Aout_Opaque *)aout->opaque;
    if(!opaque || !opaque->mtk_opaque){
        return;
    }

    SDL_LockMutex(opaque->mtk_opaque->wakeup_mutex);
    if(opaque->mtk_opaque->left_volume != left_volume || opaque->mtk_opaque->right_volume != right_volume){
        opaque->mtk_opaque->left_volume = left_volume;
        opaque->mtk_opaque->right_volume = right_volume;
        opaque->mtk_opaque->need_set_volume = 1;
    }
    SDL_CondSignal(opaque->mtk_opaque->wakeup_cond);
    SDL_UnlockMutex(opaque->mtk_opaque->wakeup_mutex);
}

static void aout_close_audio(SDL_Aout *aout)
{
    ALOGI("%s: now in\n", __FUNCTION__);
    if(!aout){
        return;
    }

    SHiJIU_MTK_Aout_Opaque *opaque = (SHiJIU_MTK_Aout_Opaque *)aout->opaque;
    if(!opaque || !opaque->mtk_opaque){
        return;
    }

    SDL_LockMutex(opaque->mtk_opaque->wakeup_mutex);
    opaque->abort_request = 1;
    SDL_CondSignal(opaque->mtk_opaque->wakeup_cond);
    SDL_UnlockMutex(opaque->mtk_opaque->wakeup_mutex);
    
    if(opaque->audio_tid != NULL)
        SDL_WaitThread(opaque->audio_tid, NULL);

    opaque->audio_tid = NULL;
}

static int aout_get_audio_session_id(SDL_Aout *aout)
{
    if(!aout){
        return 0;
    }

    SHiJIU_MTK_Aout_Opaque *opaque = (SHiJIU_MTK_Aout_Opaque *)aout->opaque;
    if(!opaque || !opaque->mtk_opaque){
        return 0;
    }

    return opaque->mtk_opaque->audio_session_id;
}

static void aout_free_l(SDL_Aout *aout)
{
    ALOGI("%s: now in\n", __FUNCTION__);
    if (!aout)
        return;

    aout_close_audio(aout);
    SDL_Aout_FreeInternal(aout);
}

static void func_set_playback_rate(SDL_Aout *aout, float speed)
{
    if (!aout)
        return;

    SHiJIU_MTK_Aout_Opaque *opaque = (SHiJIU_MTK_Aout_Opaque *)aout->opaque;
    if(!opaque || !opaque->mtk_opaque){
        return;
    }

    SDL_LockMutex(opaque->mtk_opaque->wakeup_mutex);
    if(opaque->mtk_opaque->speed != speed){
        opaque->mtk_opaque->speed = speed;
        opaque->mtk_opaque->speed_changed = 1;
    }
    SDL_CondSignal(opaque->mtk_opaque->wakeup_cond);
    SDL_UnlockMutex(opaque->mtk_opaque->wakeup_mutex);
}

//static int64_t func_get_current_position(SDL_Aout *aout)
//{
//    if (!aout)
//        return 0;
//
//    SHiJIU_MTK_Aout_Opaque *opaque = (SHiJIU_MTK_Aout_Opaque *)aout->opaque;
//    if(!opaque || !opaque->mtk_opaque){
//        return 0;
//    }
//
//    int64_t current_time = get_current_time(opaque->mtk_opaque);
//    if(current_time < 0){
//        current_time = 0;
//    }
//
//    ALOGI("%s: current_time = %lld\n", __FUNCTION__, current_time);
//    return current_time;
//}

bool SDL_AoutAndroid_IsObjectOfAudioTrack(SDL_Aout *aout)
{
    if (!aout)
        return false;

    return aout->opaque_class == &g_audiotrack_class;
}

void SDL_Init_AoutAndroid(JNIEnv *env)
{

}