target_sources(test
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