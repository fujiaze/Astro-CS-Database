# ============================================================================
# weight_chain Oracle 独立构建（自包含；无第三方依赖）
#   cmake -S lib/algorithms/integration/v6/oracle -B <build>
#   cmake --build <build> && <build>/weight_chain_selfcheck
# 说明: 前台统一构建；本文件只登记 Oracle 目标，不注册 ctest（eng/tests/ 归测试分片）。
# ============================================================================
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(astrocs_weight_chain_oracle LANGUAGES CXX)
endif()

add_executable(weight_chain_selfcheck
  "${CMAKE_CURRENT_SOURCE_DIR}/weight_chain_selfcheck.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/weight_chain.cpp")
target_include_directories(weight_chain_selfcheck PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set_target_properties(weight_chain_selfcheck PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(UNIX)
  target_link_libraries(weight_chain_selfcheck PRIVATE m)
endif()
