add_library(stdexec INTERFACE)
add_library(NVIDIA::stdexec ALIAS stdexec)

target_compile_features(stdexec INTERFACE cxx_std_23)

target_compile_options(stdexec INTERFACE $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-fcoroutines -fconcepts-diagnostics-depth=10>)

target_compile_options(stdexec INTERFACE $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:__cplusplus /Zc:preprocessor /Zc:externConstexpr /bigobj>)

target_include_directories(stdexec INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
target_link_libraries(stdexec INTERFACE Threads::Threads)

target_compile_definitions(stdexec INTERFACE "STDEXEC_NAMESPACE=stdexec")
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(stdexec INTERFACE STDEXEC_ENABLE_WINDOWS_THREAD_POOL)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(stdexec INTERFACE STDEXEC_ENABLE_LIBDISPATCH)
endif ()

add_library(parallel_scheduler STATIC)
add_library(NVIDIA::parallel_scheduler ALIAS parallel_scheduler)

target_link_libraries(parallel_scheduler PUBLIC stdexec)

target_compile_options(parallel_scheduler PUBLIC $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:__cplusplus /Zc:preprocessor /Zc:externConstexpr>)

target_sources(parallel_scheduler
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parallel_scheduler/parallel_scheduler.cpp
)