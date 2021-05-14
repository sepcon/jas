cmake_minimum_required(VERSION 3.5)

project(jas_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(jas_parser
    jas_string.h
    jas_exception.h
    jas_evaluables.h
    jas_parser.h
    jas_syntax_generator.h
    jas_result_evaluator.h
    test.cpp

    D:/SourceCode/GitHub/json11/json11.cpp
    main.cpp)
