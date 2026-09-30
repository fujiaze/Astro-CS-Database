# ============================================================================
# IMPL-AIO-001 共址单元测试 (eng/tests/unit/aio) — 独立可构建的 ctest 面
# 可独立: cmake -S eng/tests/unit/aio -B build/aio && ctest --test-dir build/aio
# 也可被根构建 add_subdirectory 复用 (不重复 project/enable_testing)。
# AIO_PRODUCT_IO_LIB_DIR 可覆盖被测源码根 (负向 mutation harness 使用)。
# ============================================================================
cmake_minimum_required(VERSION 3.16)

set(AIO_PRODUCT_IO_STANDALONE FALSE)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(AIO_PRODUCT_IO_STANDALONE TRUE)
  project(acsd_product_io_tests CXX)
endif()

if(NOT AIO_PRODUCT_IO_LIB_DIR)
  set(AIO_PRODUCT_IO_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/infrastructure/aio/product_io")
endif()
get_filename_component(AIO_PRODUCT_IO_LIB_DIR "${AIO_PRODUCT_IO_LIB_DIR}" ABSOLUTE)
get_filename_component(AIO_PRODUCT_IO_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)

add_subdirectory("${AIO_PRODUCT_IO_LIB_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/product_io_lib")

add_executable(aio_test aio_test.cpp)
target_link_libraries(aio_test PRIVATE acsd_product_io)
# 平台系统库面: 引用根 CMakeLists 唯一判定点声明的共用件 acsd_platform_math。
# 独立 configure 本文件 (AIO_PRODUCT_IO_STANDALONE, 例如 mutations/*.py 的变异
# harness 用 `cmake -S eng/tests/unit/aio -B build` 重编) 时根入口不存在 ⇒
# 共用件未定义, 此时若仍写 acsd_platform_math, CMake 会把它当**普通库名**
# 展开成 -lacsd_platform_math ⇒ 链接期 ld: cannot find -lacsd_platform_math。
# 故按「目标是否存在」守卫, 不按平台守卫 —— 平台判断仍只在共用件内部一处。
# 与 eng/cmake/cfitsio_platform.cmake 同族口径 (IF(TARGET acsd_platform_math))。
if(TARGET acsd_platform_math)
  target_link_libraries(aio_test PRIVATE acsd_platform_math)
endif()

if(AIO_PRODUCT_IO_STANDALONE)
  enable_testing()
endif()

set(AIO_PRODUCT_IO_ARTIFACT_DIR "${CMAKE_CURRENT_BINARY_DIR}/artifacts")
set(AIO_PRODUCT_IO_PYTHON "python3")

add_test(NAME aio_units COMMAND aio_test units)
add_test(NAME aio_provenance COMMAND aio_test provenance)
add_test(NAME aio_manifest COMMAND aio_test manifest)
add_test(NAME aio_fits COMMAND aio_test fits "${CMAKE_CURRENT_BINARY_DIR}/fits_work")
add_test(NAME aio_atomic COMMAND aio_test atomic "${CMAKE_CURRENT_BINARY_DIR}/atomic_work")

add_test(NAME aio_artifacts COMMAND aio_test artifacts "${AIO_PRODUCT_IO_ARTIFACT_DIR}")
set_tests_properties(aio_artifacts PROPERTIES FIXTURES_SETUP aio_artifacts)

add_test(NAME aio_oracle
  COMMAND ${AIO_PRODUCT_IO_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/oracle/aio_oracle.py"
          "${AIO_PRODUCT_IO_ARTIFACT_DIR}" "${AIO_PRODUCT_IO_REPO_ROOT}")
set_tests_properties(aio_oracle PROPERTIES FIXTURES_REQUIRED aio_artifacts)

add_test(NAME aio_oracle_negative
  COMMAND ${AIO_PRODUCT_IO_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/oracle/aio_oracle_negative.py"
          "${AIO_PRODUCT_IO_ARTIFACT_DIR}" "${AIO_PRODUCT_IO_REPO_ROOT}")
set_tests_properties(aio_oracle_negative PROPERTIES FIXTURES_REQUIRED aio_artifacts)

add_test(NAME aio_impl_mutations
  COMMAND ${AIO_PRODUCT_IO_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/mutations/mutate_and_check.py"
          "${AIO_PRODUCT_IO_REPO_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/mutations_work")
set_tests_properties(aio_impl_mutations PROPERTIES TIMEOUT 2400 LABELS "aio_slow")

# ── P-174 发布终态第三态（已发布但持久化未确认）: 可执行正/负例 ─────────────
# 正例: 无 preload 正常发布 -> status=OK / renamed / durability=DURABLE /
#       目标可见 / 无 tmp 残留;
# 负例: LD_PRELOAD 注入器只让 rename **之后**落在目录 fd 上的 fsync 失败
#       -> status=ERR_IO + renamed + durability=NOT_DURABLE +
#          **已发布对象仍在**（第三态存在证据，不得回滚删除）。
# 权威依据: docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md §4/§6;
#           docs/detail/infrastructure/17_aio.md §4（三态判据）。
if(UNIX)
  # 第三态负例依赖 LD_PRELOAD 注入目录 fd 的 fsync 失败 => POSIX-only
  # （Windows 侧第三态恒成立但不可观测，见 17_aio.md §4）。
  add_executable(aio_atomic_durability_probe atomic_durability_probe.cpp)
  target_link_libraries(aio_atomic_durability_probe PRIVATE acsd_product_io)

  add_library(aio_fsync_dir_fail_interposer SHARED fsync_dir_fail_interposer.cpp)
  target_link_libraries(aio_fsync_dir_fail_interposer PRIVATE ${CMAKE_DL_LIBS})

  add_test(NAME aio_atomic_durability
    COMMAND ${AIO_PRODUCT_IO_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/check_atomic_durability.py"
            "$<TARGET_FILE:aio_atomic_durability_probe>"
            "$<TARGET_FILE:aio_fsync_dir_fail_interposer>"
            "${CMAKE_CURRENT_BINARY_DIR}/durability_work")

  # 变异判红：删掉 kNotDurable 赋值（回到旧口径）后同一套断言必须 FAIL。
  add_test(NAME aio_atomic_durability_mutation
    COMMAND ${AIO_PRODUCT_IO_PYTHON}
            "${CMAKE_CURRENT_SOURCE_DIR}/mutations/durability_mutation_and_check.py"
            "${AIO_PRODUCT_IO_REPO_ROOT}"
            "${CMAKE_CURRENT_BINARY_DIR}/durability_mutation_work")
  set_tests_properties(aio_atomic_durability_mutation PROPERTIES TIMEOUT 1200)

  # ── 裁决 ④ 两序区分用例（机制层检测面更强）────────────────────────────────
  # 命题: 机制层序 = tmp 写 -> fsync -> **rename** -> 重开已发布对象验证 -> 失败撤销;
  #       它能检出"rename 成功之后目标才被改坏"（注入器在 rename 返回后追加坏字节），
  #       而 IO_003 §4 的生产者序（校验先于 rename）在该窗口无观测点。
  # 判据: 干净基线必须绿（证明验证函数不恒真）; 注入基线必须"检出 + 撤销"
  #       （status!=0 / renamed=0 / target_exists=0），且注入事件必须真的出现
  #       （否则 runner exit 2：注入未发生 = 零证据）。
  # 权威依据: docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md §4.1（两序正本）。
  add_executable(aio_verify_order_probe verify_order_probe.cpp)
  target_link_libraries(aio_verify_order_probe PRIVATE acsd_product_io)

  add_library(aio_corrupt_after_rename_interposer SHARED corrupt_after_rename_interposer.cpp)
  target_link_libraries(aio_corrupt_after_rename_interposer PRIVATE ${CMAKE_DL_LIBS})

  add_test(NAME aio_verify_order
    COMMAND ${AIO_PRODUCT_IO_PYTHON} "${CMAKE_CURRENT_SOURCE_DIR}/check_verify_order.py"
            "$<TARGET_FILE:aio_verify_order_probe>"
            "$<TARGET_FILE:aio_corrupt_after_rename_interposer>"
            "${CMAKE_CURRENT_BINARY_DIR}/verify_order_work")
  set_tests_properties(aio_verify_order PROPERTIES TIMEOUT 600)

  # ── P-173 并发取号唯一性（next_seq）: 正例面 ────────────────────────────
  # 台账 P-173「next_seq 非原子 static++」; 权威 = docs/detail/infrastructure/17_aio.md:26
  # （临时文件 + 校验 + fsync + 原子 rename）与 aio_atomic_file.h（<final>.tmp.<pid>.<seq>）。
  # 本单元只证**唯一性**（弱证据）; 竞态属性本身由 TSan 负例判红
  # （eng/tests/unit/aio/mutations/tmp_path_tsan_negative.py，普通构建无判别力）。
  add_executable(aio_tmp_path_uniqueness_probe tmp_path_uniqueness_probe.cpp)
  target_include_directories(aio_tmp_path_uniqueness_probe PRIVATE
    "${AIO_PRODUCT_IO_REPO_ROOT}/lib/infrastructure/aio/src")
  add_test(NAME aio_tmp_path_uniqueness
    COMMAND aio_tmp_path_uniqueness_probe 8 20000)
  set_tests_properties(aio_tmp_path_uniqueness PROPERTIES TIMEOUT 300)
endif()
