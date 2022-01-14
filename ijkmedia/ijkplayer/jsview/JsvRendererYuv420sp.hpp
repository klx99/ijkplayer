#pragma once

#include "JsvGLRenderer.hpp"

namespace jsview {
namespace plugin {

class JsvRendererYuv420sp : public JsvGLRenderer {
public:
/*** type define ***/

/*** static function and variable ***/

/*** class function and variable ***/
    explicit JsvRendererYuv420sp() = default;
    virtual ~JsvRendererYuv420sp() = default;

    virtual const char* getVertexShaderSource() override;
    virtual const char* getFragmentShaderSource() override;
    virtual int hasPrepared() override;

    virtual int prepare() override;
    int drawFrame(float mvpMat4[16],
                  int width, int height,
                  uint8_t* data, int dataSize) override;

private:
/*** type define ***/

/*** static function and variable ***/
    static constexpr const int SamplerCount = 2;

/*** class function and variable ***/
    bool prepared = false;

    //shader  yuv变量
    int glSamplers[SamplerCount] = {-1};
    int glColorConversion = -1;
    uint32_t glTextures[SamplerCount] = {0};

};

} // namespace plugin
} // namespace jsview
