#ifndef __FFPLAY_JSV_API_H__
#define __FFPLAY_JSV_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <libavutil/frame.h>

typedef struct {
    int64_t mediaPlayerHandler;
    int  (*getFrameFormatHandler)(int64_t imp, int* videoFormat, int* videoWidth, int* videoHeight);
    int  (*lockFrameBufferHandler)(int64_t imp, uint8_t** data);
    void (*unlockFrameBufferHandler)(int64_t imp);
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

int DrawJsvVideoRendererWithData(JsvContext* context,
                                 float mvpMatrix[], int matrixSize,
                                 int colorFormat, int width, int height,
                                 uint8_t* data, int dataSize);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // __FFPLAY_JSV_API_H__
