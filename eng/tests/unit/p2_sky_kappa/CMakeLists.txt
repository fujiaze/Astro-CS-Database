# SCI-502 FIX-3 专项：天光面 κ 可观测 + 粗糙度自适应（红/绿双向）
add_executable(p2_sky_kappa_test p2_sky_kappa_test.cpp)
target_include_directories(p2_sky_kappa_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/algorithms/coverage/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/algorithms/shared)
target_link_libraries(p2_sky_kappa_test PRIVATE astrocs_phase2)
target_link_libraries(p2_sky_kappa_test PRIVATE astrocs_platform_math)
add_test(NAME p2_sky_kappa COMMAND p2_sky_kappa_test)
set_tests_properties(p2_sky_kappa PROPERTIES LABELS "p2_sky;sci502")
