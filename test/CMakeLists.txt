cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

function(jas_add_executable ExecutableName)
    add_executable(${ExecutableName} ${ExecutableName}.cpp)
    target_precompile_headers(${ExecutableName} REUSE_FROM jas)
    target_link_libraries(${ExecutableName} ${JAS_ALL_NEEDED_LIBS})
endfunction(jas_add_executable)

jas_add_executable(jase)
jas_add_executable(jas_test)
