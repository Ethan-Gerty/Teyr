//
// Created by ethan on 21/10/2026.
// Last modified on 23/10/2026.
//

#pragma once

#include <cctype>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

enum class TokenType {
    _import, _namespace, _class, _public, _private, _static,
    _void, _int, _bool, _string, _var, _return, _if, _else,

    // Names and literals
    identifier, int_lit, string_lit, bool_lit,

    // Punctuation
    semi, comma, dot,
    open_paren, close_paren,
    open_brace, close_brace,
    open_bracket, close_bracket,

    // Operators
    assign, plus, minus, star, slash,
    equal_equal, bang_equal, bang,
    less, less_equal, greater, greater_equal,
    and_and, or_or
};

struct Token {
    TokenType type;
    std::optional<std::string> value{};
    std::size_t line = 1;
    std::size_t column = 1;
};

class Tokenizer {
public:
    explicit Tokenizer(std::string source) : mSource(std::move(source)) {}

    [[nodiscard]] std::vector<Token> tokenize() {
        mIndex = 0;
        mLine = 1;
        mColumn = 1;

        std::vector<Token> tokens;
        while (!atEnd()) {
            skipIgnored();
            if (atEnd()) break;

            const auto start = mIndex;
            const auto line = mLine;
            const auto column = mColumn;
            const char c = advance();

            if (isIdentifierStart(c)) {
                tokens.push_back(scanIdentifier(start, line, column));
            } else if (isDigit(c)) {
                tokens.push_back(scanNumber(start, line, column));
            } else if (c == '"') {
                tokens.push_back(scanString(line, column));
            } else {
                tokens.push_back(scanSymbol(c, line, column));
            }
        }
        return tokens;
    }

private:
    // Returns if at  the end of source file
    [[nodiscard]] bool atEnd() const { return mIndex >= mSource.size(); }

    // Returns character at mIndex + aHead (Assuming it's not at the end of file)
    [[nodiscard]] char peek(std::size_t ahead = 0) const {
        if (ahead < mSource.size() - mIndex) {
            return mSource[mIndex + ahead];
        } else {
            return '\0';
        }
    }

    // Returns character and advances to next column/line
    char advance() {
        const char c = mSource[mIndex++];
        if (c == '\n') {
            mLine++;
            mColumn = 1;
        } else {
            mColumn++;
        }
        return c;
    }

    // Checks if the next character matches given character
    bool match(char expected) {
        if (atEnd() || peek() != expected) return false;
        advance();
        return true;
    }

    // Returns true if char is digit
    static bool isDigit(char c) {
        return std::isdigit(static_cast<unsigned char>(c)) != 0;
    }

    // Returns true if char is start of identifier
    static bool isIdentifierStart(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
    }

    // Returns true if char is part of identifier
    static bool isIdentifierPart(char c) {
        return isIdentifierStart(c) || isDigit(c);
    }

    // Throws error message with line and column number
    [[noreturn]] static void fail(const std::string& message, std::size_t line, std::size_t column) {
        throw std::runtime_error("Line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + message);
    }

    // Skips spaces, "//" comments and "/*" comments
    void skipIgnored() {
        while (!atEnd()) {
            if (std::isspace(static_cast<unsigned char>(peek()))) {
                advance();
            } else if (peek() == '/' && peek(1) == '/') {
                while (!atEnd() && peek() != '\n') advance();
            } else if (peek() == '/' && peek(1) == '*') {
                const auto line = mLine;
                const auto column = mColumn;
                advance(); // /
                advance(); // *
                bool closed = false;
                while (!atEnd()) {
                    if (peek() == '*' && peek(1) == '/') {
                        advance();
                        advance();
                        closed = true;
                        break;
                    }
                    advance();
                }
                if (!closed) fail("Unterminated block comment", line, column);
            } else {
                break;
            }
        }
    }

    // Scans identifier and returns a token
    Token scanIdentifier(std::size_t start, std::size_t line, std::size_t column) {
        while (isIdentifierPart(peek())) advance();
        std::string word = mSource.substr(start, mIndex - start);

        if (word == "true" || word == "false") {
            return {TokenType::bool_lit, std::move(word), line, column};
        }

        static const std::unordered_map<std::string, TokenType> keywords = {
            {"import", TokenType::_import}, {"namespace", TokenType::_namespace},
            {"class", TokenType::_class}, {"public", TokenType::_public},
            {"private", TokenType::_private}, {"static", TokenType::_static},
            {"void", TokenType::_void}, {"int", TokenType::_int},
            {"bool", TokenType::_bool}, {"string", TokenType::_string},
            {"var", TokenType::_var}, {"return", TokenType::_return},
            {"if", TokenType::_if}, {"else", TokenType::_else}
        };

        if (const auto it = keywords.find(word); it != keywords.end()) {
            return {it->second, {}, line, column};
        }
        return {TokenType::identifier, std::move(word), line, column};
    }

    // Scans number and returns int_lit token
    Token scanNumber(std::size_t start, std::size_t line, std::size_t column) {
        while (isDigit(peek())) advance();
        return {TokenType::int_lit,mSource.substr(start, mIndex - start), line, column};
    }

    // Scans string and returns string_lit token
    Token scanString(std::size_t line, std::size_t column) {
        const auto contentStart = mIndex; // Opening quote already consumed.
        while (!atEnd() && peek() != '"') {
            if (peek() == '\n' || peek() == '\r') {
                fail("Unterminated string literal", line, column);
            }
            if (peek() == '\\') {
                advance(); // Escape marker
                if (atEnd() || peek() == '\n' || peek() == '\r') {
                    fail("Unterminated string literal", line, column);
                }
            }
            advance();
        }
        if (atEnd()) fail("Unterminated string literal", line, column);

        std::string value = mSource.substr(contentStart, mIndex - contentStart);
        advance(); // Closing quote
        return {TokenType::string_lit, std::move(value), line, column};
    }

    // Scans character and returns symbol token
    Token scanSymbol(char c, std::size_t line, std::size_t column) {
        TokenType type;
        switch (c) {
            case ';': type = TokenType::semi; break;
            case ',': type = TokenType::comma; break;
            case '.': type = TokenType::dot; break;
            case '(': type = TokenType::open_paren; break;
            case ')': type = TokenType::close_paren; break;
            case '{': type = TokenType::open_brace; break;
            case '}': type = TokenType::close_brace; break;
            case '[': type = TokenType::open_bracket; break;
            case ']': type = TokenType::close_bracket; break;
            case '+': type = TokenType::plus; break;
            case '-': type = TokenType::minus; break;
            case '*': type = TokenType::star; break;
            case '/': type = TokenType::slash; break;
            case '=': type = match('=') ? TokenType::equal_equal : TokenType::assign; break;
            case '!': type = match('=') ? TokenType::bang_equal : TokenType::bang; break;
            case '<': type = match('=') ? TokenType::less_equal : TokenType::less; break;
            case '>': type = match('=') ? TokenType::greater_equal : TokenType::greater; break;
            case '&':
                if (!match('&')) fail("Expected '&' after '&'", line, column);
                type = TokenType::and_and;
                break;
            case '|':
                if (!match('|')) fail("Expected '|' after '|'", line, column);
                type = TokenType::or_or;
                break;
            default:
                fail("Unexpected character '" + std::string(1, c) + "'", line, column);
        }
        return {type, {}, line, column};
    }

    std::string mSource;
    std::size_t mIndex = 0;
    std::size_t mLine = 1;
    std::size_t mColumn = 1;
};
