# eng/tests/system/runtime/CMakeLists.txt — RUNTIME-CI-001 运行面系统测试
#
# 自包含: cmake -S eng/tests/system/runtime -B <build> && ctest --test-dir <build>
# 也可被控制器以 add_subdirectory(eng/tests/system/runtime) 纳入根构建。
# 根/公共 CMakeLists 不属本任务写域（C-004.4）。
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(astrocs_runtime_system LANGUAGES CXX)
  enable_testing()
endif()

set(V6RT_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
get_filename_component(V6RT_REPO "${V6RT_REPO}" ABSOLUTE)

# 契约测试: 只依赖 lib/infrastructure/cli/runtime_contract.h（纯头文件，无科学依赖）。
add_executable(runtime_contract_test runtime_contract_test.cpp)
target_include_directories(runtime_contract_test PRIVATE "${V6RT_REPO}/lib/infrastructure/cli")
set_target_properties(runtime_contract_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(runtime_contract_test PRIVATE Threads::Threads)
endif()
# 系统数学库: 平台判断收在共用件 astrocs_platform_math（根 CMakeLists 唯一判定点）
target_link_libraries(runtime_contract_test PRIVATE astrocs_platform_math)

add_executable(runtime_determinism_test runtime_determinism_test.cpp)
target_include_directories(runtime_determinism_test PRIVATE "${V6RT_REPO}/lib/infrastructure/cli")
set_target_properties(runtime_determinism_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# §10.5 记录链: 真实 /proc 观测下的每线程 CPU / I/O wait 字段面。
add_executable(runtime_resource_record_test runtime_resource_record_test.cpp)
target_include_directories(runtime_resource_record_test PRIVATE "${V6RT_REPO}/lib/infrastructure/cli")
set_target_properties(runtime_resource_record_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(runtime_determinism_test PRIVATE Threads::Threads)
  target_link_libraries(runtime_resource_record_test PRIVATE Threads::Threads)
endif()
# 系统数学库: 平台判断收在共用件 astrocs_platform_math（根 CMakeLists 唯一判定点）
target_link_libraries(runtime_determinism_test PRIVATE astrocs_platform_math)
target_link_libraries(runtime_resource_record_test PRIVATE astrocs_platform_math)

# P-163: 事件流写失败（fputs/fflush 返回值）必须被检出，并在发布面变成非 0 退出码
# （IO=7 / 磁盘满 ENOSPC=10）。头文件依赖 jsonl.h（header-only）+ nlohmann/json。
add_executable(runtime_event_stream_test runtime_event_stream_test.cpp)
target_include_directories(runtime_event_stream_test PRIVATE
  "${V6RT_REPO}/lib/infrastructure/cli"
  "${V6RT_REPO}/lib/third_party")
set_target_properties(runtime_event_stream_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# ---- 契约检查项（每项独立 ctest 目标, 便于 CI 逐名验收） ----
add_test(NAME runtime_contract_units      COMMAND runtime_contract_test units)
add_test(NAME runtime_weight_token_routing        COMMAND runtime_contract_test modes)
add_test(NAME runtime_budget_single_source COMMAND runtime_contract_test budget)
add_test(NAME runtime_so05_policy         COMMAND runtime_contract_test so05)
add_test(NAME runtime_phase_isolation     COMMAND runtime_contract_test isolation)
add_test(NAME runtime_metric_fields       COMMAND runtime_contract_test metrics)
add_test(NAME runtime_contract_negative   COMMAND runtime_contract_test negative)

# ---- P-163 事件流写失败（/dev/full 与 fd 关闭注入；spawn 子进程验退出码）----
add_test(NAME runtime_event_stream COMMAND runtime_event_stream_test)

# ---- 并行确定性回归锁 ----
add_test(NAME runtime_determinism_positive
  COMMAND runtime_determinism_test positive 8 200000)
add_test(NAME runtime_determinism_negative
  COMMAND runtime_determinism_test negative 8 200000)

set_tests_properties(
  runtime_contract_units runtime_weight_token_routing runtime_budget_single_source
  runtime_so05_policy runtime_phase_isolation runtime_metric_fields
  runtime_contract_negative runtime_event_stream
  PROPERTIES LABELS "runtime;RUNTIME-CI-001" TIMEOUT 120)
add_test(NAME runtime_resource_record_positive
  COMMAND runtime_resource_record_test positive "${CMAKE_CURRENT_BINARY_DIR}/rec_pos")
add_test(NAME runtime_resource_record_negative
  COMMAND runtime_resource_record_test negative "${CMAKE_CURRENT_BINARY_DIR}/rec_neg")

set_tests_properties(runtime_determinism_positive runtime_determinism_negative
  PROPERTIES LABELS "runtime;determinism;RUNTIME-CI-001" TIMEOUT 600)
set_tests_properties(runtime_resource_record_positive runtime_resource_record_negative
  PROPERTIES LABELS "runtime;resource;RUNTIME-CI-001" TIMEOUT 300)
