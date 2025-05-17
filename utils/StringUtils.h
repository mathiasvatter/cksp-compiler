//
// Created by Mathias Vatter on 15.05.25.
//

#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace StringUtils {

/// Splits a string into a vector of substrings based on a delimiter character.
inline std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    if (s.empty()) {
        return tokens; // Return empty vector for an empty input string
    }

    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

/// Joins a vector of strings into a single string using a delimiter character.
inline std::string join(const std::vector<std::string>& elements, char delimiter) {
    if (elements.empty()) {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < elements.size(); ++i) {
        oss << elements[i];
        if (i < elements.size() - 1) {
            oss << delimiter;
        }
    }
    return oss.str();
}

/// Converts a vector of bytes to a hex string representation.
static std::string bytes_to_hex_string(const std::vector<unsigned char>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char byte : bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

/// Converts a given string to lowercase
inline std::string to_lower (std::string s) {
    std::transform (s.begin(), s.end(), s.begin(), [] (auto c) { return static_cast<char> (std::tolower (static_cast<unsigned char> (c))); });
    return s;
}

/// Converts a given string to uppercase
inline std::string to_upper (std::string s) {
    std::transform (s.begin(), s.end(), s.begin(), [] (auto c) { return static_cast<char> (std::toupper (static_cast<unsigned char> (c))); });
    return s;
}





}

