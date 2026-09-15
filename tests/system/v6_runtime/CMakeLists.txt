# tests/system/v6_runtime/CMakeLists.txt — RUNTIME-CI-001 运行面系统测试
#
# 自包含: cmake -S tests/system/v6_runtime -B <build> && ctest --test-dir <build>
# 也可被控制器以 add_subdirectory(tests/system/v6_runtime) 纳入根构建。
# 根/公共 CMakeLists 不属本任务写域（C-004.4）。
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(astrocs_v6_runtime_system LANGUAGES CXX)
  enable_testing()
endif()

set(V6RT_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
get_filename_component(V6RT_REPO "${V6RT_REPO}" ABSOLUTE)

# 契约测试: 只依赖 cli/v6_runtime_contract.h（纯头文件，无科学依赖）。
add_executable(v6_runtime_contract_test v6_runtime_contract_test.cpp)
target_include_directories(v6_runtime_contract_test PRIVATE "${V6RT_REPO}/cli")
set_target_properties(v6_runtime_contract_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(v6_runtime_contract_test PRIVATE Threads::Threads m)
else()
  target_link_libraries(v6_runtime_contract_test PRIVATE m)
endif()

add_executable(v6_runtime_determinism_test v6_runtime_determinism_test.cpp)
target_include_directories(v6_runtime_determinism_test PRIVATE "${V6RT_REPO}/cli")
set_target_properties(v6_runtime_determinism_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# §10.5 记录链: 真实 /proc 观测下的每线程 CPU / I/O wait 字段面。
add_executable(v6_runtime_resource_record_test v6_runtime_resource_record_test.cpp)
target_include_directories(v6_runtime_resource_record_test PRIVATE "${V6RT_REPO}/cli")
set_target_properties(v6_runtime_resource_record_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(v6_runtime_determinism_test PRIVATE Threads::Threads m)
  target_link_libraries(v6_runtime_resource_record_test PRIVATE Threads::Threads m)
else()
  target_link_libraries(v6_runtime_determinism_test PRIVATE m)
  target_link_libraries(v6_runtime_resource_record_test PRIVATE m)
endif()

# ---- 契约检查项（每项独立 ctest 目标, 便于 CI 逐名验收） ----
add_test(NAME v6_runtime_contract_units      COMMAND v6_runtime_contract_test units)
add_test(NAME v6_runtime_mode_routing        COMMAND v6_runtime_contract_test modes)
add_test(NAME v6_runtime_budget_single_source COMMAND v6_runtime_contract_test budget)
add_test(NAME v6_runtime_so05_policy         COMMAND v6_runtime_contract_test so05)
add_test(NAME v6_runtime_phase_isolation     COMMAND v6_runtime_contract_test isolation)
add_test(NAME v6_runtime_metric_fields       COMMAND v6_runtime_contract_test metrics)
add_test(NAME v6_runtime_contract_negative   COMMAND v6_runtime_contract_test negative)

# ---- 并行确定性回归锁 ----
add_test(NAME v6_runtime_determinism_positive
  COMMAND v6_runtime_determinism_test positive 8 200000)
add_test(NAME v6_runtime_determinism_negative
  COMMAND v6_runtime_determinism_test negative 8 200000)

set_tests_properties(
  v6_runtime_contract_units v6_runtime_mode_routing v6_runtime_budget_single_source
  v6_runtime_so05_policy v6_runtime_phase_isolation v6_runtime_metric_fields
  v6_runtime_contract_negative
  PROPERTIES LABELS "v6;runtime;RUNTIME-CI-001" TIMEOUT 120)
add_test(NAME v6_runtime_resource_record_positive
  COMMAND v6_runtime_resource_record_test positive "${CMAKE_CURRENT_BINARY_DIR}/rec_pos")
add_test(NAME v6_runtime_resource_record_negative
  COMMAND v6_runtime_resource_record_test negative "${CMAKE_CURRENT_BINARY_DIR}/rec_neg")

set_tests_properties(v6_runtime_determinism_positive v6_runtime_determinism_negative
  PROPERTIES LABELS "v6;runtime;determinism;RUNTIME-CI-001" TIMEOUT 600)
set_tests_properties(v6_runtime_resource_record_positive v6_runtime_resource_record_negative
  PROPERTIES LABELS "v6;runtime;resource;RUNTIME-CI-001" TIMEOUT 300)
