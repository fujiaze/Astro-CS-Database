# lib/acr/examples/CMakeLists.txt — 独立示例
add_executable(acr_example_minimal minimal_parallel_for.cpp)
target_link_libraries(acr_example_minimal PRIVATE acr_api)

add_executable(acr_example_chunk legacy_chunk_adapter.cpp)
target_link_libraries(acr_example_chunk PRIVATE acr_api)
