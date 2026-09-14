# ============================================================================
# IPV-FAILCLOSED-TEST — lib/plate_solve/cpp/ipv 共址回归锁
# ----------------------------------------------------------------------------
# B4-1 (M1a-C-002): extract_wcs_sip TRANS 线性奇异必须显式失败
#   (SCI-WCS-001 §8 / ALG-WCS-001 §11.1; 宪章 §6.3 禁静默降级 / §14.4 fail-fast)。
# 断言面直接是 extract_wcs_sip 本体 (不是 iter_trans_solve 等邻接函数)。
#
# 被测生产源: src/ipv_wcs.cpp + src/ipv_sip.cpp (零改动直编, 与 ipv Makefile
# SRCS 同名同选项; 不新建生产 target — 对齐 p1star/p1psf 先例)。
#
# 注册方式: 本文件位于模块自带测试目录, 由 tests/unit/CMakeLists.txt 一条
#   add_subdirectory(../../lib/plate_solve/cpp/ipv/test p1ipvtest)
# 挂入。该钩子属 B2 写域 (tests/unit/CMakeLists.txt), 见 REPORT "越域残余"。
# ============================================================================

set(P1IPVTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(P1IPV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(ipv_extract_wcs_sip_failclosed_test
  ${P1IPVTEST_DIR}/test_extract_wcs_sip_failclosed.cpp
  ${P1IPV_DIR}/src/ipv_wcs.cpp
  ${P1IPV_DIR}/src/ipv_sip.cpp)
target_include_directories(ipv_extract_wcs_sip_failclosed_test PRIVATE
  ${P1IPV_DIR}/include)
add_test(NAME ipv_extract_wcs_sip_failclosed
         COMMAND ipv_extract_wcs_sip_failclosed_test)
