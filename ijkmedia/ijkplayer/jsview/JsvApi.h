#ifndef __FFPLAY_JSV_API_H__
#define __FFPLAY_JSV_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <libavutil/frame.h>

typedef struct {
    void* videoRenderer;

    void (*videoSyncCallback)(void*);
    void *videoSyncData;
} JsvContext;

JsvContext* NewJsvContext();
void DeleteJsvContext(JsvContext** context);
int MakeJsvVideoRenderer(JsvContext* context, int colorFormat);

int DrawJsvVideoRendererWithData(JsvContext* context,
                                 float mvpMatrix[], int matrixSize,
                                 int colorFormat, int width, int height,
                                 uint8_t* data, int dataSize);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // __FFPLAY_JSV_API_H__
