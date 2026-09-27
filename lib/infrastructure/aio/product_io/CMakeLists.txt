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
  if(UNIX)
    target_link_libraries(astrocs_product_io PUBLIC m)
  endif()
endif()
