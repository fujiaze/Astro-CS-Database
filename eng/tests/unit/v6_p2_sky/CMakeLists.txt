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

# ---------------------------------------------------------------------------
# UPM-NODE-ADAPT-01：节点间距「由输入自适应导出」+「进入自适应重试回路」
# 依据 docs/science/PHASE2_UPM.md §7a（规则 1-5）。正例 + 负例（几何缺失必须显式失败）。
# ---------------------------------------------------------------------------
add_executable(v6_p2_sky_node_adapt_test v6_p2_sky_node_adapt_test.cpp)
target_include_directories(v6_p2_sky_node_adapt_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/coverage/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib//algorithms/shared)
target_link_libraries(v6_p2_sky_node_adapt_test PRIVATE astrocs_phase2)
target_compile_definitions(v6_p2_sky_node_adapt_test PRIVATE
  SKY_TEST_OUTDIR="${CMAKE_SOURCE_DIR}/run/UPM-NODE-ADAPT-01/evidence")
if(UNIX)
  target_link_libraries(v6_p2_sky_node_adapt_test PRIVATE m)
endif()

foreach(case derive_positive derive_pixel_scale derive_min_bounds derive_negative
             default_config build_geometry_required build_derives kappa_gate
             adaptive_refine adaptive_benign adaptive_penalty adaptive_geometry
             adaptive_roundtrip)
  add_test(NAME v6_p2_sky_node_adapt_${case} COMMAND v6_p2_sky_node_adapt_test ${case})
  set_tests_properties(v6_p2_sky_node_adapt_${case} PROPERTIES LABELS "v6_p2_sky;upm_node_adapt")
endforeach()
add_test(NAME v6_p2_sky_node_adapt_all COMMAND v6_p2_sky_node_adapt_test all)
set_tests_properties(v6_p2_sky_node_adapt_all PROPERTIES LABELS "v6_p2_sky;upm_node_adapt")

