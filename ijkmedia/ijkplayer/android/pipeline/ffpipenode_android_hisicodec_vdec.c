/*
 * ffpipenode_android_hisicodec_vdec.c
 *
 * Copyright (c) 2022 Zhang Rui <bbcallen@gmail.com>
 *
 * This file is part of ijkPlayer.
 *
 * ijkPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * ijkPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with ijkPlayer; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "ffpipenode_android_hisicodec_vdec.h"
#include "ijksdl/android/ijksdl_android_jni.h"
#include "ijksdl/android/ijksdl_vout_android_nativewindow.h"
#include "ijkplayer/ff_ffpipenode.h"
#include "ijkplayer/ff_ffplay.h"
#include "ijkplayer/ff_ffplay_debug.h"
#include "ffpipeline_android.h"
#include "h264_nal.h"
#include "hevc_nal.h"

#include "hisicodec/hi_unf_avplay.h"
#include "hisicodec/hi_unf_vo.h"
#include "hisicodec/hi_unf_video.h"
#include "hisicodec/vformat.h"
#include "hisicodec/hi_unf_pdm.h"
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
// #ifdef ANDROID
// #include <cutils/properties.h>
// #endif

#define MAX_LOG_LEN           (512)
#define HDR_BUF_SIZE          1024


// missing tags
#define CODEC_TAG_VC_1            (0x312D4356)
#define CODEC_TAG_RV30            (0x30335652)
#define CODEC_TAG_RV40            (0x30345652)
#define CODEC_TAG_MJPEG           (0x47504a4d)
#define CODEC_TAG_mjpeg           (0x47504a4c)
#define CODEC_TAG_jpeg            (0x6765706a)
#define CODEC_TAG_mjpa            (0x61706a6d)

// 90kHz is an amlogic magic value, all timings are specified in a 90KHz clock.
#define AM_TIME_BASE              90000
#define AM_TIME_BASE_Q            (AVRational){1, AM_TIME_BASE}

// amcodec only tracks time using a 32 bit mask
#define AM_TIME_BITS              (32ULL)
#define AM_TIME_MASK              (0xffffffffULL)

#define PLAYER_CHK_PRINTF(val, ret, printstr) /*lint -save -e506 -e774*/ \
    do \
{ \
    if ((val)) \
    { \
        ALOGE("### [TsPlayer]: [%s:%d], %s, ret %d \n", __FILE__, __LINE__, printstr, (HI_S32)ret); \
    } \
} while (0) /*lint -restore */

HI_VOID IPTV_ADAPTER_PLAYER_LOG(HI_CHAR *pFuncName, HI_U32 u32LineNum, const HI_CHAR *format, ...);

#define PLAYER_LOGE(fmt...) \
do \
{\
    IPTV_ADAPTER_PLAYER_LOG((HI_CHAR *)__FUNCTION__, (HI_U32)__LINE__, fmt);\
}while(0)

#define PLAYER_LOGI(fmt...) \
do \
{\
    IPTV_ADAPTER_PLAYER_LOG((HI_CHAR *)__FUNCTION__, (HI_U32)__LINE__, fmt);\
}while(0)

typedef struct hiHDMI_ARGS_S
{
    HI_UNF_HDMI_ID_E  enHdmi;
}HDMI_ARGS_S;

typedef struct hdr_buf {
    char *data;
    int size;
} hdr_buf_t;

typedef struct hisi_packet {
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
} hisi_packet_t;

typedef enum {
    HISI_STREAM_UNKNOWN = 0,
    HISI_STREAM_TS,
    HISI_STREAM_PS,
    HISI_STREAM_ES,
    HISI_STREAM_RM,
    HISI_STREAM_AUDIO,
    HISI_STREAM_VIDEO,
} pstream_type;

typedef struct SHIJIU_HISI_Pipenode_Opaque {
    FFPlayer            *ffp;
    Decoder             *decoder;
    pstream_type        stream_type;
    vformat_t           video_format;
    HI_UNF_VCODEC_TYPE_E hisi_video_codec;
    unsigned int        video_codec_id;
    unsigned int        video_codec_tag;
    unsigned int        video_codec_type;
    int                 video_width;
    int                 video_height;
    HI_U32              cvrs;
//    HI_HANDLE           windowHandle;
    HI_HANDLE           avPlayHandle; 
    HI_UNF_WINDOW_ATTR_S     windowAttr;
    HI_UNF_AVPLAY_ATTR_S     avplayAttr;
    HI_UNF_SYNC_ATTR_S       syncAttr;
    int                 bReadFrameSize;
    int                 paused;
    size_t              nal_size;
    HDMI_ARGS_S         stHdmiArgs;
    int                 extrasize;
    uint8_t             *extradata;
    hisi_packet_t       hisi_pkt;

    int64_t             decode_dts;
    int64_t             decode_pts;
    int                 do_discontinuity_check;
    int64_t             pcrscr_base;
    int                 window_x;
    int                 window_y;
    int                 window_width;
    int                 window_height;
    double              fps;
    float               playback_rate;
} SHIJIU_HISI_Pipenode_Opaque;

// hisi entry
static HI_S32 (*hisi_sys_init)(HI_VOID);
static HI_S32 (*hisi_sys_deinit)(HI_VOID);
//static HI_S32 (*hisi_avplay_init)(HI_VOID);
//static HI_S32 (*hisi_avplay_deinit)(HI_VOID);
//static HI_S32 (*hisi_avplay_get_default_config)(HI_UNF_AVPLAY_ATTR_S *pstAvAttr, HI_UNF_AVPLAY_STREAM_TYPE_E enCfg);
//static HI_S32 (*hisi_avplay_chn_open)(HI_HANDLE hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_E enChn, const HI_VOID *pPara);
//static HI_S32 (*hisi_avplay_chn_close)(HI_HANDLE hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_E enChn);
//static HI_S32 (*hisi_avplay_create)(const HI_UNF_AVPLAY_ATTR_S *pstAvAttr, HI_HANDLE *phAvplay);
static HI_S32 (*hisi_avplay_get_attr)(HI_HANDLE hAvplay, HI_UNF_AVPLAY_ATTR_ID_E enAttrID, HI_VOID *pPara);
static HI_S32 (*hisi_avplay_set_attr)(HI_HANDLE hAvplay, HI_UNF_AVPLAY_ATTR_ID_E enAttrID, HI_VOID *pPara);
static HI_S32 (*hisi_avplay_start)(HI_HANDLE hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_E enChn, const HI_UNF_AVPLAY_START_OPT_S *pstStartOpt);
static HI_S32 (*hisi_avplay_get_buf)(HI_HANDLE  hAvplay, HI_UNF_AVPLAY_BUFID_E enBufId, HI_U32 u32ReqLen, HI_UNF_STREAM_BUF_S  *pstData, HI_U32 u32TimeOutMs);
static HI_S32 (*hisi_avplay_put_buf)(HI_HANDLE hAvplay, HI_UNF_AVPLAY_BUFID_E enBufId, HI_U32 u32ValidDataLen, HI_U32 u32PtsMs);
static HI_S32 (*hisi_avplay_stop)(HI_HANDLE hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_E enChn, const HI_UNF_AVPLAY_STOP_OPT_S *pstStopOpt);
//static HI_S32 (*hisi_avplay_destroy)(HI_HANDLE hAvplay);
static HI_S32 (*hisi_avplay_pause)(HI_HANDLE hAvplay, const HI_UNF_AVPLAY_PAUSE_OPT_S *pstPauseOpt);
static HI_S32 (*hisi_avplay_resume)(HI_HANDLE hAvplay, const HI_UNF_AVPLAY_RESUME_OPT_S *pstResumeOpt);
static HI_S32 (*hisi_avplay_reset)(HI_HANDLE hAvplay, const HI_UNF_AVPLAY_RESET_OPT_S *pstResetOpt);
static HI_S32 (*hisi_avplay_tplay)(HI_HANDLE hAvplay, const HI_UNF_AVPLAY_TPLAY_OPT_S *pstTplayOpt);
static HI_S32 (*hisi_avplay_flush_stream)(HI_HANDLE hAvplay, HI_UNF_AVPLAY_FLUSH_STREAM_OPT_S *pstFlushOpt);
static HI_S32 (*hisi_avplay_is_buff_empty)(HI_HANDLE hAvplay, HI_BOOL * pbIsEmpty);
static HI_S32 (*hisi_avplay_register_event)(HI_HANDLE hAvplay, HI_UNF_AVPLAY_EVENT_E enEvent, HI_UNF_AVPLAY_EVENT_CB_FN pfnEventCB);
static HI_S32 (*hisi_avplay_get_status_info)(HI_HANDLE hAvplay, HI_UNF_AVPLAY_STATUS_INFO_S *pstStatusInfo);
static HI_S32 (*hisi_avplay_set_decode_mode)(HI_HANDLE hAvplay, HI_UNF_VCODEC_MODE_E enDecodeMode);
//static HI_S32 (*hisi_vo_init)(HI_UNF_VO_DEV_MODE_E enDevMode);
//static HI_S32 (*hisi_vo_create_window)(const HI_UNF_WINDOW_ATTR_S *pWinAttr, HI_HANDLE *phWindow);
//static HI_S32 (*hisi_vo_destroy_window)(HI_HANDLE hWindow);
//static HI_S32 (*hisi_vo_deinit)(HI_VOID);
//static HI_S32 (*hisi_vo_attach_window)(HI_HANDLE hWindow, HI_HANDLE hSrc);
//static HI_S32 (*hisi_vo_detach_window)(HI_HANDLE hWindow, HI_HANDLE hSrc);
static HI_S32 (*hisi_vo_set_window_enable)(HI_HANDLE hWindow, HI_BOOL bEnable);
static HI_S32 (*hisi_vo_set_window_attr)(HI_HANDLE hWindow, const HI_UNF_WINDOW_ATTR_S* pWinAttr);
static HI_S32 (*hisi_disp_init)(HI_VOID);
static HI_S32 (*hisi_disp_deinit)(HI_VOID);
static HI_S32 (*hisi_disp_open)(HI_UNF_DISP_E enDisp);
static HI_S32 (*hisi_disp_close)(HI_UNF_DISP_E enDisp);
static int (*property_get)(const char *key, char *value, const char *default_value);

