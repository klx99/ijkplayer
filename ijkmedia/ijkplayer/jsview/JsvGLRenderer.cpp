//
//  Template.cpp
//  JsView-Plugin
//
//  Created by mengxk on 12/22/21.
//  Copyright © 2021 mengxk. All rights reserved.
//

#include "JsvGLRenderer.hpp"

#include <android/log.h>
#include <GLES2/gl2.h>

namespace jsview {
namespace plugin {

/***********************************************/
/***** static variables initialize *************/
/***********************************************/
const int JsvGLRenderer::CoordsPerVertex = 2;

const float JsvGLRenderer::PositionMat42[] = {
        -1.0f, -1.0f, // bottom left
        1.0f, -1.0f,  // bottom right
        -1.0f, 1.0f,  // top left
        1.0f, 1.0f,   // top right
};

const float JsvGLRenderer::TexCoordMat42[] = {
        0.0f, 1.0f, // bottom left
        1.0f, 1.0f, // bottom right
        0.0f, 0.0f, // top left
        1.0f, 0.0f, // top right
};

const int JsvGLRenderer::VertexCount =
        sizeof(PositionMat42) / sizeof(*PositionMat42) / CoordsPerVertex;
const int JsvGLRenderer::VertexStride = CoordsPerVertex * 4; // 4 bytes per vertex


/***********************************************/
/***** static function implement ***************/
/***********************************************/
std::shared_ptr<std::vector<uint32_t>> JsvGLRenderer::MakeTextures(int count)
{
    assert(shaderSource);

    auto creater = [&count]() -> std::vector<uint32_t>* {
        auto ptr = new std::vector<uint32_t>(count, 0);
        glGenTextures(count, ptr->data());
        return ptr;
    };
    auto deleter = [count](std::vector<uint32_t>* ptr) -> void {
        glDeleteTextures(count, ptr->data());
        delete ptr;
    };
    auto textures  = std::shared_ptr<std::vector<uint32_t>>(creater(), deleter);
    CheckGLError("glCreateShader");
//    if (shader == nullptr) {
//        return nullptr;
//    }

    return textures;
}

const float* JsvGLRenderer::GetBt709ColorMat3()
{
    static constexpr const GLfloat bt709[] = {
            1.164,  1.164,  1.164,
            0.0,   -0.213,  2.112,
            1.793, -0.533,  0.0,
    };

    return bt709;
}

std::shared_ptr<uint32_t> JsvGLRenderer::MakeGLProgram(std::shared_ptr<uint32_t> vertexShader,
                                                    std::shared_ptr<uint32_t> fragmentShader)
{
    PrintGLString("Version", GL_VERSION);
    PrintGLString("Vendor", GL_VENDOR);
    PrintGLString("Renderer", GL_RENDERER);
    PrintGLString("Extensions", GL_EXTENSIONS);

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

std::shared_ptr<uint32_t> JsvGLRenderer::LoadGLShader(uint32_t shaderType,
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

void JsvGLRenderer::PrintGLShaderInfo(std::shared_ptr<uint32_t> shader)
{
    // TODO
}

void JsvGLRenderer::CheckGLError(const char* funcName) {
    GLint error = 0;
    for (error = glGetError(); error; error = glGetError()) {
        __android_log_print(ANDROID_LOG_ERROR, "JsView", "GLError: after %s(), errorCode: 0x%x", funcName, error);
    }
    if(error) {
//        throw std::runtime_error("Failed to run GLES.");
    }
}

void JsvGLRenderer::PrintGLString(const char* name, GLenum value) {
    const char *v = (const char *) glGetString(value);
    __android_log_print(ANDROID_LOG_ERROR, "JsView", "[GLES2] %s = %s", name, v);
}

/***********************************************/
/***** class public function implement  ********/
/***********************************************/
JsvGLRenderer::JsvGLRenderer()
{
}

JsvGLRenderer::~JsvGLRenderer()
{
    vertexShader.reset();
    fragmentShader.reset();
    program.reset();
}

const char* JsvGLRenderer::getVertexShaderSource()
{
    static constexpr const char* source =
        "precision highp float;"
        "varying   highp vec2 vv2_Texcoord;"
        "attribute highp vec4 av4_Position;"
        "attribute highp vec2 av2_Texcoord;"
        "uniform         mat4 um4_ModelViewProjection;"
        ""
        "void main()"
        "{"
        "    gl_Position  = um4_ModelViewProjection * av4_Position;"
        "    vv2_Texcoord = av2_Texcoord.xy;"
        "}";

    return source;
}

int JsvGLRenderer::prepare()
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

    glMVP = glGetUniformLocation(*this->program, "um4_ModelViewProjection");
    CheckGLError("glGetUniformLocation");
    glPosition = glGetAttribLocation(*this->program, "av4_Position");
    glTexCoord = glGetAttribLocation(*this->program, "av2_Texcoord");
    CheckGLError("glGetAttribLocation");

    return 0;
}

int JsvGLRenderer::preDrawFrame(float mvpMat4[16])
{
    glUseProgram(*this->program);
    CheckGLError("glUseProgram");

    glUniformMatrix4fv(glMVP, 1, GL_FALSE, mvpMat4);
    CheckGLError("glUniformMatrix4fv");

    glEnableVertexAttribArray(glPosition);
    glVertexAttribPointer(glPosition, CoordsPerVertex, GL_FLOAT, GL_FALSE, VertexStride, PositionMat42);
    glEnableVertexAttribArray(glTexCoord);
    glVertexAttribPointer(glTexCoord, CoordsPerVertex, GL_FLOAT, GL_FALSE, VertexStride, TexCoordMat42);
    CheckGLError("glVertexAttribPointer");

    return 0;
}

int JsvGLRenderer::postDrawFrame()
{
    glDisableVertexAttribArray(glPosition);
    glDisableVertexAttribArray(glTexCoord);

    return 0;
}

uint32_t JsvGLRenderer::getProgram()
{
    return *program;
}

/***********************************************/
/***** class protected function implement  *****/
/***********************************************/

/***********************************************/
/***** class private function implement  *******/
/***********************************************/

} // namespace plugin
} // namespace jsview
