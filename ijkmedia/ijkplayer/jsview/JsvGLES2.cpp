//
//  Template.cpp
//  JsView-Plugin
//
//  Created by mengxk on 12/22/21.
//  Copyright © 2021 mengxk. All rights reserved.
//

#include "JsvGLES2.hpp"

#include <android/log.h>
#include <GLES2/gl2.h>

namespace jsview {
namespace plugin {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/

/***********************************************/
/***** static function implement ***************/
/***********************************************/
std::shared_ptr<uint32_t> JsvGLES2::MakeGLProgram(std::shared_ptr<uint32_t> vertexShader,
                                                    std::shared_ptr<uint32_t> fragmentShader)
{
    auto creater = [&]() -> uint32_t* {
        GLuint program = glCreateProgram();
        if(program == 0) { return nullptr; }
        auto ptr = new uint32_t(program);
        return ptr;
        return reinterpret_cast<uint32_t*>(program);
    };
    auto deleter = [](uint32_t* ptr) -> void {
        GLuint program = *ptr;
        glDeleteProgram(program);
        delete ptr;
    };
    std::shared_ptr<uint32_t> program = std::shared_ptr<uint32_t>(creater(), deleter);
    CheckGLError("glCreateProgram");
    if (program == nullptr || *program == 0) {
        return nullptr;
    }

    glAttachShader(*program, *vertexShader);
    CheckGLError("glAttachShader(vertex)");
    glAttachShader(*program, *fragmentShader);
    CheckGLError("glAttachShader(fragment)");

    glLinkProgram(*program);
    CheckGLError("glLinkProgram");

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(*program, GL_LINK_STATUS, &linkStatus);
    CheckGLError("glGetProgramiv");
    if (linkStatus == 0) {
        return nullptr;
    }

    return program;

}

std::shared_ptr<uint32_t> JsvGLES2::LoadGLShader(uint32_t shaderType,
                                                   const char *shaderSource)
{
    assert(shaderSource);

    auto creater = [&]() -> uint32_t* {
        GLuint handle = glCreateShader(shaderType);
        if(handle == 0) { return nullptr; }
        auto ptr = new uint32_t(handle);
        return ptr;
    };
    auto deleter = [](uint32_t* ptr) -> void {
        GLuint handle = *ptr;
        glDeleteShader(handle);
        delete ptr;
    };
    std::shared_ptr<uint32_t> shader  = std::shared_ptr<uint32_t>(creater(), deleter);
    CheckGLError("glCreateShader");
    if (shader == nullptr) {
        return nullptr;
    }

    glShaderSource(*shader, 1, &shaderSource, NULL);
    CheckGLError("glShaderSource");

    glCompileShader(*shader);
    CheckGLError("glCompileShader");

    GLint compileStatus = 0;
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &compileStatus);
    CheckGLError("glGetShaderiv");
    if (compileStatus == 0) {
        return nullptr;
    }
    PrintGLShaderInfo(shader);

    return shader;
}

void JsvGLES2::PrintGLShaderInfo(std::shared_ptr<uint32_t> shader)
{
    // TODO
}

void JsvGLES2::CheckGLError(const char* funcName) {
    for (GLint error = glGetError(); error; error = glGetError()) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "GLError: after %s(), errorCode: 0x%x", funcName, error);
    }
}

/***********************************************/
/***** class public function implement  ********/
/***********************************************/
int JsvGLES2::prepare()
{
    auto vertexShaderSource = getVertexShaderSource();
    vertexShader = LoadGLShader(GL_VERTEX_SHADER, vertexShaderSource);
    if(vertexShader == nullptr) {
        return -1;
    }

    auto fragmentShaderSource = getFragmentShaderSource();
    fragmentShader = LoadGLShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if(fragmentShader == nullptr) {
        return -1;
    }

    this->program = MakeGLProgram(this->vertexShader,
                                  this->fragmentShader);
    if(this->program == nullptr) {
        return -1;
    }

    return 0;
}

/***********************************************/
/***** class protected function implement  *****/
/***********************************************/

/***********************************************/
/***** class private function implement  *******/
/***********************************************/

} // namespace plugin
} // namespace jsview
