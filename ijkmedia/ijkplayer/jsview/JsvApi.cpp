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
#include "JsvRendererYuv420p.hpp"
#include "JsvRendererYuv420sp.hpp"
#include "JsvRendererQComYuv420sp.hpp"

JsvContext* NewJsvContext()
{
    JsvContext* context = new JsvContext();
    context->mvpMat4 = nullptr; // 空矩阵

    return context;
}

void DeleteJsvContext(JsvContext** context)
{
    if(context == nullptr || *context) {
        return;
    }

    if((*context)->mvpMat4 != nullptr) {
        delete[] (*context)->mvpMat4;
    }

    if((*context)->videoRenderer != nullptr) {
        auto videoRenderer = reinterpret_cast<jsview::plugin::JsvGLRenderer*>((*context)->videoRenderer);
        delete videoRenderer;
    }

    *context = nullptr;
}

int MakeJsvVideoRenderer(JsvContext* context, int colorFormat)
{
    if(context == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to make video renderer, context has been deleted.");
        return -1;
    }
    if(context->videoRenderer != nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to make video renderer, video renderer has been created.");
        return -1;
    }

    enum SupportColorFormat : int32_t { // Same as MediaCodecInfo.CodecCapabilities
        YUV420Planar = 19,
        YUV420SemiPlanar = 21,
        YCbYCr = 25,
        QComYUV420SemiPlanar = 2141391872,
    };

    switch (colorFormat) {
    case YCbYCr:
        context->videoRenderer = new jsview::plugin::JsvRendererYuv420sp();
            break;
    case QComYUV420SemiPlanar:
        context->videoRenderer = new jsview::plugin::JsvRendererQComYuv420sp();
        break;

    case YUV420Planar:
    case YUV420SemiPlanar:
    default:
        context->videoRenderer = new jsview::plugin::JsvRendererYuv420p();
    }

    return 0;
}

int SetJsvVideoRendererMatrix4(JsvContext* context, float mvpMat4[], int viewWidth, int viewHeight)
{
    if(context == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to set matrix to video renderer, context has been deleted.");
        return -1;
    }

    float* newMat4 = nullptr;
    if(mvpMat4 != nullptr) {
        newMat4 = new float[16] {
            mvpMat4[0],  mvpMat4[1],  mvpMat4[2],  mvpMat4[3],
            mvpMat4[4],  mvpMat4[5],  mvpMat4[6],  mvpMat4[7],
            mvpMat4[8],  mvpMat4[9],  mvpMat4[10], mvpMat4[11],
            mvpMat4[12], mvpMat4[13], mvpMat4[14], mvpMat4[15],
        };
    }
    float* oldMat4 = context->mvpMat4;
    context->mvpMat4 = newMat4;
    if(oldMat4 != nullptr) {
        delete[] oldMat4;
    }

    context->viewWidth = viewWidth;
    context->viewHeight = viewHeight;

    return 0;
}

int DrawJsvVideoRendererWithData(JsvContext* context,
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
    if(context->mvpMat4 == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw frame to video renderer, mvp matrix has not exists.");
        return -1;
    }

    auto videoRenderer = reinterpret_cast<jsview::plugin::JsvGLRenderer*>(context->videoRenderer);

    if(videoRenderer->hasPrepared() == false) {
        int ret = videoRenderer->prepare();
        if(ret < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to prepare video renderer.");
            return ret;
        }
    }
    int ret = videoRenderer->drawFrame(context->mvpMat4,
                                       width, height,
                                       data, dataSize);
    if(ret < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw to video renderer.");
        return ret;
    }

    return ret;
}
