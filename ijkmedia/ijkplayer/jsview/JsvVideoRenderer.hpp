#pragma once

#include "JsvGLES2.hpp"

namespace jsview {
namespace plugin {

class JsvVideoRenderer : public JsvGLES2 {
public:
/*** type define ***/

/*** static function and variable ***/
    static constexpr const int BLOCK_COUNT = 3;

/*** class function and variable ***/
    explicit JsvVideoRenderer() = default;
    virtual ~JsvVideoRenderer() = default;

    virtual const char* getVertexShaderSource();
    virtual const char* getFragmentShaderSource();

    virtual int prepare();
    int drawWithData(float mvpMatrix[], int matrixSize,
                     int colorFormat, int width, int height,
                     uint8_t* data, int dataSize);

private:
/*** type define ***/

/*** static function and variable ***/
    static const int CoordsPerVertex;
    static const float VertexData[];
    static const int VertexCount;
    static const int VertexStride;

    static const float TextureData[];

/*** class function and variable ***/
    int glMVP;
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
