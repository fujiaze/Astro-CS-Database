# ============================================================================
# ACSD V6 科学产品 I/O 基础库 (IMPL-AIO-001, Wave 5)
# 写域: lib/infrastructure/aio/product_io/。不修改根/公共 CMakeLists (C-004.4)；
# 根构建面注册由控制器统一处理。
# ============================================================================
cmake_minimum_required(VERSION 3.16)

if(NOT AIO_PRODUCT_IO_THIRD_PARTY_DIR)
  set(AIO_PRODUCT_IO_THIRD_PARTY_DIR "../../../../lib/third_party")
endif()

if(NOT TARGET astrocs_product_io)
  add_library(astrocs_product_io STATIC
    src/sha256.cpp
    src/bunit.cpp
    src/fits.cpp
    src/atomic_publish.cpp
    src/provenance.cpp
    src/hips_manifest.cpp
    src/product_io.cpp)
  target_include_directories(astrocs_product_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${AIO_PRODUCT_IO_THIRD_PARTY_DIR})
  set_target_properties(astrocs_product_io PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
  # 平台系统库面: 引用根 CMakeLists 唯一判定点声明的共用件 astrocs_platform_math。
  # 本目录**可被独立 configure** (eng/tests/unit/aio 的变异 harness 用
  # `cmake -S eng/tests/unit/aio -B build` 把本目录当子目录拉进去); 那种情况下
  # 没有根入口 ⇒ 共用件未定义, 直接写 astrocs_platform_math 会被 CMake 当成
  # 普通库名展开成 -lastrocs_platform_math ⇒ ld: cannot find。
  # 故按「目标是否存在」守卫, 不按平台守卫 (平台判断仍只在共用件内部一处)。
  if(TARGET astrocs_platform_math)
    target_link_libraries(astrocs_product_io PUBLIC astrocs_platform_math)
  endif()
endif()
