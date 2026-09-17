# ----------------------------------------------------------------------------
# AIO 顶层测试注册（RELEASE-01 / TST-001 欠账修复）
#
# 背景: lib/infrastructure/aio/tests/ 顶层原有 20 个测试源文件**零 CMake 注册**
# （该目录此前无 CMakeLists.txt；仅 p1hips/p2hips/abi 三个子目录被注册），
# 即这些测试从未被编译、从未在 ctest 中运行 —— 属 TST-001 登记的「测试零注册」缺陷。
#
# 本文件只注册**已逐条独立复跑通过**的用例（前台 ad hoc 构建 + 运行，证据见
# reports/RELEASE-01/tests/TST-001-report.md 与 工程控制/RELEASE-01/GAP_AUDIT.md §7.9）：
#   test_checksum        49 PASS / 0 FAIL（含 CRC32C Castagnoli 标准向量）
#   test_tile_model      67 PASS / 0 FAIL
#   test_transform       75 PASS / 0 FAIL / 2 SKIP（可选依赖缺失）
#   test_query_pixel     ALL PASS
#   test_precision_dual  ALL PASS
# 未注册者（构建或断言未通过，另行登记，不混入绿门）：
#   hiss_writer_smoke     断言失败「MAGIC 不匹配」
#   hiss_correctness_test 缺 astro_calibration.h（跨模块依赖未接线）
#   pipeline_frame_contract_test  需 aio_pipeline_frame_* 符号
# 被测面: astrocs_hips（含 hiss_codec/reader/writer/tile_model/transform/stream_writer/upm）。
# ----------------------------------------------------------------------------

set(AIO_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

foreach(_t checksum tile_model transform query_pixel precision_dual)
  add_executable(aio_${_t} ${AIO_TESTS_DIR}/test_${_t}.cpp)
  target_link_libraries(aio_${_t} PRIVATE astrocs_hips)
  add_test(NAME aio_${_t} COMMAND aio_${_t})
endforeach()