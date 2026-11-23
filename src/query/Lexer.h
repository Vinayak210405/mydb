#pragma once
#include <string>
#include <vector>
#include "Token.h"

// Turns SQL text into tokens. Keywords are case-insensitive; `--` starts a
// comment that runs to the end of the line. Throws std::runtime_error on an
// unterminated string or an unexpected character.
class Lexer {
public:
    explicit Lexer(const std::string &input);
    std::vector<Token> tokenize();

private:
    char peek(size_t ahead = 0) const;
    char advance();
    bool at_end() const;
    void skip_whitespace_and_comments();

    Token read_string();
    Token read_number();
    Token read_ident_or_keyword();

    std::string input_;
    size_t      pos_ = 0;
};
