#pragma once

#include <mutex>
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

    int draw();

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
    std::shared_ptr<AVFrame> frame;

    //顶点位置
    int glAvPosition;
    //纹理位置
    int glAfPosition;

    //shader  yuv变量
    int glSamplerY;
    int glSamplerU;
    int glSamplerV;
    uint32_t glTextureYUV[3];

};

} // namespace plugin
} // namespace jsview
