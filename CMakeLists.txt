cmake_minimum_required(VERSION 3.16)
project(PikafishXiangqiGui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(src)

install(FILES README.md check_engines.ps1 install_pikafish.ps1
        DESTINATION .)

set(ENGINE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/engines")
if(EXISTS "${ENGINE_SOURCE_DIR}")
    if(EXISTS "${ENGINE_SOURCE_DIR}/pikafish/pikafish.nnue")
        install(FILES "${ENGINE_SOURCE_DIR}/pikafish/pikafish.nnue"
                DESTINATION engines/pikafish)
    endif()
    if(EXISTS "${ENGINE_SOURCE_DIR}/pikafish/Windows")
        install(DIRECTORY "${ENGINE_SOURCE_DIR}/pikafish/Windows"
                DESTINATION engines/pikafish
                FILES_MATCHING PATTERN "*.exe")
    endif()
    if(EXISTS "${ENGINE_SOURCE_DIR}/fairy-stockfish")
        install(DIRECTORY "${ENGINE_SOURCE_DIR}/fairy-stockfish/"
                DESTINATION engines/fairy-stockfish
                FILES_MATCHING
                PATTERN "*.exe"
                PATTERN "variants.ini")
    endif()
    if(EXISTS "${ENGINE_SOURCE_DIR}/fairy-stockfish-nnue")
        install(DIRECTORY "${ENGINE_SOURCE_DIR}/fairy-stockfish-nnue/"
                DESTINATION engines/fairy-stockfish-nnue
                FILES_MATCHING PATTERN "*.exe")
    endif()
endif()
