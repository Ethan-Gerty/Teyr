//
// Created by ethan on 21/10/2026.
// Last modified on 21/10/2026.
//

#pragma once

class Generator {
public:
    inline Generator(NodeReturn root)
            : mRoot(std::move(root)) {

    }

    [[nodiscard]] std::string generate() const {
        std::stringstream output;
        output << "global _start\n_start:\n";
        output << "    mov rax, 60\n";
        output << "    mov rdi, " << mRoot.expr.int_lit.value.value() << "\n";
        output << "    syscall\n";
        return output.str();
    }

private:
    const NodeReturn mRoot;
};