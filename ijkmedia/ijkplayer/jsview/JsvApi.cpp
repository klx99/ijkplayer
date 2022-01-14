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
#include "JsvRendererYuv420sp.hpp"
#include "JsvVideoRenderer.hpp"

JsvContext* NewJsvContext()
{
    JsvContext* context = new JsvContext();

    return context;
}

void DeleteJsvContext(JsvContext** context)
{
    if(context == nullptr || *context) {
        return;
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
        context->videoRenderer = new jsview::plugin::JsvVideoRenderer();
    }

    return 0;
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

    auto videoRenderer = reinterpret_cast<jsview::plugin::JsvGLRenderer*>(context->videoRenderer);

    if(videoRenderer->hasPrepared() == false) {
        int ret = videoRenderer->prepare();
        if(ret < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to prepare video renderer.");
            return ret;
        }
    }
    int ret = videoRenderer->drawFrame(mvpMatrix,
                                       width, height,
                                       data, dataSize);
    if(ret < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "Failed to draw to video renderer.");
        return ret;
    }

    return ret;
}
