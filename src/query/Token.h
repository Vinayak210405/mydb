#pragma once
#include <string>
#include <cstdint>

enum class TokenType {
    // statements
    SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, SHOW, DESCRIBE, EXPLAIN,
    // clauses / keywords
    TABLE, TABLES, INDEX, FROM, WHERE, INTO, VALUES, SET, ON, AS,
    JOIN, INNER, CROSS, ORDER, GROUP, BY, HAVING, LIMIT, ASC, DESC, DISTINCT,
    AND, OR, NOT, NULL_KW, IS, LIKE, BETWEEN, IN, IF, EXISTS,
    PRIMARY, KEY,
    // types
    INT_KW, FLOAT_KW, VARCHAR_KW, BOOL_KW,
    // literals / names
    INT_LIT, FLOAT_LIT, STRING_LIT, BOOL_LIT, IDENT,
    // operators / punctuation
    EQ, NEQ, LT, GT, LTE, GTE, PLUS, MINUS, STAR, SLASH, PERCENT,
    COMMA, SEMICOLON, LPAREN, RPAREN, DOT,
    EOF_TOK
};

struct Token {
    TokenType   type;
    std::string text;
    int64_t     int_val   = 0;
    double      float_val = 0.0;
    size_t      pos       = 0;    // offset in the SQL text (for error messages)

    Token(TokenType t, std::string txt, size_t p = 0)
        : type(t), text(std::move(txt)), pos(p) {}
};
