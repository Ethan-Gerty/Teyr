//
// Created by ethan on 21/10/2026.
// Last modified 21/10/2026.
//

#include <iostream>
#include <fstream>
#include <optional>
#include <sstream>
#include <vector>

#include "./Parser.h"
#include "./Tokenization.h"
#include "./Generation.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Incorrect usage" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <input.teyr>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string contents;
    {
        std::stringstream contentsSs;
        std::fstream input(argv[1], std::ios::in);
        contentsSs << input.rdbuf();
        contents = contentsSs.str();
    }

    Tokenizer tokenizer(std::move(contents));
    std::vector<Token> tokens = tokenizer.tokenize();

    Parser parser(std::move(tokens));
    std::optional<NodeReturn> tree = parser.parse();
    if (!tree.has_value()) {
        std::cerr << "No return statement found\n";
        exit(EXIT_FAILURE);
    }

    Generator generator(tree.value());
    {
        std::fstream file("out.asm", std::ios::out);
        file << generator.generate();
    }


    if (system("nasm -felf64 out.asm -o out.o") != 0) {
        std::cerr << "NASM compilation failed\n";
        return EXIT_FAILURE;
    }
    if (system("ld -o out out.o") != 0) {
        std::cerr << "Linking failed\n";
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}