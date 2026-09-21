# SCI-502 FIX-3 专项：天光面 κ 可观测 + 粗糙度自适应（红/绿双向）
add_executable(v6_p2_sky_kappa_test v6_p2_sky_kappa_test.cpp)
target_include_directories(v6_p2_sky_kappa_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/algorithms/coverage/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/algorithms/shared)
target_link_libraries(v6_p2_sky_kappa_test PRIVATE astrocs_phase2)
if(UNIX)
  target_link_libraries(v6_p2_sky_kappa_test PRIVATE m)
endif()
add_test(NAME v6_p2_sky_kappa COMMAND v6_p2_sky_kappa_test)
set_tests_properties(v6_p2_sky_kappa PROPERTIES LABELS "v6_p2_sky;sci502")
