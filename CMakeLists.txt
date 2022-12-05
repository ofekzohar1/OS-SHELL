cmake_minimum_required(VERSION 3.16)
project(os2 C)

set(CMAKE_C_STANDARD 11)

add_executable(os2 shell.c myshell.c)
