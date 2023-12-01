//
// Created by Mathias Vatter on 29.08.23.
//

#pragma once
#include "CompileError.h"

struct SuccessTag {};

template<typename T>
class Result {
public:
    std::variant<T, CompileError> value;

    inline explicit Result(T val) : value(std::move(val)) {}
    inline explicit Result(CompileError err) : value(std::move(err)) {}

    [[nodiscard]] inline bool is_error() const {
        return std::holds_alternative<CompileError>(value);
    }

    inline T& unwrap() {
        if(is_error()) {
            std::get<CompileError>(value).print();
            throw std::runtime_error("Attempt to unwrap error!");
        }
        return std::get<T>(value);
    }

    [[nodiscard]] inline CompileError& get_error() {
        if(!is_error()) {
            throw std::runtime_error("Attempt to get error from a successful result");
        }
        return std::get<CompileError>(value);
    }

private:

};
