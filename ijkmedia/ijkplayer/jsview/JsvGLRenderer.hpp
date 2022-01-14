#pragma once

#include <cstdint>
#include <memory>
#include <GLES2/gl2.h>

namespace jsview {
namespace plugin {

class JsvGLRenderer {
public:
/*** type define ***/
    struct TexImageInfo {
        int format;
        int width;
        int height;
        int offset;
    };

/*** static function and variable ***/
    const float* GetBt709ColorMat3();

    static void CheckGLError(const char* funcName);
    static void PrintGLString(const char* name, GLenum value);

    static const int VertexCount;

/*** class function and variable ***/
    explicit JsvGLRenderer() = default;
    virtual ~JsvGLRenderer() = default;

    virtual const char* getVertexShaderSource();
    virtual const char* getFragmentShaderSource() = 0;
    virtual int hasPrepared() = 0;

    virtual int prepare();

    virtual int preDrawFrame(float mvpMat4[16]);
    virtual int drawFrame(float mvpMat4[16],
                          int width, int height,
                          uint8_t* data, int dataSize) = 0;
    virtual int postDrawFrame();

    virtual uint32_t getProgram();

private:
/*** type define ***/

/*** static function and variable ***/
    static std::shared_ptr<uint32_t> LoadGLShader(uint32_t shaderType, const char *shaderSource);
    static std::shared_ptr<uint32_t> MakeGLProgram(std::shared_ptr<uint32_t> vertexShader,
                                                   std::shared_ptr<uint32_t> fragmentShader);
    static void PrintGLShaderInfo(std::shared_ptr<uint32_t> shader);

    static const int CoordsPerVertex;
    static const float PositionMat42[];
    static const float TexCoordMat42[];

    static const int VertexStride;

    /*** class function and variable ***/
    std::shared_ptr<uint32_t> program;
    std::shared_ptr<uint32_t> vertexShader;
    std::shared_ptr<uint32_t> fragmentShader;

    int glMVP = -1;
    int glPosition = -1;
    int glTexCoord = -1;
};

} // namespace plugin
} // namespace jsview
