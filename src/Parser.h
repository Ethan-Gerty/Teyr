//
// Created by ethan on 21/10/2026.
// Last modified on 21/10/2026.
//

#pragma once

#include <vector>

#include "Tokenization.h"



struct NodeExpr {
    Token int_lit;
};

struct NodeReturn {
    NodeExpr expr;
};


class Parser {
public:
    inline explicit Parser(std::vector<Token> tokens)
        : mTokens(std::move(tokens)) { }


    std::optional<NodeExpr> parseExpr() {
        if (peek().has_value() && peek().value().type == TokenType::int_lit) {
            return NodeExpr{ .int_lit = consume() };
        } else {
            return {};
        }
    }

    std::optional<NodeReturn> parse() {
        std::optional<NodeReturn> returnNode;
        while (peek().has_value()) {
            if (peek().value().type == TokenType::_return) {
                consume();

                if (auto nodeExpr = parseExpr()) {
                    returnNode = NodeReturn{ .expr = nodeExpr.value() };
                } else {
                    std::cerr << "Expected expression after return\n";
                    exit(EXIT_FAILURE);
                }

                if (peek().has_value() &&
                    peek().value().type == TokenType::semi) {
                    consume();
                    } else {
                        std::cerr << "Expected ';' after expression\n";
                        exit(EXIT_FAILURE);
                    }

            } else {
                std::cerr << "Unexpected token\n";
                exit(EXIT_FAILURE);
            }
        }
        return returnNode;
    }
private:
    [[nodiscard]] inline std::optional<Token> peek(size_t ahead = 0) const {
        if (mIndex + ahead >= mTokens.size()) {
            return std::nullopt;
        }

        return mTokens.at(mIndex + ahead);
    }

    inline Token consume() {
        return mTokens.at(mIndex++);
    }

    const std::vector<Token> mTokens;
    size_t mIndex = 0;
};
