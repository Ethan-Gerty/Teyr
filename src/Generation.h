//
// Created by ethan on 21/10/2026.
// Last modified on 23/10/2026.
//

#pragma once

#include "Parser.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// First Teyr backend: generate NASM x86-64 assembly for Linux.
// Supported: static int Main(), int/bool/var locals, integers, booleans,
// arithmetic, comparisons, logical operators, assignment, if/else, return.
// Not yet supported: strings, function calls, objects, and true C# types.
class Generator {
public:
    explicit Generator(const NodeProg& program) : mProgram(program) {}

    [[nodiscard]] std::string generate() {
        mBody.str("");
        mBody.clear();
        mScopes.clear();
        mSlots = 0;
        mNextLabel = 0;

        const NodeMethod& main = findMain();
        mExitLabel = newLabel("exit");

        enterScope();
        for (const auto& statement : main.body.statements) {
            generateStmt(*statement);
        }
        leaveScope();

        // Each local gets 8 bytes. Round total space up to 16 bytes.
        const std::size_t bytes = ((mSlots * 8 + 15) / 16) * 16;

        std::ostringstream output;
        output << "global _start\n";
        output << "section .text\n";
        output << "_start:\n";
        output << "    push rbp\n";
        output << "    mov rbp, rsp\n";
        if (bytes != 0) output << "    sub rsp, " << bytes << "\n";

        output << mBody.str();
        // A valid C# int Main needs a return on every reachable path.
        // For now, fallthrough exits with 0; reachability checks come later.
        output << "    mov rax, 0\n";
        output << mExitLabel << ":\n";
        output << "    mov rdi, rax\n"; // exit(status)
        output << "    mov rax, 60\n";  // Linux x86-64 SYS_exit
        output << "    syscall\n";
        return output.str();
    }

private:
    using Scope = std::unordered_map<std::string, std::size_t>;

    [[nodiscard]] const NodeMethod& findMain() const {
        const NodeMethod* result = nullptr;
        for (const auto& cls : mProgram.classes) {
            for (const auto& method : cls.methods) {
                if (method.name.value.value_or("") != "Main") continue;
                bool isStatic = false;
                for (const Token& modifier : method.modifiers) {
                    if (modifier.type == TokenType::_static) isStatic = true;
                }
                if (!isStatic || method.returnType.type != TokenType::_int) {
                    throw std::runtime_error("Entry point must be 'static int Main()'");
                }
                if (result) throw std::runtime_error("More than one Main method found");
                result = &method;
            }
        }
        if (!result) throw std::runtime_error("No 'static int Main()' method found");
        return *result;
    }

    void enterScope() { mScopes.emplace_back(); }
    void leaveScope() { mScopes.pop_back(); }

    // Offsets count downwards from rbp: -8, -16, -24, ...
    [[nodiscard]] std::size_t declareLocal(const std::string& name) {
        Scope& scope = mScopes.back();
        if (scope.find(name) != scope.end()) {
            throw std::runtime_error("Variable already declared: " + name);
        }
        const std::size_t offset = (++mSlots) * 8;
        scope.emplace(name, offset);
        return offset;
    }

    [[nodiscard]] std::size_t localOffset(const std::string& name) const {
        for (auto it = mScopes.rbegin(); it != mScopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return found->second;
        }
        throw std::runtime_error("Unknown variable: " + name);
    }

    [[nodiscard]] std::string newLabel(const std::string& prefix) {
        return ".L" + prefix + "_" + std::to_string(mNextLabel++);
    }

    void generateStmt(const NodeStmt& stmt) {
        if (const auto* block = std::get_if<NodeStmtBlock>(&stmt.var)) {
            enterScope();
            for (const auto& child : block->statements) generateStmt(*child);
            leaveScope();
        } else if (const auto* variable = std::get_if<NodeStmtVar>(&stmt.var)) {
            if (variable->type.type != TokenType::_int &&
                variable->type.type != TokenType::_bool &&
                variable->type.type != TokenType::_var) {
                throw std::runtime_error("Only int, bool and var locals are supported so far");
            }
            if (!variable->initializer) {
                throw std::runtime_error("Local variables need an initializer for now");
            }
            const std::string name = variable->name.value.value();
            // Evaluate the initializer BEFORE reserving the variable's name.
            generateExpr(*variable->initializer);
            const auto offset = declareLocal(name);
            mBody << "    mov qword [rbp - " << offset << "], rax\n";
        } else if (const auto* ret = std::get_if<NodeStmtReturn>(&stmt.var)) {
            if (!ret->value) throw std::runtime_error("int Main must return a value");
            generateExpr(*ret->value);
            mBody << "    jmp " << mExitLabel << "\n";
        } else if (const auto* expr = std::get_if<NodeStmtExpr>(&stmt.var)) {
            if (!std::holds_alternative<NodeExprAssign>(expr->expression->var)) {
                throw std::runtime_error("Only assignment expression statements are supported so far");
            }
            generateExpr(*expr->expression);
        } else if (const auto* branch = std::get_if<NodeStmtIf>(&stmt.var)) {
            const std::string elseLabel = newLabel("else");
            const std::string endLabel = newLabel("endif");
            generateExpr(*branch->condition);
            mBody << "    test rax, rax\n";
            mBody << "    jz " << elseLabel << "\n";
            enterScope();
            generateStmt(*branch->thenBranch);
            leaveScope();
            mBody << "    jmp " << endLabel << "\n";
            mBody << elseLabel << ":\n";
            if (branch->elseBranch) {
                enterScope();
                generateStmt(*branch->elseBranch);
                leaveScope();
            }
            mBody << endLabel << ":\n";
        }
    }

