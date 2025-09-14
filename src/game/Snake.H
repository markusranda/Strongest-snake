#pragma once
#include <deque>
#include <vector>
#include <glm/glm.hpp>

struct Snake
{
    std::deque<glm::vec2> cells;
    glm::ivec2 dir = {1, 0};

    double moveTimer = 0.0;
    double interpTimer = 0.0;
    const double moveTimeout = 100.0;

    std::vector<glm::vec2> fromCells;
    std::vector<glm::vec2> toCells;

    void reset(glm::vec2 start)
    {
        cells.clear();
        cells.push_back(start);
        dir = {1, 0};
        moveTimer = moveTimeout;
        interpTimer = 0.0;

        fromCells = {start};
        toCells = {start};
    }

    void act(double delta)
    {
        interpTimer += delta;
        moveTimer -= delta;

        if (moveTimer <= 0)
        {
            fromCells.assign(cells.begin(), cells.end());

            glm::vec2 newHead = cells.front() + glm::vec2(dir);
            cells.push_front(newHead);
            cells.pop_back();

            toCells.assign(cells.begin(), cells.end());

            moveTimer = moveTimeout;
            interpTimer = 0.0;
        }
    }

    void grow()
    {
        if (!fromCells.empty())
        {
            // Use the *old tail* from last frame
            glm::vec2 oldTail = fromCells.back();
            cells.push_back(oldTail);
            toCells.push_back(oldTail);
        }
        else if (!cells.empty())
        {
            // fallback: duplicate last cell
            glm::vec2 tail = cells.back();
            cells.push_back(tail);
            toCells.push_back(tail);
        }
    }

    /// Mutates `out` with interpolated positions
    void getInterpolatedCells(std::vector<glm::vec2> &out)
    {
        out.clear();
        float t = static_cast<float>(interpTimer / moveTimeout);
        if (t > 1.0f)
            t = 1.0f;

        if (!fromCells.empty() && fromCells.size() == toCells.size())
        {
            for (size_t i = 0; i < fromCells.size(); i++)
            {
                out.push_back(glm::mix(fromCells[i], toCells[i], t));
            }
        }
        else
        {
            out.assign(cells.begin(), cells.end());
        }
    }
};
