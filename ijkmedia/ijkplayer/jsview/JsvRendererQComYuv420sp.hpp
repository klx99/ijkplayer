#pragma once

#include "JsvRendererYuv420sp.hpp"

namespace jsview {
namespace plugin {

class JsvRendererQComYuv420sp : public JsvRendererYuv420sp {
public:
/*** type define ***/

/*** static function and variable ***/

/*** class function and variable ***/
    explicit JsvRendererQComYuv420sp() = default;
    virtual ~JsvRendererQComYuv420sp() = default;

    virtual const char* getVertexShaderSource() override;
    virtual const char* getFragmentShaderSource() override;

private:
/*** type define ***/

/*** static function and variable ***/

/*** class function and variable ***/

    //shader  yuv变量
};

} // namespace plugin
} // namespace jsview
