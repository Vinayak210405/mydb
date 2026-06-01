#pragma once
#include <string>
#include <cstdint>

enum class TokenType {
    SELECT, INSERT, UPDATE, DELETE, CREATE, DROP,
    TABLE, FROM, WHERE, INTO, VALUES, SET,
    AND, OR, NOT, NULL_KW, IS,
    INT_KW, FLOAT_KW, VARCHAR_KW, BOOL_KW,
    PRIMARY, KEY, NOT_NULL,
    SHOW, TABLES, DESCRIBE,
    INT_LIT, FLOAT_LIT, STRING_LIT, BOOL_LIT,
    IDENT,
    EQ, NEQ, LT, GT, LTE, GTE,
    COMMA, SEMICOLON, LPAREN, RPAREN,
    STAR, DOT,
    EOF_TOK, UNKNOWN
};

struct Token {
    TokenType   type;
    std::string text;
    int64_t     int_val   = 0;
    double      float_val = 0.0;

    Token(TokenType t, std::string txt)
        : type(t), text(std::move(txt)) {}
};
