cmake_minimum_required(VERSION 4.0)

project(Teyr)

set(CMAKE_CXX_STANDARD 20)

add_executable(Teyr
        src/main.cpp
        src/Tokenization.h
        src/Parser.h
        src/Generation.h)