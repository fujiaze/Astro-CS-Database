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

# ============================================================================
# P4-magiter — 极限星等割线迭代回归锁 (test/test_mag_iter.cpp)
# ----------------------------------------------------------------------------
# 被测函数 estimate_mag_lim_iterative 在 src/ipv_select.cpp (生产源, 零改动直编);
# 直接链接生产静态库 astrocs_p1_ipv (含 gaia_client.c), 不新建生产 target。
# 全部用例使用固定 stub 查询序列 —— 不依赖网络 / Gaia 数据 / 真实图像。
#   收敛性 / 确定性(逐位+金标准) / 触顶防御 / N=0 步长 / 查询次数上界 /
#   参数可覆盖(宪章 §10.4 禁硬编码) / 异常路径。
# ============================================================================
add_executable(ipv_mag_iter_test
  ${P1IPVTEST_DIR}/test_mag_iter.cpp)
target_include_directories(ipv_mag_iter_test PRIVATE
  ${P1IPV_DIR}/include)
target_link_libraries(ipv_mag_iter_test PRIVATE
  astrocs_p1_ipv astrocs_p1_sdet astrocs_aio astrocs_cfitsio)
add_test(NAME ipv_mag_iter COMMAND ipv_mag_iter_test)
