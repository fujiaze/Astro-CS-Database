# ============================================================================
# eng/tests/unit/v6_p1_cal/CMakeLists.txt
# IMPL-P1-CAL-001 — Phase1 校准 covariance 共址单元测试 + 专用静态库
# ----------------------------------------------------------------------------
# write_scope 自包含: 本文件只注册本任务目标，不改根/公共 CMakeLists（C-004.4）。
# 库目标 astrocs_p1_cal_v6_covariance 由未来 P1-INTEGRATE-001/RUNTIME-CI-001
# 在根构建面按需链接；测试目标直接编译被测生产源，不依赖 legacy
# astrocs_p1_calibration DLL（不接线 session）。
# 单跑: ctest -R v6_p1_cal_covariance
# ============================================================================

set(V6_CAL_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(V6_CAL_PROD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/calibration)

if(NOT TARGET astrocs_p1_cal_v6_covariance)
  add_library(astrocs_p1_cal_v6_covariance STATIC
    ${V6_CAL_PROD_DIR}/src/v6_calibration_covariance.cpp)
  target_include_directories(astrocs_p1_cal_v6_covariance PUBLIC
    ${V6_CAL_PROD_DIR}/include)
  set_target_properties(astrocs_p1_cal_v6_covariance PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON)
endif()

add_executable(v6_p1_cal_covariance_test
  ${V6_CAL_TEST_DIR}/v6_cal_covariance_test.cpp)
target_include_directories(v6_p1_cal_covariance_test PRIVATE
  ${V6_CAL_TEST_DIR}
  ${V6_CAL_PROD_DIR}/include)
target_link_libraries(v6_p1_cal_covariance_test PRIVATE
  astrocs_p1_cal_v6_covariance)
set_target_properties(v6_p1_cal_covariance_test PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)

add_test(NAME v6_p1_cal_covariance COMMAND v6_p1_cal_covariance_test)
set_tests_properties(v6_p1_cal_covariance PROPERTIES TIMEOUT 300)
