| # | 检查项 | profiles | ①脚本/触发面 | ②构建产物 | ③宿主工具/模块 | ④前序产物 | ⑤gitignored | 守卫用例 |
|--|--|--|--|--|--|--|--|--|
| 3 | PRODUCTION-GRAPH | fast,linux-main,windows-main | 死触发面 | - | - | - | - |  |
| 5 | ACR-DORMANT | fast,linux-main,windows-main | 死触发面 | - | 未声明:nm | - | - |  |
| 14 | DUPLICATION | fast,linux-main,windows-main | OK | - | 未声明:nm | - | - |  |
| 16 | GLOSSARY-DOCS | fast,linux-main,windows-main | 死触发面 | - | - | - | - |  |
| 22 | WARNING-SUPPRESSION | fast,linux-main,windows-main | OK | - | 未声明:bash | - | - |  |
| 26 | ISA-LEAK-SELFTEST | fast,linux-main,windows-main | OK | - | 未声明:objdump | - | - |  |
| 28 | PROD-REACH-SELFTEST | fast,linux-main,windows-main | OK | - | 未声明:nm | - | - |  |
| 44 | LOG-CONTRACT-SELFCHECK | fast,linux-main,windows-main | 死触发面 | - | - | - | - |  |
| 45 | TRACEABILITY-MATRIX | fast,linux-main,windows-main | 死触发面 | - | - | - | - |  |
| 46 | TESTKIT-LIST | fast,linux-main,windows-main | OK | - | 未声明:bash | - | - |  |
| 47 | WORKSPACE-ADOPTION | fast,linux-main,windows-main | 死触发面 | - | - | - | - |  |
| 48 | RECONCILE-STATE | fast,linux-main,windows-main | 死触发面 | - | - | - | - |  |
| 50 | LINUX-MAIN-FIXTURES | linux-main | OK | - | - | - | outputs-gitignored:run/ci/wf_step/LINUX-PREPARE-FIXTURES.json |  |
| 51 | LINUX-MAIN-BUILD-TREE | linux-main | OK | - | 已声明 | - | outputs-gitignored:run/ci/wf_step/LINUX-BUILD-ROOT-GRAPH.json |  |
| 52 | UT-API | linux-main | OK | - | 未声明:g++,numpy | - | - | 16/48 |
| 53 | UT-ARCH | linux-main | OK | - | 未声明:g++ | - | - | 3/36 |
| 54 | UT-BACKEND | linux-main | OK | - | 未声明:g++,gcc,cmake,objdump,taskset,numpy,astropy | - | - | 44/209 |
| 55 | UT-CLI | linux-main | OK | - | 未声明:cmake,g++,gcc,nm | - | - | 71/174 |
| 58 | UT-MONITORING | fast,linux-main,windows-main | OK | - | 未声明:procfs | - | - | 3/74 |
| 60 | UT-RUNTIME | fast,linux-main,windows-main | OK | - | 未声明:g++ | - | - | 9/70 |
| 63 | UT-ABI | linux-main | OK | - | 未声明:cmake,g++,nm | - | - |  |
| 66 | UT-IO | linux-main | OK | - | 未声明:gcc,numpy | - | - |  |
| 67 | UT-QUALITY | linux-main | OK | - | 未声明:tar,cmake,g++ | - | - | 6/63 |
| 68 | UT-CPU-BASELINE | linux-main | OK | - | 未声明:g++,gcc | - | - |  |
| 69 | UT-CPU-DISPATCH | linux-main | OK | - | 未声明:g++,gcc | - | - |  |
| 70 | UT-CPU-AVX2 | linux-main | OK | - | 未声明:g++,gcc | - | - |  |
| 71 | UT-CPU-AVX512 | linux-main | OK | - | 未声明:g++,gcc | - | - |  |
| 72 | BUILD-GCC-RELEASE | linux-main,linux-deep | OK | 消费:run/ci | 已声明 | - | outputs-gitignored:run/ci/monitor/BUILD-GCC-RELEASE.json,run/ci/build-gcc-release-summary.json |  |
| 73 | DEEP-CLANG-BUILD | linux-deep | OK | 消费:run/ci | 已声明 | - | outputs-gitignored:run/ci/monitor/DEEP-CLANG-BUILD.json,run/ci/build-clang-summary.json |  |
| 74 | DEEP-SAN-ASAN | linux-deep | OK | 消费:run/ci | 已声明 | - | outputs-gitignored:run/ci/monitor/DEEP-SAN-ASAN.json,run/ci/build-qa-asan-summary.json |  |
| 75 | DEEP-SAN-TSAN | linux-deep | OK | 消费:run/ci | 已声明 | - | outputs-gitignored:run/ci/monitor/DEEP-SAN-TSAN.json,run/ci/build-qa-tsan-summary.json |  |
| 76 | DEEP-COV-CPP | linux-deep | OK | 消费:run/ci | 已声明 | - | outputs-gitignored:run/ci/monitor/DEEP-COV-CPP.json,run/ci/build-cov-summary.json |  |
| 77 | DEEP-COV-PY | linux-deep | OK | 消费:run/ci | 已声明 | - | outputs-gitignored:run/ci/monitor/DEEP-COV-PY.json,run/ci/coverage-py |  |
| 78 | DEEP-COMPLEXITY | linux-deep | OK | 消费:run/ci | - | - | outputs-gitignored:run/ci/complexity/complexity.json |  |
| 79 | WIN-BUILD-RELEASE | windows-main | OK | 消费:run/ci | 已声明 | - | outputs-gitignored:run/ci/monitor/WIN-BUILD-RELEASE.json,run/ci/win-build-summary.json |  |
| 80 | WIN-TEST-UNIT | windows-main | OK | 消费:run/ci | 已声明 | - | outputs-gitignored:run/ci/monitor/WIN-TEST-UNIT.json,run/ci/win-test-summary.json |  |
| 81 | WIN-PACKAGE-CANDIDATE | windows-main | OK | 消费:run/ci | 已声明 | - | outputs-gitignored:run/ci/monitor/WIN-PACKAGE-CANDIDATE.json,run/ci/win-package-summary.json |  |
| 82 | WIN-CANDIDATE-VALIDATE | windows-main | OK | - | - | - | outputs-gitignored:run/ci/wf_step/WINDOWS-VALIDATE-CANDIDATE.json |  |
| 83 | WORKFLOW-REGISTRY-BINDING | fast,linux-main,windows-main | OK | 消费:run/ci | 已声明 | - | outputs-gitignored:run/ci/workflow-binding/workflow_binding.json |  |
| 84 | CI-BINDING-TESTS | fast,linux-main,windows-main | 缺失 | - | 未声明:bash,yaml | - | - |  |
| 85 | CTEST-REGISTRATION | fast,linux-main,windows-main | OK | 消费:run/ci | - | - | outputs-gitignored:run/ci/ctest-registration/ctest_registration.json |  |
| 86 | CTEST-LINUX-FULL | linux-main | OK | 消费:run/ci | - | - | outputs-gitignored:run/ci/build-gcc-release/ctest-full.json,run/ci/build-gcc-release/ctest-full.junit.xml |  |
| 87 | CTEST-PHASE2-GATES | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/phase2_gates.json |  |
| 88 | CTEST-DRIZZLE-PRECISION-DEFAULT | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/drizzle_precision_default.json |  |
| 89 | CTEST-P1001-REAL-NODES | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p1001_real_nodes.json |  |
| 90 | CTEST-P2001-REAL-NODES | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p2001_real_nodes.json |  |
| 91 | CTEST-P2002-UNC-REJ-PROV | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p2002_unc_rej_prov.json |  |
| 92 | CTEST-P3002-REAL-NODES | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p3002_real_nodes.json |  |
| 93 | CTEST-P3002-UNCERTAINTY | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p3002_uncertainty.json |  |
| 94 | CTEST-P3-PROJECTION-UNITS | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p3_projection_units.json |  |
| 95 | CTEST-P3-PROJECTION-FAULT | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p3_projection_fault.json |  |
| 96 | CTEST-P1WCS-APBP | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p1wcs_apbp.json |  |
| 97 | CTEST-P1WCS-ASTROPY-CROSS | linux-main | OK | 消费:run/ci | 已声明 | 前序产物 | outputs-gitignored:run/ci/ctest/p1wcs_astropy_cross.json |  |
| 98 | CTEST-AIO-ABI-UNITS | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/aio_abi_units.json |  |
| 99 | CTEST-AIO-ABI-NEGATIVE | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/aio_abi_negative.json |  |
| 100 | CTEST-AIO-ABI-SELFCHECK | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/aio_abi_selfcheck.json |  |
| 101 | CTEST-AIO-HIPS-PUBLISH-ATOMIC-UNITS | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/hips_publish_atomic_units.json |  |
| 102 | CTEST-AIO-HIPS-PUBLISH-ATOMIC | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/hips_publish_atomic.json |  |
| 103 | CTEST-RT001-UNIQUE-EXECUTOR | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/rt001_unique_executor.json |  |
| 104 | KNOWN-FAILURES-BASELINE-VERIFY | fast,linux-main,windows-main | OK | 消费:run/ci | - | - | outputs-gitignored:run/ci/known-failures/known_failures_verify.json |  |
| 105 | UT-GAIA-ZLIB | fast,linux-main,windows-main | OK | - | 已声明 | - | - |  |
| 106 | CTEST-P1WCS-STD-F1-BRIDGE | linux-main | OK | 消费:run/ci | 已声明 | 前序产物 | outputs-gitignored:run/ci/ctest/p1wcs_std_f1_bridge_cross.json |  |
| 107 | CTEST-P2HIPS-UNITS | linux-main | OK | 消费:run/ci | 已声明 | 前序产物 | outputs-gitignored:run/ci/ctest/p2hips_units.json |  |
| 108 | CTEST-P2HIPS-DETERMINISM | linux-main | OK | 消费:run/ci | 已声明 | 前序产物 | outputs-gitignored:run/ci/ctest/p2hips_determinism.json |  |
| 109 | CTEST-P2HIPS-NEGATIVE | linux-main | OK | 消费:run/ci | 已声明 | 前序产物 | outputs-gitignored:run/ci/ctest/p2hips_negative.json |  |
| 110 | CTEST-P2HIPS-SELFCHECK | linux-main | OK | 消费:run/ci | 已声明 | 前序产物 | outputs-gitignored:run/ci/ctest/p2hips_selfcheck.json |  |
| 111 | DOC-LINE-ANCHORS | fast,linux-main,windows-main | OK | 消费:run/ci | - | - | outputs-gitignored:run/ci/doc-anchors/doc_line_anchors.json |  |
| 112 | IPV-PLATFORM-BINDING | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/ipv_platform_binding.json |  |
| 113 | CTEST-GAIA-MANIFEST-BOUNDS | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/gaia_module_manifest_bounds.json |  |
| 114 | CTEST-XPSD-SPECTRUM-COUNT-BOUNDS | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/xpsd_spectrum_count_bounds.json |  |
| 115 | CTEST-P1PHOT-FIXGATES | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p1phot_fixgates.json |  |
| 116 | CTEST-P1STAR-MAD | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p1star_mad.json |  |
| 117 | CTEST-P1PSF-PRODPATH-CENTROID | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p1psf_prodpath_centroid.json |  |
| 118 | CTEST-IPV-EXTRACT-WCS-SIP-FAILCLOSED | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/ipv_extract_wcs_sip_failclosed.json |  |
| 119 | CTEST-IPV-MAG-ITER | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/ipv_mag_iter.json |  |
| 120 | CTEST-P1SNR-SCIENCE-ALL | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p1snr_science_all.json |  |
| 121 | CTEST-P1SNR-LINUX-ALL | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p1snr_linux_all.json |  |
| 122 | CTEST-P1DRZ-TASKSET-INVARIANCE | linux-main | OK | 消费:run/ci | 未声明:taskset | 前序产物 | outputs-gitignored:run/ci/ctest/p1drz_taskset_invariance.json |  |
| 123 | CTEST-P1STAR-ANGLE-GUARD | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p1star_angle_guard.json |  |
| 124 | CTEST-IPV-TRIANGLE-BUDGET | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/ipv_triangle_budget.json |  |
| 125 | CTEST-IPV-ABI-LAYOUT-LOCK | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/ipv_abi_layout_lock.json |  |
| 126 | CTEST-IPV-ABI-LAYOUT-LOCK-SELFCHECK | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/ipv_abi_layout_lock_selfcheck.json |  |
| 127 | CTEST-IPV-PARAMS-ABI-FAILCLOSED | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/ipv_params_abi_failclosed.json |  |
| 128 | CTEST-GAIA-MAGNITUDE-RANGE-BOUNDS | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/gaia_magnitude_range_bounds.json |  |
| 129 | CTEST-P1SNR-FRAME-PARITY | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/p1snr_frame_parity.json |  |
| 130 | CTEST-IPV-MAG-ITER-DELIVERY | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/ctest/ipv_mag_iter_delivery.json |  |
| 131 | KNOWN-FAILURES-BASELINE-CHECK | linux-main | OK | 消费:run/ci | - | 前序产物 | outputs-gitignored:run/ci/known-failures/known_failures_check.json |  |
