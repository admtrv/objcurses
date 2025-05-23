/*
 * tools.h
 */

#pragma once

#include <optional>
#include <string>

// safe operators of conversion
std::optional<int> safe_stoi(const std::string &token);      // from string to int
std::optional<float> safe_stof(const std::string &token);    // from string to float