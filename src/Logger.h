#pragma once
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>

enum class LogLevel
{
        INFO,
        WARN,
        ERROR
};

class Logger
{
public:
        static void debug(const std::string &msg)
        {
                // timestamp
                auto now = std::chrono::system_clock::now();
                auto time_t_now = std::chrono::system_clock::to_time_t(now);
                std::tm tm_now{};
#ifdef _WIN32
                localtime_s(&tm_now, &time_t_now); // Windows
#else
                localtime_r(&time_t_now, &tm_now); // Linux
#endif
                std::ostringstream ts;
                ts << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");

                const char *lvl = "DEBUG";
                std::cout << "[" << lvl << "]" << "[" << ts.str() << "] " << msg << std::endl;
        }

        static void info(const std::string &msg)
        {
                // timestamp
                auto now = std::chrono::system_clock::now();
                auto time_t_now = std::chrono::system_clock::to_time_t(now);
                std::tm tm_now{};
#ifdef _WIN32
                localtime_s(&tm_now, &time_t_now); // Windows
#else
                localtime_r(&time_t_now, &tm_now); // Linux
#endif
                std::ostringstream ts;
                ts << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");

                const char *lvl = "INFO";
                std::cout << "[" << lvl << "]" << "[" << ts.str() << "] " << msg << std::endl;
        }

        static void warn(const std::string &msg)
        {
                // timestamp
                auto now = std::chrono::system_clock::now();
                auto time_t_now = std::chrono::system_clock::to_time_t(now);
                std::tm tm_now{};
#ifdef _WIN32
                localtime_s(&tm_now, &time_t_now); // Windows
#else
                localtime_r(&time_t_now, &tm_now); // Linux
#endif
                std::ostringstream ts;
                ts << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");

                const char *lvl = "WARN";
                std::cout << "[" << lvl << "]" << "[" << ts.str() << "] " << msg << std::endl;
        }

        static void err(const std::string &msg)
        {
                // timestamp
                auto now = std::chrono::system_clock::now();
                auto time_t_now = std::chrono::system_clock::to_time_t(now);
                std::tm tm_now{};
#ifdef _WIN32
                localtime_s(&tm_now, &time_t_now); // Windows
#else
                localtime_r(&time_t_now, &tm_now); // Linux
#endif
                std::ostringstream ts;
                ts << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");

                const char *lvl = "ERROR";
                std::cerr << "[" << lvl << "]" << "[" << ts.str() << "] " << msg << std::endl;
        }
};
