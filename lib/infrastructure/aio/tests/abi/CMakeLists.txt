# ============================================================================
# AIO HiPS 跨边界结构 ABI 布局锁 (V11-N-01; ASTROCS_DESIGN §7.3)
# ----------------------------------------------------------------------------
#   A) aio_abi_layout_probe (只 include aio_hips.h, 无 windows.h/不链接生产库)
#      输出 4 个跨边界结构的 sizeof/alignof/逐字段 offsetof JSON;
#   B) aio_abi_layout_lock 与仓内唯一权威镜像
#      lib/infrastructure/aio/tools/aio_abi_mirror.py 逐字段比对: 字段名/顺序/
#      offset/size、sizeof、alignof、ABI 版本常量、*_STRUCT_SIZE 任一不一致
#      => ctest FAIL。aio_abi_layout_lock_selfcheck 用被破坏镜像 (字段重排 +
#      漏字段 + 常量污染) 反向证明该门非恒真。
#   运行时 fail-closed 面另由 p1hips negative 组 (N10) 覆盖: 无 ABI 头/错版本的
#   调用方在公共入口被拒 (-9) 且 last_error 点名 ABI。
# ============================================================================
set(AIO_ABI_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(aio_abi_layout_probe ${AIO_ABI_TEST_DIR}/aio_abi_layout_probe.cpp)
target_include_directories(aio_abi_layout_probe PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

add_test(NAME aio_abi_layout_lock
  COMMAND ${CMAKE_COMMAND} -E env
          AIO_LAYOUT_PROBE=$<TARGET_FILE:aio_abi_layout_probe>
          python3 ${AIO_ABI_TEST_DIR}/aio_abi_layout_lock.py
          --probe $<TARGET_FILE:aio_abi_layout_probe>)
set_tests_properties(aio_abi_layout_lock PROPERTIES TIMEOUT 120)

add_test(NAME aio_abi_layout_lock_selfcheck
  COMMAND ${CMAKE_COMMAND} -E env
          AIO_LAYOUT_PROBE=$<TARGET_FILE:aio_abi_layout_probe>
          python3 ${AIO_ABI_TEST_DIR}/aio_abi_layout_lock.py
          --probe $<TARGET_FILE:aio_abi_layout_probe> --selfcheck)
set_tests_properties(aio_abi_layout_lock_selfcheck PROPERTIES TIMEOUT 120)
