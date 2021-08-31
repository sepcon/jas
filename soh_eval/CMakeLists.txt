cmake_minimum_required(VERSION 3.5)

project(soh_eval LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${JAS_INCLUDE_PATHS})
add_executable(soh_eval
    main.cpp
    SOHEvaluator.cpp

    SOHEvaluator.h
    )

target_link_libraries(soh_eval ${JAS_LIBS_ALL})
target_precompile_headers(soh_eval REUSE_FROM jas)