// forward decls
static void func_destroy(IJKFF_Pipenode *node);
static int func_flush(IJKFF_Pipenode *node);
static int func_run_sync(IJKFF_Pipenode *node);
static void syncToMaster(SHIJIU_HISI_Pipenode_Opaque *opaque);
static int hisi_codec_init();
static HI_S32 hisi_codec_disp_init(SHIJIU_HISI_Pipenode_Opaque *opaque);
//HI_S32 hisi_codec_vo_init(HI_UNF_VO_DEV_MODE_E enDevMode);
static HI_VOID __GetVoAspectCvrs(SHIJIU_HISI_Pipenode_Opaque *opaque);
static void  hisi_codec_window_attr_init(HI_UNF_WINDOW_ATTR_S* pWinAttr, int VoType, SHIJIU_HISI_Pipenode_Opaque *opaque);
static HI_UNF_VCODEC_TYPE_E hisi_codecid_to_vformat(enum AVCodecID id);
static vformat_t codecid_to_vformat(enum AVCodecID id);
static vdec_type_t codec_tag_to_vdec_type(unsigned int codec_tag);
static int hisi_codec_create(SHIJIU_HISI_Pipenode_Opaque *opaque); 
HI_S32 hisi_codec_setVdecAttr(HI_HANDLE hAvplay,HI_UNF_VCODEC_TYPE_E enType,HI_UNF_VCODEC_MODE_E enMode);
static int feed_input_buffer(JNIEnv *env, IJKFF_Pipenode *node, int *enqueue_count);
static int reset(SHIJIU_HISI_Pipenode_Opaque *opaque);
static void hisi_codec_register_event(HI_HANDLE g_hAvplay);
static HI_S32 hisi_codec_evnet_handler(HI_HANDLE handle, HI_UNF_AVPLAY_EVENT_E enEvent, HI_U32 para);
static int pre_header_feeding(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt);


//视频协议相关header添加，和芯片平台无关，只和编解码协议有关。
static int write_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt);
static int hisi_codec_write_data(SHIJIU_HISI_Pipenode_Opaque *opaque, HI_U8 * buffer, int size);
static int write_av_packet(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt);
static int h264_write_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt);
static int set_header_info(SHIJIU_HISI_Pipenode_Opaque *para);


//音视频同步相关
static int64_t getAudioPts(FFPlayer *ffp, VideoState  *is);
static int64_t hisi_time_sub(int64_t a, int64_t b);
static int64_t hisi_time_sub_32(int64_t a, int64_t b);
static int64_t get_pts_pcrscr(SHIJIU_HISI_Pipenode_Opaque *opaque);
static int set_pts_pcrscr(SHIJIU_HISI_Pipenode_Opaque *opaque, int64_t value);

static int hisi_not_present = 0;
static int hisi_so_loaded = 0;

#ifdef HI_HDCP_SUPPORT
const HI_CHAR * pstencryptedHdcpKey = "EncryptedKey_332bytes.bin";
#endif
// static android::OsdManager *g_om = HI_NULL;
HI_UNF_HDMI_CALLBACK_FUNC_S g_stCallbackFunc;

HI_VOID IPTV_ADAPTER_PLAYER_LOG(HI_CHAR *pFuncName, HI_U32 u32LineNum, const HI_CHAR *format, ...)
{
    HI_CHAR     LogStr[MAX_LOG_LEN] = {0};
    va_list     args;
    HI_S32      LogLen;

    va_start(args, format);
    LogLen = vsnprintf(LogStr, MAX_LOG_LEN, format, args);
    va_end(args);
    LogStr[MAX_LOG_LEN-1] = '\0';

    ALOGE("%s[%d]:%s", pFuncName, u32LineNum, LogStr);

    return ;
}

static void * find_symbol(const char * sym)
{
  void * addr = dlsym(RTLD_DEFAULT, sym);
  if(!addr)
    hisi_not_present = 1;
  return addr;
}

