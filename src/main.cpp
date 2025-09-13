#include "renderer/ComputeShaderApplication.h"
#include "Logger.h"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

int main()
{
    Logger::info("Launching Strongest Snake");
    ComputeShaderApplication app = ComputeShaderApplication(WIDTH, HEIGHT);

    try
    {
        app.run();
    }
    catch (const std::exception &e)
    {
        Logger::err(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
