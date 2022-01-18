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

JsvContext* NewJsvContext()
{
    JsvContext* context = new JsvContext();
    context->mvpMat4 = new float[16] { // 标准矩阵
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    };

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
        ColorFormat_YUV420Planar = 19,
        ColorFormat_YUV420SemiPlanar = 21,
        ColorFormat_YCbYCr = 25
    };

    switch (colorFormat) {
    case ColorFormat_YCbYCr:
        context->videoRenderer = new jsview::plugin::JsvRendererYuv420sp();
        break;

    case ColorFormat_YUV420Planar:
    case ColorFormat_YUV420SemiPlanar:
    default:
        context->videoRenderer = new jsview::plugin::JsvRendererYuv420p();
    }

    return 0;
}

int SetJsvVideoRendererMatrix4(JsvContext* context, float data[])
{
    if(context == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to set matrix to video renderer, context has been deleted.");
        return -1;
    }

    float* newMat4 = nullptr;
    if(data != nullptr) {
        newMat4 = new float[16] {
            data[0],  data[1],  data[2],  data[3],
            data[4],  data[5],  data[6],  data[7],
            data[8],  data[9],  data[10], data[11],
            data[12], data[13], data[14], data[15],
        };
    }
    float* oldMat4 = context->mvpMat4;
    context->mvpMat4 = newMat4;
    if(oldMat4 != nullptr) {
        delete[] oldMat4;
    }

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
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw frame to video renderer, mvp matrix has been deleted.");
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
