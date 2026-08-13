cmake_minimum_required(VERSION 4.0)

project(Kurumi)

add_library(KurumiConfig INTERFACE)

target_compile_features(KurumiConfig INTERFACE cxx_std_23)

target_compile_options(KurumiConfig INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(KurumiConfig INTERFACE $<$<AND:$<CXX_COMPILER_ID:GNU>, $<PLATFORM_ID:Windows>>:-lstdc++exp>)

add_executable(test)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)