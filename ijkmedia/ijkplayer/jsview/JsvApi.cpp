//
//  Template.cpp
//  JsView-Plugin
//
//  Created by mengxk on 12/22/21.
//  Copyright © 2021 mengxk. All rights reserved.
//

#include "JsvApi.h"

#include <android/log.h>
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
        auto videoRenderer = reinterpret_cast<jsview::plugin::JsvVideoRenderer*>((*context)->videoRenderer);
        delete videoRenderer;
    }

    *context = nullptr;
}

void WriteToJsvVideoRenderer(JsvContext* context, AVFrame *frame)
{
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
