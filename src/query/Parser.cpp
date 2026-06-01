#include "Parser.h"
#include <stdexcept>

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

Token& Parser::peek()  { return tokens_[pos_]; }
Token& Parser::advance() { return tokens_[pos_++]; }

bool Parser::check(TokenType t) {
    return peek().type == t;
}

bool Parser::match(TokenType t) {
    if (check(t)) { advance(); return true; }
    return false;
}

Token Parser::expect(TokenType t, const std::string &msg) {
    if (!check(t)) throw std::runtime_error("Parse error: " + msg +
                                             " (got '" + peek().text + "')");
    return advance();
}

// ---------------------------------------------------------------
Value Parser::parse_value() {
    Token &t = peek();
    if (t.type == TokenType::INT_LIT)    { advance(); return t.int_val; }
    if (t.type == TokenType::FLOAT_LIT)  { advance(); return t.float_val; }
    if (t.type == TokenType::STRING_LIT) { advance(); return t.text; }
    if (t.type == TokenType::BOOL_LIT)   { advance(); return t.int_val != 0; }
    if (t.type == TokenType::NULL_KW)    { advance(); return std::monostate{}; }
    throw std::runtime_error("Parse error: expected value, got '" + t.text + "'");
}

std::shared_ptr<Expr> Parser::parse_primary() {
    Token &t = peek();
    if (t.type == TokenType::IDENT) {
        std::string name = t.text; advance();
        return make_col_ref(name);
    }
    return make_literal(parse_value());
}

std::shared_ptr<Expr> Parser::parse_comparison() {
    auto left = parse_primary();
    Token &op = peek();
    if (op.type == TokenType::EQ  || op.type == TokenType::NEQ ||
        op.type == TokenType::LT  || op.type == TokenType::GT  ||
        op.type == TokenType::LTE || op.type == TokenType::GTE) {
        std::string op_str = op.text; advance();
        auto right = parse_primary();
        return make_binop(op_str, left, right);
    }
    return left;
}

std::shared_ptr<Expr> Parser::parse_expr() {
    return parse_comparison();
}

// ---------------------------------------------------------------
Statement Parser::parse_select() {
    Statement stmt; stmt.type = StmtType::SELECT;
    // columns
    if (match(TokenType::STAR)) {
        // SELECT * — leave columns empty
    } else {
        stmt.select.columns.push_back(expect(TokenType::IDENT, "column name").text);
        while (match(TokenType::COMMA))
            stmt.select.columns.push_back(expect(TokenType::IDENT, "column name").text);
    }
    expect(TokenType::FROM, "FROM");
    stmt.select.table = expect(TokenType::IDENT, "table name").text;
    if (match(TokenType::WHERE))
        stmt.select.where = parse_expr();
    return stmt;
}

Statement Parser::parse_insert() {
    Statement stmt; stmt.type = StmtType::INSERT;
    expect(TokenType::INTO, "INTO");
    stmt.insert.table = expect(TokenType::IDENT, "table name").text;
    // optional column list
    if (match(TokenType::LPAREN)) {
        stmt.insert.columns.push_back(expect(TokenType::IDENT, "column name").text);
        while (match(TokenType::COMMA))
            stmt.insert.columns.push_back(expect(TokenType::IDENT, "column name").text);
        expect(TokenType::RPAREN, ")");
    }
    expect(TokenType::VALUES, "VALUES");
    expect(TokenType::LPAREN, "(");
    stmt.insert.values.push_back(parse_value());
    while (match(TokenType::COMMA))
        stmt.insert.values.push_back(parse_value());
    expect(TokenType::RPAREN, ")");
    return stmt;
}

Statement Parser::parse_update() {
    Statement stmt; stmt.type = StmtType::UPDATE;
    stmt.update.table = expect(TokenType::IDENT, "table name").text;
    expect(TokenType::SET, "SET");
    do {
        std::string col = expect(TokenType::IDENT, "column name").text;
        expect(TokenType::EQ, "=");
        Value val = parse_value();
        stmt.update.assignments.emplace_back(col, val);
    } while (match(TokenType::COMMA));
    if (match(TokenType::WHERE))
        stmt.update.where = parse_expr();
    return stmt;
}

Statement Parser::parse_delete() {
    Statement stmt; stmt.type = StmtType::DELETE;
    expect(TokenType::FROM, "FROM");
    stmt.del.table = expect(TokenType::IDENT, "table name").text;
    if (match(TokenType::WHERE))
        stmt.del.where = parse_expr();
    return stmt;
}

Statement Parser::parse_create_table() {
    Statement stmt; stmt.type = StmtType::CREATE_TABLE;
    expect(TokenType::TABLE, "TABLE");
    stmt.create_table.table = expect(TokenType::IDENT, "table name").text;
    expect(TokenType::LPAREN, "(");
    do {
        std::string col_name = expect(TokenType::IDENT, "column name").text;
        Token &type_tok = advance();
        DataType dtype;
        switch (type_tok.type) {
            case TokenType::INT_KW:     dtype = DataType::INT;     break;
            case TokenType::FLOAT_KW:   dtype = DataType::FLOAT;   break;
            case TokenType::VARCHAR_KW: dtype = DataType::VARCHAR;  break;
            case TokenType::BOOL_KW:    dtype = DataType::BOOL;    break;
            default: throw std::runtime_error("Parse error: unknown type '" + type_tok.text + "'");
        }
        bool not_null = false, pk = false;
        while (check(TokenType::NOT) || check(TokenType::PRIMARY)) {
            if (match(TokenType::NOT)) { expect(TokenType::NOT_NULL, "NULL"); not_null = true; }
            if (match(TokenType::PRIMARY)) { expect(TokenType::KEY, "KEY"); pk = true; not_null = true; }
        }
        Column col(col_name, dtype, !not_null);
        col.is_primary_key = pk;
        stmt.create_table.columns.push_back(col);
    } while (match(TokenType::COMMA));
    expect(TokenType::RPAREN, ")");
    return stmt;
}

Statement Parser::parse_drop_table() {
    Statement stmt; stmt.type = StmtType::DROP_TABLE;
    expect(TokenType::TABLE, "TABLE");
    stmt.drop_table.table = expect(TokenType::IDENT, "table name").text;
    return stmt;
}

Statement Parser::parse_show_tables() {
    Statement stmt; stmt.type = StmtType::SHOW_TABLES;
    expect(TokenType::TABLES, "TABLES");
    return stmt;
}

Statement Parser::parse_describe() {
    Statement stmt; stmt.type = StmtType::DESCRIBE;
    stmt.describe.table = expect(TokenType::IDENT, "table name").text;
    return stmt;
}

Statement Parser::parse() {
    Token &first = advance();
    switch (first.type) {
        case TokenType::SELECT:   return parse_select();
        case TokenType::INSERT:   return parse_insert();
        case TokenType::UPDATE:   return parse_update();
        case TokenType::DELETE:   return parse_delete();
        case TokenType::CREATE:   return parse_create_table();
        case TokenType::DROP:     return parse_drop_table();
        case TokenType::SHOW:     return parse_show_tables();
        case TokenType::DESCRIBE: return parse_describe();
        default:
            throw std::runtime_error("Parse error: unknown statement '" + first.text + "'");
    }
}
