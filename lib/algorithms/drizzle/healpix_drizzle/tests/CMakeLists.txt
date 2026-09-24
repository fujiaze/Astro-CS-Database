# ============================================================================
# lib/algorithms/drizzle/healpix_drizzle/tests — 端到端验收面 (DRZ-FLUX-FIX-01)
# ----------------------------------------------------------------------------
# 背景: 本目录下的验收/冻结/Oracle/CLEAN 测试此前**全部 tracked-but-unbuilt**
# (本目录无 CMakeLists.txt, 全仓无任何 add_executable 引用它们, ctest 命中 0);
# 运行器 acceptance_drizzle.py 还写着 Windows 专名 drizzle_acceptance_test.exe。
# 本文件把这些测试真正编进根图并注册 ctest; 运行器改为跨平台定位。
#
# 门锚 (DRZ-FLUX-FIX-01 / FZ-COND-FLUX-CONSERV):
#   Σ_p sumFlux_p = Σ_j x_j  —— 核按 drop 面积归一 (F&H 2002 §7.2 式(7) 下方
#   "fractional area overlap of the drop"; drizzlepac cdrizzlebox.c dover/=jaco),
#   与 pixfrac 无关 (pixfrac 只决定 footprint 大小, 不收缩总流量)。
#   pixfrac 档位: {0.1, 0.5, 0.8, 1.0} × 过采样率 {1,2,3,4};
#   负例注入 --inject-legacy-pixfrac2 (把 Σout 乘回 pixfrac² 的旧口径) 必须判红。
#
# 注意: p1drz/ 有自己的 CMakeLists 且已由 eng/tests/unit/CMakeLists.txt 注册,
# 本文件不递归它 (避免重复 target)。
# ============================================================================
set(DRZ_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(DRZ_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# ----------------------------------------------------------------------------
# 1. 端到端合成验收 (A..F: 位置/pixfrac×过采样/双向投影/尺度/广域/数值类型)
#    参数: [outdir] [--inject-legacy-pixfrac2] [--inject-flux-factor <f>]
# ----------------------------------------------------------------------------
add_executable(drizzle_acceptance_test
  ${DRZ_TESTS_DIR}/drizzle_acceptance_test.cpp)
target_include_directories(drizzle_acceptance_test PRIVATE
  ${DRZ_MODULE_DIR})
target_compile_definitions(drizzle_acceptance_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(drizzle_acceptance_test PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_acceptance
  COMMAND drizzle_acceptance_test ${CMAKE_BINARY_DIR}/drizzle_acceptance)
set_tests_properties(drizzle_acceptance PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800)

# 负例控制: 同一个可执行 + 同一判据, 注入旧 pixfrac² 口径必须判红。
# (恒真门没有证据资格; 本门能红能绿由 runner 与 ctest 双侧证明)
add_test(NAME drizzle_acceptance_negative_injection
  COMMAND drizzle_acceptance_test ${CMAKE_BINARY_DIR}/drizzle_acceptance_neg
          --inject-legacy-pixfrac2)
set_tests_properties(drizzle_acceptance_negative_injection PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800
  WILL_FAIL TRUE)

# ----------------------------------------------------------------------------
# 2. Phase1 冻结验收 (覆盖/sampling/pixfrac/FOV/reverse/HISS 往返/科学保真)
# ----------------------------------------------------------------------------
add_executable(drizzle_freeze_test
  ${DRZ_TESTS_DIR}/drizzle_freeze_test.cpp)
target_include_directories(drizzle_freeze_test PRIVATE
  ${DRZ_MODULE_DIR})
target_compile_definitions(drizzle_freeze_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(drizzle_freeze_test PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_freeze COMMAND drizzle_freeze_test)
set_tests_properties(drizzle_freeze PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800)

# ----------------------------------------------------------------------------
# 3. 候选零漏选 Oracle 矩阵 (9003 例; TEST-DRZ-CAND-001)
# ----------------------------------------------------------------------------
add_executable(candidate_oracle_test
  ${DRZ_TESTS_DIR}/candidate_oracle_test.cpp)
target_include_directories(candidate_oracle_test PRIVATE
  ${DRZ_MODULE_DIR})
target_compile_definitions(candidate_oracle_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(candidate_oracle_test PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_candidate_oracle
  COMMAND candidate_oracle_test ${CMAKE_BINARY_DIR}/drizzle_candidate)
set_tests_properties(drizzle_candidate_oracle PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800)

# ----------------------------------------------------------------------------
# 4. control 中值 MC 一致性 (Phase2 UPM 的证据面; 2000 次 MC)
# ----------------------------------------------------------------------------
add_executable(control_median_mc_test
  ${DRZ_TESTS_DIR}/control_median_mc_test.cpp)
target_include_directories(control_median_mc_test PRIVATE
  ${DRZ_MODULE_DIR})
target_compile_definitions(control_median_mc_test PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(control_median_mc_test PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_control_median_mc COMMAND control_median_mc_test)
set_tests_properties(drizzle_control_median_mc PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800)

# ----------------------------------------------------------------------------
# 5. 球面重叠面 + 通量守恒解析闭合 (drop 面积归一: Σ_p sumFlux = L_j)
# ----------------------------------------------------------------------------
add_executable(test_spherical_overlap
  ${DRZ_TESTS_DIR}/test_spherical_overlap.cpp)
target_include_directories(test_spherical_overlap PRIVATE
  ${DRZ_MODULE_DIR})
target_compile_definitions(test_spherical_overlap PRIVATE
  AIO_ENABLE_FITS AIO_ENABLE_HEALPIX)
target_link_libraries(test_spherical_overlap PRIVATE
  astrocs_drizzle ${ASTROCS_OMP_TARGET})
add_test(NAME drizzle_spherical_overlap COMMAND test_spherical_overlap)
set_tests_properties(drizzle_spherical_overlap PROPERTIES
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 600)

# ----------------------------------------------------------------------------
# 6. Python 驱动验收 (numpy 聚合断言 + 跨平台 exe 定位)
# ----------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME drizzle_acceptance_py
    COMMAND ${Python3_EXECUTABLE} ${DRZ_TESTS_DIR}/acceptance_drizzle.py
            --exe $<TARGET_FILE:drizzle_acceptance_test>
            --out ${CMAKE_BINARY_DIR}/drizzle_acceptance_py)
  set_tests_properties(drizzle_acceptance_py PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 1800)
endif()
