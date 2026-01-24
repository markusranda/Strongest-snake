// Needs to be included for stb_image to work, this should only be defined here
#define STB_IMAGE_IMPLEMENTATION

#include "Logrador.h"
#include "Window.h"
#include "Game.h"
#include "Globals.h"

// ______________________________
//         TRACY TIME
// ______________________________
#ifdef _DEBUG
#include "tracy/Tracy.hpp"
#include <cstdlib>
#include <new>

void *operator new(std::size_t sz)
{
    void *ptr = std::malloc(sz);
    TracyAlloc(ptr, sz); // Tracy tracks it
    if (!ptr)
        throw std::bad_alloc();
    return ptr;
}

void operator delete(void *ptr) noexcept
{
    TracyFree(ptr); // Tracy sees the free
    std::free(ptr);
}
#endif


int main() {
    Logrador::info("Launching Strongest Snake");

    try {
        InitGlobals();
        
        Game game = {};
        game.init();
        game.run();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
        _sleep(5000);
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
