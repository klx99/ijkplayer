#pragma once

#include <cstdint>
#include <memory>

namespace jsview {
namespace plugin {

class JsvGLRenderer {
public:
/*** type define ***/

/*** static function and variable ***/
    static void CheckGLError(const char* funcName);

/*** class function and variable ***/
    explicit JsvGLRenderer() = default;
    virtual ~JsvGLRenderer() = default;

    virtual const char* getVertexShaderSource() = 0;
    virtual const char* getFragmentShaderSource() = 0;

    virtual int prepare();
    virtual int hasPrepared() = 0;

    virtual uint32_t getProgram() {
        return *program;
    }

private:
/*** type define ***/

/*** static function and variable ***/
    static std::shared_ptr<uint32_t> LoadGLShader(uint32_t shaderType, const char *shaderSource);
    static std::shared_ptr<uint32_t> MakeGLProgram(std::shared_ptr<uint32_t> vertexShader,
                                                   std::shared_ptr<uint32_t> fragmentShader);
    static void PrintGLShaderInfo(std::shared_ptr<uint32_t> shader);

/*** class function and variable ***/
    std::shared_ptr<uint32_t> program;
    std::shared_ptr<uint32_t> vertexShader;
    std::shared_ptr<uint32_t> fragmentShader;
};

} // namespace plugin
} // namespace jsview
