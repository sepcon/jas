cmake_minimum_required(VERSION 3.5)

project(jas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(D:/SourceCode/GitHub/json11)

set(JAS_BINARIES_OUTPUT_DIR GLOBAL ${CMAKE_CURRENT_BINARY_DIR})

add_library(jas
    Version.h
    String.h
    Exception.h
    EvaluableClasses.h
    Evaluable.h
    Parser.h
    SyntaxValidator.h
    SyntaxEvaluator.h
    EvalContextIF.h
    EvalContextBase.h
    HistoricalEvalContext.h
    Json.h
    Keywords.h
    CIF.h
    ConsoleLogger.h

    Parser.cpp
    SyntaxEvaluator.cpp
    SyntaxValidator.cpp
    EvalContextBase.cpp
    HistoricalEvalContext.cpp
    CIF.cpp
    )


add_executable(jase main.cpp)
add_executable(jas_test test.cpp)

set(JAS_ALL_NEEDED_LIBS jas)
if(USE_AXZ_DICT)
    add_definitions(
        -DAXZ
        -DJAS_USE_WSTR)
    add_subdirectory(json_trait/axzdict)
    set(JAS_ALL_NEEDED_LIBS ${JAS_ALL_NEEDED_LIBS} axzdct)
elseif(USE_NLOHMANN_JSON)
    message("USE NLOHMAN")
    add_definitions(-DUSE_NLOHMANN_JSON)
else()
    add_library(json11 D:/SourceCode/GitHub/json11/json11.cpp)
    set(JAS_ALL_NEEDED_LIBS ${JAS_ALL_NEEDED_LIBS} json11)
endif(USE_AXZ_DICT)

message("ALL NEEDED LIBS: ${JAS_ALL_NEEDED_LIBS}")
target_link_libraries(jas_test ${JAS_ALL_NEEDED_LIBS})
target_link_libraries(jase ${JAS_ALL_NEEDED_LIBS})