static void set_clock_at(Clock *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

static void hisi_packet_init(SHIJIU_HISI_Pipenode_Opaque *opaque)
{
    hisi_packet_t *pkt = &(opaque->hisi_pkt);

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

    set_pts_pcrscr(opaque, 0);
}

void hisi_packet_release(hisi_packet_t *pkt)
{
  if (pkt->buf != NULL)
    free(pkt->buf), pkt->buf= NULL;
  if (pkt->hdr != NULL)
  {
    if (pkt->hdr->data != NULL)
      free(pkt->hdr->data), pkt->hdr->data = NULL;
    free(pkt->hdr), pkt->hdr = NULL;
  }

  //pkt->codec = NULL;
}

static void loadLibrary()
{
    // We use Java to call System.loadLibrary as dlopen is weird on Android.
    if(hisi_so_loaded){
        return;
    }

    void * lib = dlopen("libplayer.so", RTLD_NOW);
    if(!lib) {
      ALOGD("hisi library did not load.\n");
      hisi_not_present = 1;
      return;
    }

    hisi_sys_init                      = find_symbol("HI_SYS_Init");
    hisi_sys_deinit                    = find_symbol("HI_SYS_DeInit");
//    hisi_avplay_init                   = find_symbol("HI_UNF_AVPLAY_Init");
//    hisi_avplay_deinit                 = find_symbol("HI_UNF_AVPLAY_DeInit");
//    hisi_avplay_get_default_config     = find_symbol("HI_UNF_AVPLAY_GetDefaultConfig");
//    hisi_avplay_create                 = find_symbol("HI_UNF_AVPLAY_Create");
//    hisi_avplay_chn_open               = find_symbol("HI_UNF_AVPLAY_ChnOpen");
//    hisi_avplay_chn_close              = find_symbol("HI_UNF_AVPLAY_ChnClose");
    hisi_avplay_get_attr               = find_symbol("HI_UNF_AVPLAY_GetAttr");
    hisi_avplay_set_attr               = find_symbol("HI_UNF_AVPLAY_SetAttr");
    hisi_avplay_start                  = find_symbol("HI_UNF_AVPLAY_Start");
    hisi_avplay_get_buf                = find_symbol("HI_UNF_AVPLAY_GetBuf");
    hisi_avplay_put_buf                = find_symbol("HI_UNF_AVPLAY_PutBuf");
    hisi_avplay_stop                   = find_symbol("HI_UNF_AVPLAY_Stop");
//    hisi_avplay_destroy                = find_symbol("HI_UNF_AVPLAY_Destroy");
    hisi_avplay_pause                  = find_symbol("HI_UNF_AVPLAY_Pause");
    hisi_avplay_resume                 = find_symbol("HI_UNF_AVPLAY_Resume");
    hisi_avplay_reset                  = find_symbol("HI_UNF_AVPLAY_Reset");
    hisi_avplay_tplay                  = find_symbol("HI_UNF_AVPLAY_Tplay");
    hisi_avplay_flush_stream           = find_symbol("HI_UNF_AVPLAY_FlushStream");
    hisi_avplay_is_buff_empty          = find_symbol("HI_UNF_AVPLAY_IsBuffEmpty");
    hisi_avplay_register_event         = find_symbol("HI_UNF_AVPLAY_RegisterEvent");
    hisi_avplay_get_status_info        = find_symbol("HI_UNF_AVPLAY_GetStatusInfo");
    hisi_avplay_set_decode_mode        = find_symbol("HI_UNF_AVPLAY_SetDecodeMode");
//    hisi_vo_init                       = find_symbol("HI_UNF_VO_Init");
//    hisi_vo_create_window              = find_symbol("HI_UNF_VO_CreateWindow");
//    hisi_vo_destroy_window             = find_symbol("HI_UNF_VO_DestroyWindow");
//    hisi_vo_deinit                     = find_symbol("HI_UNF_VO_DeInit");
//    hisi_vo_attach_window              = find_symbol("HI_UNF_VO_AttachWindow");
//    hisi_vo_detach_window              = find_symbol("HI_UNF_VO_DetachWindow");
    hisi_vo_set_window_enable          = find_symbol("HI_UNF_VO_SetWindowEnable");
    hisi_vo_set_window_attr            = find_symbol("HI_UNF_VO_SetWindowAttr");
//    hisi_disp_init                     = find_symbol("HI_UNF_DISP_Init");
//    hisi_disp_deinit                   = find_symbol("HI_UNF_DISP_DeInit");
//    hisi_disp_open                     = find_symbol("HI_UNF_DISP_Open");
//    hisi_disp_close                    = find_symbol("HI_UNF_DISP_Close");
    property_get                       = find_symbol("property_get");

    hisi_so_loaded = 1;
}

IJKFF_Pipenode *ffpipenode_create_video_decoder_from_android_hisicodec(FFPlayer *ffp)
{
    if (!ffp || !ffp->is)
        return NULL;

    ALOGI("hisicodec initializing.\n");

    loadLibrary();

    if(hisi_not_present) {
        return NULL;
    }

    ALOGD("hisi library loads successfully\n");

    IJKFF_Pipenode *node = ffpipenode_alloc(sizeof(SHIJIU_HISI_Pipenode_Opaque));
    if (!node)
        return node;

    VideoState            *is     = ffp->is;

    SHIJIU_HISI_Pipenode_Opaque *opaque = node->opaque;
    node->func_destroy  = func_destroy;
    node->func_run_sync = func_run_sync;
    node->func_flush    = func_flush;

    //opaque->pipeline    = pipeline;
    opaque->ffp         = ffp;
    opaque->decoder     = &is->viddec;
    opaque->cvrs        = 1;
//    opaque->windowHandle = ffp->window_handle;
    opaque->avPlayHandle = ffp->window_handle;
    opaque->stream_type = HISI_STREAM_ES;

    AVCodecContext * avctx = opaque->decoder->avctx;
    opaque->video_codec_id = avctx->codec_id; //AV_CODEC_ID_H264;
    opaque->video_codec_tag = avctx->codec_tag; //CODEC_TAG_AVC1;

    opaque->video_format = codecid_to_vformat(avctx->codec_id); //VFORMAT_H264
    opaque->hisi_video_codec = hisi_codecid_to_vformat(avctx->codec_id); 
    if(opaque->video_format < 0)
        return NULL;

    opaque->video_codec_type = codec_tag_to_vdec_type(avctx->codec_tag);  //VIDEO_DEC_FORMAT_H264;

    opaque->video_width = avctx->width;
    opaque->video_height = avctx->height;
    opaque->bReadFrameSize = HI_FALSE;
    opaque->paused = HI_FALSE;

    opaque->decode_dts = 0;
    opaque->decode_pts = 0;
    opaque->window_x = 0;
    opaque->window_x = 0;
    opaque->window_width = 0;
    opaque->window_height = 0;
    opaque->fps = av_q2d(opaque->ffp->is->video_st->avg_frame_rate);
    opaque->playback_rate = 1.0f;

    hisi_packet_init(opaque);

    // avc1 is the codec tag when h264 is embedded in an mp4 and needs the stupid
    // nal_size and extradata stuff processed.
    if(avctx->codec_tag == CODEC_TAG_AVC1 || avctx->codec_tag == CODEC_TAG_avc1 ||
       avctx->codec_tag == CODEC_TAG_hvc1 || avctx->codec_tag == CODEC_TAG_hev1) {

        ALOGD("stream is avc1/hvc1, fixing sps/pps. extrasize:%d\n", avctx->extradata_size);

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

    hisi_codec_init(opaque);
    hisi_codec_create(opaque);

    pre_header_feeding(opaque, &opaque->hisi_pkt);

    ffp->stat.vdec_type = FFP_PROPV_DECODER_HISILICON;

    return node;
}

static void func_destroy(IJKFF_Pipenode *node)
{
    if (!node || !node->opaque)
        return;

    SHIJIU_HISI_Pipenode_Opaque *opaque   = node->opaque;
    HI_HANDLE                   hAvplay   = opaque->avPlayHandle;
//    HI_HANDLE                   hWindow   = opaque->windowHandle;

    if(hAvplay != HI_INVALID_HANDLE){
        hisi_avplay_stop(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID, NULL);
    }
//    if(hWindow != HI_INVALID_HANDLE){
//        hisi_vo_set_window_enable(hWindow, HI_FALSE);
//        hisi_vo_detach_window(hWindow, hAvplay);
//        hisi_vo_destroy_window(hWindow);
//    }
//    if(hAvplay != HI_INVALID_HANDLE){
//        hisi_avplay_chn_close(hAvplay, HI_UNF_AVPLAY_MEDIA_CHAN_VID);
//        hisi_avplay_destroy(hAvplay);
//    }
    hisi_packet_release(&opaque->hisi_pkt);
//    hisi_avplay_deinit();
//    hisi_vo_deinit();
//    hisi_disp_deinit();
    hisi_sys_deinit();
}

static int func_flush(IJKFF_Pipenode *node)
{
    return 0;
}

static void req_reset(SHIJIU_HISI_Pipenode_Opaque *opaque ) {
    FFPlayer              *ffp      = opaque->ffp;
    VideoState            *is       = ffp->is;

    ALOGD("hisi codec req_reset!");

// mengxk removed >>>
//    is->reset_req = 1;
// mengxk removed <<<
}

static int reset(SHIJIU_HISI_Pipenode_Opaque *opaque) 
{
    HI_S32 s32Ret = HI_SUCCESS;
    ALOGD("hisicodec reset!\n");

    s32Ret = hisi_avplay_reset(opaque->avPlayHandle, NULL);
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_Reset failed");

    s32Ret = hisi_avplay_resume(opaque->avPlayHandle, NULL);
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_Resume failed");

    hisi_packet_release(&opaque->hisi_pkt);
    hisi_packet_init(opaque);
    pre_header_feeding(opaque, &opaque->hisi_pkt);

    opaque->decode_dts = 0;
    opaque->decode_pts = 0;
    opaque->do_discontinuity_check = 0;


    return s32Ret;
}

static int feed_input_buffer(JNIEnv *env, IJKFF_Pipenode *node, int *enqueue_count)
{
    SHIJIU_HISI_Pipenode_Opaque *opaque   = node->opaque;
    FFPlayer                    *ffp      = opaque->ffp;
    VideoState                  *is       = ffp->is;
    Decoder                     *d        = &is->viddec;
    AVCodecContext              *avctx    = d->avctx;
    HI_HANDLE                   hAvplay   = opaque->avPlayHandle;
    hisi_packet_t * hisi_pkt = &(opaque->hisi_pkt);
    
    HI_U32                      readlen;
    HI_S32                      ret = 0;
    HI_U32                      frameSize = 0;
    static int                  counter=0;

    write_header(opaque, hisi_pkt);

    if (enqueue_count)
        *enqueue_count = 0;

    if (d->queue->abort_request) {
        ret = 0;
        goto fail;
    }

    if (is->paused) {
        if(!opaque->paused) {
            ALOGD("hisicodec pausing!\n");
            hisi_avplay_pause(hAvplay, NULL);
            opaque->paused = true;
        }

        usleep(1000000 / 30);   // Hmmm, is there a condwait for resuming?
        return 0;
    } else {
        if(opaque->paused) {
            ALOGE("hisicodec resuming!\n");
            hisi_avplay_resume(hAvplay, NULL);
            opaque->paused = false;
        }
    }

    if (!d->packet_pending || d->queue->serial != d->pkt_serial) {
        AVPacket pkt;
        do {
            if (d->queue->nb_packets == 0)
                SDL_CondSignal(d->empty_queue_cond);
            if (ffp_packet_queue_get_or_buffering(ffp, d->queue, &pkt, &d->pkt_serial, &d->finished) < 0) {
                ret = -1;
                goto fail;
            }
            if (ffp_is_flush_packet(&pkt)) {
                ALOGD("hisicodec flush packet\n");

                reset(opaque);

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

        if(dts <= opaque->decode_dts)
            dts = opaque->decode_dts + 1;

        if (pts == AV_NOPTS_VALUE)
          pts = dts;

        if (opaque->do_discontinuity_check) {

          // on a discontinuity we need to wait for all frames to popout of the decoder.
          bool discontinuity_dts = opaque->decode_dts != 0 && abs(opaque->decode_dts - dts) > AM_TIME_BASE;
          bool discontinuity_pts = opaque->decode_pts != 0 && abs(opaque->decode_pts - pts) > AM_TIME_BASE;
          if(discontinuity_dts) {
            int64_t pts_pcrscr = get_pts_pcrscr(opaque);

            // wait for pts_scr to hit the last decode_dts
            while(hisi_time_sub(pts_pcrscr, opaque->decode_dts) < 0) {
              ALOGD("clock dts discontinuity: ptsscr:%jd decode_dts:%jd dts:%jd", pts_pcrscr, opaque->decode_dts, dts);

              usleep(1000000/30);

              pts_pcrscr = get_pts_pcrscr(opaque);
            }

            req_reset(opaque);
          } else if(discontinuity_pts) {
            int64_t pts_pcrscr = get_pts_pcrscr(opaque);

            // wait for pts_scr to hit the last decode_pts
            while(hisi_time_sub(pts_pcrscr, opaque->decode_pts) < 0) {
              ALOGD("clock pts discontinuity: ptsscr:%jd decode_pts:%jd next_pts:%jd", pts_pcrscr, opaque->decode_pts, pts);

              usleep(1000000/30);

              pts_pcrscr = get_pts_pcrscr(opaque);
            }

            req_reset(opaque);
          }
        }


        opaque->decode_dts = dts;
        opaque->decode_pts = pts;

#if TIMING_DEBUG
        ALOGD("queued dts:%llu pts:%llu\n", dts, pts);
#endif

        if (!is->viddec.first_frame_decoded) {
            ALOGD("Video: first frame decoded\n");
            ffp_notify_msg1(ffp, FFP_MSG_VIDEO_DECODED_START);
            is->viddec.first_frame_decoded_time = SDL_GetTickHR();
            is->viddec.first_frame_decoded = 1;

            double time = av_gettime_relative() / 1000000.0;
            set_clock_at(&is->vidclk, pts / 90000.0, d->pkt_serial, time);
        }

        //if((pts - last_pts) > 300

        //if(pts_jump) {
        //    if (pts != AV_NOPTS_VALUE) {
        //        double time = av_gettime_relative() / 1000000.0;
        //        set_clock_at(&is->vidclk, pts / 90000.0, d->pkt_serial, time);
        //    }
        //}

        hisi_pkt->data       = d->pkt_temp.data;
        hisi_pkt->data_size  = d->pkt_temp.size;
        hisi_pkt->newflag    = 1;
        hisi_pkt->isvalid    = 1;
        hisi_pkt->avduration = 0;
        hisi_pkt->avdts      = dts;
        hisi_pkt->avpts      = pts;

        // some formats need header/data tweaks.
        // the actual write occurs once in write_av_packet
        // and is controlled by am_pkt.newflag.
        set_header_info(opaque);

        // loop until we write all into codec, am_pkt.isvalid
        // will get set to zero once everything is consumed.
        // PLAYER_SUCCESS means all is ok, not all bytes were written.
        int loop = 0;
        if(hisi_pkt->isvalid) {
            // abort on any errors.
            ALOGD("write_av_packet , pkt size =%d\n", hisi_pkt->data_size);
            if (ret = write_av_packet(opaque, hisi_pkt) != HI_SUCCESS){
                ALOGD("write_av_packet fail, ret =%d\n", ret);
            }
        }

        if (!ffp->first_video_frame_rendered && (d->pkt_temp.flags&0x1)) {
            int64_t current_t = av_gettime();
            ffp->first_video_frame_rendered = 1;
            if(!is->ic) {
                ALOGE("no ic\n");
            } else {
                ffp->stat.bit_rate = is->ic->bit_rate;
//mengxk modified >>>
//            }else if(!is->ic->pb){
//                ALOGD("avformat bytes read: %lld\n", is->ic->bytes_read);
//                ffp->stat.bit_rate = is->ic->bytes_read*8*1000000/(current_t-is->start_download_time);
//            }else{
//                ALOGD("bytes read: %lld\n", is->ic->pb->bytes_read);
//                ffp->stat.bit_rate = is->ic->pb->bytes_read*8*1000000/(current_t-is->start_download_time);
//mengxk modified <<<
            }
            ffp_notify_msg1(ffp, FFP_MSG_VIDEO_RENDERING_START);
            // ALOGE("bitrate: %jd\n", ffp->stat.bit_rate);
        }

        d->pkt_temp.dts = AV_NOPTS_VALUE;
        d->pkt_temp.pts = AV_NOPTS_VALUE;

        d->packet_pending = 0;
    }else{
        ALOGD("finished\n");
        d->finished = d->pkt_serial;
    }

 fail:
    return ret;
}

static void modifyRect(SHIJIU_HISI_Pipenode_Opaque *opaque){
    FFPlayer              *ffp      = opaque->ffp;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
// mengxk removed >>>
//    if(ffp->rect != NULL){
//        // ALOGD("modifyRect: rect = %s\n", ffp->rect);
//        int n=sscanf(ffp->rect, "%d/%d/%d/%d", &x, &y, &width, &height);
//        if(x != opaque->window_x || y != opaque->window_y || width != opaque->window_width || height != opaque->window_height){
//            ALOGD("modifyRect: x(%d), y(%d), width(%d), height(%d)\n", x, y, width, height);
//            opaque->window_x = x;
//            opaque->window_y = y;
//            opaque->window_width = width;
//            opaque->window_height = height;
//            if(opaque->windowHandle != HI_INVALID_HANDLE){
//                opaque->windowAttr.stInputRect.s32X = 0;
//                opaque->windowAttr.stInputRect.s32Y = 0;
//                opaque->windowAttr.stInputRect.s32Width = opaque->video_width;
//                opaque->windowAttr.stInputRect.s32Height = opaque->video_height;
//                opaque->windowAttr.stOutputRect.s32X = x;
//                opaque->windowAttr.stOutputRect.s32Y = y;
//                opaque->windowAttr.stOutputRect.s32Width = width;
//                opaque->windowAttr.stOutputRect.s32Height = height;
//                hisi_vo_set_window_attr(opaque->windowHandle, &(opaque->windowAttr));
//            }
//        }
//    }
// mengxk removed <<<
}

static void modifyPlayBackRate(SHIJIU_HISI_Pipenode_Opaque *opaque){
    FFPlayer              *ffp      = opaque->ffp;
    HI_S32 ret = HI_SUCCESS;

    if(ffp->pf_playback_rate != opaque->playback_rate){
        double cur_fps = opaque->fps*ffp->pf_playback_rate;

        opaque->playback_rate = ffp->pf_playback_rate;

        HI_UNF_AVPLAY_FRMRATE_PARAM_S stFramerate;
        stFramerate.enFrmRateType = HI_UNF_AVPLAY_FRMRATE_TYPE_USER;

        stFramerate.stSetFrmRate.u32fpsInteger = (HI_U32)cur_fps;
        stFramerate.stSetFrmRate.u32fpsDecimal = (HI_U32)((cur_fps - stFramerate.stSetFrmRate.u32fpsInteger) * 1000);
        hisi_avplay_set_attr(opaque->avPlayHandle, HI_UNF_AVPLAY_ATTR_ID_FRMRATE_PARAM, &stFramerate);

        // if(opaque->playback_rate == 1.0f){
        //     hisi_avplay_set_decode_mode(opaque->avPlayHandle, HI_UNF_VCODEC_MODE_NORMAL);
        //     hisi_avplay_resume(opaque->avPlayHandle, HI_NULL);
        // }else{
        //     HI_UNF_AVPLAY_TPLAY_OPT_S stTplayOpt;
        //     stTplayOpt.enTplayDirect = HI_UNF_AVPLAY_TPLAY_DIRECT_FORWARD;
        //     stTplayOpt.u32SpeedInteger = (HI_U32)opaque->playback_rate;
        //     stTplayOpt.u32SpeedDecimal = (HI_U32)((opaque->playback_rate - stTplayOpt.u32SpeedInteger) * 1000);

        //     // hisi_avplay_pause(opaque->avPlayHandle, NULL);
        //     if(opaque->playback_rate > 1.5f){
        //         hisi_avplay_set_decode_mode(opaque->avPlayHandle, HI_UNF_VCODEC_MODE_IP);
        //     }

        //     ret = hisi_avplay_tplay(opaque->avPlayHandle, &stTplayOpt);
            
        //     ALOGD("HI_UNF_AVPLAY_Tplay2 = %d\n", ret);

        //     // hisi_avplay_resume(opaque->avPlayHandle,  NULL);
        // }
    }
}

static int func_run_sync(IJKFF_Pipenode *node)
{
    JNIEnv                *env      = NULL;
    SHIJIU_HISI_Pipenode_Opaque *opaque   = node->opaque;
    FFPlayer              *ffp      = opaque->ffp;
    VideoState            *is       = ffp->is;
    Decoder               *d        = &is->viddec;
    PacketQueue           *q        = d->queue;
    int                    ret      = 0;
    int                    dequeue_count = 0;

    ALOGD("hisicodec: func_run_sync\n");
    while (!q->abort_request) {
        //
        // Send a packet (an access unit) to the decoder
        //
        ALOGD("hisicodec: feed_input_buffer\n");
        ret = feed_input_buffer(env, node, &dequeue_count);
        if (ret != 0) {
// mengxk modified >>>
            continue;
//            goto fail;
// mengxk modified <<<
        }

        
        modifyRect(opaque);
        modifyPlayBackRate(opaque);
        //
        // If we couldn't read a packet, it's probably because we're paused or at
        // the end of the stream, so sleep for a bit and resume.
        //
        if(dequeue_count == 0 || is->paused) {
            usleep(1000000/100);
            //ALOGD("no dequeue");
            continue;
        }

        //
        // Synchronize the video to the master clock
        //
        // syncToMaster(opaque);
    }
fail:
    ALOGD("hisicodec: func_run_sync goto end\n");
    return -1;
}

static void syncToMaster(SHIJIU_HISI_Pipenode_Opaque *opaque)
{
    FFPlayer     *ffp = opaque->ffp;
    VideoState   *is  = ffp->is;
    Decoder      *d   = &is->viddec;
    PacketQueue  *q        = d->queue;

    //
    //
    // We have our amlogic video running. It presents frames (that we can't access) according
    // to pts values. It uses its own clock, pcrscr, that we can read and modify.
    //
    // ijkplayer is then presenting audio according to its own audio clock, which has some pts
    // time associated with that too.
    //
    // is->audclk gives the audio pts at the last packet played (or as close as possible).
    //
    // We adjust pcrscr so that it matches audclk, but we don't want to do that instantaneously
    // every frame because then the video jitters.
    //
    //

    int64_t pts_audio = getAudioPts(ffp, is); // current pcr-scr base from ptr-audio

    int64_t last_pts = opaque->decode_pts;

    // prevent decoder/demux from getting too far ahead of video
    int slept = 0;
    while(last_pts > 0 && (hisi_time_sub_32(last_pts, pts_audio) > 90000*2)) {
        if(q->abort_request){
            return;
        }
        
        usleep(1000000/100);
        slept += 1;
        if(slept >= 100) {
            ALOGD("slept:%d pts_audio:%jd decode_dts:%jd", slept, pts_audio, last_pts);
            slept = 0;
            break;
        }
        pts_audio = getAudioPts(ffp, is);
    }

    // pts_audio = getAudioPts(ffp, is);

    // int64_t pts_pcrscr = get_pts_pcrscr(opaque);  // think this the master clock time for amcodec output
    // int64_t delta = hisi_time_sub(pts_audio, pts_pcrscr);

    // // // modify pcrscr so that the frame presentation times are adjusted 'instantly'
    // // if(is->audclk.serial == d->pkt_serial && d->first_frame_decoded) {
    //     int threshhold = 9000;
    //     if (ffp->pf_playback_rate != 1.0) {
    //         threshhold = 1000;
    //     }
    //     if(abs(delta) > threshhold) {
    //         opaque->pcrscr_base = pts_pcrscr + delta;
    //         set_pts_pcrscr(opaque, pts_pcrscr + delta);
    //         ALOGD("set pcr %jd, %jd, %jd!", pts_pcrscr, delta, pts_pcrscr + delta);
    //     }

    //     opaque->do_discontinuity_check = 1;
    // }

// #if TIMING_DEBUG
//     ALOGD("pts_audio:%jd pts_pcrscr:%jd delta:%jd offset:%jd last_pts:%jd slept:%d sync_master:%d",
//         pts_audio, pts_pcrscr, delta, offset, last_pts, slept, get_master_sync_type(is));
// #endif
}

static int64_t getAudioPts(FFPlayer *ffp, VideoState  *is) {
    int64_t pts_audio = is->audclk.pts * 90000.0;

    // since audclk.pts was recorded time has advanced so take that into account.
    int64_t offset = av_gettime_relative() - ffp->audio_callback_time;
    offset = offset * 9/100;    // convert 1000KHz counter to 90KHz
    pts_audio += offset;

    pts_audio += 10000;    // Magic!

    return pts_audio;
}

static int64_t hisi_time_sub(int64_t a, int64_t b)
{
    // int64_t shift = 64 - AM_TIME_BITS;
    // return ((a - b) << shift) >> shift;
    return a - b;
}

// 这个sub只拿后32位
static int64_t hisi_time_sub_32(int64_t a, int64_t b)
{
    int64_t shift = 64 - AM_TIME_BITS;
    return ((a - b) << shift) >> shift;
}

// 拿origin_time的后32位
static int64_t get_32bit_time(int64_t origin_time) {
    return origin_time & 0x00000000ffffffff;
}

static void printSysInfo(HI_UNF_SYNC_STATUS_S *pstSynInfo){
    if(pstSynInfo != NULL){
        ALOGD("Avplay Syn info: \r\ns32DiffAvPlayTime: %d\r\nu32FirstAudPts: %d\r\nu32FirstVidPts: %d\r\nu32LastAudPts: %d\r\nu32LastVidPts: %d\r\nu32LocalTime: %d\r\nu32PlayTime: %d\n", 
            pstSynInfo->s32DiffAvPlayTime, pstSynInfo->u32FirstAudPts, pstSynInfo->u32FirstVidPts,
            pstSynInfo->u32LastAudPts, pstSynInfo->u32LastVidPts, pstSynInfo->u32LocalTime, pstSynInfo->u32PlayTime);
    }
}

// 因为pts_pcrscr是32位的，而原数据是64位。所以将这个数据的低32位和读的pts_pcrsrc进行比较，得到offset，
// 然后再加到原来的数据上，恢复为一个64位的数据返回并保存下来，供下次使用。
static int64_t get_pts_pcrscr(SHIJIU_HISI_Pipenode_Opaque *opaque) {
    HI_S32 ret;
    HI_UNF_AVPLAY_STATUS_INFO_S pstStatusInfo;
    
    ret = hisi_avplay_get_status_info(opaque->avPlayHandle, &pstStatusInfo);
    if (ret == HI_SUCCESS) {
        printSysInfo(&(pstStatusInfo.stSyncStatus));
        unsigned long pts = pstStatusInfo.stSyncStatus.u32LastVidPts;
        int64_t pcrscr_32bit = get_32bit_time(opaque->pcrscr_base);
        if (pcrscr_32bit - (int64_t) pts > 0) {
            // looped
            opaque->pcrscr_base = opaque->pcrscr_base + (((int64_t)1) << AM_TIME_BITS + (int64_t)pts - pcrscr_32bit);
        } else {
            opaque->pcrscr_base = opaque->pcrscr_base + ((int64_t)pts - pcrscr_32bit);
        }

        return opaque->pcrscr_base;
    }else {
        ALOGE("get_pts_pcrscr error\n");
    }

    
    return ret;
}

static int set_pts_pcrscr(SHIJIU_HISI_Pipenode_Opaque *opaque, int64_t value)
{
    HI_S32 ret;

    ALOGD("set_pts_pcrscr: %lld\n", value);

    ret = hisi_avplay_get_attr(opaque->avPlayHandle, HI_UNF_AVPLAY_ATTR_ID_SYNC, &(opaque->syncAttr));
    PLAYER_CHK_PRINTF((HI_SUCCESS != ret), ret, "Call HI_UNF_AVPLAY_GetAttr:ID_SYNC failed");

    // opaque->syncAttr.enSyncRef = HI_UNF_SYNC_REF_NONE;
    opaque->syncAttr.enSyncRef = HI_UNF_SYNC_REF_PCR;
    opaque->syncAttr.stSyncStartRegion.s32VidPlusTime = 60;
    opaque->syncAttr.stSyncStartRegion.s32VidNegativeTime = -20;
    opaque->syncAttr.stSyncStartRegion.bSmoothPlay = HI_TRUE;
    opaque->syncAttr.s32VidPtsAdjust = value;
    ret = hisi_avplay_set_attr(opaque->avPlayHandle, HI_UNF_AVPLAY_ATTR_ID_SYNC, &(opaque->syncAttr));
    PLAYER_CHK_PRINTF((HI_SUCCESS != ret), ret, "Call HI_UNF_AVPLAY_SetAttr:ID_SYNC failed");
    
    return ret;
}


static int hisi_codec_init(SHIJIU_HISI_Pipenode_Opaque *opaque){
    HI_S32 ret;

    hisi_sys_init();

    // ret = hisi_codec_disp_init(opaque);
    // if(ret != HI_SUCCESS){
    //     ALOGE("hisi display init failed\n");
    // }

//    ret = hisi_codec_vo_init(HI_UNF_VO_DEV_MODE_NORMAL);
//    if (ret != HI_SUCCESS)
//    {
//        ALOGE("hisi vo init failed\n");
//    }

//    ret = hisi_avplay_init();
//    if(ret != HI_SUCCESS){
//        ALOGE("hisi avplay init failed, ret = %d\n", ret);
//    }

    __GetVoAspectCvrs(opaque);
    hisi_codec_window_attr_init(&opaque->windowAttr, HI_UNF_DISPLAY1, opaque);
    
init_faild:
    return 0;
}

HI_S32 hisi_codec_disp_init(SHIJIU_HISI_Pipenode_Opaque *opaque)
{
    HI_S32                      Ret;
    HI_UNF_DISP_BG_COLOR_S      BgColor;
    HI_UNF_DISP_INTF_S          stIntf[2];
    HI_UNF_DISP_OFFSET_S        offset;
    HI_UNF_ENC_FMT_E            enFormat = HI_UNF_ENC_FMT_1080P_50;
    HI_UNF_ENC_FMT_E            SdFmt = HI_UNF_ENC_FMT_PAL;

    // Ret = hisi_disp_init();
    // if (Ret != HI_SUCCESS)
    // {
    //     ALOGE("call HI_UNF_DISP_Init failed. ret = %d\n", Ret);
    //     return Ret;
    // }

   
    // Ret = hisi_disp_open(HI_UNF_DISPLAY1);
    // if (Ret != HI_SUCCESS)
    // {
    //     ALOGE("call HI_UNF_DISP_Open DISPLAY1 failed, Ret=%#x.\n", Ret);
    //     hisi_disp_detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
    //     hisi_disp_deinit();
    //     return Ret;
    // }

    // Ret = hisi_disp_open(HI_UNF_DISPLAY0);
    // if (Ret != HI_SUCCESS)
    // {
    //     ALOGE("call HI_UNF_DISP_Open DISPLAY0 failed, Ret=%#x.\n", Ret);
    //     hisi_disp_close(HI_UNF_DISPLAY1);
    //     hisi_disp_detach(HI_UNF_DISPLAY0, HI_UNF_DISPLAY1);
    //     hisi_disp_deinit();
    //     return Ret;
    // }

    return HI_SUCCESS;
}

//HI_S32 hisi_codec_vo_init(HI_UNF_VO_DEV_MODE_E enDevMode)
//{
//    HI_S32             Ret;
//
//
//    Ret = hisi_vo_init(enDevMode);
//    if (Ret != HI_SUCCESS)
//    {
//        ALOGE("call HI_UNF_VO_Init failed. ret = 0x%x\n", Ret);
//        return Ret;
//    }
//
//    return HI_SUCCESS;
//}

static HI_VOID __GetVoAspectCvrs(SHIJIU_HISI_Pipenode_Opaque *opaque)
{
    char buffer[1024];

    memset(buffer, 0, 1024);
    property_get("persist.sys.video.cvrs", buffer, "1");
    opaque->cvrs = atoi(buffer);

    ALOGI("### CTsPlayer::__GetVoAspectCvrs(%d)\n", opaque->cvrs);

    return;
}

static void  hisi_codec_window_attr_init(HI_UNF_WINDOW_ATTR_S* pWinAttr, int VoType, SHIJIU_HISI_Pipenode_Opaque *opaque)
{
    pWinAttr->enDisp = VoType;
    pWinAttr->bVirtual        = HI_FALSE;
    pWinAttr->stWinAspectAttr.enAspectCvrs = (HI_UNF_VO_ASPECT_CVRS_E)opaque->cvrs;//HI_UNF_VO_ASPECT_CVRS_IGNORE;
    pWinAttr->stWinAspectAttr.bUserDefAspectRatio = HI_FALSE;
    pWinAttr->stWinAspectAttr.u32UserAspectWidth  = 0;
    pWinAttr->stWinAspectAttr.u32UserAspectHeight = 0;
    pWinAttr->bUseCropRect = HI_FALSE;
    pWinAttr->stInputRect.s32X = 0;//opaque->window_x;
    pWinAttr->stInputRect.s32Y = 0;//opaque->window_y;
    pWinAttr->stInputRect.s32Width = opaque->video_width;//opaque->window_width;
    pWinAttr->stInputRect.s32Height = opaque->video_height;//opaque->window_height;
    memset(&(pWinAttr->stOutputRect), 0x0, sizeof(HI_RECT_S));
    pWinAttr->stOutputRect.s32X = opaque->window_x;
    pWinAttr->stOutputRect.s32Y = opaque->window_y;
    pWinAttr->stOutputRect.s32Width = opaque->window_width;
    pWinAttr->stOutputRect.s32Height = opaque->window_height;

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
      format = VFORMAT_UNSUPPORT;
      break;
  }

  ALOGD("codecid_to_vformat, id(%d) -> vformat(%d)", (int)id, format);
  return format;
}

static HI_UNF_VCODEC_TYPE_E hisi_codecid_to_vformat(enum AVCodecID id)
{
  HI_UNF_VCODEC_TYPE_E format;
  switch (id)
  {
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
//    case AV_CODEC_ID_MPEG2VIDEO_XVMC:
      format = HI_UNF_VCODEC_TYPE_MPEG2;
      break;
    case AV_CODEC_ID_H263:
    case AV_CODEC_ID_H263P:
    case AV_CODEC_ID_H263I:
        format = HI_UNF_VCODEC_TYPE_H263;
        break;
    case AV_CODEC_ID_MPEG4:
    case AV_CODEC_ID_FLV1:
        format = HI_UNF_VCODEC_TYPE_MPEG4;
        break;
    case AV_CODEC_ID_MSMPEG4V2:
    case AV_CODEC_ID_MSMPEG4V3:
        format = HI_UNF_VCODEC_TYPE_MSMPEG4V2;
        break;
    case AV_CODEC_ID_RV10:
    case AV_CODEC_ID_RV20:
    case AV_CODEC_ID_RV30:
    case AV_CODEC_ID_RV40:
      format = HI_UNF_VCODEC_TYPE_REAL8;
      break;
    case AV_CODEC_ID_H264:
      format = HI_UNF_VCODEC_TYPE_H264;
      break;
    /*
    case AV_CODEC_ID_H264MVC:
      // H264 Multiview Video Coding (3d blurays)
      format = VFORMAT_H264MVC;
      break;
    */
    case AV_CODEC_ID_MJPEG:
      format = HI_UNF_VCODEC_TYPE_MJPEG;
      break;
    case AV_CODEC_ID_VC1:
    case AV_CODEC_ID_WMV3:
      format = HI_UNF_VCODEC_TYPE_VC1;
      break;
    case AV_CODEC_ID_AVS:
    case AV_CODEC_ID_CAVS:
      format = HI_UNF_VCODEC_TYPE_AVS;
      break;
    case AV_CODEC_ID_HEVC:
      format = HI_UNF_VCODEC_TYPE_HEVC;
      break;
    default:
      format = -1;
      break;
  }

  ALOGD("codecid_to_vformat, id(%d) -> vformat(%d)", (int)id, format);
  return format;
}

static vdec_type_t codec_tag_to_vdec_type(unsigned int codec_tag)
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
      dec_type = VIDEO_DEC_FORMAT_AVS;
      break;
    case AV_CODEC_ID_HEVC:
      // h265
      dec_type = VIDEO_DEC_FORMAT_H265;
      break;
    default:
      dec_type = VIDEO_DEC_FORMAT_UNKNOW;
      break;
  }

  ALOGD("codec_tag_to_vdec_type, codec_tag(%d) -> vdec_type(%d)", codec_tag, dec_type);
  return dec_type;
}

HI_S32 hisi_codec_setVdecAttr(HI_HANDLE hAvplay,HI_UNF_VCODEC_TYPE_E enType,HI_UNF_VCODEC_MODE_E enMode)
{
    HI_S32 Ret;
    HI_UNF_VCODEC_ATTR_S        VdecAttr;

    Ret = hisi_avplay_get_attr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, &VdecAttr);
    if (HI_SUCCESS != Ret)
    {
        PLAYER_LOGE("HI_UNF_AVPLAY_GetAttr failed:%#x\n",Ret);
        return Ret;
    }

    VdecAttr.enType = enType;
    VdecAttr.enMode = enMode;
    VdecAttr.u32ErrCover = 100;
    VdecAttr.u32Priority = 3;

    Ret = hisi_avplay_set_attr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_VDEC, &VdecAttr);
    if (Ret != HI_SUCCESS)
    {
        PLAYER_LOGE("call HI_UNF_AVPLAY_SetAttr failed.\n");
        return Ret;
    }

    return Ret;
}


