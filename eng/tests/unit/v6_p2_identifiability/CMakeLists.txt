# UPM-KAPPA-UNIFY-01：唯一「可辨识性/病态」判据的红/绿双向回归锁
add_executable(v6_p2_identifiability_test v6_p2_identifiability_test.cpp)
target_include_directories(v6_p2_identifiability_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/algorithms/coverage/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib/algorithms/shared)
target_link_libraries(v6_p2_identifiability_test PRIVATE astrocs_phase2)
if(UNIX)
  target_link_libraries(v6_p2_identifiability_test PRIVATE m)
endif()
add_test(NAME v6_p2_identifiability COMMAND v6_p2_identifiability_test)
set_tests_properties(v6_p2_identifiability PROPERTIES LABELS "v6_p2;sci502;identifiability")
