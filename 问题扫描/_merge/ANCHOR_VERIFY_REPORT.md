# 锚点机械核验报告 v3

- 生成器：_tools/verify_anchors.js（v3）；复跑 node 问题扫描/_tools/verify_anchors.js
- 本次扫描 273 份审计 md、9527 次引用：精确在位 7117 次；分五类，处置强度不同
- **v3 判据（由 M7 自我否证确立）**：判「路径缺失」**必须以存在性检索为准**；某个字面串 0 命中只说明**用词不同**，不是文件不存在。
  因此：一、PATH_MISMATCH **不得撤条**，只改路径写法；三、SYMBOL_ELSEWHERE 多半是「定义在他处/字段名不同」，**先补文件再定档**；只有二、ABSENT 与四、SYMBOL_ABSENT 才走撤条或重锚。

## 一、路径写错但同名文件唯一存在（210 种）→ 只改锚，不撤证据
| 档案里的写法 | 应改为 | 首次出现 |
|---|---|---|
| 基线/checks.json | ci/checks.json | 问题扫描/40_OWNER_DECISIONS.md:65 |
| diagnostics/TROUBLESHOOTING.md | docs/diagnostics/TROUBLESHOOTING.md | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:17 |
| docs/science/PHASE2_SAMPLER.md | docs/algorithms/PHASE2_SAMPLER.md | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:209 |
| ipv_api.h/ipv_wcs.h | lib/plate_solve/cpp/ipv/include/ipv_wcs.h | 问题扫描/_cache/L01.md:6 |
| include/ipv_wcs.h | lib/plate_solve/cpp/ipv/include/ipv_wcs.h | 问题扫描/_cache/L01.md:13 |
| include/ipv_types.h | lib/plate_solve/cpp/ipv/include/ipv_types.h | 问题扫描/_cache/L01.md:13 |
| include/ipv_api.h | lib/plate_solve/cpp/ipv/include/ipv_api.h | 问题扫描/_cache/L01.md:13 |
| src/ipv_solver.cpp | lib/plate_solve/cpp/ipv/src/ipv_solver.cpp | 问题扫描/_cache/L01.md:13 |
| src/ipv_select.cpp | lib/plate_solve/cpp/ipv/src/ipv_select.cpp | 问题扫描/_cache/L01.md:13 |
| src/ipv_sip.cpp | lib/plate_solve/cpp/ipv/src/ipv_sip.cpp | 问题扫描/_cache/L01.md:13 |
| src/ipv_ransac.cpp | lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp | 问题扫描/_cache/L01.md:13 |
| src/ipv_entry.cpp | lib/plate_solve/cpp/ipv/src/ipv_entry.cpp | 问题扫描/_cache/L01.md:13 |
| src/ipv_itertrans.cpp | lib/plate_solve/cpp/ipv/src/ipv_itertrans.cpp | 问题扫描/_cache/L01.md:13 |
| src/ipv_triangle.cpp | lib/plate_solve/cpp/ipv/src/ipv_triangle.cpp | 问题扫描/_cache/L01.md:13 |
| cpp/ipv/REPORT.md | lib/plate_solve/cpp/ipv/REPORT.md | 问题扫描/_cache/L01.md:14 |
| cpp/ipv/siril_atpmatch_b64.txt | lib/plate_solve/cpp/ipv/siril_atpmatch_b64.txt | 问题扫描/_cache/L01.md:14 |
| make_clean_out.txt/make_clean_err.txt | lib/plate_solve/cpp/ipv/make_clean_err.txt | 问题扫描/_cache/L01.md:14 |
| cpp/ipv/test/test_synthetic.cpp | lib/plate_solve/cpp/ipv/test/test_synthetic.cpp | 问题扫描/_cache/L01.md:14 |
| lib/plate_solve/SIRIL_COMPARISON.md | lib/plate_solve/cpp/ipv/SIRIL_COMPARISON.md | 问题扫描/_cache/L01.md:21 |
| contracts/DATA_SEMANTICS.md | docs/contracts/DATA_SEMANTICS.md | 问题扫描/_cache/L01.md:23 |
| src/ipv_wcs.cpp | lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp | 问题扫描/_cache/L01.md:168 |
| include/ipv_solver.h | lib/plate_solve/cpp/ipv/include/ipv_solver.h | 问题扫描/_cache/L01.md:251 |
| docs/24_WCS_VALIDATION_V2_SPEC.md | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/docs/24_WCS_VALIDATION_V2_SPEC.md | 问题扫描/_cache/L01.md:254 |
| docs/25_AUTHORITATIVE_MATCH_PAIR_CONTRACT.md | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/docs/25_AUTHORITATIVE_MATCH_PAIR_CONTRACT.md | 问题扫描/_cache/L01.md:254 |
| 20Database/lib/plate_solve/cpp/ipv/src/ipv_solver.cpp | lib/plate_solve/cpp/ipv/src/ipv_solver.cpp | 问题扫描/_cache/L01.md:348 |
| contracts/PUBLIC_API.md | docs/contracts/PUBLIC_API.md | 问题扫描/_cache/L02.md:10 |
| tests/test_healpix_oracle.cpp | lib/common/healpix/tests/test_healpix_oracle.cpp | 问题扫描/_cache/L03.md:13 |
| tests/test_hips_tile_mapping.cpp | lib/common/healpix/tests/test_hips_tile_mapping.cpp | 问题扫描/_cache/L03.md:13 |
| tests/test_healpix_neighbors.cpp | lib/common/healpix/tests/test_healpix_neighbors.cpp | 问题扫描/_cache/L03.md:14 |
| include/astrocs/hips/types.h | lib/hips/include/astrocs/hips/types.h | 问题扫描/_cache/L03.md:21 |
| include/astrocs/hips/publish.h | lib/hips/include/astrocs/hips/publish.h | 问题扫描/_cache/L03.md:21 |
| src/aio_publish.cpp | lib/hips/src/aio_publish.cpp | 问题扫描/_cache/L03.md:22 |
| phase2/tools/stage2.cpp | lib/phase2/tools/stage2.cpp | 问题扫描/_cache/L03.md:70 |
| healpix/healpix_core.h | lib/common/healpix/healpix_core.h | 问题扫描/_cache/L03.md:277 |
| include/astrocs/drizzle/types.h | lib/drizzle/include/astrocs/drizzle/types.h | 问题扫描/_cache/L04.md:6 |
| lib/orchestrator/cpp/orchestrator.cpp | lib/orchestrator/cpp/src/orchestrator.cpp | 问题扫描/_cache/L04.md:6 |
| lib/orchestrator/cpp/json_config.h | lib/orchestrator/cpp/include/json_config.h | 问题扫描/_cache/L04.md:37 |
| p1drz/CMakeLists.txt | lib/healpix_db/healpix_drizzle/tests/p1drz/CMakeLists.txt | 问题扫描/_cache/L04.md:91 |
| src/calibrator.cpp | lib/calibration/src/calibrator.cpp | 问题扫描/_cache/L05.md:6 |
| src/master_generator.cpp | lib/calibration/src/master_generator.cpp | 问题扫描/_cache/L05.md:6 |
| src/cosmetic_corrector.cpp | lib/calibration/src/cosmetic_corrector.cpp | 问题扫描/_cache/L05.md:6 |
| src/ac_api.cpp | lib/calibration/src/ac_api.cpp | 问题扫描/_cache/L05.md:6 |
| include/astrocs/calibration/types.h | lib/calibration/include/astrocs/calibration/types.h | 问题扫描/_cache/L05.md:6 |
| integration/astrocs_p1_calibration.integration.json | lib/calibration/integration/astrocs_p1_calibration.integration.json | 问题扫描/_cache/L05.md:6 |
| cpp/cosmetic_corrector.cpp | lib/calibration/cpp/cosmetic_corrector.cpp | 问题扫描/_cache/L05.md:6 |
| include/astrocs/cosmetic/types.h | lib/cosmetic/include/astrocs/cosmetic/types.h | 问题扫描/_cache/L05.md:6 |
| src/photometry_apply.h | lib/calibration/src/photometry_apply.h | 问题扫描/_cache/L05.md:9 |
| p1cal_fixtures.hpp/p1cos_fixtures.hpp | lib/cosmetic/tests/p1cos/p1cos_fixtures.hpp | 问题扫描/_cache/L05.md:9 |
| configs/stage1.schema.json | lib/orchestrator/configs/stage1.schema.json | 问题扫描/_cache/L05.md:207 |
| p1cal_oracle.hpp/p1cos_oracle.hpp | lib/cosmetic/tests/p1cos/p1cos_oracle.hpp | 问题扫描/_cache/L05.md:317 |
| cpp/test/test_spectrum_integrator_golden_results.json | lib/photometric_calib/cpp/test/test_spectrum_integrator_golden_results.json | 问题扫描/_cache/L07.md:18 |
| src/snr_estimator.cpp | lib/snr_estimator/cpp/src/snr_estimator.cpp | 问题扫描/_cache/L07.md:231 |
| src/noise_model.cpp | lib/snr_estimator/cpp/src/noise_model.cpp | 问题扫描/_cache/L07.md:231 |
| lib/phase2/src/block.cpp/integrate.cpp/async_io.cpp/rejection.cpp/cuda_bridge_stub.cpp | lib/phase2/src/cuda_bridge_stub.cpp | 问题扫描/_cache/L08.md:9 |
| fixtures/phase2_typed_dag.json | runtime/pipeline/fixtures/phase2_typed_dag.json | 问题扫描/_cache/L09.md:4 |
| engineering/control/archive/.../09_WINDOWS_AND_FATDUCK_VALIDATION.md | engineering/control/archive/2026-09-09_superseded_V8.1_CI_CONTROL_20260905/09_WINDOWS_AND_FATDUCK_VALIDATION.md | 问题扫描/_cache/L09.md:218 |
| src/gaia_client.h | lib/gaia_xpsd_client/src/gaia_client.h | 问题扫描/_cache/L10.md:7 |
| include/astrocs/gaia/types.h | lib/gaia_xpsd_client/include/astrocs/gaia/types.h | 问题扫描/_cache/L10.md:7 |
| src/module_entry.c | lib/gaia_xpsd_client/src/module_entry.c | 问题扫描/_cache/L10.md:7 |
| integration/astrocs_catalog_gaia.integration.json | lib/gaia_xpsd_client/integration/astrocs_catalog_gaia.integration.json | 问题扫描/_cache/L10.md:7 |
| test/test_gaia_race.c | lib/gaia_xpsd_client/test/test_gaia_race.c | 问题扫描/_cache/L10.md:7 |
| test/test_spec_collector_ownership.c | lib/gaia_xpsd_client/test/test_spec_collector_ownership.c | 问题扫描/_cache/L10.md:7 |
| test_production_store.py/test_provenance.py | tests/artifact/test_provenance.py | 问题扫描/_cache/L10.md:11 |
| src/hips/aio_hips_writer.cpp | lib/astro_image_io/src/hips/aio_hips_writer.cpp | 问题扫描/_cache/L10.md:12 |
| src/checksum.c | lib/astro_image_io/third_party/cfitsio/checksum.c | 问题扫描/_cache/L10.md:237 |
| include/astrocs/cpu/capability_v1.h | providers/cpu/common/include/astrocs/cpu/capability_v1.h | 问题扫描/_cache/L11.md:5 |
| schemas/cpu_capability.schema.json | providers/cpu/common/schemas/cpu_capability.schema.json | 问题扫描/_cache/L11.md:5 |
| tests/test_ci001_failclosed.py | ci/tests/test_ci001_failclosed.py | 问题扫描/_cache/L11.md:5 |
| schemas/astrocs-product.schema.json | packaging/schemas/astrocs-product.schema.json | 问题扫描/_cache/L12.md:10 |
| schemas/install-tree-contract.schema.json | packaging/schemas/install-tree-contract.schema.json | 问题扫描/_cache/L12.md:10 |
| src/noop_module.c | modules/conformance/noop/src/noop_module.c | 问题扫描/_cache/L12.md:11 |
| src/aio_pipeline.cpp | lib/astro_image_io/src/aio_pipeline.cpp | 问题扫描/_cache/L12.md:39 |
| src/aio_pipeline_engine.cpp | lib/astro_image_io/src/aio_pipeline_engine.cpp | 问题扫描/_cache/L12.md:39 |
| src/upm.cpp | lib/phase2/src/upm.cpp | 问题扫描/_cache/L12.md:53 |
| acr/api/kernel_registry.cpp | lib/acr/api/kernel_registry.cpp | 问题扫描/_cache/L12.md:53 |
| acr/backends/cuda/cuda_bridge_loader.cpp | lib/acr/backends/cuda/cuda_bridge_loader.cpp | 问题扫描/_cache/L12.md:53 |
| cpp/src/main.cpp | lib/orchestrator/cpp/src/main.cpp | 问题扫描/_cache/L12.md:54 |
| calibration/module.yaml | lib/calibration/module.yaml | 问题扫描/_cache/L12.md:136 |
| star_detector/module.yaml | lib/star_detector/module.yaml | 问题扫描/_cache/L12.md:137 |
| contracts/artifact_abi_v1.h | include/astrocs/contracts/artifact_abi_v1.h | 问题扫描/_cache/L12.md:156 |
| io/aio_abi_v1.h | include/astrocs/io/aio_abi_v1.h | 问题扫描/_cache/L12.md:156 |
| include/astrocs/exit_codes.h | cli/exit_codes.h | 问题扫描/_cache/L12.md:200 |
| schemas/jsonl_event_v1.schema.json | contracts/schemas/jsonl_event_v1.schema.json | 问题扫描/_cache/L12.md:237 |
| core/contracts.h | include/astrocs/core/contracts.h | 问题扫描/_cache/L13.md:4 |
| core/executor.h | include/astrocs/core/executor.h | 问题扫描/_cache/L13.md:4 |
| abi/status_codes.h | include/astrocs/abi/status_codes.h | 问题扫描/_cache/L13.md:4 |
| module_loader/README.md | runtime/module_loader/README.md | 问题扫描/_cache/L14.md:45 |
| C_ABI_STANDARD.md/CONCURRENCY_STANDARD.md | docs/standards/CONCURRENCY_STANDARD.md | 问题扫描/_cache/L14.md:49 |
| 883/upm.cpp | lib/phase2/src/upm.cpp | 问题扫描/_cache/L14.md:131 |
| sampler.h/upm.h | lib/phase2/include/astro/phase2/upm.h | 问题扫描/_cache/L14.md:133 |
| src/gaia_client.c | lib/gaia_xpsd_client/src/gaia_client.c | 问题扫描/_cache/L15.md:587 |
| schemas/traceability_matrix.schema.json | contracts/schemas/traceability_matrix.schema.json | 问题扫描/_cache/L16.md:318 |
| acr_kernels.cpp/cuda_bridge_stub.cpp | lib/phase2/src/cuda_bridge_stub.cpp | 问题扫描/_cache/L17.md:201 |
| tools/stage2.cpp | lib/phase2/tools/stage2.cpp | 问题扫描/_cache/L17.md:547 |
| schemas/version.schema.json | contracts/schemas/version.schema.json | 问题扫描/_cache/L18.md:186 |
| README/CHANGELOG/build.sh/toolchain.ps1 | toolchain.ps1 | 问题扫描/_cache/L18.md:187 |
| docs/science/HEALPIX_MAPPING.md | docs/algorithms/HEALPIX_MAPPING.md | 问题扫描/_cache/L18.md:291 |
| docs/algorithms/REJECTION.md | docs/science/REJECTION.md | 问题扫描/_cache/L18.md:291 |
| docs/algorithms/INTEGRATION.md | docs/science/INTEGRATION.md | 问题扫描/_cache/L18.md:291 |
| contracts/INDEX.yaml | docs/contracts/INDEX.yaml | 问题扫描/_cache/L18.md:354 |
| owner/RELEASE_STATUS.md | docs/owner/RELEASE_STATUS.md | 问题扫描/_cache/L18.md:405 |
| ipv_wcs.cpp/p3_wcs.cpp | lib/phase3_session/p3_wcs.cpp | 问题扫描/_cache/L19.md:495 |
| examples/phase3_planar_fits_v1.example.json | contracts/data/examples/phase3_planar_fits_v1.example.json | 问题扫描/_cache/L21.md:10 |
| DATA_SEMANTICS/GLOSSARY/INTEGRATION.md | docs/science/INTEGRATION.md | 问题扫描/_cache/L21.md:265 |
| ALG/DATA/snr_estimator.h | lib/snr_estimator/cpp/include/snr_estimator.h | 问题扫描/_cache/L21.md:284 |
| 串行/drizzle_engine.cpp | lib/healpix_db/healpix_drizzle/drizzle_engine.cpp | 问题扫描/_cache/L22.md:15 |
| p1noise/CMakeLists.txt | lib/snr_estimator/tests/p1noise/CMakeLists.txt | 问题扫描/_cache/L23.md:57 |
| tests/unit/noise_model_science_test.cpp | lib/snr_estimator/cpp/test/noise_model_science_test.cpp | 问题扫描/_cache/L23.md:137 |
| memory.md/README/test_report.md | lib/astro_image_io/tests/test_report.md | 问题扫描/_cache/L23.md:341 |
| include/aio_hips_reader.h | lib/astro_image_io/include/aio_hips_reader.h | 问题扫描/_cache/L24.md:170 |
| contracts/API_CONTRACTS.csv | docs/contracts/API_CONTRACTS.csv | 问题扫描/_cache/L24.md:182 |
| ahpx/aio_ahpx_reader.cpp | lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp | 问题扫描/_cache/L24.md:268 |
| PUBLIC_API/API_CONTRACTS.csv | docs/contracts/API_CONTRACTS.csv | 问题扫描/_cache/L24.md:322 |
| heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.h | lib/astro_image_io/third_party/cfitsio/fitsio.h | 问题扫描/_cache/L25.md:13 |
| drizzle/CMakeLists.txt | lib/drizzle/CMakeLists.txt | 问题扫描/_cache/L25.md:61 |
| nlohmann/json.hpp | third_party/nlohmann/json.hpp | 问题扫描/_cache/L25.md:61 |
| healpix_drizzle/nanoflann.hpp | lib/healpix_db/healpix_drizzle/nanoflann.hpp | 问题扫描/_cache/L25.md:95 |
| fitsio.h/ChangeLog/License.txt/zcompress.c | lib/astro_image_io/third_party/cfitsio/zcompress.c | 问题扫描/_cache/L25.md:241 |
| include/astro/phase2/rejection.h | lib/phase2/include/astro/phase2/rejection.h | 问题扫描/_cache/L25.md:244 |
| tools/rcr_oracle_compare.py | lib/phase2/tools/rcr_oracle_compare.py | 问题扫描/_cache/L25.md:244 |
| tools/rejection_oracle_compare.py | lib/phase2/tools/rejection_oracle_compare.py | 问题扫描/_cache/L25.md:244 |
| src/rejection.cpp | lib/phase2/src/rejection.cpp | 问题扫描/_cache/L25.md:244 |
| arch/check_thread_budget.py | tools/arch/check_thread_budget.py | 问题扫描/_cache/L25.md:247 |
| arch/build_production_execution_inventory.py | tools/arch/build_production_execution_inventory.py | 问题扫描/_cache/L25.md:247 |
| REPORT.md/IPV_PIPELINE.md | lib/plate_solve/IPV_PIPELINE.md | 问题扫描/_cache/L25.md:258 |
| lib/plate_solve/cpp/ipv/ipv_polygon.h | lib/plate_solve/cpp/ipv/include/ipv_polygon.h | 问题扫描/_cache/L26.md:173 |
| lib/plate_solve/cpp/ipv/test_synthetic.cpp | lib/plate_solve/cpp/ipv/test/test_synthetic.cpp | 问题扫描/_cache/L26.md:173 |
| healpix_browser_qt/tools/gen_geometry_truth.py | lib/healpix_db/healpix_browser_qt/tools/gen_geometry_truth.py | 问题扫描/_cache/L26.md:201 |
| core/healpix_math.h | lib/healpix_db/healpix_browser_qt/core/healpix_math.h | 问题扫描/_cache/L26.md:222 |
| phase3_session/p3_resample.cpp | lib/phase3_session/p3_resample.cpp | 问题扫描/_cache/L26.md:228 |
| lib/core/plan_estimator.cpp | lib/core/src/plan_estimator.cpp | 问题扫描/_cache/L26.md:229 |
| echo/README.md | modules/conformance/echo/README.md | 问题扫描/_cache/L27.md:182 |
| include/astro_calibration.h | lib/calibration/include/astro_calibration.h | 问题扫描/_cache/L27.md:265 |
| tests/hiss_write_probe.cpp | lib/healpix_db/healpix_drizzle/tests/hiss_write_probe.cpp | 问题扫描/_cache/L27.md:265 |
| lib/phase2_rej/src/rejection.cpp | lib/phase2/src/rejection.cpp | 问题扫描/_merge/00_COORDINATION.md:40 |
| modules/services/io/src/fits_core.c | runtime/io/fits_core.c | 问题扫描/_merge/00_COORDINATION.md:407 |
| lib/.../hips_properties.cpp | lib/phase3_session/hips_properties.cpp | 问题扫描/_merge/00_COORDINATION.md:502 |
| drizzle/types.h | lib/drizzle/include/astrocs/drizzle/types.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:423 |
| docs/architecture/PUBLIC_API.md | docs/contracts/PUBLIC_API.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:435 |
| .github/workflows/ci.yml | lib/astro_image_io/third_party/cfitsio/.github/workflows/ci.yml | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:441 |
| lib/phase3_rsmp/p3_resample.h | lib/phase3_session/p3_resample.h | 问题扫描/_merge/M1a.md:153 |
| p1drz/p1drz_tests_core.cpp | lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_core.cpp | 问题扫描/_merge/M2a.md:62 |
| configs/stage1.template.json | lib/orchestrator/configs/stage1.template.json | 问题扫描/_merge/M2a.md:116 |
| tests/p1drz/CMakeLists.txt | lib/healpix_db/healpix_drizzle/tests/p1drz/CMakeLists.txt | 问题扫描/_merge/M2a.md:130 |
| tests/pipeline/test_phase_lifecycle.py | tests/runtime/test_phase_lifecycle.py | 问题扫描/_merge/M2a.md:142 |
| tests/hips_core_selftest.c | modules/services/io/tests/hips_core_selftest.c | 问题扫描/_merge/M2b.md:83 |
| dynamic_psf/README.md | lib/dynamic_psf/README.md | 问题扫描/_merge/M3.md:49 |
| lib/calibration/tests/calibration_adapter_test.cpp | tests/unit/calibration_adapter_test.cpp | 问题扫描/_merge/M3.md:87 |
| ../lib/phase1_session/p1_session.cpp | lib/phase1_session/p1_session.cpp | 问题扫描/_merge/M3.md:148 |
| 1478/sampler.cpp | lib/phase2/src/sampler.cpp | 问题扫描/_merge/M4.md:70 |
| upm.cpp/sampler.cpp | lib/phase2/src/sampler.cpp | 问题扫描/_merge/M4.md:110 |
| star_detector.h/dynamic_psf.h | lib/dynamic_psf/include/dynamic_psf.h | 问题扫描/_merge/M6a.md:39 |
| cli/pc_api.cpp | lib/photometric_calib/cpp/src/pc_api.cpp | 问题扫描/_merge/M6a.md:118 |
| hips/aio_hips_reader.h | lib/astro_image_io/include/aio_hips_reader.h | 问题扫描/_merge/M6a.md:137 |
| tests/p1noise/CMakeLists.txt | lib/snr_estimator/tests/p1noise/CMakeLists.txt | 问题扫描/_merge/M6a.md:158 |
| src/photometry_apply.cpp | lib/calibration/src/photometry_apply.cpp | 问题扫描/_merge/M6a.md:230 |
| quality/check_traceability.py | tools/quality/check_traceability.py | 问题扫描/_merge/M6b.md:63 |
| runtime/runtime.h | include/astrocs/core/runtime.h | 问题扫描/_merge/M6b.md:109 |
| libaio/src/aio_api.cpp | lib/astro_image_io/src/aio_api.cpp | 问题扫描/_merge/M7.md:141 |
| plate_solve/README.md | lib/plate_solve/README.md | 问题扫描/_merge/M8.md:104 |
| snr_estimator/README.md | lib/snr_estimator/README.md | 问题扫描/_merge/M8.md:105 |
| docs/software/fitsio/fitsio.h | lib/astro_image_io/third_party/cfitsio/fitsio.h | 问题扫描/_merge/M8a.md:99 |
| tools/backend/rcr_oracle_compare.py | lib/phase2/tools/rcr_oracle_compare.py | 问题扫描/_merge/M8a.md:126 |
| tools/quality/rcr_oracle_compare.py | lib/phase2/tools/rcr_oracle_compare.py | 问题扫描/_merge/M8a.md:126 |
| lib/phase2_rej/include/astro/phase2/rejection.h | lib/phase2/include/astro/phase2/rejection.h | 问题扫描/_merge/M8a.md:126 |
| tools/quality/check_release_consistency.py | tools/check_release_consistency.py | 问题扫描/_merge/M8a.md:126 |
| healpix_drizzle/README.md | lib/healpix_db/healpix_drizzle/README.md | 问题扫描/_merge/M8a.md:239 |
| src/stage2.cpp | lib/phase2/tools/stage2.cpp | 问题扫描/_merge/M9.md:175 |
| include/ipv_polygon.h | lib/plate_solve/cpp/ipv/include/ipv_polygon.h | 问题扫描/_merge/M9.md:215 |
| tools/monitoring/run_provider_oracle_checks.py | tests/cpu/baseline/run_provider_oracle_checks.py | 问题扫描/findings/A_SCI_DEF/p2/M8_L22_L23.md:9 |
| app/browser_cli.cpp | lib/healpix_db/healpix_browser_qt/app/browser_cli.cpp | 问题扫描/findings/A_SCI_DEF/p2/M9_L24_L26.md:10 |
| tools/gen_geometry_truth.py | lib/healpix_db/healpix_browser_qt/tools/gen_geometry_truth.py | 问题扫描/findings/B_STD_MISMATCH/p0/M2b_L03_L15.md:19 |
| include/astrocs/io/hips_input_v1.h | modules/services/io/include/astrocs/io/hips_input_v1.h | 问题扫描/findings/B_STD_MISMATCH/p0/M2b_L03_L15.md:20 |
| lib/core/src/artifact.h | include/astrocs/core/artifact.h | 问题扫描/findings/B_STD_MISMATCH/p1/M8_L22_L23.md:11 |
| p3_resample.h/p3_session.cpp | lib/phase3_session/p3_session.cpp | 问题扫描/findings/C_DOC_CODE_GAP/p0/M6a_L13_L14.md:26 |
| tests/p1drz/p1drz_tests_core.cpp | lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_core.cpp | 问题扫描/findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.md:38 |
| sampler.cpp/upm.cpp | lib/phase2/src/upm.cpp | 问题扫描/findings/C_DOC_CODE_GAP/p1/M5a_L11.md:48 |
| src/ipv_polygon.cpp | lib/plate_solve/cpp/ipv/src/ipv_polygon.cpp | 问题扫描/findings/C_DOC_CODE_GAP/p1/M9_L24_L26.md:55 |
| test/test_synthetic.cpp | lib/plate_solve/cpp/ipv/test/test_synthetic.cpp | 问题扫描/findings/C_DOC_CODE_GAP/p1/M9_L24_L26.md:57 |
| healpix_browser_qt/widgets/sphere_view.h | lib/healpix_db/healpix_browser_qt/widgets/sphere_view.h | 问题扫描/findings/C_DOC_CODE_GAP/p1/M9_L24_L26.md:58 |
| lib/plate_solve/src/ipv_triangle.cpp | lib/plate_solve/cpp/ipv/src/ipv_triangle.cpp | 问题扫描/findings/C_DOC_CODE_GAP/p2/M8_L22_L23.md:9 |
| lib/astro_calibration/include/astro_calibration.h | lib/calibration/include/astro_calibration.h | 问题扫描/findings/D_COMMENT/p1/M3_L05_L07.md:13 |
| cosmetic/src/module_entry.cpp | lib/cosmetic/src/module_entry.cpp | 问题扫描/findings/D_COMMENT/p1/M6a_L13_L14.md:79 |
| core/hips_browser_backend.cpp | lib/healpix_db/healpix_browser_qt/core/hips_browser_backend.cpp | 问题扫描/findings/D_COMMENT/p2/M6a_L13_L14.md:126 |
| lib/plate_solve/cpp/ipv/src/ipv_solver.h | lib/plate_solve/cpp/ipv/include/ipv_solver.h | 问题扫描/findings/E_TRACE_BREAK/p1/M1a_L01_L02.md:23 |
| lib/phase3_rsmp/src/p3_resample.cpp | lib/phase3_session/p3_resample.cpp | 问题扫描/findings/E_TRACE_BREAK/p1/M5b_L12_L17.md:20 |
| snr_estimator.h/photometry_apply.h | lib/calibration/src/photometry_apply.h | 问题扫描/findings/E_TRACE_BREAK/p1/M6a_L13_L14.md:4 |
| docs/validation/TEST_MATRIX.md | docs/contracts/TEST_MATRIX.md | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.md:50 |
| src/dark_optimizer.cpp | lib/calibration/src/dark_optimizer.cpp | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.md:69 |
| include/snr_estimator.h | lib/snr_estimator/cpp/include/snr_estimator.h | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.md:69 |
| include/astrocs/io/fits_stream_v1.h | modules/services/io/include/astrocs/io/fits_stream_v1.h | 问题扫描/findings/E_TRACE_BREAK/p2/M6b_L16_L18.md:59 |
| tests/fits_core_selftest.c | modules/services/io/tests/fits_core_selftest.c | 问题扫描/findings/E_TRACE_BREAK/p2/M6b_L16_L18.md:59 |
| astrocs/cosmetic/types.h | lib/cosmetic/include/astrocs/cosmetic/types.h | 问题扫描/findings/E_TRACE_BREAK/p2/M8a_L25_L27.md:34 |
| lib/phase1/session/test_hips_tile_mapping.cpp | lib/common/healpix/tests/test_hips_tile_mapping.cpp | 问题扫描/findings/F_TEST_GAP/p0/M2b_L03_L15.md:18 |
| add_executable/add_test/checks.json | ci/checks.json | 问题扫描/findings/F_TEST_GAP/p0/M9_L24_L26.md:20 |
| ../../lib/astro_image_io/tests/test_p1_io_hardening.cpp | lib/astro_image_io/tests/test_p1_io_hardening.cpp | 问题扫描/findings/F_TEST_GAP/p0/M9_L24_L26.md:32 |
| p1_hips/publish_atomic_test.c | tests/unit/p1_hips/publish_atomic_test.c | 问题扫描/findings/F_TEST_GAP/p0/M9_L24_L26.md:32 |
| fixtures/dangling_ref.json | tests/traceability/fixtures/dangling_ref.json | 问题扫描/findings/F_TEST_GAP/p1/M6b_L16_L18.md:21 |
| tests/synth/test_drizzle_oracle.py | tests/backend/test_drizzle_oracle.py | 问题扫描/findings/F_TEST_GAP/p2/M2a_L04_L10.md:10 |
| registry/astrocs.phase2.resample.md | docs/modules/registry/astrocs.phase2.resample.md | 问题扫描/findings/G_GOV_GATE/p0/M6b_L16_L18.md:33 |
| archive/review/RELEASE_STATUS.md | docs/archive/review/RELEASE_STATUS.md | 问题扫描/findings/G_GOV_GATE/p0/M6b_L16_L18.md:62 |
| p1star/CMakeLists.txt | lib/star_detector/tests/p1star/CMakeLists.txt | 问题扫描/findings/G_GOV_GATE/p0/M8a_L25_L27.md:22 |
| licenses/License.txt | lib/astro_image_io/third_party/cfitsio/licenses/License.txt | 问题扫描/findings/G_GOV_GATE/p0/M8a_L25_L27.md:34 |
| nlohmann/json-schema.hpp | lib/orchestrator/cpp/third_party/json-schema-validator/nlohmann/json-schema.hpp | 问题扫描/findings/G_GOV_GATE/p2/M8a_L25_L27.md:12 |
| docs/contracts/DATA-002_PHASE_PRODUCT_EXCHANGE.md | docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md | 问题扫描/findings/H_NUMERIC/p2/M2a_L04_L10.md:28 |
| tests/backend/test_photometry_apply.cpp | lib/calibration/tests/test_photometry_apply.cpp | 问题扫描/findings/I_DOC_HYGIENE/p1/M3_L05_L07.md:9 |
| docs/algorithm.md | lib/photometric_calib/docs/algorithm.md | 问题扫描/findings/I_DOC_HYGIENE/p2/M3_L05_L07.md:13 |
| lib/photometric_calib/tests/p1phot/p1phot_tests_core.cpp | lib/phase1/tests/p1phot/p1phot_tests_core.cpp | 问题扫描/findings/I_DOC_HYGIENE/p2/M3_L05_L07.md:24 |
| docs/interfaces/PUBLIC_API.md | docs/contracts/PUBLIC_API.md | 问题扫描/findings/I_DOC_HYGIENE/p2/M8_L22_L23.md:15 |
| noop/README.md | modules/conformance/noop/README.md | 问题扫描/findings/I_DOC_HYGIENE/p2/M8a_L25_L27.md:93 |

