# ============================================================================
# AstroCS V6 科学产品 I/O 基础库 (IMPL-AIO-001, Wave 5)
# 写域: lib/infrastructure/aio/v6/。不修改根/公共 CMakeLists (C-004.4)；
# 根构建面注册由控制器在 W5 后统一处理。
# ============================================================================
cmake_minimum_required(VERSION 3.16)

if(NOT AIO_V6_THIRD_PARTY_DIR)
  set(AIO_V6_THIRD_PARTY_DIR "../../../../lib/third_party")
endif()

if(NOT TARGET astrocs_v6_aio)
  add_library(astrocs_v6_aio STATIC
    src/v6_sha256.cpp
    src/v6_bunit.cpp
    src/v6_fits.cpp
    src/v6_atomic_publish.cpp
    src/v6_provenance.cpp
    src/v6_hips_manifest.cpp
    src/v6_product_io.cpp)
  target_include_directories(astrocs_v6_aio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${AIO_V6_THIRD_PARTY_DIR})
  set_target_properties(astrocs_v6_aio PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
  if(UNIX)
    target_link_libraries(astrocs_v6_aio PUBLIC m)
  endif()
endif()
