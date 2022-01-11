//
//  Template.cpp
//  JsView-Plugin
//
//  Created by mengxk on 12/22/21.
//  Copyright © 2021 mengxk. All rights reserved.
//

#include "JsvApi.h"

#include <chrono>
#include <android/log.h>
#include "JsvVideoRenderer.hpp"

JsvContext* NewJsvContext()
{
    JsvContext* context = new JsvContext();
    context->videoRenderer = new jsview::plugin::JsvVideoRenderer();

//    pthread_mutex_init(&context->mediaCodecInfo.mutex, NULL);

//    context->mediaCodecInfo.outputBufferSize = 1024*1024*10;
//    context->mediaCodecInfo.outputBuffer = new uint8_t[context->mediaCodecInfo.outputBufferSize];

    return context;
}

void DeleteJsvContext(JsvContext** context)
{
    if(context == nullptr || *context) {
        return;
    }

    if((*context)->videoRenderer != nullptr) {
        auto videoRenderer = reinterpret_cast<jsview::plugin::JsvVideoRenderer*>((*context)->videoRenderer);
        delete videoRenderer;
    }

//    pthread_mutex_destroy(&(*context)->mediaCodecInfo.mutex);
//    delete[] (*context)->mediaCodecInfo.outputBuffer;

    *context = nullptr;
}

void WriteToJsvVideoRenderer(JsvContext* context, AVFrame *frame)
{
//    using namespace std::chrono;
//    int64_t startTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    if(context == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw frame to video renderer, context has been deleted.");
        return;
    }
    if(context->videoRenderer == nullptr) {
        context->videoRenderer = new jsview::plugin::JsvVideoRenderer();
    }

    auto videoRenderer = reinterpret_cast<jsview::plugin::JsvVideoRenderer*>(context->videoRenderer);
    videoRenderer->write(frame);

    if(context->videoSyncCallback && context->videoSyncData) {
        context->videoSyncCallback(context->videoSyncData);
    }
//    int64_t endTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
//    int64_t deltaTime = endTime - startTime;
//    __android_log_print(ANDROID_LOG_INFO, "JsView", "======= video sync time: start/end=%lld/%lld, delta=%lld", startTime, endTime, deltaTime);
}

int DrawJsvVideoRenderer(JsvContext* context, float mvpMatrix[], int size)

{
    if(context == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw frame to video renderer, context has been deleted.");
        return -1;
    }
    if(context->videoRenderer == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw frame to video renderer, video renderer has been deleted.");
        return -1;
    }

    auto videoRenderer = reinterpret_cast<jsview::plugin::JsvVideoRenderer*>(context->videoRenderer);

    if(context->videoPrepared == false) {
        int ret = videoRenderer->prepare();
        if(ret < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to prepare video renderer.");
            return ret;
        }
        context->videoPrepared = true;
    }
    int ret = videoRenderer->draw(mvpMatrix, size);
    if(ret < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw to video renderer.");
        return ret;
    }

    return ret;
}

int DrawJsvVideoRendererWithData(JsvContext* context,
                                 float mvpMatrix[], int matrixSize,
                                 int colorFormat, int width, int height,
                                 uint8_t* data, int dataSize)

{
    if(context == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw frame to video renderer, context has been deleted.");
        return -1;
    }
    if(context->videoRenderer == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw frame to video renderer, video renderer has been deleted.");
        return -1;
    }

    auto videoRenderer = reinterpret_cast<jsview::plugin::JsvVideoRenderer*>(context->videoRenderer);

    if(context->videoPrepared == false) {
        int ret = videoRenderer->prepare();
        if(ret < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to prepare video renderer.");
            return ret;
        }
        context->videoPrepared = true;
    }
    int ret = videoRenderer->drawWithData(mvpMatrix, matrixSize,
                                          colorFormat, width, height,
                                          data, dataSize);
    if(ret < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw to video renderer.");
        return ret;
    }

    return ret;
}
