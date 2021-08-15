cmake_minimum_required(VERSION 3.5)

project(jas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JAS_INCLUDE_PATHS include)

set(JAS_LIBS_ALL jas)

if(NOT JAS_JSON_TYPE)
    set(JAS_JSON_TYPE nlohmann)
elseif("${JAS_JSON_TYPE}" STREQUAL "axzdict")
    message("USE ${JAS_JSON_TYPE}")
    add_definitions(-DJAS_USE_WSTR)
    if(NOT AXZ_AGENT_VERSION_STRING)
        set(AXZ_AGENT_VERSION_STRING "1.1")
    endif(NOT AXZ_AGENT_VERSION_STRING)
    add_subdirectory(3rd/axzdict)
    set(JAS_LIBS_ALL ${JAS_LIBS_ALL} axzdct)

elseif("${JAS_JSON_TYPE}" STREQUAL "json11")
    message("USE ${JAS_JSON_TYPE}")
    add_subdirectory(3rd/json11)
    set(JAS_LIBS_ALL ${JAS_LIBS_ALL} json11)
else()
    message(ERROR "Unsupported json type")
endif()

set(JAS_INCLUDE_PATHS ${JAS_INCLUDE_PATHS} 3rd/${JAS_JSON_TYPE})
set(JAS_INCLUDE_PATHS ${JAS_INCLUDE_PATHS} include/jas/json_trait/${JAS_JSON_TYPE})

include_directories(${JAS_INCLUDE_PATHS})

add_library(jas
    include/jas/Version.h
    include/jas/String.h
    include/jas/Exception.h
    include/jas/EvaluableClasses.h
    include/jas/Evaluable.h
    include/jas/Parser.h
    include/jas/SyntaxValidator.h
    include/jas/SyntaxEvaluator.h
    include/jas/EvalContextIF.h
    include/jas/EvalContextBase.h
    include/jas/HistoricalEvalContext.h
    include/jas/Json.h
    include/jas/jaop.h
    include/jas/Keywords.h
    include/jas/CIF.h
    include/jas/ConsoleLogger.h

    src/Parser.cpp
    src/SyntaxEvaluator.cpp
    src/SyntaxValidator.cpp
    src/EvalContextBase.cpp
    src/HistoricalEvalContext.cpp
    src/CIF.cpp
    )

add_subdirectory(test)
include(pch.cmake)
target_precompile_headers(jas PRIVATE ${JAS_PRECOMPILE_HEADERS})

message("ALL NEEDED LIBS: ${JAS_LIBS_ALL}")

