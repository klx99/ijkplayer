#pragma once

#include "JsvGLRenderer.hpp"

namespace jsview {
namespace plugin {

class JsvRendererYuv420p : public JsvGLRenderer {
public:
/*** type define ***/

/*** static function and variable ***/

/*** class function and variable ***/
    explicit JsvRendererYuv420p() = default;
    virtual ~JsvRendererYuv420p() = default;

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
    static constexpr const int SamplerCount = 3;

/*** class function and variable ***/
    bool prepared = false;

    //shader  yuv变量
    int glvarSamplers[SamplerCount] = {-1};
    int glvarColorConversion = -1;
    std::shared_ptr<std::vector<uint32_t>> glvarTextures;

};

} // namespace plugin
} // namespace jsview
