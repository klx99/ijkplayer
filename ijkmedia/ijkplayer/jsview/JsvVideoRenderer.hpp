#pragma once

#include <mutex>
#include <EGL/egl.h>
#include "JsvGLES2.hpp"

struct AVFrame;

namespace jsview {
namespace plugin {

class JsvVideoRenderer : public JsvGLES2 {
public:
/*** type define ***/

/*** static function and variable ***/
    static constexpr const int BLOCK_COUNT = 3;

/*** class function and variable ***/
    virtual int prepare();
    int write(AVFrame* frame);

    int draw(float mvpMatrix[], int size);
    int drawWithData(float mvpMatrix[], int matrixSize,
                     int colorFormat, int width, int height,
                     uint8_t* data, int dataSize);

    virtual const char* getVertexShaderSource();
    virtual const char* getFragmentShaderSource();

private:
/*** type define ***/

/*** static function and variable ***/
    static const int CoordsPerVertex;
    static const float VertexData[];
    static const int VertexCount;
    static const int VertexStride;

    static const float TextureData[];

/*** class function and variable ***/
    std::mutex mutex;
    std::shared_ptr<AVFrame> lastFrame;

//    EGLDisplay eglDisplay;

    int glumMVP;
    //顶点位置
    int glavPosition;
    //纹理位置
    int glafPosition;

    //shader  yuv变量
    int glSamplerY;
    int glSamplerU;
    int glSamplerV;
    uint32_t glTextureYUV[3];

};

} // namespace plugin
} // namespace jsview