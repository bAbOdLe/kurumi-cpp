set(KURUMI_HEADER_DIR ${PROJECT_SOURCE_DIR}/include/Kurumi)

target_sources(test
PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
        ${PROJECT_SOURCE_DIR}/include
    FILES
        ${KURUMI_HEADER_DIR}/RingBuffer.hpp

PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cpp
)

target_link_libraries(test
PUBLIC
    httplib
    nlohmann::json
    NVIDIA::stdexec
PRIVATE
    KurumiConfig
    NVIDIA::parallel_scheduler
)