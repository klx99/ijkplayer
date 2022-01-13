#pragma once

#include "JsvGLRenderer.hpp"

namespace jsview {
namespace plugin {

class JsvVideoRenderer : public JsvGLRenderer {
public:
/*** type define ***/

/*** static function and variable ***/
    static constexpr const int BLOCK_COUNT = 3;

/*** class function and variable ***/
    explicit JsvVideoRenderer() = default;
    virtual ~JsvVideoRenderer() = default;

    virtual const char* getVertexShaderSource() override;
    virtual const char* getFragmentShaderSource() override;

    virtual int prepare() override;
    virtual int hasPrepared() override;
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
    bool prepared = false;

    int glMVP = -1;
    //顶点位置
    int glAvPosition = -1;
    //纹理位置
    int glAfPosition = -1;

    //shader  yuv变量
    int glSamplerY = -1;
    int glSamplerU = -1;
    int glSamplerV = -1;
    uint32_t glTextureYUV[3] = {};

};

} // namespace plugin
} // namespace jsview
