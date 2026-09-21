# FIX-A 天光面链 — Phase2 稀疏天光面共址测试
# 被测面 = astrocs_phase2（含 lib/algorithms/coverage/src/sky_plane.cpp）。
# 未编入 astrocs_phase2 时链接失败 => 构建即红灯。
add_executable(v6_p2_sky_test v6_p2_sky_test.cpp)
target_include_directories(v6_p2_sky_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/coverage/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/shared)
target_link_libraries(v6_p2_sky_test PRIVATE astrocs_phase2)
# 运行产物显式落 run/（不依赖 cwd 是仓库根）
target_compile_definitions(v6_p2_sky_test PRIVATE
  SKY_TEST_OUTDIR="${CMAKE_SOURCE_DIR}/run/RELEASE-02/FIX-A")
if(UNIX)
  target_link_libraries(v6_p2_sky_test PRIVATE m)
endif()

foreach(case patch starmask recovery gauge weighting negative memory determinism seam gain dcgain overflow)
  add_test(NAME v6_p2_sky_${case} COMMAND v6_p2_sky_test ${case})
  set_tests_properties(v6_p2_sky_${case} PROPERTIES LABELS "v6_p2_sky")
endforeach()
add_test(NAME v6_p2_sky_all COMMAND v6_p2_sky_test all)
set_tests_properties(v6_p2_sky_all PROPERTIES LABELS "v6_p2_sky")
