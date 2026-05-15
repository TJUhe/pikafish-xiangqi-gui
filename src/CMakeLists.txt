add_executable(xiangqi WIN32 main.cpp)

if(MSVC)
    target_compile_options(xiangqi PRIVATE /utf-8 /W4)
else()
    target_compile_options(xiangqi PRIVATE -Wall -Wextra -pedantic)
endif()

target_link_libraries(xiangqi PRIVATE user32 gdi32 comctl32)
