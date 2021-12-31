#ifndef __FFPLAY_JSV_API_H__
#define __FFPLAY_JSV_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <libavutil/frame.h>

typedef struct {
    void* videoRenderer;
    bool videoPrepared;

    void (*videoSyncCallback)(void*);
    void *videoSyncData;
} JsvContext;

JsvContext* NewJsvContext();
void DeleteJsvContext(JsvContext** context);

void WriteToJsvVideoRenderer(JsvContext* context, AVFrame *frame);
int DrawJsvVideoRenderer(JsvContext* context, float mvpMatrix[], int size);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif // __FFPLAY_JSV_API_H__
