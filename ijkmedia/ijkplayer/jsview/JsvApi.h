#ifndef __FFPLAY_JSV_API_H__
#define __FFPLAY_JSV_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <libavutil/frame.h>

typedef struct {
    int  (*get_frame_format_handler)(int64_t imp, int* videoFormat, int* videoWidth, int* videoHeight);
    int  (*obtain_frame_buffer_handler)(int64_t imp, uint8_t** data, int* size);
    void (*release_frame_buffer_handler)(int64_t imp, int index);
} MediaCodecHandler;

typedef struct {
    void* videoRenderer;
    bool videoPrepared;

    void (*videoSyncCallback)(void*);
    void *videoSyncData;

    MediaCodecHandler mediaCodecHandler;
} JsvContext;

JsvContext* NewJsvContext();
void DeleteJsvContext(JsvContext** context);

void WriteToJsvVideoRenderer(JsvContext* context, AVFrame *frame);
int DrawJsvVideoRenderer(JsvContext* context, float mvpMatrix[], int size);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif // __FFPLAY_JSV_API_H__