static int hisi_codec_create(SHIJIU_HISI_Pipenode_Opaque *opaque){
    HI_S32    s32Ret = 0;
    HI_UNF_AVPLAY_STOP_OPT_S    Stop;

    PLAYER_LOGI("### hisicodec::Prepare \n");

//    s32Ret = hisi_vo_create_window(&(opaque->windowAttr), &(opaque->windowHandle));
//    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_VO_CreateWindow failed");

//    s32Ret = hisi_avplay_get_default_config(&(opaque->avplayAttr), HI_UNF_AVPLAY_STREAM_TYPE_ES);
//    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_GetDefaultConfig failed");
//
//    s32Ret = hisi_avplay_create(&(opaque->avplayAttr), &(opaque->avPlayHandle));
//    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_Create failed");

    s32Ret = hisi_avplay_get_attr(opaque->avPlayHandle, HI_UNF_AVPLAY_ATTR_ID_SYNC, &(opaque->syncAttr));
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_GetAttr:ID_SYNC failed");

    // opaque->syncAttr.enSyncRef = HI_UNF_SYNC_REF_NONE;
    opaque->syncAttr.enSyncRef = HI_UNF_SYNC_REF_PCR;
    opaque->syncAttr.stSyncStartRegion.s32VidPlusTime = 60;
    opaque->syncAttr.stSyncStartRegion.s32VidNegativeTime = -20;
    s32Ret = hisi_avplay_set_attr(opaque->avPlayHandle, HI_UNF_AVPLAY_ATTR_ID_SYNC, &(opaque->syncAttr));
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_SetAttr:ID_SYNC failed");

//    s32Ret = hisi_avplay_chn_open(opaque->avPlayHandle, HI_UNF_AVPLAY_MEDIA_CHAN_VID, HI_NULL);
//    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_ChnOpen VID failed");

    s32Ret = hisi_codec_setVdecAttr(opaque->avPlayHandle, opaque->hisi_video_codec, HI_UNF_VCODEC_MODE_NORMAL);
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HIADP_AVPlay_SetVdecAttr failed");

    // HI_UNF_AVPLAY_FRMRATE_PARAM_S stFramerate;
    // stFramerate.enFrmRateType = HI_UNF_AVPLAY_FRMRATE_TYPE_USER;
    // stFramerate.stSetFrmRate.u32fpsInteger = frame_rate;
    // stFramerate.stSetFrmRate.u32fpsDecimal = 0;
    // HI_UNF_AVPLAY_SetAttr(hAvplay, HI_UNF_AVPLAY_ATTR_ID_FRMRATE_PARAM, &stFramerate);

    HI_UNF_AVPLAY_FRMRATE_PARAM_S stFramerate;
    stFramerate.enFrmRateType = HI_UNF_AVPLAY_FRMRATE_TYPE_USER;
    stFramerate.stSetFrmRate.u32fpsInteger = (HI_U32)opaque->fps;
    stFramerate.stSetFrmRate.u32fpsDecimal = (HI_U32)((opaque->fps - stFramerate.stSetFrmRate.u32fpsInteger) * 1000);
    hisi_avplay_set_attr(opaque->avPlayHandle, HI_UNF_AVPLAY_ATTR_ID_FRMRATE_PARAM, &stFramerate);
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_SetAttr:ID_FRMRATE_PARAM failed");

//    s32Ret = hisi_vo_attach_window(opaque->windowHandle, opaque->avPlayHandle);
//    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_VO_AttachWindow failed");

//    s32Ret = hisi_vo_set_window_enable(opaque->windowHandle, HI_TRUE);
//    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_VO_SetWindowEnable failed");

    s32Ret = hisi_avplay_start(opaque->avPlayHandle, HI_UNF_AVPLAY_MEDIA_CHAN_VID, NULL);
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_Start failed");

    hisi_codec_register_event(opaque->avPlayHandle);

    PLAYER_LOGI("### hisicodec::Prepare Quit \n");

    return 0;
}

