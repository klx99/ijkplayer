#ifndef __FFPLAY_JSV_API_H__
#define __FFPLAY_JSV_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <libavutil/frame.h>

typedef struct {
    float* mvpMat4;
    int viewWidth;
    int viewHeight;

    void* videoRenderer;

    void (*videoSyncCallback)(void*);
    void *videoSyncData;
} JsvContext;

JsvContext* NewJsvContext();
void DeleteJsvContext(JsvContext** context);
int MakeJsvVideoRenderer(JsvContext* context, int colorFormat);

int SetJsvVideoRendererMatrix4(JsvContext* context, float mvpMat4[], int viewWidth, int viewHeight);
int DrawJsvVideoRendererWithData(JsvContext* context,
                                 int colorFormat, int width, int height,
                                 uint8_t* data, int dataSize);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // __FFPLAY_JSV_API_H__
