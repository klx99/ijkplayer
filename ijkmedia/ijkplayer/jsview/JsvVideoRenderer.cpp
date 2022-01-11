//
//  Template.cpp
//  JsView-Plugin
//
//  Created by mengxk on 12/22/21.
//  Copyright © 2021 mengxk. All rights reserved.
//

#include "JsvVideoRenderer.hpp"

#include <android/log.h>
//#include <EGL/eglext.h>
//#include <EGL/eglplatform.h>
#include <GLES2/gl2.h>

#include <unistd.h>

extern "C" {
#include <libavutil/frame.h>
}


namespace jsview {
namespace plugin {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/
//每一次取点的时候取几个点
const int JsvVideoRenderer::CoordsPerVertex = 3;

//顶点坐标
const float JsvVideoRenderer::VertexData[] = {   // in counterclockwise order:
        -1.0f, -1.0f, 0.0f, // bottom left
        1.0f, -1.0f, 0.0f, // bottom right
        -1.0f, 1.0f, 0.0f, // top left
        1.0f, 1.0f, 0.0f,  // top right
};

const int JsvVideoRenderer::VertexCount = sizeof(VertexData) / sizeof(*VertexData) / CoordsPerVertex;
//每一次取的总的点 大小
const int JsvVideoRenderer::VertexStride = CoordsPerVertex * 4; // 4 bytes per vertex

//纹理坐标
const float JsvVideoRenderer::TextureData[] = {   // in counterclockwise order:
        0.0f, 1.0f, 0.0f, // bottom left
        1.0f, 1.0f, 0.0f, // bottom right
        0.0f, 0.0f, 0.0f, // top left
        1.0f, 0.0f, 0.0f,  // top right
};

/***********************************************/
/***** static function implement ***************/
/***********************************************/

/***********************************************/
/***** class public function implement  ********/
/***********************************************/
int JsvVideoRenderer::prepare()
{
//    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
//    if (eglDisplay == EGL_NO_DISPLAY) {
//        __android_log_print(ANDROID_LOG_INFO, "JsView", "[EGL] eglGetDisplay failed.");
//        return EGL_FALSE;
//    }

    int ret = JsvGLES2::prepare();
    if(ret < 0) {
        return ret;
    }

    auto program = getProgram();

    glumMVP = glGetUniformLocation(program, "um_MVP");

    //获取顶点坐标字段
    glavPosition = glGetAttribLocation(program, "av_Position");
    //获取纹理坐标字段
    glafPosition = glGetAttribLocation(program, "af_Position");
    //获取yuv字段
    glSamplerY = glGetUniformLocation(program, "sampler_y");
    glSamplerU = glGetUniformLocation(program, "sampler_u");
    glSamplerV = glGetUniformLocation(program, "sampler_v");

    //创建3个纹理
    glGenTextures(3, glTextureYUV);

    //绑定纹理
    for (int id : glTextureYUV) {
        glBindTexture(GL_TEXTURE_2D, id);
        //环绕（超出纹理坐标范围）  （s==x t==y GL_REPEAT 重复）
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        //过滤（纹理像素映射到坐标点）  （缩小、放大：GL_LINEAR线性）
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    return 0;
}

int JsvVideoRenderer::write(AVFrame* frame)
{
    auto creater = []() -> AVFrame* {
        return av_frame_alloc();
    };
    auto deleter = [](AVFrame* ptr) -> void {
        av_frame_free(&ptr);
    };
    auto sharedFrame = std::shared_ptr<AVFrame>(creater(), deleter);

    av_frame_move_ref(sharedFrame.get(), frame);

    {
        std::lock_guard <std::mutex> lock(mutex);
        this->lastFrame = sharedFrame;
    }

    return 0;
}

int JsvVideoRenderer::draw(float mvpMatrix[], int size)
{
    if(!this->lastFrame) {
        return 0;
    }

    using namespace std::chrono;
    int64_t startTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    auto program = getProgram();

    glUseProgram(program);

    glUniformMatrix4fv(glumMVP, 1, GL_FALSE, mvpMatrix);

    glEnableVertexAttribArray(glavPosition);
    glVertexAttribPointer(glavPosition, CoordsPerVertex, GL_FLOAT, false, VertexStride, VertexData);

    glEnableVertexAttribArray(glafPosition);
    glVertexAttribPointer(glafPosition, CoordsPerVertex, GL_FLOAT, false, VertexStride, TextureData);

//    EGLint eglImageAttributes[] = {EGL_WIDTH, frame->width, EGL_HEIGHT, frame->height,
//                                   EGL_IMAGE_FORMAT_FSL, EGL_FORMAT_YUV_YV12_FSL, EGL_NONE};
//    auto eglImage = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_NEW_IMAGE_FSL, NULL, eglImageAttributes);
//    struct EGLImageInfoFSL eglImageInfo;
//    eglQueryImageFSL(eglDisplay, eglImage, EGL_CLIENTBUFFER_TYPE_FSL, (EGLint *)&eglImageInfo);

    std::shared_ptr<AVFrame> sharedFrame;
    {
        std::lock_guard <std::mutex> lock(mutex);
        sharedFrame = this->lastFrame;
    }

    //激活纹理0来绑定y数据
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glTextureYUV[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, sharedFrame->width, sharedFrame->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, sharedFrame->data[0]);

    //激活纹理1来绑定u数据
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, glTextureYUV[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, sharedFrame->width / 2, sharedFrame->height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, sharedFrame->data[1]);

    //激活纹理2来绑定u数据
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, glTextureYUV[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, sharedFrame->width / 2, sharedFrame->height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, sharedFrame->data[2]);

    //给fragment_shader里面yuv变量设置值   0 1 2 标识纹理x
    glUniform1i(glSamplerY, 0);
    glUniform1i(glSamplerU, 1);
    glUniform1i(glSamplerV, 2);

    //绘制
    glDrawArrays(GL_TRIANGLE_STRIP, 0, VertexCount);

    glDisableVertexAttribArray(glafPosition);
    glDisableVertexAttribArray(glavPosition);

//    int64_t endTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
//    int64_t deltaTime = endTime - startTime;
//    __android_log_print(ANDROID_LOG_INFO, "JsView", "%s time: start/end=%lld/%lld, delta=%lld", __PRETTY_FUNCTION__, startTime, endTime, deltaTime);

    return 0;
}

int JsvVideoRenderer::drawWithData(float mvpMatrix[], int matrixSize,
                                   int colorFormat, int width, int height,
                                   uint8_t* data, int dataSize)
{
    auto program = getProgram();

    glUseProgram(program);

    glUniformMatrix4fv(glumMVP, 1, GL_FALSE, mvpMatrix);

    glEnableVertexAttribArray(glavPosition);
    glVertexAttribPointer(glavPosition, CoordsPerVertex, GL_FLOAT, false, VertexStride, VertexData);

    glEnableVertexAttribArray(glafPosition);
    glVertexAttribPointer(glafPosition, CoordsPerVertex, GL_FLOAT, false, VertexStride, TextureData);

    //激活纹理0来绑定y数据
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glTextureYUV[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);

    //激活纹理1来绑定u数据
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, glTextureYUV[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width / 2, height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data + width*height);

    //激活纹理2来绑定u数据
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, glTextureYUV[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width / 2, height / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data + width*height + width*height/4);

    //给fragment_shader里面yuv变量设置值   0 1 2 标识纹理x
    glUniform1i(glSamplerY, 0);
    glUniform1i(glSamplerU, 1);
    glUniform1i(glSamplerV, 2);

    //绘制
    glDrawArrays(GL_TRIANGLE_STRIP, 0, VertexCount);

    glDisableVertexAttribArray(glafPosition);
    glDisableVertexAttribArray(glavPosition);

    return 0;
}

const char* JsvVideoRenderer::getVertexShaderSource()
{
    static constexpr const char* source =
        "uniform mat4 um_MVP;"
        "attribute vec4 av_Position;" //顶点位置
        "attribute vec2 af_Position;" //纹理位置
        "varying vec2 v_texPo;" //纹理位置  与fragment_shader交互
        "void main() {"
        "    v_texPo = af_Position;"
        "    gl_Position  = um_MVP * av_Position;"
    "}";

return source;
}

const char* JsvVideoRenderer::getFragmentShaderSource()
{
    static constexpr const char* source =
        "precision mediump float;"//精度 为float
        "varying vec2 v_texPo;"//纹理位置  接收于vertex_shader
        "uniform sampler2D sampler_y;"//纹理y
        "uniform sampler2D sampler_u;"//纹理u
        "uniform sampler2D sampler_v;"//纹理v
        ""
        "void main() {" //yuv420->rgb
        "    float y,u,v;"
        "    y = texture2D(sampler_y,v_texPo).r;"
        "    u = texture2D(sampler_u,v_texPo).r- 0.5;"
        "    v = texture2D(sampler_v,v_texPo).r- 0.5;"
        "    vec3 rgb;"
        "    rgb.r = y + 1.403 * v;"
        "    rgb.g = y - 0.344 * u - 0.714 * v;"
        "    rgb.b = y + 1.770 * u;"
        ""
        "    gl_FragColor=vec4(rgb,1);"
        "}";

    return source;
}

/***********************************************/
/***** class protected function implement  *****/
/***********************************************/

/***********************************************/
/***** class private function implement  *******/
/***********************************************/


} // namespace plugin
} // namespace jsview