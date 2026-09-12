# ============================================================================
# SCI-F3-001 — lib/astro_image_io/src/hips NREJ/NUSED + §30.3 provenance 通道
# 验收测试 (TEST-P2HIPS-UNC-PROV-001)
# ----------------------------------------------------------------------------
# 合同锚: docs/contracts/DATA_SEMANTICS.md §30.2 (DATA-P2-REJ-001) /
#         §30.3 (DATA-P2-PROV-001), DATA-UNC-001 FROZEN。
# 被测面: astrocs_hips (根 CMakeLists.txt, aio_hips_writer.cpp +
#         aio_hips_reader.cpp C ABI 导出面)。
# 结构对齐 P1-HIPS-TEST 先例 lib/astro_image_io/tests/p1hips/CMakeLists.txt。
#
# CTest 注册 (单跑: ctest -R p2hips_):
#   p2hips_units        — §30.2 nrej/nused int32 通道(目录/BITPIX=32/512²/
#                         payload 字节) + 独立直方图 oracle(写值恰现/无负值)
#                         + §30.3 五键 properties↔manifest 双写真实值
#                         + verify 双向断言(available rc==0 / unavailable
#                           显式登记且禁 variance/ivar 占位)
#   p2hips_determinism  — 两跑 nrej/nused tile 逐字节 + 五键值面逐字节
#   p2hips_negative     — 参数域负例(未定义 flags/未启用通道/指针 NULL/
#                         负值哨兵) + §30.3 全或无(非法参数 ⇒ 五键整体不落盘)
#                         + available 缺子产品位 finalize fail-closed
#                         + verify 篡改判别(声明无落盘 / 伪造占位 / 值分叉)
#   p2hips_selfcheck    — 故障注入必败自检: 必须在注入模式下运行且 PASS,
#                         同时基线(无注入)必 FAIL ⇒ 双向排除恒常断言
#
# 故障注入 (等价缺陷注入; 未设置时逐行零行为差异):
#   ASTROCS_HIPS_PROV_FAULT=missing_key|value_drift
#   ASTROCS_HIPS_DIAG_FAULT=sentinel|skip_write
# 判别力实测 (baseline GREEN, 注入 RED; 失败面与注入点一一对应):
#   missing_key  → units_keys/units_verify 红, units_diag 绿 (prov 面专属)
#   value_drift  → units_verify V6 红
#   sentinel     → units_diag/units_verify 红, units_keys 绿 (diag 面专属)
#   skip_write   → units/units_keys/units_verify 全红 (声明无落盘)
# ============================================================================

set(P2HIPS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(P2HIPS_INC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

add_executable(p2hips_tests ${P2HIPS_DIR}/p2hips_unc_prov_test.cpp)
target_include_directories(p2hips_tests PRIVATE
  ${P2HIPS_INC}
  ${P2HIPS_DIR})
target_link_libraries(p2hips_tests PRIVATE astrocs_hips ${ASTROCS_OMP_TARGET})

add_test(NAME p2hips_units COMMAND p2hips_tests units)
add_test(NAME p2hips_determinism COMMAND p2hips_tests determinism)
add_test(NAME p2hips_negative COMMAND p2hips_tests negative)

# 故障注入必败自检 (验收: 注入下必 PASS 且能观测到等价缺陷; 基线必 FAIL)
add_test(NAME p2hips_selfcheck COMMAND p2hips_tests selfcheck)
set_tests_properties(p2hips_selfcheck PROPERTIES ENVIRONMENT
  "ASTROCS_HIPS_PROV_FAULT=missing_key")
