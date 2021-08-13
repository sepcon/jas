cmake_minimum_required(VERSION 3.5)

project(jas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    jaop.h
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

include(pch.cmake)
target_precompile_headers(jas PRIVATE ${JAS_PRECOMPILE_HEADERS})
target_precompile_headers(jas_test REUSE_FROM jas)
target_precompile_headers(jase REUSE_FROM jas)

set(JAS_ALL_NEEDED_LIBS jas)

if(NOT JAS_JSON_TYPE)
  message("USE NLOHMAN")
  add_definitions(-DUSE_NLOHMANN_JSON)
elseif("${JAS_JSON_TYPE}" STREQUAL "AXZ_JSON")
  message("USE ${JAS_JSON_TYPE}")
  add_definitions(
    -DAXZ_JSON
    -DJAS_USE_WSTR)
  if(NOT AXZ_AGENT_VERSION_STRING)
    set(AXZ_AGENT_VERSION_STRING "1.1")
  endif(NOT AXZ_AGENT_VERSION_STRING)
  add_subdirectory(json_trait/axzdict)
  set(JAS_ALL_NEEDED_LIBS ${JAS_ALL_NEEDED_LIBS} axzdct)

elseif("${JAS_JSON_TYPE}" STREQUAL "JSON11")
  message("USE ${JAS_JSON_TYPE}")
  add_definitions(-DUSE_JSON11)
  add_subdirectory(json_trait/json11)
  include_directories(json_trait/json11)
  set(JAS_ALL_NEEDED_LIBS ${JAS_ALL_NEEDED_LIBS} json11)
else()
  message(ERROR "Unsupported json type")
endif()

message("ALL NEEDED LIBS: ${JAS_ALL_NEEDED_LIBS}")
target_link_libraries(jas_test ${JAS_ALL_NEEDED_LIBS})
target_link_libraries(jase ${JAS_ALL_NEEDED_LIBS})
