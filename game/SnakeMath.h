#include <cmath>

namespace SnakeMath
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = 2.0f * PI;
    static std::mt19937 rng(std::random_device{}());

    // TODO Fix before use
    inline float fSin(float x)
    {
        // normalize to -PI..PI
        x = fmodf(x + PI, 2 * PI) - PI;
        const float B = 4 / PI;
        const float C = -4 / (PI * PI);
        float y = B * x + C * x * fabsf(x);
        const float P = 0.225f;
        y = P * (y * fabsf(y) - y) + y;
        return y;
    }

    // TODO Fix before use
    inline float fCos(float x)
    {
        return fSin(x + SnakeMath::PI * 0.5f);
    }

    inline glm::vec2 getRotationVector2(float rotation)
    {
        return glm::vec2(cos(rotation), sin(rotation));
    }

    // TODO Fix before use
    inline float fMod(float a, float b)
    {
        int val = (int)(a * 100000) % (int)(b * 100000);
        return val / 1000.0f;
    }

    inline bool chance(double probability)
    {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng) < probability;
    }

    inline float randomBetween(float min, float max)
    {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng);
    }
}
