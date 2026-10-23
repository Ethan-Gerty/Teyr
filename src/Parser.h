//
// Created by ethan on 21/10/2026.
// Last modified on 23/10/2026.
//

#pragma once

#include "Tokenization.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// AST = Abstract Syntax Tree. Each node represents the MEANINGFUL
// structure of the source rather than individual punctuation tokens.

// Expression Nodes
struct NodeExpr;
using ExprPtr = std::unique_ptr<NodeExpr>;

struct NodeExprLiteral { Token literal; };
struct NodeExprIdentifier { Token name; };
struct NodeExprUnary { Token op; ExprPtr operand; };
struct NodeExprBinary { ExprPtr left; Token op; ExprPtr right; };
struct NodeExprAssign { ExprPtr target; ExprPtr value; };
struct NodeExprMember { ExprPtr object; Token name; };
struct NodeExprCall { ExprPtr callee; std::vector<ExprPtr> arguments; };

struct NodeExpr {
    using Value = std::variant<NodeExprLiteral, NodeExprIdentifier,
                               NodeExprUnary, NodeExprBinary,
                               NodeExprAssign, NodeExprMember, NodeExprCall>;
    Value var;
};


// Statement Nodes
struct NodeStmt;
using StmtPtr = std::unique_ptr<NodeStmt>;

struct NodeStmtBlock { std::vector<StmtPtr> statements; };
struct NodeStmtVar { Token type; Token name; ExprPtr initializer; };
struct NodeStmtReturn { ExprPtr value; }; // nullptr represents "return;"
struct NodeStmtExpr { ExprPtr expression; };
struct NodeStmtIf { ExprPtr condition; StmtPtr thenBranch; StmtPtr elseBranch; };

struct NodeStmt {
    using Value = std::variant<NodeStmtBlock, NodeStmtVar, NodeStmtReturn,
                               NodeStmtExpr, NodeStmtIf>;
    Value var;
};


struct NodeMethod {
    std::vector<Token> modifiers;  // e.g. public static
    Token returnType;
    Token name;
    NodeStmtBlock body;
};


struct NodeClass {
    std::vector<Token> modifiers;
    Token name;
    std::vector<NodeMethod> methods;
};


struct NodeProg {
    std::vector<std::string> imports;
    std::vector<NodeClass> classes;
};



class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : mTokens(std::move(tokens)) {}

    // Entry point: parse the ENTIRE input and report any unexpected tokens.
    [[nodiscard]] NodeProg parseProg() {
        mIndex = 0;
        NodeProg program;

        while (match(TokenType::_import)) {
            program.imports.push_back(parseQualifiedName());
            expect(TokenType::semi, "Expected ';' after using directive");
        }

        while (!atEnd()) {
            program.classes.push_back(parseClass());
        }

        if (program.classes.empty()) error("Expected a class declaration");
        return program;
    }