static void hisi_codec_register_event(HI_HANDLE g_hAvplay){
    HI_S32    s32Ret = 0;

    PLAYER_LOGI("### hisicodec::register event \n");

    s32Ret = hisi_avplay_register_event(g_hAvplay, HI_UNF_AVPLAY_EVENT_NEW_VID_FRAME, hisi_codec_evnet_handler);
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_RegisterEvent new frm failed");

    s32Ret = hisi_avplay_register_event(g_hAvplay, HI_UNF_AVPLAY_EVENT_IFRAME_ERR, hisi_codec_evnet_handler);
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_RegisterEvent frm err failed");

    s32Ret = hisi_avplay_register_event(g_hAvplay, HI_UNF_AVPLAY_EVENT_RNG_BUF_STATE, hisi_codec_evnet_handler);
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_RegisterEvent buf state failed");

    s32Ret = hisi_avplay_register_event(g_hAvplay, HI_UNF_AVPLAY_EVENT_EOS, hisi_codec_evnet_handler);
    PLAYER_CHK_PRINTF((HI_SUCCESS != s32Ret), s32Ret, "Call HI_UNF_AVPLAY_RegisterEvent eos failed");
}

static HI_S32 hisi_codec_evnet_handler(HI_HANDLE handle, HI_UNF_AVPLAY_EVENT_E enEvent, HI_U32 para)
{
    ALOGE("hisi_codec_evnet_handler, enEvent: %d, para: 0x%x", enEvent, para);
    return 0;
}

