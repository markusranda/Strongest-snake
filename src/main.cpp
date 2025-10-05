#include "engine/Engine.h"
#include "engine/Window.h"
#include "game/Game.h"
#include "Logger.h"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 800;

int main()
{
    Logger::info("Launching Strongest Snake");

    Window window = Window(WIDTH, HEIGHT, "StrongestSnake");
    Game game{window};
    game.init();
    game.run();

    return EXIT_SUCCESS;
}
