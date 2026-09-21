# ============================================================================
# IMPL-P1-PSFW-001 — Phase1 W_info / PSFSW 共址单元测试
# ----------------------------------------------------------------------------
# write_scope: eng/tests/unit/v6_p1_psfw/
# 被测生产源 (独立编入测试目标, 不新建生产 target; 对齐 p1psf/p1star 先例):
#   lib/algorithms/psf/src/psf_information.cpp          (A_NEA / effective PSF)
#   lib/algorithms/noise_snr/cpp/src/information_weight.cpp (W_info/Q/F_hat/白噪条件)
#   lib/algorithms/photometry/cpp/src/psfsw.cpp          (四分量/共同星集/门/cov)
# 根构建面注册由控制器统一处理 (C-004.4)。
#
# 单跑: ./v6_p1_psfw_tests <group>|all
# CTest: v6_p1_psfw_{anea,winfo,oracle,components,common,gates,record,negative}
#        v6_p1_psfw_selfcheck (故障注入相必须 rc!=0, WILL_FAIL 断言非恒真)
# ============================================================================
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(astrocs_v6_p1_psfw LANGUAGES CXX)
  set(ASTROCS_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
  enable_testing()
else()
  set(ASTROCS_REPO_ROOT "${CMAKE_SOURCE_DIR}")
endif()

set(V6_P1_PSFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(V6_P1_PSFW_PROD
  "${ASTROCS_REPO_ROOT}/lib/algorithms/psf/src/psf_information.cpp"
  "${ASTROCS_REPO_ROOT}/lib/algorithms/noise_snr/cpp/src/information_weight.cpp"
  "${ASTROCS_REPO_ROOT}/lib/algorithms/photometry/cpp/src/psfsw.cpp")

add_executable(v6_p1_psfw_tests
  ${V6_P1_PSFW_DIR}/tests_main.cpp
  ${V6_P1_PSFW_DIR}/p1psfw_tests_anea.cpp
  ${V6_P1_PSFW_DIR}/p1psfw_tests_winfo.cpp
  ${V6_P1_PSFW_DIR}/p1psfw_tests_oracle.cpp
  ${V6_P1_PSFW_DIR}/p1psfw_tests_psfsw.cpp
  ${V6_P1_PSFW_DIR}/p1psfw_tests_gates.cpp
  ${V6_P1_PSFW_DIR}/p1psfw_tests_record.cpp
  ${V6_P1_PSFW_DIR}/p1psfw_tests_negative.cpp
  ${V6_P1_PSFW_PROD})
target_include_directories(v6_p1_psfw_tests PRIVATE
  ${V6_P1_PSFW_DIR}
  "${ASTROCS_REPO_ROOT}/lib/algorithms/psf/include"
  "${ASTROCS_REPO_ROOT}/lib/algorithms/noise_snr/include"
  "${ASTROCS_REPO_ROOT}/lib/algorithms/photometry/include")
target_compile_features(v6_p1_psfw_tests PRIVATE cxx_std_17)
if(UNIX)
  target_link_libraries(v6_p1_psfw_tests PRIVATE m)
endif()

set(V6_P1_PSFW_GROUPS anea winfo oracle components common gates record negative)
foreach(g ${V6_P1_PSFW_GROUPS})
  add_test(NAME v6_p1_psfw_${g} COMMAND v6_p1_psfw_tests ${g})
  set_tests_properties(v6_p1_psfw_${g} PROPERTIES LABELS "v6_p1_psfw" TIMEOUT 600)
endforeach()

# 故障注入自检: 注入 oracle_mc_var 后该断言确定性翻转 -> 必败 (WILL_FAIL)
add_test(NAME v6_p1_psfw_selfcheck
  COMMAND ${CMAKE_COMMAND} -E env ASTROCS_P1PSFW_FAULT=oracle_mc_var,winfo_wn,neg_w1
          $<TARGET_FILE:v6_p1_psfw_tests> all)
set_tests_properties(v6_p1_psfw_selfcheck PROPERTIES
  LABELS "v6_p1_psfw" TIMEOUT 600 WILL_FAIL TRUE)
