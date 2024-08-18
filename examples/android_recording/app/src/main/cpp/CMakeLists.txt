cmake_minimum_required(VERSION 3.4.1)
project(awaudio LANGUAGES C CXX)

add_library(awaudio
        SHARED
        src/base/checks.cc
        src/base/logging.cpp
        src/base/time_utils.cpp
        src/utility/file_wrapper.cc
        src/utility/helpers_android.cpp
        src/utility/jvm.cpp
        src/utility/jvm_android.cpp
        src/utility/wav_file.cc
        src/utility/wav_header.cc
        src/audio_engine.cpp
        src/audio_manager.cpp
        src/audio_record_jni.cpp
        src/audio_recorder_impl.cpp
        src/debug_utils.cpp
        src/media_jni.cpp)

target_include_directories(awaudio
        PRIVATE
        src)

#include libraries needed for echo lib
target_link_libraries(awaudio
        PRIVATE
        android
        log
        atomic)

target_compile_options(awaudio
        PRIVATE
        -Wall)
