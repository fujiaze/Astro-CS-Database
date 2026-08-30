# CORE-*/IO-*/CPU-* 单元测试 (G2/G3 任务逐步加入)
add_executable(core_contracts_test core_contracts_test.cpp)
target_include_directories(core_contracts_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_contracts_test PRIVATE astrocs_contracts)
add_test(NAME core_contracts COMMAND core_contracts_test)

add_executable(core_artifact_test core_artifact_test.cpp)
target_include_directories(core_artifact_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_artifact_test PRIVATE astrocs_core)
add_test(NAME core_artifact COMMAND core_artifact_test)

add_executable(core_module_test core_module_test.cpp)
target_include_directories(core_module_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_module_test PRIVATE astrocs_core)
add_test(NAME core_module COMMAND core_module_test)

add_executable(core_pipeline_test core_pipeline_test.cpp)
target_include_directories(core_pipeline_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_pipeline_test PRIVATE astrocs_core)
add_test(NAME core_pipeline COMMAND core_pipeline_test)

add_executable(core_context_test core_context_test.cpp)
target_include_directories(core_context_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_context_test PRIVATE astrocs_core)
add_test(NAME core_context COMMAND core_context_test)

add_executable(core_scheduler_test core_scheduler_test.cpp)
target_include_directories(core_scheduler_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_scheduler_test PRIVATE astrocs_core)
add_test(NAME core_scheduler COMMAND core_scheduler_test)

add_executable(core_checkpoint_test core_checkpoint_test.cpp)
target_include_directories(core_checkpoint_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_checkpoint_test PRIVATE astrocs_core)
add_test(NAME core_checkpoint COMMAND core_checkpoint_test)

add_executable(core_logging_test core_logging_test.cpp)
target_include_directories(core_logging_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(core_logging_test PRIVATE astrocs_core)
add_test(NAME core_logging COMMAND core_logging_test)

add_executable(io_adapter_test io_adapter_test.cpp)
target_include_directories(io_adapter_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(io_adapter_test PRIVATE astrocs_io)
add_test(NAME io_adapter COMMAND io_adapter_test)

add_executable(io_ownership_test io_ownership_test.cpp)
target_include_directories(io_ownership_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/include)
target_link_libraries(io_ownership_test PRIVATE astrocs_aio astrocs_core)
add_test(NAME io_ownership COMMAND io_ownership_test)

add_executable(io_reentrant_test io_reentrant_test.cpp)
target_include_directories(io_reentrant_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/astro_image_io/third_party/cfitsio)
target_link_libraries(io_reentrant_test PRIVATE astrocs_cfitsio z m)
add_test(NAME io_reentrant COMMAND io_reentrant_test)

add_executable(cpu_abi_test cpu_abi_test.cpp)
target_include_directories(cpu_abi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(cpu_abi_test PRIVATE astrocs_cpu astrocs_contracts)
add_test(NAME cpu_abi COMMAND cpu_abi_test)
