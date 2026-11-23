#include "Parser.h"
#include "Lexer.h"
#include <algorithm>

Parser::Parser(std::vector<Token> tokens, std::string source)
    : tokens_(std::move(tokens)), source_(std::move(source)) {}

std::vector<Statement> Parser::parse_sql(const std::string &sql) {
    Lexer lexer(sql);
    Parser parser(lexer.tokenize(), sql);
    return parser.parse_all();
}

// ---------------------------------------------------------------------------
// Token helpers
// ---------------------------------------------------------------------------
const Token& Parser::peek(size_t ahead) const {
    size_t i = std::min(pos_ + ahead, tokens_.size() - 1);
    return tokens_[i];
}
const Token& Parser::advance() {
    const Token &t = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) pos_++;
    return t;
}
bool Parser::check(TokenType t) const { return peek().type == t; }
bool Parser::match(TokenType t) {
    if (!check(t)) return false;
    advance();
    return true;
}
void Parser::error(const std::string &msg) const {
    throw std::runtime_error("syntax error: " + msg + " near '" + peek().text +
                             "' (position " + std::to_string(peek().pos) + ")");
}
const Token& Parser::expect(TokenType t, const std::string &what) {
    if (!check(t)) error("expected " + what);
    return advance();
}
std::string Parser::ident(const std::string &what) {
    return expect(TokenType::IDENT, what).text;
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------
std::vector<Statement> Parser::parse_all() {
    std::vector<Statement> out;
    while (true) {
        while (match(TokenType::SEMICOLON)) {}
        if (check(TokenType::EOF_TOK)) break;
        size_t start = peek().pos;
        Statement s = parse_statement();
        size_t end = peek().pos;
        if (!check(TokenType::SEMICOLON) && !check(TokenType::EOF_TOK))
            error("expected ';' or end of statement");
        if (!source_.empty() && end >= start) {
            std::string text = source_.substr(start, end - start);
            while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();
            s.sql = text;
        }
        out.push_back(std::move(s));
    }
    return out;
}

Statement Parser::parse() {
    Statement s = parse_statement();
    match(TokenType::SEMICOLON);
    if (!check(TokenType::EOF_TOK)) error("unexpected input after the statement");
    return s;
}

Statement Parser::parse_statement() {
    switch (peek().type) {
        case TokenType::SELECT:   return parse_select();
        case TokenType::INSERT:   return parse_insert();
        case TokenType::UPDATE:   return parse_update();
        case TokenType::DELETE:   return parse_delete();
        case TokenType::CREATE:   return parse_create();
        case TokenType::DROP:     return parse_drop();
        case TokenType::SHOW:     return parse_show();
        case TokenType::DESCRIBE:
        case TokenType::DESC:     return parse_describe();
        case TokenType::EXPLAIN:  return parse_explain();
        default: error("expected a statement (SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, SHOW, DESCRIBE, EXPLAIN)");
    }
}

Statement Parser::parse_select() {
    expect(TokenType::SELECT, "SELECT");
    Statement stmt;
    stmt.type = StmtType::SELECT;
    SelectStmt &s = stmt.select;
    s.distinct = match(TokenType::DISTINCT);

    // select list
    do {
        SelectItem item;
        if (match(TokenType::STAR)) {
            item.star = true;
        } else if (check(TokenType::IDENT) && peek(1).type == TokenType::DOT &&
                   peek(2).type == TokenType::STAR) {
            item.star = true;
            item.star_table = advance().text;
            advance(); advance();
        } else {
            item.expr = parse_expr();
            if (match(TokenType::AS)) item.alias = ident("alias after AS");
            else if (check(TokenType::IDENT)) item.alias = advance().text;
        }
        s.items.push_back(std::move(item));
    } while (match(TokenType::COMMA));

    // FROM is optional: "SELECT 1 + 2"
    if (match(TokenType::FROM)) {
        auto table_ref = [&]() {
            TableRef r;
            r.name = ident("table name");
            if (match(TokenType::AS)) r.alias = ident("table alias");
            else if (check(TokenType::IDENT)) r.alias = advance().text;
            if (r.alias.empty()) r.alias = r.name;
            return r;
        };
        s.from.push_back(table_ref());
        while (true) {
            if (match(TokenType::COMMA)) {                    // implicit cross join
                s.from.push_back(table_ref());
            } else if (check(TokenType::JOIN) || check(TokenType::INNER) || check(TokenType::CROSS)) {
                bool cross = match(TokenType::CROSS);
                if (!cross) match(TokenType::INNER);
                expect(TokenType::JOIN, "JOIN");
                TableRef r = table_ref();
                if (!cross) {
                    expect(TokenType::ON, "ON after JOIN");
                    r.join_on = parse_expr();
                }
                s.from.push_back(std::move(r));
            } else {
                break;
            }
        }
    }

    if (match(TokenType::WHERE)) s.where = parse_expr();
    if (match(TokenType::GROUP)) {
        expect(TokenType::BY, "BY after GROUP");
        do { s.group_by.push_back(parse_expr()); } while (match(TokenType::COMMA));
    }
    if (match(TokenType::HAVING)) s.having = parse_expr();
    if (match(TokenType::ORDER)) {
        expect(TokenType::BY, "BY after ORDER");
        do {
            OrderItem o;
            o.expr = parse_expr();
            if (match(TokenType::DESC)) o.desc = true;
            else match(TokenType::ASC);
            s.order_by.push_back(std::move(o));
        } while (match(TokenType::COMMA));
    }
    if (match(TokenType::LIMIT)) {
        const Token &t = expect(TokenType::INT_LIT, "a number after LIMIT");
        s.limit = t.int_val;
    }
    if (s.from.empty() && (s.where || !s.group_by.empty()))
        error("WHERE / GROUP BY need a FROM clause");
    for (auto &it : s.items)
        if (it.star && s.from.empty()) error("SELECT * needs a FROM clause");
    return stmt;
}

Statement Parser::parse_insert() {
    expect(TokenType::INSERT, "INSERT");
    expect(TokenType::INTO, "INTO");
    Statement stmt;
    stmt.type = StmtType::INSERT;
    stmt.insert.table = ident("table name");
    if (match(TokenType::LPAREN)) {
        do { stmt.insert.columns.push_back(ident("column name")); } while (match(TokenType::COMMA));
        expect(TokenType::RPAREN, "')'");
    }
    expect(TokenType::VALUES, "VALUES");
    do {
        expect(TokenType::LPAREN, "'(' before values");
        std::vector<ExprPtr> row;
        do { row.push_back(parse_expr()); } while (match(TokenType::COMMA));
        expect(TokenType::RPAREN, "')' after values");
        stmt.insert.rows.push_back(std::move(row));
    } while (match(TokenType::COMMA));
    return stmt;
}

Statement Parser::parse_update() {
    expect(TokenType::UPDATE, "UPDATE");
    Statement stmt;
    stmt.type = StmtType::UPDATE;
    stmt.update.table = ident("table name");
    expect(TokenType::SET, "SET");
    do {
        std::string col = ident("column name");
        expect(TokenType::EQ, "'='");
        stmt.update.assignments.emplace_back(col, parse_expr());
    } while (match(TokenType::COMMA));
    if (match(TokenType::WHERE)) stmt.update.where = parse_expr();
    return stmt;
}

Statement Parser::parse_delete() {
    expect(TokenType::DELETE, "DELETE");
    expect(TokenType::FROM, "FROM");
    Statement stmt;
    stmt.type = StmtType::DELETE;
    stmt.del.table = ident("table name");
    if (match(TokenType::WHERE)) stmt.del.where = parse_expr();
    return stmt;
}

// CREATE INDEX [name] ON table (column)  — the name is accepted and ignored;
// an index is identified by its table and column.
void Parser::parse_index_target(IndexStmt &out) {
    if (check(TokenType::IDENT)) advance();            // optional index name
    expect(TokenType::ON, "ON");
    out.table = ident("table name");
    expect(TokenType::LPAREN, "'('");
    out.column = ident("column name");
    expect(TokenType::RPAREN, "')'");
}

Statement Parser::parse_create() {
    expect(TokenType::CREATE, "CREATE");
    Statement stmt;
    if (match(TokenType::INDEX)) {
        stmt.type = StmtType::CREATE_INDEX;
        parse_index_target(stmt.index);
        return stmt;
    }
    expect(TokenType::TABLE, "TABLE or INDEX after CREATE");
    stmt.type = StmtType::CREATE_TABLE;
    if (match(TokenType::IF)) {
        expect(TokenType::NOT, "NOT");
        expect(TokenType::EXISTS, "EXISTS");
        stmt.create_table.if_not_exists = true;
    }
    stmt.create_table.table = ident("table name");
    expect(TokenType::LPAREN, "'('");
    do {
        std::string col_name = ident("column name");
        DataType dtype;
        switch (advance().type) {
            case TokenType::INT_KW:     dtype = DataType::INT;     break;
            case TokenType::FLOAT_KW:   dtype = DataType::FLOAT;   break;
            case TokenType::VARCHAR_KW: dtype = DataType::VARCHAR; break;
            case TokenType::BOOL_KW:    dtype = DataType::BOOL;    break;
            default: pos_--; error("expected a column type (INT, FLOAT, VARCHAR, BOOL)");
        }
        if (match(TokenType::LPAREN)) {                    // VARCHAR(255): length accepted, not enforced
            expect(TokenType::INT_LIT, "a length");
            expect(TokenType::RPAREN, "')'");
        }
        bool not_null = false, pk = false;
        while (true) {
            if (match(TokenType::NOT)) { expect(TokenType::NULL_KW, "NULL after NOT"); not_null = true; }
            else if (match(TokenType::PRIMARY)) { expect(TokenType::KEY, "KEY"); pk = true; not_null = true; }
            else if (match(TokenType::NULL_KW)) { /* explicit NULL: default */ }
            else break;
        }
        for (auto &c : stmt.create_table.columns)
            if (c.name == col_name) error("duplicate column '" + col_name + "'");
        Column col(col_name, dtype, !not_null);
        col.is_primary_key = pk;
        stmt.create_table.columns.push_back(col);
    } while (match(TokenType::COMMA));
    expect(TokenType::RPAREN, "')'");
    int pks = 0;
    for (auto &c : stmt.create_table.columns) pks += c.is_primary_key;
    if (pks > 1) error("only one PRIMARY KEY column is supported");
    return stmt;
}

Statement Parser::parse_drop() {
    expect(TokenType::DROP, "DROP");
    Statement stmt;
    if (match(TokenType::INDEX)) {
        stmt.type = StmtType::DROP_INDEX;
        parse_index_target(stmt.index);
        return stmt;
    }
    expect(TokenType::TABLE, "TABLE or INDEX after DROP");
    stmt.type = StmtType::DROP_TABLE;
    if (match(TokenType::IF)) {
        expect(TokenType::EXISTS, "EXISTS");
        stmt.drop_table.if_exists = true;
    }
    stmt.drop_table.table = ident("table name");
    return stmt;
}

Statement Parser::parse_show() {
    expect(TokenType::SHOW, "SHOW");
    expect(TokenType::TABLES, "TABLES");
    Statement stmt;
    stmt.type = StmtType::SHOW_TABLES;
    return stmt;
}

Statement Parser::parse_describe() {
    advance();   // DESCRIBE or DESC
    Statement stmt;
    stmt.type = StmtType::DESCRIBE;
    stmt.describe.table = ident("table name");
    return stmt;
}

Statement Parser::parse_explain() {
    expect(TokenType::EXPLAIN, "EXPLAIN");
    Statement stmt;
    stmt.type = StmtType::EXPLAIN;
    if (!check(TokenType::SELECT) && !check(TokenType::UPDATE) && !check(TokenType::DELETE))
        error("EXPLAIN supports SELECT, UPDATE and DELETE");
    stmt.explained = std::make_shared<Statement>(parse_statement());
    return stmt;
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------
ExprPtr Parser::parse_expr() { return parse_or(); }

ExprPtr Parser::parse_or() {
    ExprPtr left = parse_and();
    while (match(TokenType::OR)) left = make_binary("OR", left, parse_and());
    return left;
}

ExprPtr Parser::parse_and() {
    ExprPtr left = parse_not();
    while (match(TokenType::AND)) left = make_binary("AND", left, parse_not());
    return left;
}

ExprPtr Parser::parse_not() {
    if (match(TokenType::NOT)) return make_unary("NOT", parse_not());
    return parse_comparison();
}

ExprPtr Parser::parse_comparison() {
    ExprPtr left = parse_additive();
    switch (peek().type) {
        case TokenType::EQ:  advance(); return make_binary("=",  left, parse_additive());
        case TokenType::NEQ: advance(); return make_binary("!=", left, parse_additive());
        case TokenType::LT:  advance(); return make_binary("<",  left, parse_additive());
        case TokenType::GT:  advance(); return make_binary(">",  left, parse_additive());
        case TokenType::LTE: advance(); return make_binary("<=", left, parse_additive());
        case TokenType::GTE: advance(); return make_binary(">=", left, parse_additive());
        default: break;
    }
    if (match(TokenType::IS)) {
        auto e = std::make_shared<Expr>();
        e->type = ExprType::IS_NULL;
        e->negated = match(TokenType::NOT);
        expect(TokenType::NULL_KW, "NULL after IS");
        e->args.push_back(left);
        return e;
    }
    bool negate = false;
    if (check(TokenType::NOT) &&
        (peek(1).type == TokenType::LIKE || peek(1).type == TokenType::BETWEEN ||
         peek(1).type == TokenType::IN)) {
        advance();
        negate = true;
    }
    if (match(TokenType::LIKE)) {
        ExprPtr e = make_binary("LIKE", left, parse_additive());
        e->negated = negate;
        return e;
    }
    if (match(TokenType::BETWEEN)) {                       // a BETWEEN x AND y  →  a >= x AND a <= y
        ExprPtr lo = parse_additive();
        expect(TokenType::AND, "AND in BETWEEN");
        ExprPtr hi = parse_additive();
        ExprPtr e = make_binary("AND", make_binary(">=", left, lo), make_binary("<=", left, hi));
        return negate ? make_unary("NOT", e) : e;
    }
    if (match(TokenType::IN)) {                            // a IN (x, y)  →  a = x OR a = y
        expect(TokenType::LPAREN, "'(' after IN");
        ExprPtr e;
        do {
            ExprPtr eq = make_binary("=", left, parse_expr());
            e = e ? make_binary("OR", e, eq) : eq;
        } while (match(TokenType::COMMA));
        expect(TokenType::RPAREN, "')'");
        return negate ? make_unary("NOT", e) : e;
    }
    if (negate) error("expected LIKE, BETWEEN or IN after NOT");
    return left;
}

ExprPtr Parser::parse_additive() {
    ExprPtr left = parse_term();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = advance().text;
        left = make_binary(op, left, parse_term());
    }
    return left;
}

ExprPtr Parser::parse_term() {
    ExprPtr left = parse_unary();
    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
        std::string op = advance().text;
        left = make_binary(op, left, parse_unary());
    }
    return left;
}

