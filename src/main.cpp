#include "engine/Engine.h"
#include "engine/Window.h"
#include "game/Game.h"
#include "Logger.h"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

int main()
{
    Logger::info("Launching Strongest Snake");
    Window window = Window(WIDTH, HEIGHT, "StrongestSnake");
    Engine engine = Engine(WIDTH, HEIGHT, window);
    Game game = Game(window, engine);

    try
    {
        game.run();
    }
    catch (const std::exception &e)
    {
        Logger::err(e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
