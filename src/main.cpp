// Needs to be included for stb_image to work, this should only be defined here
#define STB_IMAGE_IMPLEMENTATION

#include "engine/Engine.h"
#include "engine/Window.h"
#include "game/Game.h"
#include "Logger.h"

const uint32_t WIDTH = 1920;
const uint32_t HEIGHT = 1080;

int main()
{
    Logger::info("Launching Strongest Snake");

    Window window = Window(WIDTH, HEIGHT, "StrongestSnake");
    Game game{window};
    game.init();
    game.run();

    return EXIT_SUCCESS;
}
