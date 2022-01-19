//
//  Template.cpp
//  JsView-Plugin
//
//  Created by mengxk on 12/22/21.
//  Copyright © 2021 mengxk. All rights reserved.
//

#include "JsvRendererQComYuv420sp.hpp"

#include <android/log.h>
#include <GLES2/gl2.h>

#include <unistd.h>


namespace jsview {
namespace plugin {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/

/***********************************************/
/***** static function implement ***************/
/***********************************************/

/***********************************************/
/***** class public function implement  ********/
/***********************************************/
const char* JsvRendererQComYuv420sp::getVertexShaderSource()
{
    return JsvRendererYuv420sp::getVertexShaderSource();
}

const char* JsvRendererQComYuv420sp::getFragmentShaderSource()
{
    static constexpr const char* source =
        "precision highp float;"
        "varying   highp vec2 vv2_Texcoord;"
        "uniform         mat3 um3_ColorConversion;"
        "uniform   lowp  sampler2D us2_SamplerY;"
        "uniform   lowp  sampler2D us2_SamplerUV;"
        ""
        "void main()"
        "{"
        "    mediump vec3 yuv;"
        "    lowp    vec3 rgb;"
        ""
        "    yuv.x  = texture2D(us2_SamplerY,  vv2_Texcoord).r  - 0.062745;" // 0.062745 = (16.0 / 255.0)
        "    yuv.yz = texture2D(us2_SamplerUV, vv2_Texcoord).ar - vec2(0.5, 0.5);" // UV => VU
        "    rgb = um3_ColorConversion * yuv;"
        "    gl_FragColor = vec4(rgb, 1);"
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