## 二、真源中确无此路径（364 种）→ 需三级复核后改述或撤条
| 引用 | 近似名候选 | 首次出现 |
|---|---|---|
| _cache/Lxx.md | （无同名近似） | 问题扫描/00_README.md:34 |
| _merge/Mx.md | （无同名近似） | 问题扫描/00_README.md:36 |
| README/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/10_PROTOCOL.md:37 |
| _merge/00_COORDINATION.md | （无同名近似） | 问题扫描/40_OWNER_DECISIONS.md:44 |
| G_GOV_GATE/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/INDEX.md:39 |
| C_DOC_CODE_GAP/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:40 |
| C_DOC_CODE_GAP/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:41 |
| D_COMMENT/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:42 |
| A_SCI_DEF/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:43 |
| D_COMMENT/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:44 |
| F_TEST_GAP/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:45 |
| G_GOV_GATE/p0/M5b_L12_L17.md | （无同名近似） | 问题扫描/INDEX.md:46 |
| G_GOV_GATE/p1/M5a_L11.md | （无同名近似） | 问题扫描/INDEX.md:47 |
| B_STD_MISMATCH/p0/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:48 |
| C_DOC_CODE_GAP/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/INDEX.md:49 |
| E_TRACE_BREAK/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/INDEX.md:50 |
| I_DOC_HYGIENE/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/INDEX.md:51 |
| C_DOC_CODE_GAP/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:52 |
| C_DOC_CODE_GAP/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:53 |
| F_TEST_GAP/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:54 |
| C_DOC_CODE_GAP/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:55 |
| G_GOV_GATE/p0/M5a_L11_L17.md | （无同名近似） | 问题扫描/INDEX.md:56 |
| C_DOC_CODE_GAP/p1/M5a_L11.md | （无同名近似） | 问题扫描/INDEX.md:57 |
| A_SCI_DEF/p1/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:58 |
| D_COMMENT/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:59 |
| E_TRACE_BREAK/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:60 |
| A_SCI_DEF/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:61 |
| F_TEST_GAP/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:62 |
| G_GOV_GATE/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:63 |
| B_STD_MISMATCH/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:64 |
| C_DOC_CODE_GAP/p0/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:65 |
| A_SCI_DEF/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:66 |
| A_SCI_DEF/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:67 |
| B_STD_MISMATCH/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:68 |
| E_TRACE_BREAK/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:69 |
| I_DOC_HYGIENE/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:70 |
| I_DOC_HYGIENE/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/INDEX.md:71 |
| B_STD_MISMATCH/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:72 |
| A_SCI_DEF/p0/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:73 |
| C_DOC_CODE_GAP/p1/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:74 |
| F_TEST_GAP/p0/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:75 |
| I_DOC_HYGIENE/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:76 |
| F_TEST_GAP/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:77 |
| C_DOC_CODE_GAP/p0/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:78 |
| C_DOC_CODE_GAP/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:79 |
| I_DOC_HYGIENE/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:80 |
| G_GOV_GATE/p0/M6b_L16_L18.md | （无同名近似） | 问题扫描/INDEX.md:81 |
| G_GOV_GATE/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/INDEX.md:82 |
| D_COMMENT/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:83 |
| D_COMMENT/p2/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:84 |
| E_TRACE_BREAK/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/INDEX.md:85 |
| A_SCI_DEF/p2/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:86 |
| C_DOC_CODE_GAP/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/INDEX.md:87 |
| E_TRACE_BREAK/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/INDEX.md:88 |
| C_DOC_CODE_GAP/p2/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:89 |
| A_SCI_DEF/p0/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:90 |
| F_TEST_GAP/p0/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:91 |
| C_DOC_CODE_GAP/p2/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:92 |
| D_COMMENT/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:93 |
| E_TRACE_BREAK/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:94 |
| C_DOC_CODE_GAP/p0/M5b_L12_L17.md | （无同名近似） | 问题扫描/INDEX.md:95 |
| B_STD_MISMATCH/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:96 |
| H_NUMERIC/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:97 |
| G_GOV_GATE/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:98 |
| D_COMMENT/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:99 |
| A_SCI_DEF/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:100 |
| C_DOC_CODE_GAP/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:101 |
| H_NUMERIC/p1/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:102 |
| C_DOC_CODE_GAP/p0/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:103 |
| G_GOV_GATE/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:104 |
| H_NUMERIC/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:105 |
| F_TEST_GAP/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:106 |
| A_SCI_DEF/p0/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:107 |
| C_DOC_CODE_GAP/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:108 |
| I_DOC_HYGIENE/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/INDEX.md:109 |
| G_GOV_GATE/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/INDEX.md:110 |
| C_DOC_CODE_GAP/p2/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:111 |
| A_SCI_DEF/p0/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:112 |
| F_TEST_GAP/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:113 |
| I_DOC_HYGIENE/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:114 |
| H_NUMERIC/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:115 |
| I_DOC_HYGIENE/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:116 |
| I_DOC_HYGIENE/p2/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:117 |
| A_SCI_DEF/p2/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:118 |
| G_GOV_GATE/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:119 |
| G_GOV_GATE/p2/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:120 |
| H_NUMERIC/p2/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:121 |
| D_COMMENT/p2/M4_L08_L09.md | （无同名近似） | 问题扫描/INDEX.md:122 |
| B_STD_MISMATCH/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:123 |
| A_SCI_DEF/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:124 |
| B_STD_MISMATCH/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:125 |
| C_DOC_CODE_GAP/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:126 |
| F_TEST_GAP/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:127 |
| D_COMMENT/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:128 |
| E_TRACE_BREAK/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:129 |
| F_TEST_GAP/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:130 |
| G_GOV_GATE/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:131 |
| H_NUMERIC/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:132 |
| F_TEST_GAP/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/INDEX.md:133 |
| F_TEST_GAP/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/INDEX.md:134 |
| E_TRACE_BREAK/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/INDEX.md:135 |
| D_COMMENT/p1/M5a_L11.md | （无同名近似） | 问题扫描/INDEX.md:136 |
| F_TEST_GAP/p1/M5a_L11.md | （无同名近似） | 问题扫描/INDEX.md:137 |
| A_SCI_DEF/p1/M5a_L11.md | （无同名近似） | 问题扫描/INDEX.md:138 |
| I_DOC_HYGIENE/p1/M5a_L11.md | （无同名近似） | 问题扫描/INDEX.md:139 |
| D_COMMENT/p2/M5a_L11.md | （无同名近似） | 问题扫描/INDEX.md:140 |
| I_DOC_HYGIENE/p2/M5a_L11.md | （无同名近似） | 问题扫描/INDEX.md:141 |
| E_TRACE_BREAK/p1/M5a_L11.md | （无同名近似） | 问题扫描/INDEX.md:142 |
| H_NUMERIC/p1/M5a_L11.md | （无同名近似） | 问题扫描/INDEX.md:143 |
| F_TEST_GAP/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/INDEX.md:144 |
| H_NUMERIC/p0/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:145 |
| E_TRACE_BREAK/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:146 |
| G_GOV_GATE/p0/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:147 |
| E_TRACE_BREAK/p1/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:148 |
| D_COMMENT/p2/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:149 |
| I_DOC_HYGIENE/p2/M3b_L06.md | （无同名近似） | 问题扫描/INDEX.md:150 |
| G_GOV_GATE/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/INDEX.md:151 |
| E_TRACE_BREAK/p0/M6b_L16_L18.md | （无同名近似） | 问题扫描/INDEX.md:152 |
| C_DOC_CODE_GAP/p0/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:153 |
| C_DOC_CODE_GAP/p0/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:154 |
| C_DOC_CODE_GAP/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:155 |
| I_DOC_HYGIENE/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/INDEX.md:156 |
| E_TRACE_BREAK/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:157 |
| E_TRACE_BREAK/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:158 |
| B_STD_MISMATCH/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:159 |
| A_SCI_DEF/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:160 |
| E_TRACE_BREAK/p0/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:161 |
| E_TRACE_BREAK/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:162 |
| F_TEST_GAP/p0/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:163 |
| G_GOV_GATE/p0/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:164 |
| G_GOV_GATE/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:165 |
| F_TEST_GAP/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/INDEX.md:166 |
| F_TEST_GAP/p0/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:167 |
| G_GOV_GATE/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/INDEX.md:168 |
| F_TEST_GAP/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/INDEX.md:169 |
| E_TRACE_BREAK/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:170 |
| H_NUMERIC/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:171 |
| H_NUMERIC/p2/M2b_L03_L15.md | （无同名近似） | 问题扫描/INDEX.md:172 |
| _merge/ANCHOR_VERIFY_REPORT.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:154 |
| _cache/L01.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:155 |
| _merge/M1a.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:155 |
| findings/B_STD_MISMATCH/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:155 |
| findings/E_TRACE_BREAK/p0/M6b_L16_L18.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:188 |
| docs/spec/PHASE1_PIPELINE_REDESIGN_SPEC.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:189 |
| Makefile/build.ps1 | build.sh ci/steps/linux_build_root_graph.sh docs/architecture/BUILD_GRAPH.md | 问题扫描/_cache/L01.md:21 |
| docs/05_STAR_DETECT_PSF_DEDUP_SPEC.md | （无同名近似） | 问题扫描/_cache/L01.md:253 |
| D.spherical-projection/D.h | （无同名近似） | 问题扫描/_cache/L02.md:6 |
| 83/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_cache/L02.md:57 |
| schemas/phase3_request_v1.schema.json | ci/checks.schema.json ci/ci_result.schema.json contracts/config/cli_modules_list.schema.json | 问题扫描/_cache/L02.md:228 |
| src/module_entry.cpp | lib/gaia_xpsd_client/src/module_entry.c | 问题扫描/_cache/L03.md:22 |
| CMakeLists.txt/tests/unit/CMakeLists.txt | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/archive/ACR_FOCUSED_CONTROL_PACKAGE_V4/examples/weighted_integration/CMakeLists.reference.txt | 问题扫描/_cache/L04.md:69 |
| test_drizzle_oracle.py/parallel.py | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/08_CPU_PARALLEL_BACKEND.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/agent/PARALLELIZATION_AND_DEPENDENCY_RULES.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/docs/19_ROADMAP_GATES_AND_PARALLEL_PLAN.md | 问题扫描/_cache/L04.md:237 |
| descriptor/integration.json | docs/algorithms/INTEGRATION_ALGORITHMS.md docs/algorithms/PHASE2_INTEGRATION.md docs/science/INTEGRATION.md | 问题扫描/_cache/L05.md:192 |
| lib/phase1/stars/star_measure_io.cpp | （无同名近似） | 问题扫描/_cache/L06.md:16 |
| _cache/L06.md | （无同名近似） | 问题扫描/_cache/L06.md:400 |
| lib/photometric_calib/flux_calibrator/python/star_matcher.py | lib/photometric_calib/cpp/src/star_matcher.cpp lib/photometric_calib/cpp/src/star_matcher.h | 问题扫描/_cache/L07.md:268 |
| README/module.yaml/memory.md | cli/memory_growth.h cli/memory_report.h docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md | 问题扫描/_cache/L08.md:9 |
| docs/interfaces/data/DATA-001_ARTIFACT_CONTRACT.md | Testing/Temporary/CTestCostData.txt docs/architecture/DATA_FLOW.md docs/archive/history/v19/DATA_CONTRACTS.md | 问题扫描/_cache/L10.md:187 |
| contracts/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_cache/L10.md:432 |
| cmake/astrocs.product.windows.json | cmake/astrocs.product.windows.json.in packaging/astrocs.product.json packaging/schemas/astrocs-product.schema.json | 问题扫描/_cache/L12.md:10 |
| lib/orchestrator/cpp/CMakeLists.txt | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/archive/ACR_FOCUSED_CONTROL_PACKAGE_V4/examples/weighted_integration/CMakeLists.reference.txt | 问题扫描/_cache/L12.md:54 |
| tests/abi/run_abi_checks.sh | （无同名近似） | 问题扫描/_cache/L12.md:167 |
| tools/check_cli_protocol.py | （无同名近似） | 问题扫描/_cache/L12.md:194 |
| docs/owner/PHASE_OVERVIEW.md | （无同名近似） | 问题扫描/_cache/L17.md:11 |
| module_version/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_cache/L17.md:96 |
| docs/architecture/cpu/ARCH_CONTRACTS.md | （无同名近似） | 问题扫描/_cache/L18.md:31 |
| docs/TRACEABILITY_family.json | （无同名近似） | 问题扫描/_cache/L18.md:73 |
| modules/services/io/include/astrocs/io/io_module_api_v1.h | （无同名近似） | 问题扫描/_cache/L18.md:216 |
| docs/history/README.md | VISUAL_CHECK_README.md docs/README-DOCS.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/templates/MODULE_README.md | 问题扫描/_cache/L18.md:229 |
| docs/history/v19/ARCHITECTURE.md | docs/archive/review/ARCHITECTURE_OVERVIEW.md docs/owner/ARCHITECTURE_OVERVIEW.md docs/review/ARCHITECTURE_OVERVIEW.md | 问题扫描/_cache/L18.md:230 |
| docs/architecture/CPU_ADAPTIVE_V1.md | （无同名近似） | 问题扫描/_cache/L18.md:316 |
| www.itl.nist.gov/div898/handbook/eda/section3/eda35h3.h | （无同名近似） | 问题扫描/_cache/L20.md:18 |
| docs/interfaces/io/IO_002.md | docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md | 问题扫描/_cache/L21.md:9 |
| module.yaml/README.md/memory.md | cli/memory_growth.h cli/memory_report.h docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md | 问题扫描/_cache/L22.md:168 |
| usr/lib/python3.13/unittest/loader.py | docs/architecture/abi/ABI_003_SECURE_LOADER.md lib/acr/backends/cuda/cuda_bridge_loader.cpp lib/backend_host/backend_loader.cpp | 问题扫描/_cache/L23.md:7 |
| unittest/loader.py | docs/architecture/abi/ABI_003_SECURE_LOADER.md lib/acr/backends/cuda/cuda_bridge_loader.cpp lib/backend_host/backend_loader.cpp | 问题扫描/_cache/L23.md:87 |
| tests/cpu/baseline/run_provider_baseline_checks.py | （无同名近似） | 问题扫描/_cache/L23.md:160 |
| tests/realdata/test_realdata_index_v12.py | （无同名近似） | 问题扫描/_cache/L23.md:247 |
| _merge/M9.md | （无同名近似） | 问题扫描/_cache/L24.md:370 |
| _cache/L24.md | （无同名近似） | 问题扫描/_cache/L24.md:370 |
| gitlab.com/free-astro/siril/-/raw/master/README.md | VISUAL_CHECK_README.md docs/README-DOCS.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/templates/MODULE_README.md | 问题扫描/_cache/L25.md:10 |
| www.gnu.org/software/gsl/doc/html/gpl.h | （无同名近似） | 问题扫描/_cache/L25.md:11 |
| _cache/F00_FRONT_SPOTCHECKS.md | （无同名近似） | 问题扫描/_cache/L26.md:12 |
| docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md | （无同名近似） | 问题扫描/_cache/L27.md:155 |
| docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md | （无同名近似） | 问题扫描/_cache/L27.md:156 |
| docs/superpowers/plans/2026-07-13-cpp-qt-browser.md | lib/astro_image_io/tests/test_wph_cli_browser.cpp lib/healpix_db/healpix_browser_qt/app/browser_cli.cpp lib/healpix_db/healpix_browser_qt/core/browser_backend.cpp | 问题扫描/_cache/L27.md:157 |
| tasks/SCI-F3-001.md | ci/tests/__pycache__/test_ci001_failclosed.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_ci001_failclosed.cpython-313.pyc ci/tests/test_ci001_failclosed.py | 问题扫描/_cache/L27.md:168 |
| ../10_PROTOCOL.md | （无同名近似） | 问题扫描/_cache/README.md:4 |
| ../20_AGENT_PLAN.md | （无同名近似） | 问题扫描/_cache/README.md:7 |
| ../_cache/F00_FRONT_SPOTCHECKS.md | （无同名近似） | 问题扫描/_merge/00_COORDINATION.md:4 |
| docs/TRACEABILITY.c | contracts/schemas/traceability_matrix.schema.json docs/TRACEABILITY.csv docs/traceability/TRACEABILITY_LAYERS.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:199 |
| docs/traceability/TRACEABILITY_MATRIX.c | contracts/schemas/traceability_matrix.schema.json docs/traceability/TRACEABILITY_MATRIX.csv docs/traceability/TRACEABILITY_MATRIX.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:200 |
| cmake/install_layout.c | cmake/install_layout.cmake | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:201 |
| docs/architecture/PRODUCTION_EXECUTION_INVENTORY.c | docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv tools/arch/__pycache__/build_production_execution_inventory.cpython-313.pyc tools/arch/build_production_execution_inventory.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:202 |
| docs/contracts/API_CONTRACTS.c | docs/contracts/API_CONTRACTS.csv tools/quality/contracts/check_api_contracts.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:203 |
| tools/quality/contracts/fixtures/check_api_contracts/invalid_missing_symbol.c | tools/quality/contracts/fixtures/check_api_contracts/invalid_missing_symbol.csv tools/quality/contracts/fixtures/check_doc_symbols/invalid_missing_symbol.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:204 |
| docs/architecture/api_inventory.c | docs/architecture/api_inventory.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:205 |
| cmake/cfitsio_sources.c | cmake/cfitsio_sources.cmake | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:206 |
| docs/traceability/TRACEABILITY_LAYERS.c | docs/traceability/TRACEABILITY_LAYERS.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:207 |
| docs/audit/doc_classification.c | docs/audit/doc_classification.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:208 |
| docs/audit/inventory.c | ci/INVENTORY_REPORT.md docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv docs/architecture/api_inventory.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:209 |
| docs/audit/risk_verification_T012.c | docs/audit/risk_verification_T012.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:210 |
| contracts/API_CONTRACTS.c | docs/contracts/API_CONTRACTS.csv tools/quality/contracts/check_api_contracts.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:211 |
| tests/test_index.c | tests/realdata/__pycache__/test_index_v12.cpython-313-pytest-8.3.5.pyc tests/realdata/test_index_v12.py tests/test_index.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:212 |
| _cache/L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:213 |
| lib/phase1/ups/build_upm.cpp | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:214 |
| contracts/schemas/phase3_request_v1.schema.json | ci/checks.schema.json ci/ci_result.schema.json contracts/config/cli_modules_list.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:215 |
| aah3860/aah3860.right.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:216 |
| p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:217 |
| p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:218 |
| docs.astropy.org/en/stable/api/astropy.modeling.functional_models.Moffat2D.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:219 |
| 13/TROUBLESHOOTING.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:220 |
| _cache/L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:221 |
| M5a-G-001/002/003/005.md | astrocs_run_7d4cd130056e.json engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/tasks/P10-005.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/tasks/P11-005.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:222 |
| M5a-G-004/006/007/008/009/010.md | lib/acr/docs/ADR-010-delete-per-kernel-routing.md testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@064010-180S-Green.fts testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@010214-600S-Oiii.fts | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:223 |
| M5a_D-001/002.md | docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/evidence/P12-002/raw_logs/test_photometric_calib_p12_002.log | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:224 |
| M5a_I-001/002.md | docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/evidence/P12-002/raw_logs/test_photometric_calib_p12_002.log | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:225 |
| _cache/L12.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:226 |
| _cache/L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:227 |
| tests/unit/p1_phot_test.cpp | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:228 |
| _cache/L13.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:229 |
| findings/G_GOV_GATE/p0/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:230 |
| findings/D_COMMENT/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:231 |
| findings/D_COMMENT/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:232 |
| ../../10_PROTOCOL.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:233 |
| ../../_merge/M3.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:234 |
| README/头文件/integration.json | docs/algorithms/INTEGRATION_ALGORITHMS.md docs/algorithms/PHASE2_INTEGRATION.md docs/science/INTEGRATION.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:235 |
| lib/core/src/astrocs_phase2_writer.cpp | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:236 |
| ALG/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:237 |
| ALG/DATA/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:238 |
| 11.1/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:239 |
| docs/architecture/abi/ABI_002_MODULE_ABI.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:240 |
| schemas/phase3_config_v1.schema.json | ci/checks.schema.json ci/ci_result.schema.json contracts/config/cli_modules_list.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:241 |
| docs/standards/DIAGNOSTICS_STANDARD.md | docs/standards/LOGGING_DIAGNOSTICS_STANDARD.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:242 |
| tests/unit/gaia_xpsd_fixture_sp_gen.c | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:243 |
| tools/quality/contracts/fixtures/check_traceability/valid_min.c | tools/quality/contracts/fixtures/check_api_contracts/valid_min.csv tools/quality/contracts/fixtures/check_build_graph/valid_min.json tools/quality/contracts/fixtures/check_comments/valid_min.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:244 |
| docs/science/UNCERTAINTY.md | contracts/data/phase2_uncertainty_rejection_provenance_v1.json docs/science/UNCERTAINTY_AND_COVARIANCE.md tests/unit/p3002_uncertainty_test.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:245 |
| .github/workflows/build.yml | build.sh ci/steps/linux_build_root_graph.sh docs/architecture/BUILD_GRAPH.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:246 |
| tools/check_domain_interfaces.py | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:247 |
| lib/astro_image_io/src/astro_image_io.cpp | docs/modules/astro_image_io.md lib/astro_image_io/astro_image_io.dll lib/astro_image_io/include/astro_image_io.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:248 |
| docs/contracts/SCIENCE_PROFILES.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:249 |
| docs/traceability/TRACEABILITY_MATRIX.md | contracts/schemas/traceability_matrix.schema.json docs/traceability/TRACEABILITY_MATRIX.csv docs/traceability/TRACEABILITY_MATRIX.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:250 |
| counters/p1_stack.json | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:251 |
| docs/RELEASE_AUDIT_2026-09-05.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:252 |
| docs/standards/SCIENCE_CONTRACT_PROCESS.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:253 |
| p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:254 |
| findings/A_SCI_DEF/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:255 |
| findings/A_SCI_DEF/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:256 |
| findings/H_NUMERIC/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:257 |
| findings/H_NUMERIC/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:258 |
| findings/C_DOC_CODE_GAP/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:259 |
| findings/C_DOC_CODE_GAP/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:260 |
| findings/F_TEST_GAP/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:261 |
| findings/G_GOV_GATE/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:262 |
| findings/E_TRACE_BREAK/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:263 |
| findings/D_COMMENT/p2/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:264 |
| findings/I_DOC_HYGIENE/p2/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:265 |
| findings/G_GOV_GATE/p0/M5a_L11_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:266 |
| findings/G_GOV_GATE/p0/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:267 |
| findings/G_GOV_GATE/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:268 |
| findings/C_DOC_CODE_GAP/p0/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:269 |
| findings/C_DOC_CODE_GAP/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:270 |
| findings/E_TRACE_BREAK/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:271 |
| findings/E_TRACE_BREAK/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:272 |
| findings/I_DOC_HYGIENE/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:273 |
| findings/I_DOC_HYGIENE/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:274 |
| findings/F_TEST_GAP/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:275 |
| findings/F_TEST_GAP/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:276 |
| _cache/L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:277 |
| _cache/L21.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:278 |
| _cache/L22.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:279 |
| findings/B_STD_MISMATCH/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:280 |
| findings/F_TEST_GAP/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:281 |
| ../../../_merge/M5a.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:282 |
| _merge/M4.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:283 |
| _merge/M6b.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:284 |
| _merge/M2a.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:285 |
| docs/architecture/ARCH_CONTRACTS.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:286 |
| docs/validation/ACCEPTANCE_GATES.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:287 |
| include/astrocs/io/io_module_api_v1.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:288 |
| _merge/M5b.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:289 |
| _merge/M5a.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:290 |
| _merge/M3b.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:291 |
| modules/services/io/src/hips_input_v1.c | modules/services/io/include/astrocs/io/hips_input_v1.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:292 |
| lib/phase3_session/hips_reader_p3.c | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:293 |
| www.ivoa.net/documents/MOC/20220727/REC-MOC-2.0-20220727.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:294 |
| _cache/L05.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:295 |
| _cache/L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:296 |
| module.yaml/types.h/integration.json | docs/algorithms/INTEGRATION_ALGORITHMS.md docs/algorithms/PHASE2_INTEGRATION.md docs/science/INTEGRATION.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:297 |
| contracts/schemas/test_ids.schema.json | ci/checks.schema.json ci/ci_result.schema.json contracts/config/cli_modules_list.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:298 |
| docs/TRACEABILITY.md | contracts/schemas/traceability_matrix.schema.json docs/TRACEABILITY.csv docs/traceability/TRACEABILITY_LAYERS.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:299 |
| docs/README.md | VISUAL_CHECK_README.md docs/README-DOCS.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/templates/MODULE_README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:300 |
| _merge/M7.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:301 |
| p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:302 |
| src/core/psfmatching.c | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:303 |
| findings/E_TRACE_BREAK/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:304 |
| findings/E_TRACE_BREAK/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:305 |
| findings/G_GOV_GATE/p0/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:306 |
| findings/G_GOV_GATE/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:307 |
| findings/G_GOV_GATE/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:308 |
| findings/I_DOC_HYGIENE/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:309 |
| findings/I_DOC_HYGIENE/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:310 |
| findings/C_DOC_CODE_GAP/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:311 |
| findings/F_TEST_GAP/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:312 |
| _cache/L16.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:313 |
| _cache/L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:314 |
| README/module.yaml/CMakeLists/io_module_api_v1.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:315 |
| lib/astro_image_io/CMakeLists.txt | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/archive/ACR_FOCUSED_CONTROL_PACKAGE_V4/examples/weighted_integration/CMakeLists.reference.txt | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:316 |
| findings/G_GOV_GATE/p1/M7_G_GOV_GATE_p1.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:317 |
| docs/standards/SCIENTIFIC_CONSTANTS.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:318 |
| _cache/L20.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:319 |
| _cache/L19.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:320 |
| docs.astropy.org/en/stable/wcs/sip.h | lib/healpix_db/healpix_drizzle/tests/mini_sip1000.cpp lib/healpix_db/healpix_drizzle/tests/p0_sip_order_guard_test.cpp lib/healpix_db/healpix_drizzle/wcs_sip.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:321 |
| _cache/L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:322 |
| findings/F_TEST_GAP/p0/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:323 |
| findings/F_TEST_GAP/p1/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:324 |
| findings/F_TEST_GAP/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:325 |
| findings/G_GOV_GATE/p1/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:326 |
| findings/C_DOC_CODE_GAP/p1/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:327 |
| findings/C_DOC_CODE_GAP/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:328 |
| findings/B_STD_MISMATCH/p1/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:329 |
| findings/H_NUMERIC/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:330 |
| findings/I_DOC_HYGIENE/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:331 |
| findings/E_TRACE_BREAK/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:332 |
| findings/A_SCI_DEF/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:333 |
| module.yaml/memory.md/README.md | VISUAL_CHECK_README.md docs/README-DOCS.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/templates/MODULE_README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:334 |
| _cache/L25.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:335 |
| _cache/L27.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:336 |
| _cache/L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:337 |
| _merge/M2b.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:338 |
| findings/G_GOV_GATE/p0/M8a_L25_L27.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:339 |
| LICENSES/NOTICE.txt | engineering/control/archive/2026-09-09_superseded_V8.1_CI_CONTROL_20260905/15_SUPERSESSION_NOTICE.md lib/common/healpix/THIRD_PARTY_NOTICE.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:340 |
| _cache/L26.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:341 |
| _merge/M8.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:342 |
| findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:343 |
| docs/DATA_STRUCTURED_CONTRACT_TOOLING.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:344 |
| findings/G_GOV_GATE/p0/M7_G_GOV_GATE_p0.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:345 |
| findings/A_SCI_DEF/p0/M7_A_SCI_DEF_p0.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:346 |
| findings/F_TEST_GAP/p1/M7_F_TEST_GAP_p1.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:347 |
| tools/monitoring/run_cpu_baseline_checks.py | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:348 |
| _merge/M8a.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:350 |
| lib/hips_p2/src/hips_p2_io.cpp | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:351 |
| tools/orchestrator/run_evidence_orchestrator.ps1 | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:352 |
| lib/cosmetic/src/real_ingest.cpp | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:353 |
| module.yaml/types.h | contracts/data/artifact_types.registry.json lib/plate_solve/cpp/ipv/include/ipv_types.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:354 |
| docs/pipeline/integration.json | docs/algorithms/INTEGRATION_ALGORITHMS.md docs/algorithms/PHASE2_INTEGRATION.md docs/science/INTEGRATION.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:355 |
| lib/hips/types.h | contracts/data/artifact_types.registry.json lib/plate_solve/cpp/ipv/include/ipv_types.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:356 |
| lib/photometric_calib/cpp/include/PhotometryCalculator.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:357 |
| docs/science/SNR_SCIENCE_DERIVATION.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:358 |
| tasks/02_ABI_BUILD_CLI_TASKS.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:359 |
| ../../lib/snr_estimator/tests/p1noise/CMakeLists.txt | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/archive/ACR_FOCUSED_CONTROL_PACKAGE_V4/examples/weighted_integration/CMakeLists.reference.txt | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:360 |
| tools/redteam_v19r3_sanitizer.sh | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:361 |
| _merge/M3.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:362 |
| lib/photometric_calib/src/photometry.c | docs/modules/registry/astrocs.phase1.photometry.md docs/science/PHOTOMETRY.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/checklists/G11_WCS_AND_PHOTOMETRY.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:363 |
| usr/include/gsl/gsl_multifit_nlinear.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:364 |
| M2a/D.c | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:365 |
| D.fits/D.c | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:366 |
| p0/M8a_L25_L27.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:367 |
| tools/quality/check_third_party_registry.py | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:368 |
| docs/modules/cosmetic.md | docs/algorithms/COSMETIC_ALGORITHMS.md docs/modules/registry/astrocs.phase1.cosmetic.md lib/calibration/cpp/cosmetic_corrector.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:369 |
| docs/science/PHASE1_API.md | docs/api/PHASE1_API_V1.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:370 |
| docs/TRACEABILITY_FAMILY.csv | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:371 |
| engineering_authoritative/docs/04_RESOURCE_AWARE_ORCHESTRATOR_SPEC.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:372 |
| docs/contracts/TRACEABILITY.csv | contracts/schemas/traceability_matrix.schema.json docs/traceability/TRACEABILITY_LAYERS.csv docs/traceability/TRACEABILITY_MATRIX.csv | 问题扫描/_merge/M7.md:261 |

