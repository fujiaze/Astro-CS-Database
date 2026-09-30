# ----------------------------------------------------------------------------
# AIO 顶层测试注册
#
# 背景: lib/infrastructure/aio/tests/ 顶层原有 20 个测试源文件**零 CMake 注册**
# （该目录此前无 CMakeLists.txt；仅 p1hips/p2hips/abi 三个子目录被注册），
# 即这些测试从未被编译、从未在 ctest 中运行 —— 属 TST-001 登记的「测试零注册」缺陷。
#
# 本文件只注册**已逐条独立复跑通过**的用例（前台 ad hoc 构建 + 运行复跑记录）：
#   test_checksum        49 PASS / 0 FAIL（含 CRC32C Castagnoli 标准向量）
#   test_tile_model      67 PASS / 0 FAIL
#   test_transform       75 PASS / 0 FAIL / 2 SKIP（可选依赖缺失）
#   test_query_pixel     ALL PASS
#   test_precision_dual  ALL PASS
# 未注册者（构建或断言未通过，另行登记，不混入绿门）：
#   hiss_writer_smoke     断言失败「MAGIC 不匹配」
#   hiss_correctness_test 缺 astro_calibration.h（跨模块依赖未接线）
#   pipeline_frame_contract_test  需 aio_pipeline_frame_* 符号
# 被测面: acsd_hips（含 hiss_codec/reader/writer/tile_model/transform/stream_writer/upm）。
# ----------------------------------------------------------------------------

set(AIO_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

foreach(_t checksum tile_model transform query_pixel precision_dual)
  add_executable(aio_${_t} ${AIO_TESTS_DIR}/test_${_t}.cpp)
  target_link_libraries(aio_${_t} PRIVATE acsd_hips)
  add_test(NAME aio_${_t} COMMAND aio_${_t})
endforeach()

# ----------------------------------------------------------------------------
# 原子发布验证面（docs/ACSD_DESIGN §10「I/O 与原子产品」）:
# HiPS tile 原子发布 + 完成清单 fail-closed 验证面。
#   被测面: lib/infrastructure/aio/src/hips/aio_hips_writer.cpp（每 tile 私有临时
#   文件 → DATASUM/CHECKSUM 哈希校验 → fsync → 原子 rename; 全部 tile 完成后最后
#   落完成清单）+ aio_hips_reader.cpp（无清单 ⇒ 消费者拒绝）。
#   用例: 正常发布 / 无清单拒绝（能绿能红）/ 中途 SIGKILL 无半成品且重跑不消费残留 /
#   磁盘满等五个注入必败且正式目录零 .fits / 截断 tile 判别力红锚。
# ----------------------------------------------------------------------------
# WIN-PORT (FINAL-07 可移植性): 本用例的核心锚是"写入进程中途被 SIGKILL ⇒ 正式目录零半成品、
# 无私有临时文件残留、重跑不消费残留" —— 需要 fork 子进程 + kill(pid,SIGKILL) 的进程级强杀，
# 且被强杀的写盘体是本进程内的 C++ 代码（无法跨进程度传递）。Windows 无 fork/SIGKILL 等价
# 原语（TerminateProcess 只能作用在自我 spawn 的独立进程上，需把写盘体拆成独立入口），
# ⇒ 等价语义不可表达。整块按 platform 口径显式限定到 UNIX；目标与 add_test 仍留在 if(UNIX)
# 文本内（eng/ci/checks.json CHK-UNIT 的 ctest_targets 登记与静态扫描仍可见，计数口径不变）。
if(UNIX)
add_executable(aio_hips_atomic_publish
  ${AIO_TESTS_DIR}/test_hips_atomic_publish.cpp)
target_link_libraries(aio_hips_atomic_publish PRIVATE acsd_hips)
add_test(NAME aio_hips_atomic_publish COMMAND aio_hips_atomic_publish)
else()
message(STATUS "AIO(aio_hips_atomic_publish): 原子发布 + 中途 SIGKILL 崩溃一致性用例依赖 POSIX fork/kill ⇒ 本平台 ${CMAKE_SYSTEM_NAME} 显式跳过注册（FINAL-07 WIN-PORT）")
endif()