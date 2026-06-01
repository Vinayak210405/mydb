#pragma once
#include <string>
#include <vector>
#include "Token.h"

class Lexer {
public:
    explicit Lexer(const std::string &input);
    std::vector<Token> tokenize();

private:
    char peek() const;
    char advance();
    bool at_end() const;
    void skip_whitespace();

    Token read_string();
    Token read_number();
    Token read_ident_or_keyword();

    std::string input_;
    size_t      pos_ = 0;
};
