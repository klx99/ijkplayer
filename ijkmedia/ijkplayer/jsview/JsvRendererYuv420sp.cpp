//
//  Template.cpp
//  JsView-Plugin
//
//  Created by mengxk on 12/22/21.
//  Copyright © 2021 mengxk. All rights reserved.
//

#include "JsvRendererYuv420sp.hpp"

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
int JsvRendererYuv420sp::prepare() {
    int ret = JsvGLRenderer::prepare();
    if (ret < 0) {
        return ret;
    }

    auto program = getProgram();

    glSamplers[0] = glGetUniformLocation(program, "us2_SamplerX");
    glSamplers[1] = glGetUniformLocation(program, "us2_SamplerY");
    glColorConversion = glGetUniformLocation(program, "um3_ColorConversion");
    CheckGLError("glGetUniformLocation");

    //创建3个纹理
    glGenTextures(SamplerCount, glTextures);

    //绑定纹理
    for (int id : glTextures) {
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        CheckGLError("glTexParameter");
    }

    prepared = true;

    return 0;
}

int JsvRendererYuv420sp::hasPrepared() {
    return prepared;
}


int JsvRendererYuv420sp::drawFrame(float mvpMat4[16],
                                   int width, int height,
                                   uint8_t *data, int dataSize) {
    int ret = JsvGLRenderer::preDrawFrame(mvpMat4);
    if(ret < 0) {
        return ret;
    }

    TexImageInfo texInfo[SamplerCount] = {
            {.format = GL_LUMINANCE, .width = width, .height = height, .offset = 0},
            {.format = GL_LUMINANCE_ALPHA, .width = width / 2, .height = height / 2, .offset =
            width * height},
    };
    for (int idx = 0; idx < SamplerCount; idx++) {
        glActiveTexture(GL_TEXTURE0 + idx);
        glBindTexture(GL_TEXTURE_2D, glTextures[idx]);
        glTexImage2D(GL_TEXTURE_2D,
                     0, texInfo[idx].format,
                     texInfo[idx].width, texInfo[idx].height,
                     0, texInfo[idx].format,
                     GL_UNSIGNED_BYTE, data + texInfo[idx].offset);
        CheckGLError("glTexImage2D");

        glUniform1i(glSamplers[idx], idx);
        CheckGLError("glUniform1i");
    }

    glUniformMatrix3fv(glColorConversion, 1, GL_FALSE, GetBt709ColorMat3());

    glDrawArrays(GL_TRIANGLE_STRIP, 0, VertexCount);

    ret = JsvGLRenderer::postDrawFrame();
    if(ret < 0) {
        return ret;
    }

    return 0;
}

const char *JsvRendererYuv420sp::getVertexShaderSource()
{
    return JsvGLRenderer::getVertexShaderSource();
}

const char* JsvRendererYuv420sp::getFragmentShaderSource()
{
    static constexpr const char* source =
        "precision highp float;"
        "varying   highp vec2 vv2_Texcoord;"
        "uniform         mat3 um3_ColorConversion;"
        "uniform   lowp  sampler2D us2_SamplerX;"
        "uniform   lowp  sampler2D us2_SamplerY;"
        ""
        "void main()"
        "{"
        "    mediump vec3 yuv;"
        "    lowp    vec3 rgb;"
        ""
        "    yuv.x  = (texture2D(us2_SamplerX,  vv2_Texcoord).r  - (16.0 / 255.0));"
        "    yuv.yz = (texture2D(us2_SamplerY,  vv2_Texcoord).ra - vec2(0.5, 0.5));"
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