private:
    // Returns true if at the end of token Vector
    [[nodiscard]] bool atEnd() const { return mIndex >= mTokens.size(); }

    // Returns pointer to token at mIndex + ahead
    [[nodiscard]] const Token* peek(std::size_t ahead = 0) const {
        if (ahead >= mTokens.size() - mIndex) return nullptr;
        return &mTokens[mIndex + ahead];
    }

    // Returns true if current token is given type
    [[nodiscard]] bool check(TokenType type) const {
        const Token* token = peek();
        return token && token->type == type;
    }

    // Returns true if current token is given type AND increments mIndex
    [[nodiscard]] bool match(TokenType type) {
        if (!check(type)) return false;
        ++mIndex;
        return true;
    }

    // Returns token at incremented mIndex
    Token consume() { return mTokens[mIndex++]; } // Call only after a check.

    // Throws error message with the current tokens line and column no.
    [[noreturn]] void error(const std::string& message) const {
        std::size_t line = 1;
        std::size_t column = 1;

        if (const Token* token = peek()) {
            line = token->line;
            column = token->column;
        } else if (!mTokens.empty()) {
            line = mTokens.back().line;
            column = mTokens.back().column + 1;
        }

        throw std::runtime_error("Line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + message);
    }

    // Returns next token only if current token is the given(expected) token
    Token expect(TokenType type, const std::string& message) {
        if (!check(type)) error(message);
        return consume();
    }

    [[nodiscard]] static bool isType(TokenType type) {
        return type == TokenType::_int || type == TokenType::_bool ||
               type == TokenType::_string || type == TokenType::_void ||
               type == TokenType::identifier;
    }

    [[nodiscard]] static bool isModifier(TokenType type) {
        return type == TokenType::_public || type == TokenType::_private ||
               type == TokenType::_static;
    }


    std::vector<Token> parseModifiers() {
        std::vector<Token> result;
        while (peek() && isModifier(peek()->type)) {
            result.push_back(consume());
        }
        return result;
    }

    std::string parseQualifiedName() {
        std::string name = expect(TokenType::identifier,
                                  "Expected identifier after 'using'").value.value();
        while (match(TokenType::dot)) {
            name += "." + expect(TokenType::identifier,
                                  "Expected identifier after '.'").value.value();
        }
        return name;
    }

    NodeClass parseClass() {
        auto modifiers = parseModifiers();
        expect(TokenType::_class, "Expected 'class' declaration");
        Token name = expect(TokenType::identifier, "Expected class name");
        expect(TokenType::open_brace, "Expected '{' after class name");

        std::vector<NodeMethod> methods;
        while (!check(TokenType::close_brace)) {
            if (atEnd()) error("Expected '}' to close class");
            methods.push_back(parseMethod());
        }
        consume(); // }
        return NodeClass{std::move(modifiers), std::move(name), std::move(methods)};
    }

    NodeMethod parseMethod() {
        auto modifiers = parseModifiers();
        if (!peek() || !isType(peek()->type)) error("Expected method return type");
        Token returnType = consume();
        Token name = expect(TokenType::identifier, "Expected method name");
        expect(TokenType::open_paren, "Expected '(' after method name");
        // First version: methods can have NO parameters.
        expect(TokenType::close_paren, "Method parameters aren't supported yet; expected ')'");
        NodeStmtBlock body = parseBlock();
        return NodeMethod{std::move(modifiers), std::move(returnType),
                          std::move(name), std::move(body)};
    }

    NodeStmtBlock parseBlock() {
        expect(TokenType::open_brace, "Expected '{' to start block");
        NodeStmtBlock result;
        while (!check(TokenType::close_brace)) {
            if (atEnd()) error("Expected '}' to close block");
            result.statements.push_back(parseStmt());
        }
        consume(); // }
        return result;
    }

    StmtPtr parseStmt() {
        if (check(TokenType::open_brace)) {
            return std::make_unique<NodeStmt>(NodeStmt{parseBlock()});
        }

        if (match(TokenType::_return)) {
            ExprPtr value;
            if (!check(TokenType::semi)) value = parseExpr();
            expect(TokenType::semi, "Expected ';' after return statement");
            return std::make_unique<NodeStmt>(NodeStmt{NodeStmtReturn{std::move(value)}});
        }

        if (match(TokenType::_if)) {
            expect(TokenType::open_paren, "Expected '(' after if");
            ExprPtr condition = parseExpr();
            expect(TokenType::close_paren, "Expected ')' after if condition");
            StmtPtr thenBranch = parseStmt();
            StmtPtr elseBranch;
            if (match(TokenType::_else)) elseBranch = parseStmt();
            return std::make_unique<NodeStmt>(NodeStmt{
                NodeStmtIf{std::move(condition), std::move(thenBranch),
                           std::move(elseBranch)}
            });
        }

        // Built-in declaration: int x = 1; / var x = 1;
        // User-defined type: Thing item = ...; (identifier identifier)
        if (check(TokenType::_int) || check(TokenType::_bool) ||
            check(TokenType::_string) || check(TokenType::_var) ||
            (check(TokenType::identifier) && peek(1) &&
             peek(1)->type == TokenType::identifier)) {
            Token type = consume();
            Token name = expect(TokenType::identifier, "Expected variable name");
            ExprPtr initializer;
            if (match(TokenType::assign)) initializer = parseExpr();
            if (type.type == TokenType::_var && !initializer) {
                error("'var' requires an initializer");
            }
            expect(TokenType::semi, "Expected ';' after variable declaration");
            return std::make_unique<NodeStmt>(NodeStmt{
                NodeStmtVar{std::move(type), std::move(name), std::move(initializer)}
            });
        }

        ExprPtr expression = parseExpr();
        expect(TokenType::semi, "Expected ';' after expression statement");
        return std::make_unique<NodeStmt>(NodeStmt{NodeStmtExpr{std::move(expression)}});
    }

    // Expression grammar:
    // expression -> assignment
    // assignment -> binary ('=' assignment)?
    // binary     -> unary (binary_operator unary)* (with precedence)
    // unary      -> ('!' | '-' | '+') unary | postfix
    // postfix    -> primary ('.' identifier | '(' args? ')')*
    // primary    -> literal | identifier | '(' expression ')'

    ExprPtr parseExpr() {
        ExprPtr left = parseBinary(1);
        if (match(TokenType::assign)) {
            if (!std::holds_alternative<NodeExprIdentifier>(left->var) &&
                !std::holds_alternative<NodeExprMember>(left->var)) {
                error("Invalid assignment target");
            }
            ExprPtr right = parseExpr(); // right-associative: a = b = 1
            return std::make_unique<NodeExpr>(NodeExpr{
                NodeExprAssign{std::move(left), std::move(right)}
            });
        }
        return left;
    }

    [[nodiscard]] static int precedence(TokenType type) {
        switch (type) {
            case TokenType::or_or: return 1;
            case TokenType::and_and: return 2;
            case TokenType::equal_equal:
            case TokenType::bang_equal: return 3;
            case TokenType::less:
            case TokenType::less_equal:
            case TokenType::greater:
            case TokenType::greater_equal: return 4;
            case TokenType::plus:
            case TokenType::minus: return 5;
            case TokenType::star:
            case TokenType::slash: return 6;
            default: return 0;
        }
    }

    ExprPtr parseBinary(int minPrecedence) {
        ExprPtr left = parseUnary();
        while (peek() && precedence(peek()->type) >= minPrecedence) {
            Token op = consume();
            const int nextMinimum = precedence(op.type) + 1; // left-associative
            ExprPtr right = parseBinary(nextMinimum);
            left = std::make_unique<NodeExpr>(NodeExpr{
                NodeExprBinary{std::move(left), std::move(op), std::move(right)}
            });
        }
        return left;
    }

    ExprPtr parseUnary() {
        if (check(TokenType::bang) || check(TokenType::minus) ||
            check(TokenType::plus)) {
            Token op = consume();
            ExprPtr operand = parseUnary();
            return std::make_unique<NodeExpr>(NodeExpr{
                NodeExprUnary{std::move(op), std::move(operand)}
            });
        }
        return parsePostfix();
    }

    ExprPtr parsePostfix() {
        ExprPtr expr = parsePrimary();
        while (true) {
            if (match(TokenType::dot)) {
                Token name = expect(TokenType::identifier,
                                    "Expected member name after '.'");
                expr = std::make_unique<NodeExpr>(NodeExpr{
                    NodeExprMember{std::move(expr), std::move(name)}
                });
            } else if (match(TokenType::open_paren)) {
                std::vector<ExprPtr> args;
                if (!check(TokenType::close_paren)) {
                    do {
                        args.push_back(parseExpr());
                    } while (match(TokenType::comma));
                }
                expect(TokenType::close_paren, "Expected ')' after call arguments");
                expr = std::make_unique<NodeExpr>(NodeExpr{
                    NodeExprCall{std::move(expr), std::move(args)}
                });
            } else {
                break;
            }
        }
        return expr;
    }

    ExprPtr parsePrimary() {
        if (check(TokenType::int_lit) || check(TokenType::string_lit) ||
            check(TokenType::bool_lit)) {
            return std::make_unique<NodeExpr>(NodeExpr{NodeExprLiteral{consume()}});
        }
        if (check(TokenType::identifier)) {
            return std::make_unique<NodeExpr>(NodeExpr{NodeExprIdentifier{consume()}});
        }
        if (match(TokenType::open_paren)) {
            ExprPtr expr = parseExpr();
            expect(TokenType::close_paren, "Expected ')' after expression");
            return expr;
        }
        error("Expected expression");
    }

    std::vector<Token> mTokens;
    std::size_t mIndex = 0;
};