ExprPtr Parser::parse_unary() {
    if (match(TokenType::MINUS)) {
        ExprPtr child = parse_unary();
        if (child->type == ExprType::LITERAL && is_int(child->literal))
            return make_literal(-std::get<int64_t>(child->literal));
        if (child->type == ExprType::LITERAL && is_float(child->literal))
            return make_literal(-std::get<double>(child->literal));
        return make_unary("-", child);
    }
    if (match(TokenType::PLUS)) return parse_unary();
    return parse_primary();
}

ExprPtr Parser::parse_primary() {
    const Token &t = peek();
    switch (t.type) {
        case TokenType::INT_LIT:    advance(); return make_literal(t.int_val);
        case TokenType::FLOAT_LIT:  advance(); return make_literal(t.float_val);
        case TokenType::STRING_LIT: advance(); return make_literal(t.text);
        case TokenType::BOOL_LIT:   advance(); return make_literal(t.int_val != 0);
        case TokenType::NULL_KW:    advance(); return make_literal(std::monostate{});
        case TokenType::LPAREN: {
            advance();
            ExprPtr e = parse_expr();
            expect(TokenType::RPAREN, "')'");
            return e;
        }
        case TokenType::IDENT: {
            std::string name = advance().text;
            if (match(TokenType::LPAREN)) {                // function call
                auto e = std::make_shared<Expr>();
                e->type = ExprType::FUNCTION;
                std::string up = name;
                for (auto &c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                e->name = up;
                if (!is_aggregate_name(up))
                    error("unknown function '" + name + "' (supported: COUNT, SUM, AVG, MIN, MAX)");
                if (match(TokenType::STAR)) {
                    if (up != "COUNT") error("only COUNT accepts *");
                    e->star_arg = true;
                } else {
                    e->distinct = match(TokenType::DISTINCT);
                    e->args.push_back(parse_expr());
                }
                expect(TokenType::RPAREN, "')'");
                return e;
            }
            if (match(TokenType::DOT))
                return make_column(name, ident("column name after '.'"));
            return make_column("", name);
        }
        default:
            error("expected an expression");
    }
}
