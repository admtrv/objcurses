/*
 * tools.cpp
 */

#include "tools.h"

std::optional<int> safe_stoi(const std::string &token)
{
    try {
        size_t pos = 0;
        int v = std::stoi(token, &pos, 10);
        if (pos != token.size())
            return std::nullopt;
        return v;
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}

std::optional<float> safe_stof(const std::string &token)
{
    try {
        size_t pos = 0;
        float v = std::stof(token, &pos);
        if (pos != token.size())
            return std::nullopt;
        return v;
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}