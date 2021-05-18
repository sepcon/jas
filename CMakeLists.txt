cmake_minimum_required(VERSION 3.5)

project(jas_parser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(D:/SourceCode/GitHub/json11)
add_library(jas_parser
    String.h
    Exception.h
    EvaluableClasses.h
    Evaluable.h
    Parser.h
    SyntaxValidator.h
    SyntaxEvaluator.h
    EvalContext.h
    JevalContext.h
    EvaluableClasses.h
    Json.h
    Invocable.h
    Keywords.h
    SupportedFunctions.h

    Parser.cpp
    SyntaxEvaluator.cpp
    SyntaxValidator.cpp
    JevalContext.cpp
    Invocable.cpp
    )

add_definitions(-DJAS_CHAR_TYPE=char)

add_executable(jas_test test.cpp)
add_executable(jas_sample main.cpp)

add_library(json D:/SourceCode/GitHub/json11/json11.cpp)

target_link_libraries(jas_parser json)
target_link_libraries(jas_test json jas_parser)
target_link_libraries(jas_sample json jas_parser)