## 三、符号在被引文件 0 命中、但在其他真源文件存在（31 种）→ 补文件锚点即可，禁止据此撤条
| 引用 | 实际所在 | 首次出现 |
|---|---|---|
| docs/science/CALIBRATION.md::min_samples | docs/API_REFERENCE.md docs/algorithms/NOISE_ESTIMATION.md docs/algorithms/PHASE2_REJECTION.md | 问题扫描/40_OWNER_DECISIONS.md:12 |
| docs/contracts/TEST_MATRIX.md::Drizzle | ASTROCS_PROJECT_CONSTITUTION.md CHANGELOG.md HANDOVER.md | 问题扫描/_cache/L23.md:137 |
| lib/astro_image_io/src/hips/aio_hips_reader.cpp::read_hips_tile | tests/unit/p1001_real_nodes_test.cpp | 问题扫描/_cache/L24.md:357 |
| tools/gen_v19_evidence.py::SKIP_PARTS | tools/gen_v19_source_snapshot.py tools/quality/build_v19r2_package.py tools/quality/build_v19r3_package.py | 问题扫描/_cache/L25.md:87 |
| lib/orchestrator/cpp/include/json_config.h::DrizzleConfig | docs/API_REFERENCE.md docs/ARCHITECTURE.md docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:401 |
| lib/gaia_xpsd_client/src/gaia_client.c::record | CMakeLists.txt ci/ctest_baseline.json ci/tests/test_ci001b_binding.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:403 |
| lib/gaia_xpsd_client/include/astrocs/gaia/types.h::GaiaStar | docs/API_REFERENCE.md docs/contracts/PUBLIC_API.md lib/gaia_xpsd_client/README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:407 |
| lib/phase2/src/rejection.cpp::gather | HANDOVER.md contracts/data/phase2_uncertainty_rejection_provenance_v1.json docs/algorithms/PHASE2_INTEGRATION.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:408 |
| tests/backend/test_p3_projection_oracle.py::car_ | lib/phase3_proj/p3_projection.cpp tests/artifact/test_production_store.py tests/artifact/test_provenance.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:412 |
| tools/quality/check_serial_heavy.py::docstring | docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md runtime/artifact_store/provenance.py tests/backend/test_p2007_joint_gate.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:414 |
| tools/check_module_readmes.py::docstring | docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md runtime/artifact_store/provenance.py tests/backend/test_p2007_joint_gate.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:416 |
| tests/test_index.csv::SCI | ASTROCS_PROJECT_CONSTITUTION.md AstroCS_ENGINEERING_CONSTRAINTS.md CHANGELOG.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:417 |
| docs/contracts/RT-001.md::DOC | .github/workflows/ci-windows.yml AstroCS_ENGINEERING_CONSTRAINTS.md CHANGELOG.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:418 |
| docs/contracts/INDEX.yaml::entries | ci/actions.lock.json ci/reconcile_state.py ci/run.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:420 |
| photometric_calib.h::PhotometricCalibration | CHANGELOG.md docs/API_REFERENCE.md docs/ARCHITECTURE.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:422 |
| tests/unit/p3002_real_nodes_test.cpp::write_props | tests/backend/test_hips_properties.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:430 |
| lib/cosmetic/src/module_entry.cpp::calibrate_frame | docs/API_REFERENCE.md docs/ARCHITECTURE.md docs/algorithms/CALIBRATION_ALGORITHMS.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:431 |
| lib/photometric_calib/cpp/include/photometric_calib.h::PhotometricCalibration | CHANGELOG.md docs/API_REFERENCE.md docs/ARCHITECTURE.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:432 |
| lib/astro_image_io/src/hips/aio_hips_reader.cpp::manifest | ASTROCS_PROJECT_CONSTITUTION.md AstroCS_ENGINEERING_CONSTRAINTS.md CMakeLists.txt | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:433 |
| lib/healpix_db/healpix_drizzle/drizzle_engine.cpp::validate | .github/workflows/ci-linux.yml .github/workflows/ci-windows.yml .github/workflows/fatduck.yml | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:434 |
| docs/architecture/PUBLIC_API.md::P1CAL | lib/calibration/tests/p1cal/CMakeLists.txt lib/calibration/tests/p1cal/p1cal_fixtures.hpp lib/calibration/tests/p1cal/p1cal_oracle.hpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:435 |
| lib/drizzle/include/astrocs/drizzle/types.h::DrizzleConfig | docs/API_REFERENCE.md docs/ARCHITECTURE.md docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:436 |
| include/astrocs/common_abi_v1.h::acs_module_descriptor_v1 | docs/algorithms/CALIBRATION_ALGORITHMS.md include/astrocs/abi/module_api_v1.h lib/calibration/README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:437 |
| .github/workflows/ci.yml::V19R4 | docs/contracts/DATA_SEMANTICS.md tools/docs_machine_consistency.py tools/quality/build_v19r4_package.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:441 |
| tools/docs_machine_consistency.py::docstring | docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md runtime/artifact_store/provenance.py tests/backend/test_p2007_joint_gate.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:443 |
| tools/quality/contracts/check_traceability.py::TABLE | ci/validate_workflow_binding.py ci/wf_step.py docs/algorithms/HIPS_WRITER.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:444 |
| ci/verify_toolchain.py::docstring | docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md runtime/artifact_store/provenance.py tests/backend/test_p2007_joint_gate.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:445 |
| tools/quality/gen_module_readmes.py::docstring | docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md runtime/artifact_store/provenance.py tests/backend/test_p2007_joint_gate.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:446 |
| docs/modules/registry/astrocs.phase2.coverage.md::front | cli/commands.cpp cli/memory_growth.h cli/monitor.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:447 |
| tests/unit/master_flat_median_test.cpp::TEST_F | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/evidence/P13-001/scripts/test_stage1_batch_runner.py lib/acr/ci/check_acr_dormant.py lib/backend_host/bench_harness.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:448 |
| docs/contracts/DATA_SEMANTICS.md::electron | docs/GLOSSARY.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/03_TARGET_ARCHITECTURE.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/07_SCIENCE_AND_TEST_MATRIX.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:449 |

