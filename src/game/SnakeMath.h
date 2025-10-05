#include <cmath>

namespace SnakeMath
{
    constexpr float PI = 3.14159265358979323846f;
    inline float fastSin(float x)
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
    inline float fastCos(float x)
    {
        return fastSin(x + SnakeMath::PI * 0.5f);
    }
}
