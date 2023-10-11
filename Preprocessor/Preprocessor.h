//
// Created by Mathias Vatter on 23.09.23.
//

#pragma once
#include <unordered_set>
#include <utility>

#include "../Tokenizer/Tokenizer.h"
#include "../AST.h"
#include "../Parser.h"

class Preprocessor : public Parser {
public:
    Preprocessor(std::vector<Token> tokens, std::string current_file);
	~Preprocessor() = default;
    std::vector<Token> get_tokens();
    void process();
protected:
    std::string m_current_file;

    void remove_last();
    void remove_tokens(size_t start, size_t end);


private:


};



