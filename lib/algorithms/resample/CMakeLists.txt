# AstroCS V6 Phase3 重采样传播模块（IMPL-P3-RSMP-001）
#
# 写域说明：根 CMakeLists.txt / tests/unit/CMakeLists.txt 非本任务写域（C-004.4）；
# 由控制器在 W5 后以独立集成提交 add_subdirectory(lib/algorithms/resample) 注册。
add_library(astrocs_p3_rsmp STATIC
  p3_rsmp_units.cpp
  p3_rsmp_kernel_registry.cpp
  p3_rsmp_operator.cpp
  p3_rsmp_covariance.cpp
  p3_rsmp_propagation.cpp
  p3_rsmp_failclosed.cpp
)
target_include_directories(astrocs_p3_rsmp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(astrocs_p3_rsmp PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)
