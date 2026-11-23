#pragma once
#include <vector>
#include <string>
#include <stdexcept>
#include "Token.h"
#include "AST.h"

// ---------------------------------------------------------------------------
// Recursive-descent SQL parser.
//
// Expression grammar (lowest precedence first):
//   expr       := or
//   or         := and ( OR and )*
//   and        := not ( AND not )*
//   not        := NOT not | comparison
//   comparison := additive [ (= | != | < | > | <= | >=) additive
//                          | IS [NOT] NULL
//                          | [NOT] LIKE additive
//                          | [NOT] BETWEEN additive AND additive
//                          | [NOT] IN ( expr, ... ) ]
//   additive   := term ( (+ | -) term )*
//   term       := unary ( (* | / | %) unary )*
//   unary      := - unary | primary
//   primary    := literal | name [. name] | func ( [DISTINCT] expr | * ) | ( expr )
// ---------------------------------------------------------------------------
class Parser {
public:
    Parser(std::vector<Token> tokens, std::string source = "");

    // Parse every statement in a script (statements separated by ';').
    std::vector<Statement> parse_all();
    // Parse exactly one statement (a trailing ';' is allowed).
    Statement parse();

    static std::vector<Statement> parse_sql(const std::string &sql);

private:
    const Token& peek(size_t ahead = 0) const;
    const Token& advance();
    bool  check(TokenType t) const;
    bool  match(TokenType t);
    const Token& expect(TokenType t, const std::string &what);
    [[noreturn]] void error(const std::string &msg) const;
    std::string ident(const std::string &what);

    Statement parse_statement();
    Statement parse_select();
    Statement parse_insert();
    Statement parse_update();
    Statement parse_delete();
    Statement parse_create();
    Statement parse_drop();
    Statement parse_show();
    Statement parse_describe();
    Statement parse_explain();
    void      parse_index_target(IndexStmt &out);

    ExprPtr parse_expr();
    ExprPtr parse_or();
    ExprPtr parse_and();
    ExprPtr parse_not();
    ExprPtr parse_comparison();
    ExprPtr parse_additive();
    ExprPtr parse_term();
    ExprPtr parse_unary();
    ExprPtr parse_primary();

    std::vector<Token> tokens_;
    std::string        source_;
    size_t             pos_ = 0;
};
