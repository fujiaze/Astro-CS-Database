# ============================================================================
# P2-INTEGRATE-001 集成测试注册（write_scope: eng/tests/integration/p2_integrate/）
# C-004.4：根/公共 CMakeLists 由 RUNTIME-CI-001/W9 统一注册；本文件自包含可
# 独立构建: cmake -S eng/tests/integration/p2_integrate -B build/p2_int && ctest --test-dir build/p2_int
# ============================================================================
cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(astrocs_p2_integrate LANGUAGES C CXX)
  enable_testing()
endif()

set(P2_IT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(P2_REPO "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
get_filename_component(P2_REPO "${P2_REPO}" ABSOLUTE)

add_subdirectory("${P2_REPO}/lib/algorithms/integration/phase2_integrate"
                 "${CMAKE_CURRENT_BINARY_DIR}/phase2_int_lib")

add_executable(p2_integrate_test "${P2_IT_DIR}/p2_integrate_test.cpp")
target_link_libraries(p2_integrate_test PRIVATE astrocs_phase2_integrate)
# 单一口径门引用 CLI 契约头（纯头文件）：--mode token 面必须对全部 token fail-closed。
target_include_directories(p2_integrate_test PRIVATE "${P2_REPO}/lib/infrastructure/cli")
set_target_properties(p2_integrate_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

set(P2_WORK "${CMAKE_CURRENT_BINARY_DIR}/work")
set(P2_PYTHON "python3" CACHE STRING "Python interpreter for the independent oracle")

add_test(NAME p2_singlepath COMMAND p2_integrate_test singlepath "${P2_WORK}")
add_test(NAME p2_write COMMAND p2_integrate_test write "${P2_WORK}")
add_test(NAME p2_negative COMMAND p2_integrate_test negative "${P2_WORK}")
add_test(NAME p2_halfproduct COMMAND p2_integrate_test halfproduct "${P2_WORK}")
set_tests_properties(p2_singlepath PROPERTIES TIMEOUT 300)
set_tests_properties(p2_write PROPERTIES TIMEOUT 900)
set_tests_properties(p2_negative PROPERTIES TIMEOUT 900)
set_tests_properties(p2_halfproduct PROPERTIES TIMEOUT 300)

add_test(NAME p2_oracle
  COMMAND ${P2_PYTHON} "${P2_IT_DIR}/oracle/p2_oracle.py"
          --workdir "${P2_WORK}" --repo "${P2_REPO}")
set_tests_properties(p2_oracle PROPERTIES DEPENDS "p2_write" TIMEOUT 600)

# M06 逐像素权重面判据（稀疏层绝对 SNR 的消费面 + 误乘帧级标量的反例 +
# 两条逐像素入口逐像素一致）。直接链生产库，不使用测试专用后门。
add_executable(p2_pixel_weight_test "${P2_IT_DIR}/p2_pixel_weight_test.cpp")
target_link_libraries(p2_pixel_weight_test PRIVATE astrocs_phase2_integrate)
set_target_properties(p2_pixel_weight_test PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME p2_pixel_weight COMMAND p2_pixel_weight_test)
set_tests_properties(p2_pixel_weight PROPERTIES TIMEOUT 300)

# M06 判据 (D)：module_adapters 的逐像素面必须调用**同一**生产 API（单一实现），
# 且稀疏层语义判别式必须随层传入（不写「消费时不得乘/除帧级 SNR」的旁路实现）。
add_test(NAME p2_pixel_weight_wiring
  COMMAND ${P2_PYTHON} "${P2_IT_DIR}/oracle/p2_pixel_weight_wiring.py"
          --repo "${P2_REPO}")
set_tests_properties(p2_pixel_weight_wiring PROPERTIES TIMEOUT 120)
