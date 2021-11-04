cmake_minimum_required(VERSION 3.20)
project(SimpleVector)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-O3 -Wall -Wextra -Werror -march=native -mtune=native -fsanitize=address)
add_link_options(-fsanitize=address)

add_executable(SimpleVector main.cpp vector.h)