static int h264_add_header(unsigned char *buf, int size, hisi_packet_t *pkt)
{
    if (size > HDR_BUF_SIZE)
    {
        free(pkt->hdr->data);
        pkt->hdr->data = (char *)malloc(size);
        if (!pkt->hdr->data)
            return HI_FAILURE;
    }

    memcpy(pkt->hdr->data, buf, size);
    pkt->hdr->size = size;
    return HI_SUCCESS;
}

static int h264_write_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
{
    ALOGD("h264_write_header");
    int ret = h264_add_header(para->extradata, para->extrasize, pkt);
    if (ret == HI_SUCCESS) {
        pkt->newflag = 1;
        ret = write_av_packet(para, pkt);
    }
    return ret;
}

/*************************************************************************/
static int m4s2_dx50_mp4v_add_header(unsigned char *buf, int size,  hisi_packet_t *pkt)
{
    if (size > pkt->hdr->size) {
        free(pkt->hdr->data), pkt->hdr->data = NULL;
        pkt->hdr->size = 0;

        pkt->hdr->data = (char*)malloc(size);
        if (!pkt->hdr->data) {
            ALOGD("[m4s2_dx50_add_header] NOMEM!");
            return HI_FAILURE;
        }
    }

    pkt->hdr->size = size;
    memcpy(pkt->hdr->data, buf, size);

    return HI_SUCCESS;
}

