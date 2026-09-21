# eng/tests/unit/v6_p3_proj/CMakeLists.txt — IMPL-P3-PROJ-001 共址测试注册
#
# 只注册本任务 write_scope 内目标；根/公共 CMakeLists 注册由控制器统一处理
# （控制包 CONTROLLER_LOG C-004.4；CTRL-AR033）。本文件可被
# add_subdirectory(eng/tests/unit/v6_p3_proj) 或独立 cmake -S 直接使用。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.16)
  project(v6_p3_proj_tests CXX)
  enable_testing()
endif()

set(V6_P3PROJ_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/projection)

add_executable(v6_p3_proj_test
  v6_p3_proj_test.cpp
  ${V6_P3PROJ_DIR}/p3_proj_v6.cpp)
target_include_directories(v6_p3_proj_test PRIVATE ${V6_P3PROJ_DIR})
target_compile_features(v6_p3_proj_test PRIVATE cxx_std_17)
add_test(NAME v6_p3_proj_units COMMAND v6_p3_proj_test)

add_executable(v6_p3_proj_probe
  v6_p3_proj_probe.cpp
  ${V6_P3PROJ_DIR}/p3_proj_v6.cpp)
target_include_directories(v6_p3_proj_probe PRIVATE ${V6_P3PROJ_DIR})
target_compile_features(v6_p3_proj_probe PRIVATE cxx_std_17)

# 负向门: 故障注入等价缺陷必败（测试级注入，生产源零 getenv）
foreach(_m const_omega legacy_car legacy_ait swap_norm naive_wrap)
  add_test(NAME v6_p3_proj_fault_${_m} COMMAND v6_p3_proj_test)
  set_tests_properties(v6_p3_proj_fault_${_m} PROPERTIES
    ENVIRONMENT "ASTROCS_P3PROJ_V6_FAULT=${_m}")
endforeach()

find_program(PYTHON3_EXECUTABLE python3)
if(PYTHON3_EXECUTABLE)
  # 独立 WCS Oracle: astropy/WCSLIB 对拍（正反投影 + 逐像素 Ω + wrap + R/S）
  add_test(NAME v6_p3_proj_wcs_oracle
    COMMAND ${PYTHON3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/p3_proj_wcs_oracle.py
            run --probe $<TARGET_FILE:v6_p3_proj_probe>
            --json ${CMAKE_CURRENT_BINARY_DIR}/oracle_results.json)

  # 偏差证据: legacy registry v1 CAR/AIT vs 标准 astropy（finding 证据）
  add_executable(v6_p3_proj_legacy_probe
    v6_p3_proj_legacy_probe.cpp
    ${V6_P3PROJ_DIR}/p3_projection.cpp)
  target_include_directories(v6_p3_proj_legacy_probe PRIVATE ${V6_P3PROJ_DIR})
  target_compile_features(v6_p3_proj_legacy_probe PRIVATE cxx_std_17)
  add_test(NAME v6_p3_proj_legacy_deviation
    COMMAND ${PYTHON3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/p3_proj_legacy_deviation.py
            $<TARGET_FILE:v6_p3_proj_legacy_probe>)
endif()
