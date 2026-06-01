#include "Lexer.h"
#include <cctype>
#include <unordered_map>
#include <stdexcept>

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"SELECT",   TokenType::SELECT},
    {"INSERT",   TokenType::INSERT},
    {"UPDATE",   TokenType::UPDATE},
    {"DELETE",   TokenType::DELETE},
    {"CREATE",   TokenType::CREATE},
    {"DROP",     TokenType::DROP},
    {"TABLE",    TokenType::TABLE},
    {"FROM",     TokenType::FROM},
    {"WHERE",    TokenType::WHERE},
    {"INTO",     TokenType::INTO},
    {"VALUES",   TokenType::VALUES},
    {"SET",      TokenType::SET},
    {"AND",      TokenType::AND},
    {"OR",       TokenType::OR},
    {"NOT",      TokenType::NOT},
    {"NULL",     TokenType::NULL_KW},
    {"IS",       TokenType::IS},
    {"INT",      TokenType::INT_KW},
    {"INTEGER",  TokenType::INT_KW},
    {"FLOAT",    TokenType::FLOAT_KW},
    {"DOUBLE",   TokenType::FLOAT_KW},
    {"VARCHAR",  TokenType::VARCHAR_KW},
    {"TEXT",     TokenType::VARCHAR_KW},
    {"BOOL",     TokenType::BOOL_KW},
    {"BOOLEAN",  TokenType::BOOL_KW},
    {"PRIMARY",  TokenType::PRIMARY},
    {"KEY",      TokenType::KEY},
    {"SHOW",     TokenType::SHOW},
    {"TABLES",   TokenType::TABLES},
    {"DESCRIBE", TokenType::DESCRIBE},
    {"DESC",     TokenType::DESCRIBE},
    {"TRUE",     TokenType::BOOL_LIT},
    {"FALSE",    TokenType::BOOL_LIT},
};

Lexer::Lexer(const std::string &input) : input_(input) {}

char Lexer::peek() const {
    return at_end() ? '\0' : input_[pos_];
}

char Lexer::advance() {
    return input_[pos_++];
}

bool Lexer::at_end() const {
    return pos_ >= input_.size();
}

void Lexer::skip_whitespace() {
    while (!at_end() && std::isspace(peek())) advance();
}

Token Lexer::read_string() {
    advance(); // skip opening quote
    std::string s;
    while (!at_end() && peek() != '\'') {
        if (peek() == '\\') advance(); // escape
        s += advance();
    }
    if (!at_end()) advance(); // skip closing quote
    return Token(TokenType::STRING_LIT, s);
}

Token Lexer::read_number() {
    std::string s;
    bool is_float = false;
    while (!at_end() && (std::isdigit(peek()) || peek() == '.')) {
        if (peek() == '.') is_float = true;
        s += advance();
    }
    Token t(is_float ? TokenType::FLOAT_LIT : TokenType::INT_LIT, s);
    if (is_float) t.float_val = std::stod(s);
    else          t.int_val   = std::stoll(s);
    return t;
}

Token Lexer::read_ident_or_keyword() {
    std::string s;
    while (!at_end() && (std::isalnum(peek()) || peek() == '_'))
        s += advance();

    // uppercase for keyword lookup
    std::string up = s;
    for (auto &c : up) c = std::toupper(c);

    auto it = KEYWORDS.find(up);
    if (it != KEYWORDS.end()) {
        Token t(it->second, s);
        if (it->second == TokenType::BOOL_LIT)
            t.int_val = (up == "TRUE") ? 1 : 0;
        return t;
    }
    return Token(TokenType::IDENT, s);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skip_whitespace();
        if (at_end()) break;

        char c = peek();

        if (c == '\'')           { tokens.push_back(read_string()); continue; }
        if (std::isdigit(c))     { tokens.push_back(read_number()); continue; }
        if (std::isalpha(c) || c == '_') { tokens.push_back(read_ident_or_keyword()); continue; }

        advance();
        switch (c) {
            case ',': tokens.emplace_back(TokenType::COMMA,     ","); break;
            case ';': tokens.emplace_back(TokenType::SEMICOLON, ";"); break;
            case '(': tokens.emplace_back(TokenType::LPAREN,    "("); break;
            case ')': tokens.emplace_back(TokenType::RPAREN,    ")"); break;
            case '*': tokens.emplace_back(TokenType::STAR,      "*"); break;
            case '.': tokens.emplace_back(TokenType::DOT,       "."); break;
            case '=': tokens.emplace_back(TokenType::EQ,        "="); break;
            case '<':
                if (!at_end() && peek() == '=') { advance(); tokens.emplace_back(TokenType::LTE, "<="); }
                else tokens.emplace_back(TokenType::LT, "<");
                break;
            case '>':
                if (!at_end() && peek() == '=') { advance(); tokens.emplace_back(TokenType::GTE, ">="); }
                else tokens.emplace_back(TokenType::GT, ">");
                break;
            case '!':
                if (!at_end() && peek() == '=') { advance(); tokens.emplace_back(TokenType::NEQ, "!="); }
                break;
            default: tokens.emplace_back(TokenType::UNKNOWN, std::string(1,c)); break;
        }
    }
    tokens.emplace_back(TokenType::EOF_TOK, "");
    return tokens;
}
