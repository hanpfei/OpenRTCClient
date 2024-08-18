cmake_minimum_required(VERSION 3.4.1)
project(jnitest LANGUAGES C CXX)

add_library(jnitest
        SHARED
        src/debug_utils.cpp
        src/media_jni.cpp)

target_include_directories(jnitest
        PRIVATE
        src)

#include libraries needed for echo lib
target_link_libraries(jnitest
        PRIVATE
        OpenSLES
        aaudio
        android
        log
        atomic)

target_compile_options(jnitest
        PRIVATE
        -Wall)
