cmake_minimum_required(VERSION 3.15)

# Project definitions
add_definitions(
    -D_CRT_SECURE_NO_DEPRECATE
    -D_CRT_SECURE_NO_WARNINGS
    -DUNICODE
    -D_UNICODE
)

project(windows_internals CXX)

add_executable(getversionex source.cpp
#app.manifest
)

target_compile_features(getversionex PUBLIC cxx_std_23)
