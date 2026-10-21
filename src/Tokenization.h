//
// Created by ethan on 21/10/2026.
// Last modified on 21/10/2026.
//

#pragma once

#include <iostream>
#include <string>
#include <optional>
#include <vector>


enum class TokenType {
    _return,
    int_lit,
    semi,
};

struct Token {
    TokenType type;
    std::optional<std::string> value {};
};



class Tokenizer {
public:
    explicit Tokenizer(std::string src)
        : mSrc(std::move(src)), mIndex(0) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        std::string buf;

        while (peek().has_value()) {
            char c = peek().value();

            if (std::isalpha(static_cast<unsigned char>(c))) {
                buf.push_back(consume());

                while (peek().has_value() &&
                       std::isalnum(static_cast<unsigned char>(peek().value()))) {
                    buf.push_back(consume());
                }

                if (buf == "return") {
                    tokens.push_back({.type = TokenType::_return});
                    buf.clear();
                } else {
                    std::cerr << "Ya gone fucked up buckaroo" << std::endl;
                    exit(EXIT_FAILURE);
                }

            } else if (std::isdigit(static_cast<unsigned char>(c))) {
                buf.push_back(consume());

                while (peek().has_value() &&
                       std::isdigit(static_cast<unsigned char>(peek().value()))) {
                    buf.push_back(consume());
                }

                tokens.push_back({
                    .type = TokenType::int_lit,
                    .value = buf
                });
                buf.clear();

            } else if (c == ';') {
                tokens.push_back({.type = TokenType::semi});
                consume();

            } else if (std::isspace(static_cast<unsigned char>(c))) {
                consume();

            } else {
                std::cerr << "Ya gone fucked up buckaroo" << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        return tokens;
    }

private:
    [[nodiscard]] inline std::optional<char> peek(size_t ahead = 0) const {
        if (mIndex + ahead >= mSrc.length()) {
            return std::nullopt;
        }

        return mSrc.at(mIndex + ahead);
    }

    inline char consume() {
        return mSrc.at(mIndex++);
    }

    const std::string mSrc;
    size_t mIndex = 0;
};