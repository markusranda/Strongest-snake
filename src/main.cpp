#include "engine/Engine.h"
#include "Logger.h"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

int main()
{
    Logger::info("Launching Strongest Snake");
    Engine app = Engine(WIDTH, HEIGHT);

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
