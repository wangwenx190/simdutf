cmake_minimum_required(VERSION 4.0)

project(simdutf
    VERSION 9.1.0
    DESCRIPTION "Fast Unicode validation, transcoding and processing"
    LANGUAGES CXX
)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "simdutf supports only 64-bit targets.")
endif()
set(__target_processor "${CMAKE_SYSTEM_PROCESSOR}")
if(MSVC AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
    set(__target_processor "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
endif()
string(TOLOWER "${__target_processor}" __target_processor)
if(NOT __target_processor MATCHES "^(amd64|x64|x86_64|aarch64|arm64)$")
    message(FATAL_ERROR
        "simdutf supports only x86_64 and arm64 targets; got '${__target_processor}'."
    )
endif()

if(WIN32)
    enable_language(RC)
endif()
if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT "x${CMAKE_CXX_FLAGS}" STREQUAL "x")
    if(MSVC)
        string(REGEX REPLACE " [/-]W[01234] " " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    else()
        string(REGEX REPLACE " -W(all)?(extra)? " " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
        string(REGEX REPLACE " -[W]?pedantic " " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    endif()
endif()
string(APPEND CMAKE_CXX_FLAGS " -w ")

set(__top_level ON)
if("x${CMAKE_SOURCE_DIR}" STREQUAL "x${PROJECT_SOURCE_DIR}")
    message("SIMDUTF: Building as a top-level project.")
else()
    set(__top_level OFF)
    message("SIMDUTF: Building as a subproject.")
endif()

option(SIMDUTF_BUILD_SHARED "Build shared library edition of simdutf." ON)
option(SIMDUTF_BUILD_STATIC "Build static library edition of simdutf." ON)
option(SIMDUTF_INSTALL "Enable installation for simdutf." OFF)
if(NOT SIMDUTF_BUILD_SHARED AND NOT SIMDUTF_BUILD_STATIC)
    message(SEND_ERROR "You MUST build either the shared library or the static library or both of them!")
endif()

set(__targets)
if(SIMDUTF_BUILD_SHARED)
    add_library(${PROJECT_NAME}_shared SHARED)
    add_library(${PROJECT_NAME}::${PROJECT_NAME}_shared ALIAS ${PROJECT_NAME}_shared)
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
    if(WIN32)
        # CMake cannot generate an automatic export table from MSVC LTCG objects.
        set_property(TARGET ${PROJECT_NAME}_shared PROPERTY
            INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF
        )
    endif()
    target_compile_definitions(${PROJECT_NAME}_shared
        PRIVATE SIMDUTF_BUILDING_WINDOWS_DYNAMIC_LIBRARY=1
        INTERFACE SIMDUTF_USING_WINDOWS_DYNAMIC_LIBRARY=1
    )
    if(WIN32)
        target_sources(${PROJECT_NAME}_shared PRIVATE ${PROJECT_NAME}.rc)
    endif()
    list(APPEND __targets ${PROJECT_NAME}_shared)
endif()
if(SIMDUTF_BUILD_STATIC)
    add_library(${PROJECT_NAME}_static STATIC)
    add_library(${PROJECT_NAME}::${PROJECT_NAME}_static ALIAS ${PROJECT_NAME}_static)
    list(APPEND __targets ${PROJECT_NAME}_static)
endif()
if(SIMDUTF_BUILD_SHARED AND NOT SIMDUTF_BUILD_STATIC)
    add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME}_shared)
endif()
if(SIMDUTF_BUILD_STATIC AND NOT SIMDUTF_BUILD_SHARED)
    add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME}_static)
endif()

include(GNUInstallDirs)
file(GLOB_RECURSE __headers LIST_DIRECTORIES false CONFIGURE_DEPENDS
    "./include/*.h" "./src/*.h"
)
set(__sources ${__headers} src/simdutf.cpp)
foreach(__target IN LISTS __targets)
    target_sources(${__target} PRIVATE ${__sources})
    target_include_directories(${__target}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    if(__top_level)
        if(WIN32)
            target_compile_definitions(${__target} PRIVATE
                UNICODE _UNICODE
                NOMINMAX
                WIN32_LEAN_AND_MEAN
            )
        endif()
        if(MSVC)
            target_compile_options(${__target} PRIVATE
                /utf-8 /permissive-
                $<$<NOT:$<CONFIG:Debug>>:/Gw /Gy /Zc:inline>
            )
            target_link_options(${__target} PRIVATE
                $<$<NOT:$<CONFIG:Debug>>:/OPT:REF /OPT:ICF>
            )
        else()
            target_compile_options(${__target} PRIVATE
                $<$<NOT:$<CONFIG:Debug>>:-ffunction-sections -fdata-sections>
            )
            if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
                    AND __target_processor MATCHES "^(amd64|x64|x86_64)$")
                target_compile_options(${__target} PRIVATE
                    -mno-avx256-split-unaligned-load
                    -mno-avx256-split-unaligned-store
                )
            endif()
            if(APPLE)
                target_link_options(${__target} PRIVATE
                    $<$<NOT:$<CONFIG:Debug>>:-Wl,-dead_strip>
                )
            else()
                target_link_options(${__target} PRIVATE
                    $<$<NOT:$<CONFIG:Debug>>:-Wl,--gc-sections>
                )
            endif()
        endif()
    endif()
    set_target_properties(${__target} PROPERTIES VERSION 35.0.0)
    get_target_property(__type ${__target} TYPE)
    if(NOT "x${__type}" STREQUAL "xSTATIC_LIBRARY")
        set_target_properties(${__target} PROPERTIES SOVERSION 35)
    endif()
    if(SIMDUTF_INSTALL)
        install(TARGETS ${__target}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()
endforeach()

if(SIMDUTF_INSTALL)
    install(
        FILES include/simdutf.h include/simdutf_c.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(
        DIRECTORY include/simdutf
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )
endif()