## 四、符号全仓 0 命中（31 种）→ 最强假阳/失效信号，逐条走整句→关键词→定点 read
| 引用 | 首次出现 |
|---|---|
| tests/io/test_fits_stream_contract.py::test_hips_rewriter_drops_bad_keyword | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:107 |
| test_monitor_contract.py::_compile_probe | 问题扫描/_cache/L23.md:29 |
| tests/backend/test_isa_avx.py::TestISA002AvxSubset | 问题扫描/_cache/L23.md:262 |
| tests/monitoring/test_monitor_contract.py::_compile_probe | 问题扫描/_cache/L23.md:372 |
| lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp::parseBlockIndex | 问题扫描/_cache/L24.md:216 |
| lib/astro_image_io/src/hips/aio_hips_reader.cpp::parse_snr_catalog | 问题扫描/_cache/L24.md:327 |
| orchestrator.cpp::compute_pixel_scale_arcsec | 问题扫描/_cache/L26.md:243 |
| lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::solve_pix2world_iterative | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:398 |
| lib/astro_image_io/src/hips/aio_hips_writer.cpp::write_tile_core | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:399 |
| lib/healpix_db/healpix_drizzle/spherical_overlap.cpp::compute_drop_corners | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:400 |
| lib/phase3_session/p3_wcs.cpp::p3_wcs_validate_descriptor | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:402 |
| runtime/io/fits_core.c::fio_write_hdu | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:404 |
| lib/gaia_xpsd_client/src/module_entry.c::build_result_json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:405 |
| lib/plate_solve/cpp/ipv/include/ipv_types.h::IPVSelectParams | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:406 |
| cli/parser.cpp::kAllowedFlags | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:409 |
| cli/commands.cpp::cmd_benchmark_cpu | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:410 |
| cli/runtime_client.cpp::exit_code_from_error | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:411 |
| tests/unit/gaia_xpsd_fixture_gen.c::xpsd_write_test_file | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:413 |
| cli/commands.cpp::cmd_bench_cpu | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:415 |
| aio_hips_writer.cpp::write_tile_core | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:419 |
| tests/unit/master_flat_median_test.cpp::RejectsNegativeMedianMaster | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:421 |
| gaia_client.c::trace_seq | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:424 |
| lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::build_fits_wcs_from_solution | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:425 |
| lib/healpix_db/healpix_drizzle/healpix_core.h::HealpixGrid | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:426 |
| lib/phase2/src/stage2_common.cpp::p2_stage2_config_from_json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:427 |
| packaging/astrocs.product.json::required_units | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:428 |
| lib/phase3_session/hips_properties.cpp::hips_properties_parse_file | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:429 |
| ipv_wcs.cpp::build_fits_wcs_from_solution | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:438 |
| lib/astro_image_io/tests/p1hips/p1hips_tests_units.cpp::u5_snr | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:439 |
| lib/snr_estimator/cpp/test/noise_model_science_test.cpp::test_variance_field_fit_snr006 | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:440 |
| tools/traceability/check_traceability_matrix.py::_check_refs | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:442 |

## 五、行号越过文件当前末尾（7 种）→ 重取行号或改 path::符号
| 引用 | 现行数 | 首次出现 |
|---|---|---|
| modules/services/io/include/astrocs/io/fits_stream_v1.h:236 | 228 | 问题扫描/_cache/L10.md:231 |
| docs/contracts/DATA_ARTIFACTS.md:150 | 104 | 问题扫描/_cache/L10.md:247 |
| docs/science/DRIZZLE.md:186 | 161 | 问题扫描/_cache/L16.md:351 |
| docs/science/REJECTION.md:155 | 149 | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:380 |
| docs/development/CONFIG_SCHEMA.md:264 | 86 | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:381 |
| tools/README.md:95 | 84 | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:382 |
| docs/modules/photometric_calib.md:117 | 63 | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:383 |
