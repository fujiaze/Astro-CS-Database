# ============================================================================
# weight_chain Oracle 独立构建（自包含；无第三方依赖）
#   cmake -S lib/algorithms/integration/phase2_integrate/oracle -B <build>
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
# 平台系统库面: 引用根 CMakeLists 唯一判定点声明的共用件 astrocs_platform_math。
# 本目录**可被独立 configure** (行 8 的「当前目录即工程根」守卫 + 独立 project()):
# 那种情况下没有根入口 ⇒ 共用件未定义, 直接写 astrocs_platform_math 会被 CMake 当
# 成**普通库名**展开成 -lastrocs_platform_math ⇒ 链接期 ld: cannot find。实测
# (FINAL-07): 独立 configure 后未守卫时 build rc=1, 链接行含 -lastrocs_platform_math。
# 故按「目标是否存在」守卫, 不按平台守卫 (平台判断仍只在共用件内部一处)。
if(TARGET astrocs_platform_math)
  target_link_libraries(weight_chain_selfcheck PRIVATE astrocs_platform_math)
endif()

# 重建算子 dump harness：供 recon_exp04_parity.py 与实验单元 EXP-04 的 Python
# 算子实现逐像素对拍（证明生产实现 = 实验被测算子）。非生产路径。
add_executable(recon_dump
  "${CMAKE_CURRENT_SOURCE_DIR}/recon_dump.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/weight_chain.cpp")
target_include_directories(recon_dump PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../include")
set_target_properties(recon_dump PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# 同上: recon_dump 也是独立 configure 面上的目标, 同样按目标存在性守卫。
if(TARGET astrocs_platform_math)
  target_link_libraries(recon_dump PRIVATE astrocs_platform_math)
endif()
