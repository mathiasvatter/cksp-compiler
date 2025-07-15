//
// Created by Mathias Vatter on 15.05.25.
//

#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace StringUtils {

inline bool is_whitespace (char c) { return c == ' ' || (c <= 13 && c >= 9); }
inline bool is_digit (char c) { return static_cast<uint32_t> (c - '0') < 10; }

inline long count_char(const std::string& str, char c) {
	return std::ranges::count(str, c);
}

/// Replaces all occurrences of a one or more substrings.
/// The arguments must be a sequence of pairs of strings, where the first of each pair is the string to
/// look for, followed by its replacement.
template <typename StringType, typename... OtherReplacements>
[[nodiscard]] std::string replace (StringType text_to_search,
                                   std::string_view first_substring_to_replace, std::string_view first_replacement,
                                   OtherReplacements&&... other_pairs_of_strings_to_replace) {
    static_assert ((sizeof... (other_pairs_of_strings_to_replace) & 1u) == 0,
                   "This function expects a list of pairs of strings as its arguments");

    if constexpr (std::is_same<const StringType, const std::string_view>::value || std::is_same<const StringType, const char* const>::value) {
        return replace (std::string (text_to_search), first_substring_to_replace, first_replacement,
                        std::forward<OtherReplacements> (other_pairs_of_strings_to_replace)...);
    } else if constexpr (sizeof... (other_pairs_of_strings_to_replace) == 0) {
        size_t pos = 0;

        for (;;) {
            pos = text_to_search.find (first_substring_to_replace, pos);

            if (pos == std::string::npos)
                return text_to_search;

            text_to_search.replace (pos, first_substring_to_replace.length(), first_replacement);
            pos += first_replacement.length();
        }
    } else {
        return replace (replace (std::move (text_to_search), first_substring_to_replace, first_replacement),
                        std::forward<OtherReplacements> (other_pairs_of_strings_to_replace)...);
    }
}

/// Returns a string with any whitespace trimmed from its start.
[[nodiscard]] inline std::string trim_start (std::string text_to_trim) {
    auto i = text_to_trim.begin();

    if (i == text_to_trim.end())        return {};
    if (! is_whitespace (*i))    return text_to_trim;

    for (;;) {
        ++i;
        if (i == text_to_trim.end())        return {};
        if (! is_whitespace (*i))    return { i, text_to_trim.end() };
    }
}

/// Returns a string with any whitespace trimmed from its start.
[[nodiscard]] inline std::string_view trim_start (std::string_view text_to_trim) {
    size_t i = 0;

    for (auto c : text_to_trim) {
        if (! is_whitespace (c)) {
            text_to_trim.remove_prefix (i);
            return text_to_trim;
        }
        ++i;
    }
    return {};
}

/// Returns a string with any whitespace trimmed from its end.
[[nodiscard]] inline std::string trim_end (std::string text_to_trim) {
    for (auto i = text_to_trim.end();;) {
        if (i == text_to_trim.begin())
            return {};
        --i;
        if (! is_whitespace (*i)) {
            text_to_trim.erase (i + 1, text_to_trim.end());
            return text_to_trim;
        }
    }
}

/// Returns a string with any whitespace trimmed from its end.
[[nodiscard]] inline std::string_view trim_end (std::string_view text_to_trim) {
    for (auto i = text_to_trim.length(); i != 0; --i)
        if (! is_whitespace (text_to_trim[i - 1]))
            return text_to_trim.substr (0, i);

    return {};
}

/// Returns a string with any whitespace trimmed from its start and end.
[[nodiscard]] inline std::string trim (std::string text_to_trim) { return trim_start (trim_end (std::move (text_to_trim))); }

/// Returns a string with any whitespace trimmed from its start and end.
[[nodiscard]] inline std::string_view trim (std::string_view text_to_trim) { return trim_start (trim_end (std::move (text_to_trim))); }

/// Returns true if this text contains the given sub-string.
inline bool contains(std::string_view t, std::string_view s) { return t.find (s) != std::string::npos; }
/// Returns true if this text starts with the given character.
inline bool starts_with (std::string_view t, char s) { return ! t.empty() && t.front() == s; }
/// Returns true if this text starts with the given sub-string.
inline bool starts_with (std::string_view t, std::string_view s) {
    auto len = s.length();
    return t.length() >= len && t.substr (0, len) == s;
}
/// Returns true if this text ends with the given sub-string.
inline bool ends_with (std::string_view t, char s) { return ! t.empty() && t.back()  == s; }
/// Returns true if this text ends with the given sub-string.
inline bool ends_with (std::string_view t, std::string_view s)
{
    auto len1 = t.length(), len2 = s.length();
    return len1 >= len2 && t.substr (len1 - len2) == s;
}

/// If the given character is at the start and end of the string, it trims it away.
[[nodiscard]] inline std::string remove_outer_character (std::string t, char outer_char)
{
    if (t.length() >= 2 && t.front() == outer_char && t.back() == outer_char)
        return t.substr (1, t.length() - 2);

    return t;
}
[[nodiscard]] inline std::string remove_double_quotes (std::string text) {
    return remove_outer_character (std::move (text), '"');
}
[[nodiscard]] inline std::string remove_single_quotes (std::string text) { return remove_outer_character (std::move (text), '\''); }

/// Removes a single pair of outer quotes (single or double) if present.
inline std::string remove_quotes(std::string text) {
    if (text.length() >= 2) {
        if (text.front() == '"' && text.back() == '"') {
            return text.substr(1, text.length() - 2);
        }
        if (text.front() == '\'' && text.back() == '\'') {
            return text.substr(1, text.length() - 2);
        }
    }
    return text;
}

