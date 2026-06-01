#pragma once
#include <vector>
#include <stdexcept>
#include "Token.h"
#include "AST.h"

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Statement parse();

private:
    Token& peek();
    Token& advance();
    bool check(TokenType t);
    bool match(TokenType t);
    Token expect(TokenType t, const std::string &msg);

    Statement parse_select();
    Statement parse_insert();
    Statement parse_update();
    Statement parse_delete();
    Statement parse_create_table();
    Statement parse_drop_table();
    Statement parse_show_tables();
    Statement parse_describe();

    std::shared_ptr<Expr> parse_expr();
    std::shared_ptr<Expr> parse_comparison();
    std::shared_ptr<Expr> parse_primary();
    Value                 parse_value();

    std::vector<Token> tokens_;
    size_t             pos_ = 0;
};
