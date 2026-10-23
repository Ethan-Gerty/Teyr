//
// Created by ethan on 21/10/2026.
// Last modified on 23/10/2026.
//

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "Tokenization.h"
#include "Parser.h"
#include "Generation.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input.teyr>\n";
        return EXIT_FAILURE;
    }

    std::ifstream input(argv[1]);
    if (!input) {
        std::cerr << "Couldn't open: " << argv[1] << '\n';
        return EXIT_FAILURE;
    }

    try {
        std::string source(std::istreambuf_iterator<char>{input}, {});
        auto tokens = Tokenizer(std::move(source)).tokenize();
        NodeProg program = Parser(std::move(tokens)).parseProg();
        Generator generator(program);

        std::ofstream assembly("out.asm");
        if (!assembly) throw std::runtime_error("Couldn't create out.asm");
        assembly << generator.generate();
        assembly.close();
        if (!assembly) throw std::runtime_error("Couldn't write out.asm");

        if (std::system("nasm -f elf64 out.asm -o out.o") != 0) {
            throw std::runtime_error("NASM failed");
        }
        if (std::system("ld out.o -o out") != 0) {
            throw std::runtime_error("Linking failed");
        }

        std::cout << "Compiled successfully: ./out\n";
    } catch (const std::exception& e) {
        std::cerr << "Teyr error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}