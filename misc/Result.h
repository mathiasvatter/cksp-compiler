//
// Created by Mathias Vatter on 29.08.23.
//

#pragma once

#include <variant>

#include "Diagnostic.h"

struct SuccessTag {};

/**
 * @class Result
 * @brief A simple Result type to represent either a successful value of type T or a Diagnostic.
 * Provides utility functions to check if the result is an error and to unwrap the value or get the error.
 */
template<typename T>
class Result {
public:
    std::variant<T, Diagnostic> value;

    inline explicit Result(T val) : value(std::move(val)) {}
    inline explicit Result(Diagnostic err) : value(std::move(err)) {}

    [[nodiscard]] inline bool is_error() const {
        return std::holds_alternative<Diagnostic>(value);
    }

    inline T& unwrap() {
        if(is_error()) {
            std::get<Diagnostic>(value).exit();
        }
        return std::get<T>(value);
    }

    [[nodiscard]] inline Diagnostic& get_error() {
        if(!is_error()) {
            throw std::runtime_error("Attempt to get error from a successful result");
        }
        return std::get<Diagnostic>(value);
    }

private:

};
