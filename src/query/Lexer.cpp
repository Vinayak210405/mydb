#include "Lexer.h"
#include <cctype>
#include <unordered_map>
#include <stdexcept>

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"SELECT", TokenType::SELECT},   {"INSERT", TokenType::INSERT},
    {"UPDATE", TokenType::UPDATE},   {"DELETE", TokenType::DELETE},
    {"CREATE", TokenType::CREATE},   {"DROP", TokenType::DROP},
    {"SHOW", TokenType::SHOW},       {"DESCRIBE", TokenType::DESCRIBE},
    {"EXPLAIN", TokenType::EXPLAIN},
    {"TABLE", TokenType::TABLE},     {"TABLES", TokenType::TABLES},
    {"INDEX", TokenType::INDEX},     {"FROM", TokenType::FROM},
    {"WHERE", TokenType::WHERE},     {"INTO", TokenType::INTO},
    {"VALUES", TokenType::VALUES},   {"SET", TokenType::SET},
    {"ON", TokenType::ON},           {"AS", TokenType::AS},
    {"JOIN", TokenType::JOIN},       {"INNER", TokenType::INNER},
    {"CROSS", TokenType::CROSS},     {"ORDER", TokenType::ORDER},
    {"GROUP", TokenType::GROUP},     {"BY", TokenType::BY},
    {"HAVING", TokenType::HAVING},   {"LIMIT", TokenType::LIMIT},
    {"ASC", TokenType::ASC},         {"DESC", TokenType::DESC},
    {"DISTINCT", TokenType::DISTINCT},
    {"AND", TokenType::AND},         {"OR", TokenType::OR},
    {"NOT", TokenType::NOT},         {"NULL", TokenType::NULL_KW},
    {"IS", TokenType::IS},           {"LIKE", TokenType::LIKE},
    {"BETWEEN", TokenType::BETWEEN}, {"IN", TokenType::IN},
    {"IF", TokenType::IF},           {"EXISTS", TokenType::EXISTS},
    {"PRIMARY", TokenType::PRIMARY}, {"KEY", TokenType::KEY},
    {"INT", TokenType::INT_KW},      {"INTEGER", TokenType::INT_KW},
    {"BIGINT", TokenType::INT_KW},
    {"FLOAT", TokenType::FLOAT_KW},  {"DOUBLE", TokenType::FLOAT_KW},
    {"REAL", TokenType::FLOAT_KW},
    {"VARCHAR", TokenType::VARCHAR_KW}, {"TEXT", TokenType::VARCHAR_KW},
    {"BOOL", TokenType::BOOL_KW},    {"BOOLEAN", TokenType::BOOL_KW},
    {"TRUE", TokenType::BOOL_LIT},   {"FALSE", TokenType::BOOL_LIT},
};

Lexer::Lexer(const std::string &input) : input_(input) {}

char Lexer::peek(size_t ahead) const {
    return pos_ + ahead < input_.size() ? input_[pos_ + ahead] : '\0';
}
char Lexer::advance() { return input_[pos_++]; }
bool Lexer::at_end() const { return pos_ >= input_.size(); }

void Lexer::skip_whitespace_and_comments() {
    while (!at_end()) {
        unsigned char c = static_cast<unsigned char>(peek());
        if (std::isspace(c)) { advance(); continue; }
        if (c == '-' && peek(1) == '-') {              // -- comment
            while (!at_end() && peek() != '\n') advance();
            continue;
        }
        break;
    }
}

// 'it''s' → it's   (SQL standard doubling of quotes)
Token Lexer::read_string() {
    size_t start = pos_;
    advance();   // opening quote
    std::string s;
    while (true) {
        if (at_end()) throw std::runtime_error("unterminated string literal starting at position " +
                                               std::to_string(start));
        char c = advance();
        if (c == '\'') {
            if (peek() == '\'') { s += '\''; advance(); continue; }
            break;
        }
        s += c;
    }
    return Token(TokenType::STRING_LIT, s, start);
}

Token Lexer::read_number() {
    size_t start = pos_;
    std::string s;
    bool is_float = false;
    while (!at_end() && (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '.')) {
        if (peek() == '.') {
            if (is_float) break;
            is_float = true;
        }
        s += advance();
    }
    Token t(is_float ? TokenType::FLOAT_LIT : TokenType::INT_LIT, s, start);
    try {
        if (is_float) t.float_val = std::stod(s);
        else          t.int_val   = std::stoll(s);
    } catch (...) {
        throw std::runtime_error("invalid number '" + s + "'");
    }
    return t;
}

Token Lexer::read_ident_or_keyword() {
    size_t start = pos_;
    std::string s;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
        s += advance();
    std::string up = s;
    for (auto &c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    auto it = KEYWORDS.find(up);
    if (it != KEYWORDS.end()) {
        Token t(it->second, s, start);
        if (it->second == TokenType::BOOL_LIT) t.int_val = (up == "TRUE") ? 1 : 0;
        return t;
    }
    return Token(TokenType::IDENT, s, start);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skip_whitespace_and_comments();
        if (at_end()) break;
        char c = peek();
        size_t start = pos_;

        if (c == '\'') { tokens.push_back(read_string()); continue; }
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && std::isdigit(static_cast<unsigned char>(peek(1))))) {
            tokens.push_back(read_number());
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(read_ident_or_keyword());
            continue;
        }

        advance();
        auto emit = [&](TokenType t, const char *txt) { tokens.emplace_back(t, txt, start); };
        switch (c) {
            case ',': emit(TokenType::COMMA, ","); break;
            case ';': emit(TokenType::SEMICOLON, ";"); break;
            case '(': emit(TokenType::LPAREN, "("); break;
            case ')': emit(TokenType::RPAREN, ")"); break;
            case '*': emit(TokenType::STAR, "*"); break;
            case '.': emit(TokenType::DOT, "."); break;
            case '+': emit(TokenType::PLUS, "+"); break;
            case '-': emit(TokenType::MINUS, "-"); break;
            case '/': emit(TokenType::SLASH, "/"); break;
            case '%': emit(TokenType::PERCENT, "%"); break;
            case '=':
                if (peek() == '=') advance();
                emit(TokenType::EQ, "=");
                break;
            case '<':
                if (peek() == '=')      { advance(); emit(TokenType::LTE, "<="); }
                else if (peek() == '>') { advance(); emit(TokenType::NEQ, "!="); }
                else emit(TokenType::LT, "<");
                break;
            case '>':
                if (peek() == '=') { advance(); emit(TokenType::GTE, ">="); }
                else emit(TokenType::GT, ">");
                break;
            case '!':
                if (peek() == '=') { advance(); emit(TokenType::NEQ, "!="); break; }
                [[fallthrough]];
            default:
                throw std::runtime_error(std::string("unexpected character '") + c +
                                         "' at position " + std::to_string(start));
        }
    }
    tokens.emplace_back(TokenType::EOF_TOK, "end of input", input_.size());
    return tokens;
}