    // Contract: generateExpr leaves its result in rax.
    void generateExpr(const NodeExpr& expr) {
        if (const auto* lit = std::get_if<NodeExprLiteral>(&expr.var)) {
            if (lit->literal.type == TokenType::int_lit) {
                const std::string& text = lit->literal.value.value();
                try { (void)std::stoll(text); }
                catch (const std::exception&) {
                    throw std::runtime_error("Integer literal out of 64-bit range: " + text);
                }
                mBody << "    mov rax, " << text << "\n";
            } else if (lit->literal.type == TokenType::bool_lit) {
                mBody << "    mov rax, " << (lit->literal.value.value() == "true" ? 1 : 0) << "\n";
            } else {
                throw std::runtime_error("String literals aren't supported by the generator yet");
            }
        } else if (const auto* ident = std::get_if<NodeExprIdentifier>(&expr.var)) {
            const auto offset = localOffset(ident->name.value.value());
            mBody << "    mov rax, qword [rbp - " << offset << "]\n";
        } else if (const auto* unary = std::get_if<NodeExprUnary>(&expr.var)) {
            generateExpr(*unary->operand);
            switch (unary->op.type) {
                case TokenType::plus: break;
                case TokenType::minus: mBody << "    neg rax\n"; break;
                case TokenType::bang:
                    mBody << "    test rax, rax\n";
                    mBody << "    sete al\n";
                    mBody << "    movzx rax, al\n";
                    break;
                default: throw std::runtime_error("Unsupported unary operator");
            }
        } else if (const auto* binary = std::get_if<NodeExprBinary>(&expr.var)) {
            generateBinary(*binary);
        } else if (const auto* assign = std::get_if<NodeExprAssign>(&expr.var)) {
            const auto* target = std::get_if<NodeExprIdentifier>(&assign->target->var);
            if (!target) throw std::runtime_error("Only local-variable assignment is supported");
            const auto offset = localOffset(target->name.value.value());
            generateExpr(*assign->value);
            mBody << "    mov qword [rbp - " << offset << "], rax\n";
        } else {
            throw std::runtime_error("Calls and member access aren't supported by the generator yet");
        }
    }

    void generateBinary(const NodeExprBinary& binary) {
        const auto op = binary.op.type;
        if (op == TokenType::and_and || op == TokenType::or_or) {
            // Short circuit: don't evaluate the RHS unless needed.
            const std::string shortcut = newLabel("logical_shortcut");
            const std::string end = newLabel("logical_end");
            generateExpr(*binary.left);
            mBody << "    test rax, rax\n";
            mBody << (op == TokenType::and_and ? "    jz " : "    jnz ") << shortcut << "\n";
            generateExpr(*binary.right);
            mBody << "    test rax, rax\n";
            mBody << "    setne al\n";
            mBody << "    movzx rax, al\n";
            mBody << "    jmp " << end << "\n";
            mBody << shortcut << ":\n";
            mBody << "    mov rax, " << (op == TokenType::and_and ? 0 : 1) << "\n";
            mBody << end << ":\n";
            return;
        }

        generateExpr(*binary.left);  // left in rax
        mBody << "    push rax\n";  // save it
        generateExpr(*binary.right); // right in rax
        mBody << "    pop rcx\n";   // left in rcx, right in rax

        switch (op) {
            case TokenType::plus: mBody << "    add rax, rcx\n"; break;
            case TokenType::minus:
                mBody << "    sub rcx, rax\n    mov rax, rcx\n";
                break;
            case TokenType::star: mBody << "    imul rax, rcx\n"; break;
            case TokenType::slash:
                mBody << "    mov r10, rax\n"; // divisor (right)
                mBody << "    mov rax, rcx\n"; // dividend (left)
                mBody << "    cqo\n";          // sign-extend rax into rdx:rax
                mBody << "    idiv r10\n";     // quotient -> rax
                break;
            case TokenType::equal_equal:
            case TokenType::bang_equal:
            case TokenType::less:
            case TokenType::less_equal:
            case TokenType::greater:
            case TokenType::greater_equal: {
                mBody << "    cmp rcx, rax\n";
                const char* set = nullptr;
                switch (op) {
                    case TokenType::equal_equal: set = "sete"; break;
                    case TokenType::bang_equal: set = "setne"; break;
                    case TokenType::less: set = "setl"; break;
                    case TokenType::less_equal: set = "setle"; break;
                    case TokenType::greater: set = "setg"; break;
                    case TokenType::greater_equal: set = "setge"; break;
                    default: break;
                }
                mBody << "    " << set << " al\n";
                mBody << "    movzx rax, al\n";
                break;
            }
            default: throw std::runtime_error("Unsupported binary operator");
        }
    }

    const NodeProg& mProgram;
    std::ostringstream mBody;
    std::vector<Scope> mScopes;
    std::size_t mSlots = 0;
    std::size_t mNextLabel = 0;
    std::string mExitLabel;
};