static int m4s2_dx50_mp4v_write_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
{
    ALOGD("m4s2_dx50_mp4v_write_header");
    int ret = m4s2_dx50_mp4v_add_header(para->extradata, para->extrasize, pkt);
    if (ret == HI_SUCCESS) {
        pkt->newflag = 1;
        ret = write_av_packet(para, pkt);
    }
    return ret;
}

static int divx3_data_prefeeding(hisi_packet_t *pkt, unsigned w, unsigned h)
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
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

static int divx3_write_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
{
    ALOGD("divx3_write_header");
    divx3_data_prefeeding(pkt, para->video_width, para->video_height);
    pkt->newflag = 1;
    write_av_packet(para, pkt);
    return HI_SUCCESS;
}

static int mjpeg_data_prefeeding(hisi_packet_t *pkt)
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
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

static int mjpeg_write_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
{
    mjpeg_data_prefeeding(pkt);
    pkt->newflag = 1;
    write_av_packet(para, pkt);
    return HI_SUCCESS;
}

static int hevc_add_header(unsigned char *buf, int size,  hisi_packet_t *pkt)
{
    if (size > HDR_BUF_SIZE)
    {
        free(pkt->hdr->data);
        pkt->hdr->data = (char *)malloc(size);
        if (!pkt->hdr->data)
            return HI_FAILURE;
    }

    memcpy(pkt->hdr->data, buf, size);
    pkt->hdr->size = size;
    return HI_SUCCESS;
}

