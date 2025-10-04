#pragma once
#include "../engine/Engine.h"
#include "../engine/Window.h"
#include "../engine/Vertex.h"
#include <deque>
#include "EntityManager.h"

struct Transform2D
{
    glm::vec2 position;
    glm::vec2 size;
    glm::vec2 dir = glm::vec2(0.0f, 0.0f);
};

struct Ground
{
    Entity entity;
    bool dead;
};

struct Player
{
    Entity entity;
};

struct Background
{
    Entity entity;
};

struct GameState
{
    EntityManager entityManager;

    bool gameOver = false;
    Background background;
    Player player;
    std::unordered_map<Entity, Transform2D> transforms;
    std::unordered_map<Entity, Quad> quads;
    std::unordered_map<Entity, Ground> grounds;

    uint32_t rows = 20;
    uint32_t columns = 20;
    uint32_t cellSize = 1;

    float holdTimer = 0;
};

class Game
{
public:
    Game(Window &window);

    void run();

private:
    Window window;
    Engine engine;

    float lastFrameTime = 0.0f;
    double lastTime = 0.0f;

    float cellW;
    float cellH;
    GameState state;
    EntityManager entityManager;

    void updateGame(double delta);
    void Game::updateDirection();
    void Game::updateGraphics();
    void Game::checkCollision();
};