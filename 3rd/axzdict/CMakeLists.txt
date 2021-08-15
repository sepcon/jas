cmake_minimum_required( VERSION 3.5.0 )

project( axzdct )

if ( MSVC )
    add_definitions( -DAXZDICT_EXPORT -DUNICODE )
endif()

set( CMAKE_SKIP_BUILD_RPATH FALSE )
set( CMAKE_BUILD_WITH_INSTALL_RPATH TRUE )
set( CMAKE_INSTALL_RPATH "$ORIGIN/" )

# for headers included
include_directories( ${CMAKE_SOURCE_DIR}/common )

# for module sources
file( GLOB LIB_DICT_SOURCES *.cpp )

# do generate shared library from sources
add_library( ${PROJECT_NAME} SHARED ${LIB_DICT_SOURCES} )

if ( UNIX AND NOT APPLE )
    # link to specified libstdc++.so file
    target_link_libraries( ${PROJECT_NAME} stdc++ gcc_s )
endif()

# for version
set_target_properties( ${PROJECT_NAME} PROPERTIES VERSION ${AXZ_AGENT_VERSION_STRING} )