static int hevc_write_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
{
    int ret = -1;

    if (para->extradata) {
      ret = hevc_add_header(para->extradata, para->extrasize, pkt);
    }
    if (ret == HI_SUCCESS) {
      pkt->newflag = 1;
      ret = write_av_packet(para, pkt);
    }
    return ret;
}

static int wmv3_write_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
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

static int wvc1_write_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
{
    ALOGD("wvc1_write_header");
    memcpy(pkt->hdr->data, para->extradata + 1, para->extrasize - 1);
    pkt->hdr->size = para->extrasize - 1;
    pkt->newflag = 1;
    return write_av_packet(para, pkt);
}

static int mpeg_add_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
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

static int write_av_packet(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
{
    HI_S32    ret = HI_SUCCESS;
    unsigned char *buf;
    int size;

    // do we need to check in pts or write the header ?
    if (pkt->newflag) {
        if (write_header(para, pkt) == HI_FAILURE) {
            ALOGE("[%s]write header failed!", __FUNCTION__);
            return HI_FAILURE;
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
        ret = hisi_codec_write_data(para, buf, size);
        if (ret != HI_SUCCESS) {
            ALOGE("write codec data failed");
        }
    } 

    return ret;
}

static int write_header(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
{
    HI_S32 ret = HI_SUCCESS; 

    ALOGD("write_header\n");
    if (pkt->hdr && pkt->hdr->size > 0) {
        if (/*(NULL == pkt->codec) || */(NULL == pkt->hdr->data)) {
            ALOGE("[write_header]codec null!");
            return HI_FAILURE;
        }
        //some wvc1 es data not need to add header
        if (para->video_format == HI_UNF_VCODEC_TYPE_VC1 && para->video_codec_type == VIDEO_DEC_FORMAT_WVC1) {
            if ((pkt->data) && (pkt->data_size >= 4)
              && (pkt->data[0] == 0) && (pkt->data[1] == 0)
              && (pkt->data[2] == 1) && (pkt->data[3] == 0xd || pkt->data[3] == 0xf)) {
                return HI_SUCCESS;
            }
        }
        
        ALOGD("write_header, size = %d\n", pkt->hdr->size);
        ret = hisi_codec_write_data(para, pkt->hdr->data , pkt->hdr->size);
        if (ret != HI_SUCCESS) {
            ALOGE("ERROR:write header failed!");
        }
    }

    return ret;
}

static int hisi_codec_write_data(SHIJIU_HISI_Pipenode_Opaque *opaque, HI_U8 * buffer, int size){
    HI_S32 ret;
    HI_UNF_STREAM_BUF_S         streamBuf;

    // ALOGD("hisi_codec_write_data: size=%d\n", size);
    ret = hisi_avplay_get_buf(opaque->avPlayHandle, HI_UNF_AVPLAY_BUF_ID_ES_VID, size, &streamBuf, 0);
    if (HI_SUCCESS == ret){
        memcpy(streamBuf.pu8Data, buffer, size);
        ret = hisi_avplay_put_buf(opaque->avPlayHandle, HI_UNF_AVPLAY_BUF_ID_ES_VID, size, 0);
        if (ret != HI_SUCCESS){
            ALOGE("call HI_UNF_AVPLAY_PutBuf failed.\n");
        }
    }else{
        ALOGE("call HI_UNF_AVPLAY_GetBuf failed.\n");
    }
}

static int pre_header_feeding(SHIJIU_HISI_Pipenode_Opaque *para, hisi_packet_t *pkt)
{
    int ret;
    if (para->stream_type == HISI_STREAM_ES) {
        if (pkt->hdr == NULL) {
            pkt->hdr = (hdr_buf_t*)malloc(sizeof(hdr_buf_t));
            pkt->hdr->data = (char *)malloc(HDR_BUF_SIZE);
            if (!pkt->hdr->data) {
                ALOGE("[pre_header_feeding] NOMEM!");
                return HI_FAILURE;
            }
        }

        if (VFORMAT_H264 == para->video_format) {
            ret = h264_write_header(para, pkt);
            if (ret != HI_SUCCESS) {
                return ret;
            }
        } else if ((VFORMAT_MPEG4 == para->video_format) && (VIDEO_DEC_FORMAT_MPEG4_3 == para->video_codec_type)) {
            ret = divx3_write_header(para, pkt);
            if (ret != HI_SUCCESS) {
                return ret;
            }
        } else if ((CODEC_TAG_M4S2 == para->video_codec_tag)
                || (CODEC_TAG_DX50 == para->video_codec_tag)
                || (CODEC_TAG_mp4v == para->video_codec_tag)) {
            ret = m4s2_dx50_mp4v_write_header(para, pkt);
            if (ret != HI_SUCCESS) {
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
            if (ret != HI_SUCCESS) {
                return ret;
            }
        } else if ((CODEC_TAG_WVC1 == para->video_codec_tag)
                || (CODEC_TAG_VC_1 == para->video_codec_tag)
                || (CODEC_TAG_WMVA == para->video_codec_tag)) {
            ALOGD("CODEC_TAG_WVC1 == para->video_codec_tag");
            ret = wvc1_write_header(para, pkt);
            if (ret != HI_SUCCESS) {
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
            if (ret != HI_SUCCESS) {
                return ret;
            }
        } else if (VFORMAT_H265 == para->video_format) {
            ret = hevc_write_header(para, pkt);
            if (ret != HI_SUCCESS) {
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
    }
    else if (para->stream_type == HISI_STREAM_PS) {
        if (pkt->hdr == NULL) {
            pkt->hdr = (hdr_buf_t*)malloc(sizeof(hdr_buf_t));
            pkt->hdr->data = (char*)malloc(HDR_BUF_SIZE);
            if (!pkt->hdr->data) {
                ALOGD("[pre_header_feeding] NOMEM!");
                return HI_FAILURE;
            }
        }
        if (( AV_CODEC_ID_MPEG1VIDEO == para->video_codec_id)
          || (AV_CODEC_ID_MPEG2VIDEO == para->video_codec_id)
          /*|| (AV_CODEC_ID_MPEG2VIDEO_XVMC == para->video_codec_id)*/) {
            ret = mpeg_add_header(para, pkt);
            if (ret != HI_SUCCESS) {
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
    }
    return HI_SUCCESS;
}

static int divx3_prefix(hisi_packet_t *pkt)
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
            return HI_FAILURE;
        }

        pkt->hdr->data = NULL;
        pkt->hdr->size = 0;
    }

    pkt->hdr->data = (char*)malloc(DIVX311_CHUNK_HEAD_SIZE + 4);
    if (pkt->hdr->data == NULL) {
        ALOGD("[divx3_prefix] NOMEM!");
        return HI_FAILURE;
    }

    memcpy(pkt->hdr->data, divx311_chunk_prefix, DIVX311_CHUNK_HEAD_SIZE);

    pkt->hdr->data[DIVX311_CHUNK_HEAD_SIZE + 0] = (pkt->data_size >> 24) & 0xff;
    pkt->hdr->data[DIVX311_CHUNK_HEAD_SIZE + 1] = (pkt->data_size >> 16) & 0xff;
    pkt->hdr->data[DIVX311_CHUNK_HEAD_SIZE + 2] = (pkt->data_size >>  8) & 0xff;
    pkt->hdr->data[DIVX311_CHUNK_HEAD_SIZE + 3] = pkt->data_size & 0xff;

    pkt->hdr->size = DIVX311_CHUNK_HEAD_SIZE + 4;
    pkt->newflag = 1;

    return HI_SUCCESS;
}


static int set_header_info(SHIJIU_HISI_Pipenode_Opaque *para)
{
  hisi_packet_t *pkt = &para->hisi_pkt;

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
        return HI_FAILURE;
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
                    return HI_FAILURE;
                }

                pkt->hdr->data = NULL;
                pkt->hdr->size = 0;
            }

            if (pkt->avpkt.flags) {
                pkt->hdr->data = (char*)malloc(para->extrasize + 26 + 22);
                if (pkt->hdr->data == NULL) {
                    return HI_FAILURE;
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
                    return HI_FAILURE;
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
                    return HI_FAILURE;
                }

                pkt->hdr->data = NULL;
                pkt->hdr->size = 0;
            }

            pkt->hdr->data = (char*)malloc(4);
            if (pkt->hdr->data == NULL) {
                ALOGD("[wvc1_prefix] NOMEM!");
                return HI_FAILURE;
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
  return HI_SUCCESS;
}