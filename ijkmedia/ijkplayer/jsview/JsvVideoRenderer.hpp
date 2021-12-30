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

/*** class function and variable ***/

    std::mutex mutex;
    std::shared_ptr<AVFrame> frame;
    std::shared_ptr<uint8_t> data[BLOCK_COUNT];
};

} // namespace plugin
} // namespace jsview