[[nodiscard]] inline std::string add_double_quotes (std::string text) { return "\"" + std::move (text) + "\""; }
[[nodiscard]] inline std::string add_single_quotes (std::string text) { return "'" + std::move (text) + "'"; }


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

template<typename Container, typename Protector = std::string, typename Proj>
std::string join_apply(const Container& elements, Proj proj, const std::string& delimiter = ", ") {
    std::ostringstream oss;
    auto it  = std::begin(elements);
    auto end = std::end(elements);
    if (it == end) return "";

    // erstes Element
    oss << proj(*it);
    ++it;

    // Rest mit Delimiter
    for (; it != end; ++it) {
        oss << delimiter << proj(*it);
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

/// Removes the first occurrence of a substring from a string.
inline std::string remove(const std::string& str, const std::string& substring) {
    // Suche nach dem Substring im Hauptstring
    // Wenn der Substring gefunden wurde, entferne ihn
    if (const size_t pos = str.find(substring); pos != std::string::npos) {
        auto new_str = str;
        new_str.erase(pos, substring.length());
        return new_str;
    }
    return str;
}

/// Encodes a string as a legal URI, using percent-encoding (aka URL encoding)
inline std::string percent_encode_uri (std::string_view text) {
    std::string result;
    result.reserve (text.length());

    for (auto c : text) {
        if (std::string_view ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-.~").find (c) != std::string_view::npos) {
            result += c;
        } else {
            result += '%';
            result += "0123456789abcdef"[static_cast<uint8_t> (c) >> 4];
            result += "0123456789abcdef"[static_cast<uint8_t> (c) & 15u];
        }
    }
    return result;
}

/// Returns a truncated, easy-to-read version of a time as hours, seconds or milliseconds,
/// depending on its magnitude.
inline std::string get_duration_description (std::chrono::duration<double, std::micro> d) {
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds> (d).count();

    if (microseconds < 0)    return "-" + get_duration_description (-d);
    if (microseconds == 0)   return "0 sec";

    std::string result;

    auto add_level = [&] (int64_t size, std::string_view units, int64_t decimal_scale, int64_t modulo) -> bool {
        if (microseconds < size) return false;

        if (! result.empty()) result += ' ';

        auto scaled = (microseconds * decimal_scale + size / 2) / size;
        auto whole = scaled / decimal_scale;

        if (modulo != 0) whole = whole % modulo;

        result += std::to_string (whole);

        if (auto fraction = scaled % decimal_scale) {
            result += '.';
            result += static_cast<char> ('0' + (fraction / 10));

            if (fraction % 10 != 0)
                result += static_cast<char> ('0' + (fraction % 10));
        }

        result += (whole == 1 && units.length() > 3 && units.back() == 's') ? units.substr (0, units.length() - 1) : units;
        return true;
    };

    bool hours = add_level (60000000ll * 60ll, " hours", 1, 0);
    bool mins  = add_level (60000000ll,        " min", 1, hours ? 60 : 0);

    if (hours) return result;

    if (mins) {
        add_level (1000000, " sec", 1, 60);
    } else {
        if (! add_level (1000000,   " sec", 100, 0))
            if (! add_level (1000,  " ms", 100, 0))
                add_level (1,       " microseconds", 100, 0);
    }

    return result;
}

/// Returns an easy-to-read description of a size in bytes.
inline std::string get_byte_size_description (uint64_t size) {
    auto int_to_string_with_1_dec_place = [] (uint64_t n, uint64_t divisor) -> std::string {
        auto scaled = (n * 10 + divisor / 2) / divisor;
        auto result = std::to_string (scaled / 10);

        if (auto fraction = scaled % 10) {
            result += '.';
            result += static_cast<char> ('0' + fraction);
        }

        return result;
    };

    static constexpr uint64_t max_value = std::numeric_limits<uint64_t>::max() / 10;

    if (size >= 0x40000000)  return int_to_string_with_1_dec_place (std::min (max_value, size), 0x40000000) + " GB";
    if (size >= 0x100000)    return int_to_string_with_1_dec_place (size, 0x100000) + " MB";
    if (size >= 0x400)       return int_to_string_with_1_dec_place (size, 0x400)    + " KB";
    if (size != 1)           return std::to_string (size) + " bytes";

    return "1 byte";
}

/// Calculates the Levenshtein distance between two strings.
template <typename StringType>
size_t get_levenshtein_distance (const StringType& string1, const StringType& string2) {
    if (string1.empty())  return string2.length();
    if (string2.empty())  return string1.length();

    auto calculate = [] (size_t* costs, size_t num_costs, const StringType& s1, const StringType& s2) -> size_t {
        for (size_t i = 0; i < num_costs; ++i)
            costs[i] = i;

        size_t p1 = 0;

        for (auto c1 : s1) {
            auto corner = p1;
            *costs = p1 + 1;
            size_t p2 = 0;

            for (auto c2 : s2) {
                auto upper = costs[p2 + 1];
                costs[p2 + 1] = c1 == c2 ? corner : (std::min (costs[p2], std::min (upper, corner)) + 1);
                ++p2;
                corner = upper;
            }
            ++p1;
        }
        return costs[num_costs - 1];
    };

    auto size_needed = string2.length() + 1;
    constexpr size_t max_stack_size = 96;

    if (size_needed <= max_stack_size) {
        size_t costs[max_stack_size];
        return calculate (costs, size_needed, string1, string2);
    }

    std::unique_ptr<size_t[]> costs (new size_t[size_needed]);
    return calculate (costs.get(), size_needed, string1, string2);
}





}

