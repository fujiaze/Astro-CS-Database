# 锚点机械核验报告 v3

- 生成器：_tools/verify_anchors.js（v3）；复跑 node 问题扫描/_tools/verify_anchors.js
- 本次扫描 297 份审计 md、12254 次引用：精确在位 8720 次；分五类，处置强度不同
- **v3 判据（由 M7 自我否证确立）**：判「路径缺失」**必须以存在性检索为准**；某个字面串 0 命中只说明**用词不同**，不是文件不存在。
  因此：一、PATH_MISMATCH **不得撤条**，只改路径写法；三、SYMBOL_ELSEWHERE 多半是「定义在他处/字段名不同」，**先补文件再定档**；只有二、ABSENT 与四、SYMBOL_ABSENT 才走撤条或重锚。

## 一、路径写错但同名文件唯一存在（340 种）→ 只改锚，不撤证据
| 档案里的写法 | 应改为 | 首次出现 |
|---|---|---|
| ahpx/DEPRECATED.md | lib/astro_image_io/src/ahpx/DEPRECATED.md | 问题扫描/40_OWNER_DECISIONS.md:67 |
| 基线/checks.json | ci/checks.json | 问题扫描/40_OWNER_DECISIONS.md:79 |
| TU/checks.json | ci/checks.json | 问题扫描/_cache/E1.md:1 |
| include/astrocs/noise/types.h | lib/snr_estimator/include/astrocs/noise/types.h | 问题扫描/_cache/E1.md:24 |
| p1noise/CMakeLists.txt | lib/snr_estimator/tests/p1noise/CMakeLists.txt | 问题扫描/_cache/E1.md:25 |
| .../src/aio_pipeline.cpp | lib/astro_image_io/src/aio_pipeline.cpp | 问题扫描/_cache/E2.md:60 |
| .../src/aio_pipeline_engine.cpp | lib/astro_image_io/src/aio_pipeline_engine.cpp | 问题扫描/_cache/E2.md:60 |
| lib/gaia_xpsd_client/CMakeFiles/astrocs_catalog_gaia.dir/src/module_entry.c | lib/gaia_xpsd_client/src/module_entry.c | 问题扫描/_cache/E3.md:20 |
| cli/browser_cli.cpp | lib/healpix_db/healpix_browser_qt/app/browser_cli.cpp | 问题扫描/_cache/E4.md:68 |
| cli/hardware_inspect.cpp | lib/backend_host/hardware_inspect.cpp | 问题扫描/_cache/E4.md:68 |
| tools/quality/check_api_docs.py | tools/check_api_docs.py | 问题扫描/_cache/E4.md:164 |
| healpix_drizzle/fits_reader.cpp | lib/healpix_db/healpix_drizzle/fits_reader.cpp | 问题扫描/_cache/E4.md:244 |
| ahpx/aio_ahpx_reader.cpp | lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp | 问题扫描/_cache/E4.md:244 |
| healpix/aio_healpix_io.cpp | lib/astro_image_io/src/healpix/aio_healpix_io.cpp | 问题扫描/_cache/E4.md:244 |
| acr/backends/cuda/cuda_bridge_loader.cpp | lib/acr/backends/cuda/cuda_bridge_loader.cpp | 问题扫描/_cache/E4.md:244 |
| diagnostics/TROUBLESHOOTING.md | docs/diagnostics/TROUBLESHOOTING.md | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:17 |
| docs/science/PHASE2_SAMPLER.md | docs/algorithms/PHASE2_SAMPLER.md | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:209 |
| docs/THIRD_PARTY_NOTICE.md | lib/common/healpix/THIRD_PARTY_NOTICE.md | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:230 |
| FINAL3/templates/AGENTS.md | AGENTS.md | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:349 |
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
| tests/unit/noise_model_science_test.cpp | lib/snr_estimator/cpp/test/noise_model_science_test.cpp | 问题扫描/_cache/L23.md:137 |
| memory.md/README/test_report.md | lib/astro_image_io/tests/test_report.md | 问题扫描/_cache/L23.md:341 |
| include/aio_hips_reader.h | lib/astro_image_io/include/aio_hips_reader.h | 问题扫描/_cache/L24.md:170 |
| contracts/API_CONTRACTS.csv | docs/contracts/API_CONTRACTS.csv | 问题扫描/_cache/L24.md:182 |
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
| aio_hips.h/aio_healpix_io.h/hiss_format.h/aio_ahpx_format.h/aio_hips_reader.h | lib/astro_image_io/include/aio_hips_reader.h | 问题扫描/_cache/L28b.md:70 |
| phase2/upm.h | lib/phase2/include/astro/phase2/upm.h | 问题扫描/_cache/L28b.md:165 |
| lib/.../docs/algorithm.md | lib/photometric_calib/docs/algorithm.md | 问题扫描/_cache/L28c.md:59 |
| p3_resample.h/p3_output.h/p3_projection.h | lib/phase3_proj/p3_projection.h | 问题扫描/_cache/L28c.md:99 |
| providers/cpu/baseline/include/.../baseline_provider_v1.h | providers/cpu/baseline/include/astrocs/cpu/baseline_provider_v1.h | 问题扫描/_cache/L28c.md:109 |
| healpix_drizzle/healpix_core.h | lib/healpix_db/healpix_drizzle/healpix_core.h | 问题扫描/_cache/L28c.md:164 |
| ahpx/aio_ahpx_api.cpp | lib/astro_image_io/src/ahpx/aio_ahpx_api.cpp | 问题扫描/_cache/L28d.md:52 |
| astro_image_io/src/aio_abi.cpp | lib/astro_image_io/src/aio_abi.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/aio_api.cpp | lib/astro_image_io/src/aio_api.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/aio_compressor.cpp | lib/astro_image_io/src/aio_compressor.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/aio_fits.cpp | lib/astro_image_io/src/aio_fits.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/aio_log.cpp | lib/astro_image_io/src/aio_log.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/aio_pipeline.cpp | lib/astro_image_io/src/aio_pipeline.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/aio_upm.cpp | lib/astro_image_io/src/aio_upm.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/aio_xisf.cpp | lib/astro_image_io/src/aio_xisf.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/healpix/aio_healpix_io.cpp | lib/astro_image_io/src/healpix/aio_healpix_io.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/hips/aio_hips_reader.cpp | lib/astro_image_io/src/hips/aio_hips_reader.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/hips/aio_hips_writer.cpp | lib/astro_image_io/src/hips/aio_hips_writer.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/hiss_codec.cpp | lib/astro_image_io/src/hiss_codec.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/hiss_common.cpp | lib/astro_image_io/src/hiss_common.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/hiss_reader.cpp | lib/astro_image_io/src/hiss_reader.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/hiss_stream_writer.cpp | lib/astro_image_io/src/hiss_stream_writer.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/hiss_tile_model.cpp | lib/astro_image_io/src/hiss_tile_model.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/hiss_transform.cpp | lib/astro_image_io/src/hiss_transform.cpp | 问题扫描/_cache/L28d.md:172 |
| astro_image_io/src/hiss_writer.cpp | lib/astro_image_io/src/hiss_writer.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/avx2_backend.cpp | lib/backend_host/avx2_backend.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/avx512_backend.cpp | lib/backend_host/avx512_backend.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/backend_loader.cpp | lib/backend_host/backend_loader.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/baseline_backend.cpp | lib/backend_host/baseline_backend.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/bench_harness.cpp | lib/backend_host/bench_harness.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/bench_report.cpp | lib/backend_host/bench_report.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/cpu_features.cpp | lib/backend_host/cpu_features.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/cpu_routing.cpp | lib/backend_host/cpu_routing.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/hardware_inspect.cpp | lib/backend_host/hardware_inspect.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/host_services.cpp | lib/backend_host/host_services.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/profile_gen.cpp | lib/backend_host/profile_gen.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/profile_gen_v2.cpp | lib/backend_host/profile_gen_v2.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/profile_store.cpp | lib/backend_host/profile_store.cpp | 问题扫描/_cache/L28d.md:172 |
| backend_host/worker_advisor.cpp | lib/backend_host/worker_advisor.cpp | 问题扫描/_cache/L28d.md:172 |
| calibration/src/ac_api.cpp | lib/calibration/src/ac_api.cpp | 问题扫描/_cache/L28d.md:172 |
| calibration/src/calibrator.cpp | lib/calibration/src/calibrator.cpp | 问题扫描/_cache/L28d.md:172 |
| calibration/src/cosmetic_corrector.cpp | lib/calibration/src/cosmetic_corrector.cpp | 问题扫描/_cache/L28d.md:172 |
| calibration/src/master_generator.cpp | lib/calibration/src/master_generator.cpp | 问题扫描/_cache/L28d.md:172 |
| calibration/src/module_entry.cpp | lib/calibration/src/module_entry.cpp | 问题扫描/_cache/L28d.md:172 |
| common/crypto/sha256.cpp | lib/common/crypto/sha256.cpp | 问题扫描/_cache/L28d.md:172 |
| common/healpix/healpix_core.cpp | lib/common/healpix/healpix_core.cpp | 问题扫描/_cache/L28d.md:172 |
| core/src/artifact.cpp | lib/core/src/artifact.cpp | 问题扫描/_cache/L28d.md:172 |
| core/src/artifact_store.cpp | lib/core/src/artifact_store.cpp | 问题扫描/_cache/L28d.md:172 |
| core/src/checkpoint.cpp | lib/core/src/checkpoint.cpp | 问题扫描/_cache/L28d.md:172 |
| core/src/context.cpp | lib/core/src/context.cpp | 问题扫描/_cache/L28d.md:172 |
| core/src/logging.cpp | lib/core/src/logging.cpp | 问题扫描/_cache/L28d.md:172 |
| core/src/module.cpp | lib/core/src/module.cpp | 问题扫描/_cache/L28d.md:172 |
| core/src/module_adapters.cpp | lib/core/src/module_adapters.cpp | 问题扫描/_cache/L28d.md:172 |
| core/src/pipeline.cpp | lib/core/src/pipeline.cpp | 问题扫描/_cache/L28d.md:172 |
| core/src/runtime.cpp | lib/core/src/runtime.cpp | 问题扫描/_cache/L28d.md:172 |
| core/src/scheduler.cpp | lib/core/src/scheduler.cpp | 问题扫描/_cache/L28d.md:172 |
| cosmetic/src/module_entry.cpp | lib/cosmetic/src/module_entry.cpp | 问题扫描/_cache/L28d.md:172 |
| drizzle/src/module_entry.cpp | lib/drizzle/src/module_entry.cpp | 问题扫描/_cache/L28d.md:172 |
| dynamic_psf/src/dpsf_image.cpp | lib/dynamic_psf/src/dpsf_image.cpp | 问题扫描/_cache/L28d.md:172 |
| dynamic_psf/src/dpsf_log.cpp | lib/dynamic_psf/src/dpsf_log.cpp | 问题扫描/_cache/L28d.md:172 |
| dynamic_psf/src/dpsf_psf.cpp | lib/dynamic_psf/src/dpsf_psf.cpp | 问题扫描/_cache/L28d.md:172 |
| gaia_xpsd_client/src/gaia_client.c | lib/gaia_xpsd_client/src/gaia_client.c | 问题扫描/_cache/L28d.md:172 |
| gaia_xpsd_client/src/module_entry.c | lib/gaia_xpsd_client/src/module_entry.c | 问题扫描/_cache/L28d.md:172 |
| healpix_db/healpix_drizzle/astro_sphere_sink.cpp | lib/healpix_db/healpix_drizzle/astro_sphere_sink.cpp | 问题扫描/_cache/L28d.md:172 |
| healpix_db/healpix_drizzle/drizzle_engine.cpp | lib/healpix_db/healpix_drizzle/drizzle_engine.cpp | 问题扫描/_cache/L28d.md:172 |
| healpix_db/healpix_drizzle/fits_reader.cpp | lib/healpix_db/healpix_drizzle/fits_reader.cpp | 问题扫描/_cache/L28d.md:172 |
| healpix_db/healpix_drizzle/hp_drizzle_api.cpp | lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp | 问题扫描/_cache/L28d.md:172 |
| healpix_db/healpix_drizzle/hp_drizzle_hips_api.cpp | lib/healpix_db/healpix_drizzle/hp_drizzle_hips_api.cpp | 问题扫描/_cache/L28d.md:172 |
| healpix_db/healpix_drizzle/poly_clip.cpp | lib/healpix_db/healpix_drizzle/poly_clip.cpp | 问题扫描/_cache/L28d.md:172 |
| healpix_db/healpix_drizzle/reverse_drizzle.cpp | lib/healpix_db/healpix_drizzle/reverse_drizzle.cpp | 问题扫描/_cache/L28d.md:172 |
| healpix_db/healpix_drizzle/snr_evaluator.cpp | lib/healpix_db/healpix_drizzle/snr_evaluator.cpp | 问题扫描/_cache/L28d.md:172 |
| healpix_db/healpix_drizzle/spherical_overlap.cpp | lib/healpix_db/healpix_drizzle/spherical_overlap.cpp | 问题扫描/_cache/L28d.md:172 |
| healpix_db/healpix_drizzle/wcs_sip.cpp | lib/healpix_db/healpix_drizzle/wcs_sip.cpp | 问题扫描/_cache/L28d.md:172 |
| hips/src/aio_publish.cpp | lib/hips/src/aio_publish.cpp | 问题扫描/_cache/L28d.md:172 |
| hips/src/module_entry.cpp | lib/hips/src/module_entry.cpp | 问题扫描/_cache/L28d.md:172 |
| io/src/io_adapter.cpp | lib/io/src/io_adapter.cpp | 问题扫描/_cache/L28d.md:172 |
| phase1/noise/noise_model.cpp | lib/phase1/noise/noise_model.cpp | 问题扫描/_cache/L28d.md:172 |
| phase1/photometry/photometer.cpp | lib/phase1/photometry/photometer.cpp | 问题扫描/_cache/L28d.md:172 |
| phase1/stars/star_detector.cpp | lib/phase1/stars/star_detector.cpp | 问题扫描/_cache/L28d.md:172 |
| phase1/wcs/wcs_tan.cpp | lib/phase1/wcs/wcs_tan.cpp | 问题扫描/_cache/L28d.md:172 |
| phase1_session/p1_session.cpp | lib/phase1_session/p1_session.cpp | 问题扫描/_cache/L28d.md:172 |
| phase2/src/block.cpp | lib/phase2/src/block.cpp | 问题扫描/_cache/L28d.md:172 |
| phase2/src/coverage.cpp | lib/phase2/src/coverage.cpp | 问题扫描/_cache/L28d.md:172 |
| phase2/src/cuda_bridge_stub.cpp | lib/phase2/src/cuda_bridge_stub.cpp | 问题扫描/_cache/L28d.md:172 |
| phase2/src/integrate.cpp | lib/phase2/src/integrate.cpp | 问题扫描/_cache/L28d.md:172 |
| phase2/src/rejection.cpp | lib/phase2/src/rejection.cpp | 问题扫描/_cache/L28d.md:172 |
| phase2/src/sampler.cpp | lib/phase2/src/sampler.cpp | 问题扫描/_cache/L28d.md:172 |
| phase2/src/stage2_common.cpp | lib/phase2/src/stage2_common.cpp | 问题扫描/_cache/L28d.md:172 |
| phase2/src/upm.cpp | lib/phase2/src/upm.cpp | 问题扫描/_cache/L28d.md:172 |
| phase2_session/p2_session.cpp | lib/phase2_session/p2_session.cpp | 问题扫描/_cache/L28d.md:172 |
| phase3_session/hips_properties.cpp | lib/phase3_session/hips_properties.cpp | 问题扫描/_cache/L28d.md:172 |
| phase3_session/p3_output.cpp | lib/phase3_session/p3_output.cpp | 问题扫描/_cache/L28d.md:172 |
| phase3_session/p3_session.cpp | lib/phase3_session/p3_session.cpp | 问题扫描/_cache/L28d.md:172 |
| phase3_session/p3_wcs.cpp | lib/phase3_session/p3_wcs.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_angle.cpp | lib/plate_solve/cpp/ipv/src/ipv_angle.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_distortion.cpp | lib/plate_solve/cpp/ipv/src/ipv_distortion.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_entry.cpp | lib/plate_solve/cpp/ipv/src/ipv_entry.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_itertrans.cpp | lib/plate_solve/cpp/ipv/src/ipv_itertrans.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_kvector.cpp | lib/plate_solve/cpp/ipv/src/ipv_kvector.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_polygon.cpp | lib/plate_solve/cpp/ipv/src/ipv_polygon.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_ransac.cpp | lib/plate_solve/cpp/ipv/src/ipv_ransac.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_robust_refine.cpp | lib/plate_solve/cpp/ipv/src/ipv_robust_refine.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_select.cpp | lib/plate_solve/cpp/ipv/src/ipv_select.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_sip.cpp | lib/plate_solve/cpp/ipv/src/ipv_sip.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_solver.cpp | lib/plate_solve/cpp/ipv/src/ipv_solver.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_triangle.cpp | lib/plate_solve/cpp/ipv/src/ipv_triangle.cpp | 问题扫描/_cache/L28d.md:172 |
| plate_solve/cpp/ipv/src/ipv_wcs.cpp | lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp | 问题扫描/_cache/L28d.md:172 |
| star_detector/src/sdet_api.cpp | lib/star_detector/src/sdet_api.cpp | 问题扫描/_cache/L28d.md:172 |
| star_detector/src/sdet_background.cpp | lib/star_detector/src/sdet_background.cpp | 问题扫描/_cache/L28d.md:172 |
| star_detector/src/sdet_detector.cpp | lib/star_detector/src/sdet_detector.cpp | 问题扫描/_cache/L28d.md:172 |
| star_detector/src/sdet_image.cpp | lib/star_detector/src/sdet_image.cpp | 问题扫描/_cache/L28d.md:172 |
| star_detector/src/sdet_log.cpp | lib/star_detector/src/sdet_log.cpp | 问题扫描/_cache/L28d.md:172 |
| descriptor/registry.json | tests/testkit/registry.json | 问题扫描/_cache/L28e.md:109 |
| gaia/types.h | lib/gaia_xpsd_client/include/astrocs/gaia/types.h | 问题扫描/_cache/L28e.md:119 |
| snr_estimator/README.md | lib/snr_estimator/README.md | 问题扫描/_cache/L28e.md:121 |
| lib/phase2_rej/src/rejection.cpp | lib/phase2/src/rejection.cpp | 问题扫描/_merge/00_COORDINATION.md:40 |
| modules/services/io/src/fits_core.c | runtime/io/fits_core.c | 问题扫描/_merge/00_COORDINATION.md:407 |
| lib/.../hips_properties.cpp | lib/phase3_session/hips_properties.cpp | 问题扫描/_merge/00_COORDINATION.md:502 |
| orchestrator/cpp/src/checkpoint.cpp | lib/orchestrator/cpp/src/checkpoint.cpp | 问题扫描/_merge/00_COORDINATION.md:539 |
| docs/architecture/PUBLIC_API.md | docs/contracts/PUBLIC_API.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:277 |
| lib/phase3_rsmp/p3_resample.h | lib/phase3_session/p3_resample.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:278 |
| tests/pipeline/test_phase_lifecycle.py | tests/runtime/test_phase_lifecycle.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:279 |
| lib/calibration/tests/calibration_adapter_test.cpp | tests/unit/calibration_adapter_test.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:280 |
| ../lib/phase1_session/p1_session.cpp | lib/phase1_session/p1_session.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:281 |
| 1478/sampler.cpp | lib/phase2/src/sampler.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:282 |
| upm.cpp/sampler.cpp | lib/phase2/src/sampler.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:283 |
| star_detector.h/dynamic_psf.h | lib/dynamic_psf/include/dynamic_psf.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:284 |
| cli/pc_api.cpp | lib/photometric_calib/cpp/src/pc_api.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:285 |
| docs/algorithm.md | lib/photometric_calib/docs/algorithm.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:286 |
| tests/hips_core_selftest.c | modules/services/io/tests/hips_core_selftest.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:287 |
| quality/check_traceability.py | tools/quality/check_traceability.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:288 |
| plate_solve/README.md | lib/plate_solve/README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:289 |
| app/browser_cli.cpp | lib/healpix_db/healpix_browser_qt/app/browser_cli.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:290 |
| dynamic_psf/README.md | lib/dynamic_psf/README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:291 |
| include/snr_estimator.h | lib/snr_estimator/cpp/include/snr_estimator.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:292 |
| p1drz/p1drz_tests_core.cpp | lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_core.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:293 |
| avx2/src/avx2_provider.cpp | providers/cpu/avx2/src/avx2_provider.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:294 |
| tests/p1noise/CMakeLists.txt | lib/snr_estimator/tests/p1noise/CMakeLists.txt | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:295 |
| test/test_synthetic.cpp | lib/plate_solve/cpp/ipv/test/test_synthetic.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:296 |
| cpp/test/test_spectrum_integrator_golden.py | lib/photometric_calib/cpp/test/test_spectrum_integrator_golden.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:297 |
| docs/standards/DATA_ARTIFACTS.md | docs/contracts/DATA_ARTIFACTS.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:298 |
| astrocs/cosmetic/types.h | lib/cosmetic/include/astrocs/cosmetic/types.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:299 |
| tests/p1drz/CMakeLists.txt | lib/healpix_db/healpix_drizzle/tests/p1drz/CMakeLists.txt | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:300 |
| tests/fits_core_selftest.c | modules/services/io/tests/fits_core_selftest.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:301 |
| include/astrocs/io/fits_stream_v1.h | modules/services/io/include/astrocs/io/fits_stream_v1.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:302 |
| fixtures/dangling_ref.json | tests/traceability/fixtures/dangling_ref.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:303 |
| .github/workflows/ci.yml | lib/astro_image_io/third_party/cfitsio/.github/workflows/ci.yml | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:304 |
| registry/astrocs.phase2.resample.md | docs/modules/registry/astrocs.phase2.resample.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:305 |
| nlohmann/json-schema.hpp | lib/orchestrator/cpp/third_party/json-schema-validator/nlohmann/json-schema.hpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:306 |
| archive/review/RELEASE_STATUS.md | docs/archive/review/RELEASE_STATUS.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:307 |
| hips/aio_hips_reader.h | lib/astro_image_io/include/aio_hips_reader.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:308 |
| runtime/runtime.h | include/astrocs/core/runtime.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:309 |
| libaio/src/aio_api.cpp | lib/astro_image_io/src/aio_api.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:310 |
| docs/software/fitsio/fitsio.h | lib/astro_image_io/third_party/cfitsio/fitsio.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:311 |
| tools/backend/rcr_oracle_compare.py | lib/phase2/tools/rcr_oracle_compare.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:312 |
| tools/quality/rcr_oracle_compare.py | lib/phase2/tools/rcr_oracle_compare.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:313 |
| lib/phase2_rej/include/astro/phase2/rejection.h | lib/phase2/include/astro/phase2/rejection.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:314 |
| tools/quality/check_release_consistency.py | tools/check_release_consistency.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:315 |
| src/stage2.cpp | lib/phase2/tools/stage2.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:316 |
| tools/monitoring/run_provider_oracle_checks.py | tests/cpu/baseline/run_provider_oracle_checks.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:317 |
| lib/core/src/artifact.h | include/astrocs/core/artifact.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:318 |
| p3_resample.h/p3_session.cpp | lib/phase3_session/p3_session.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:319 |
| sampler.cpp/upm.cpp | lib/phase2/src/upm.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:320 |
| lib/plate_solve/src/ipv_triangle.cpp | lib/plate_solve/cpp/ipv/src/ipv_triangle.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:321 |
| lib/astro_calibration/include/astro_calibration.h | lib/calibration/include/astro_calibration.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:322 |
| lib/plate_solve/cpp/ipv/src/ipv_solver.h | lib/plate_solve/cpp/ipv/include/ipv_solver.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:323 |
| lib/phase3_rsmp/src/p3_resample.cpp | lib/phase3_session/p3_resample.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:324 |
| snr_estimator.h/photometry_apply.h | lib/calibration/src/photometry_apply.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:325 |
| docs/validation/TEST_MATRIX.md | docs/contracts/TEST_MATRIX.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:326 |
| lib/phase1/session/test_hips_tile_mapping.cpp | lib/common/healpix/tests/test_hips_tile_mapping.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:327 |
| add_executable/add_test/checks.json | ci/checks.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:328 |
| ../../lib/astro_image_io/tests/test_p1_io_hardening.cpp | lib/astro_image_io/tests/test_p1_io_hardening.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:329 |
| tests/synth/test_drizzle_oracle.py | tests/backend/test_drizzle_oracle.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:330 |
| docs/contracts/DATA-002_PHASE_PRODUCT_EXCHANGE.md | docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:331 |
| tests/backend/test_photometry_apply.cpp | lib/calibration/tests/test_photometry_apply.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:332 |
| lib/photometric_calib/tests/p1phot/p1phot_tests_core.cpp | lib/phase1/tests/p1phot/p1phot_tests_core.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:333 |
| docs/interfaces/PUBLIC_API.md | docs/contracts/PUBLIC_API.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:334 |
| drizzle/types.h | lib/drizzle/include/astrocs/drizzle/types.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:335 |
| configs/stage1.template.json | lib/orchestrator/configs/stage1.template.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:336 |
| src/photometry_apply.cpp | lib/calibration/src/photometry_apply.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:337 |
| healpix_drizzle/README.md | lib/healpix_db/healpix_drizzle/README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:338 |
| include/ipv_polygon.h | lib/plate_solve/cpp/ipv/include/ipv_polygon.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:339 |
| tools/gen_geometry_truth.py | lib/healpix_db/healpix_browser_qt/tools/gen_geometry_truth.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:340 |
| include/astrocs/io/hips_input_v1.h | modules/services/io/include/astrocs/io/hips_input_v1.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:341 |
| tests/p1drz/p1drz_tests_core.cpp | lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_tests_core.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:342 |
| src/ipv_polygon.cpp | lib/plate_solve/cpp/ipv/src/ipv_polygon.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:343 |
| healpix_browser_qt/widgets/sphere_view.h | lib/healpix_db/healpix_browser_qt/widgets/sphere_view.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:344 |
| core/hips_browser_backend.cpp | lib/healpix_db/healpix_browser_qt/core/hips_browser_backend.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:345 |
| src/dark_optimizer.cpp | lib/calibration/src/dark_optimizer.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:346 |
| p1_hips/publish_atomic_test.c | tests/unit/p1_hips/publish_atomic_test.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:347 |
| p1star/CMakeLists.txt | lib/star_detector/tests/p1star/CMakeLists.txt | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:348 |
| licenses/License.txt | lib/astro_image_io/third_party/cfitsio/licenses/License.txt | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:349 |
| noop/README.md | modules/conformance/noop/README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:350 |

## 二、真源中确无此路径（417 种）→ 需三级复核后改述或撤条
| 引用 | 近似名候选 | 首次出现 |
|---|---|---|
| _cache/Lxx.md | （无同名近似） | 问题扫描/00_README.md:34 |
| _merge/Mx.md | （无同名近似） | 问题扫描/00_README.md:36 |
| README/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/10_PROTOCOL.md:37 |
| _merge/00_COORDINATION.md | （无同名近似） | 问题扫描/40_OWNER_DECISIONS.md:44 |
| findings/G_GOV_GATE/p1/FD_shadow_agents_md.md | （无同名近似） | 问题扫描/40_OWNER_DECISIONS.md:48 |
| findings/D_COMMENT/p1/L28b.md | （无同名近似） | 问题扫描/40_OWNER_DECISIONS.md:51 |
| bin/_astrocs.py | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/01_ASTROCS_ENGINEERING_CONSTRAINTS.md lib/photometric_calib/cpp/test/gate4_dr3sp_gaiaxpy/fsyn_astrocs.py tools/astrometry_oracle/__pycache__/make_astrocs_ref.cpython-313.pyc | 问题扫描/40_OWNER_DECISIONS.md:90 |
| _merge/M8.md | （无同名近似） | 问题扫描/40_OWNER_DECISIONS.md:100 |
| _merge/CHANGED_FILES_WATCH.md | （无同名近似） | 问题扫描/SUMMARY.md:174 |
| _cache/L23.md | （无同名近似） | 问题扫描/_cache/E1.md:4 |
| _cache/L28b.md | （无同名近似） | 问题扫描/_cache/E1.md:4 |
| _merge/M9.md | （无同名近似） | 问题扫描/_cache/E1.md:4 |
| src/module_entry.cpp | lib/gaia_xpsd_client/src/module_entry.c | 问题扫描/_cache/E1.md:24 |
| sanitize_wsl_v4/v5.sh | （无同名近似） | 问题扫描/_cache/E1.md:36 |
| findings/F_TEST_GAP/p0/M4_L08_L09.md | （无同名近似） | 问题扫描/_cache/E2.md:194 |
| cmake/astrocs.product.windows.json | cmake/astrocs.product.windows.json.in packaging/astrocs.product.json packaging/schemas/astrocs-product.schema.json | 问题扫描/_cache/E2.md:439 |
| findings/G_GOV_GATE/p0/M5b_L12_L17.md | （无同名近似） | 问题扫描/_cache/E2.md:503 |
| findings/F_TEST_GAP/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/_cache/E2.md:505 |
| findings/F_TEST_GAP/p1/M8_L22_L23.md | （无同名近似） | 问题扫描/_cache/E2.md:506 |
| findings/G_GOV_GATE/p0/M5a_L11_L17.md | （无同名近似） | 问题扫描/_cache/E2.md:507 |
| findings/C_DOC_CODE_GAP/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/_cache/E2.md:508 |
| findings/G_GOV_GATE/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/_cache/E2.md:509 |
| origtest/../orig_gaia2.c | （无同名近似） | 问题扫描/_cache/E3.md:31 |
| _cache/L24.md | （无同名近似） | 问题扫描/_cache/E4.md:324 |
| _merge/ANCHOR_VERIFY_REPORT.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:154 |
| _cache/L01.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:155 |
| _merge/M1a.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:155 |
| findings/B_STD_MISMATCH/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:155 |
| findings/E_TRACE_BREAK/p0/M6b_L16_L18.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:188 |
| docs/spec/PHASE1_PIPELINE_REDESIGN_SPEC.md | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:189 |
| tests/abi/test_io_ownership_contract.py | （无同名近似） | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:265 |
| Makefile/build.ps1 | build.sh ci/steps/linux_build_root_graph.sh docs/architecture/BUILD_GRAPH.md | 问题扫描/_cache/L01.md:21 |
| docs/05_STAR_DETECT_PSF_DEDUP_SPEC.md | （无同名近似） | 问题扫描/_cache/L01.md:253 |
| D.spherical-projection/D.h | （无同名近似） | 问题扫描/_cache/L02.md:6 |
| 83/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_cache/L02.md:57 |
| schemas/phase3_request_v1.schema.json | ci/checks.schema.json ci/ci_result.schema.json contracts/config/cli_modules_list.schema.json | 问题扫描/_cache/L02.md:228 |
| CMakeLists.txt/tests/unit/CMakeLists.txt | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/archive/ACR_FOCUSED_CONTROL_PACKAGE_V4/examples/weighted_integration/CMakeLists.reference.txt | 问题扫描/_cache/L04.md:69 |
| test_drizzle_oracle.py/parallel.py | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/08_CPU_PARALLEL_BACKEND.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/agent/PARALLELIZATION_AND_DEPENDENCY_RULES.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/docs/19_ROADMAP_GATES_AND_PARALLEL_PLAN.md | 问题扫描/_cache/L04.md:237 |
| descriptor/integration.json | docs/algorithms/INTEGRATION_ALGORITHMS.md docs/algorithms/PHASE2_INTEGRATION.md docs/science/INTEGRATION.md | 问题扫描/_cache/L05.md:192 |
| lib/phase1/stars/star_measure_io.cpp | （无同名近似） | 问题扫描/_cache/L06.md:16 |
| _cache/L06.md | （无同名近似） | 问题扫描/_cache/L06.md:400 |
| lib/photometric_calib/flux_calibrator/python/star_matcher.py | lib/photometric_calib/cpp/src/star_matcher.cpp lib/photometric_calib/cpp/src/star_matcher.h | 问题扫描/_cache/L07.md:268 |
| README/module.yaml/memory.md | cli/memory_growth.h cli/memory_report.h docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md | 问题扫描/_cache/L08.md:9 |
| docs/interfaces/data/DATA-001_ARTIFACT_CONTRACT.md | Testing/Temporary/CTestCostData.txt docs/architecture/DATA_FLOW.md docs/archive/history/v19/DATA_CONTRACTS.md | 问题扫描/_cache/L10.md:187 |
| contracts/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_cache/L10.md:432 |
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
| gitlab.com/free-astro/siril/-/raw/master/README.md | VISUAL_CHECK_README.md docs/README-DOCS.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/templates/MODULE_README.md | 问题扫描/_cache/L25.md:10 |
| www.gnu.org/software/gsl/doc/html/gpl.h | （无同名近似） | 问题扫描/_cache/L25.md:11 |
| _cache/F00_FRONT_SPOTCHECKS.md | （无同名近似） | 问题扫描/_cache/L26.md:12 |
| docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md | （无同名近似） | 问题扫描/_cache/L27.md:155 |
| docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md | （无同名近似） | 问题扫描/_cache/L27.md:156 |
| docs/superpowers/plans/2026-07-13-cpp-qt-browser.md | lib/astro_image_io/tests/test_wph_cli_browser.cpp lib/healpix_db/healpix_browser_qt/app/browser_cli.cpp lib/healpix_db/healpix_browser_qt/core/browser_backend.cpp | 问题扫描/_cache/L27.md:157 |
| tasks/SCI-F3-001.md | ci/tests/__pycache__/test_ci001_failclosed.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_ci001_failclosed.cpython-313.pyc ci/tests/test_ci001_failclosed.py | 问题扫描/_cache/L27.md:168 |
| p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/_cache/L28b.md:7 |
| findings/D_COMMENT/p2/L28b.md | （无同名近似） | 问题扫描/_cache/L28b.md:11 |
| star_finder.c/PSF.c | docs/algorithms/STAR_PSF_ALGORITHMS.md docs/modules/dynamic_psf.md docs/modules/registry/astrocs.phase1.star-psf.md | 问题扫描/_cache/L28b.md:120 |
| providers/cpu/baseline/include/.../v1.h | （无同名近似） | 问题扫描/_cache/L28b.md:168 |
| findings/E_TRACE_BREAK/p1/L28c.md | （无同名近似） | 问题扫描/_cache/L28c.md:58 |
| findings/D_COMMENT/p1/L28c.md | （无同名近似） | 问题扫描/_cache/L28c.md:60 |
| findings/D_COMMENT/p2/L28c.md | （无同名近似） | 问题扫描/_cache/L28c.md:61 |
| wiki/Reverse_Drizzle.md | lib/healpix_db/healpix_drizzle/reverse_drizzle.cpp lib/healpix_db/healpix_drizzle/reverse_drizzle.h lib/healpix_db/healpix_drizzle/tests/reverse_drizzle_science_test.cpp | 问题扫描/_cache/L28c.md:127 |
| _cache/L28c.md | （无同名近似） | 问题扫描/_cache/L28c.md:169 |
| p2/L28c.md | （无同名近似） | 问题扫描/_cache/L28c.md:169 |
| p2/L28d.md | （无同名近似） | 问题扫描/_cache/L28d.md:10 |
| findings/E_TRACE_BREAK/p1/L28e.md | （无同名近似） | 问题扫描/_cache/L28e.md:131 |
| findings/D_COMMENT/p1/L28e.md | （无同名近似） | 问题扫描/_cache/L28e.md:134 |
| findings/E_TRACE_BREAK/p2/L28e.md | （无同名近似） | 问题扫描/_cache/L28e.md:135 |
| findings/D_COMMENT/p2/L28e.md | （无同名近似） | 问题扫描/_cache/L28e.md:137 |
| ../10_PROTOCOL.md | （无同名近似） | 问题扫描/_cache/README.md:4 |
| ../20_AGENT_PLAN.md | （无同名近似） | 问题扫描/_cache/README.md:7 |
| ../_cache/F00_FRONT_SPOTCHECKS.md | （无同名近似） | 问题扫描/_merge/00_COORDINATION.md:4 |
| tools/astrometry_oracle/__pycache__/make_astrocs_ref.cpython-313.py | ci/__pycache__/bootstrap.cpython-313.pyc ci/__pycache__/check_version.cpython-313.pyc ci/__pycache__/ci_repair_round.cpython-313.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:361 |
| ci/tests/__pycache__/test_ci001_failclosed.cpython-313-pytest-8.3.5.py | ci/tests/__pycache__/test_ci001_failclosed.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_bootstrap_utf8.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_ci001_failclosed.cpython-313.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:424 |
| ci/tests/__pycache__/test_ci001_failclosed.cpython-313.py | ci/tests/__pycache__/test_ci001_failclosed.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_ci001_failclosed.cpython-313.pyc ci/__pycache__/bootstrap.cpython-313.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:424 |
| ci/__pycache__/bootstrap.cpython-313.py | ci/__pycache__/bootstrap.cpython-313.pyc ci/steps/__pycache__/collect_bootstrap_diag.cpython-313.pyc ci/tests/__pycache__/test_bootstrap_utf8.cpython-313-pytest-8.3.5.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:443 |
| ci/__pycache__/check_version.cpython-313.py | ci/__pycache__/check_version.cpython-313.pyc ci/__pycache__/bootstrap.cpython-313.pyc ci/__pycache__/ci_repair_round.cpython-313.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:443 |
| ci/__pycache__/ci_repair_round.cpython-313.py | ci/__pycache__/ci_repair_round.cpython-313.pyc ci/tests/__pycache__/test_ci_repair_round.cpython-313.pyc ci/__pycache__/bootstrap.cpython-313.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:443 |
| ci/tests/__pycache__/test_bootstrap_utf8.cpython-313-pytest-8.3.5.py | ci/tests/__pycache__/test_bootstrap_utf8.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_bootstrap_utf8.cpython-313.pyc ci/tests/__pycache__/test_ci001_failclosed.cpython-313-pytest-8.3.5.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:444 |
| _tools/anchor_repair_manual.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:446 |
| ci/steps/__pycache__/collect_bootstrap_diag.cpython-313.py | ci/steps/__pycache__/collect_bootstrap_diag.cpython-313.pyc ci/__pycache__/bootstrap.cpython-313.pyc ci/__pycache__/check_version.cpython-313.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:447 |
| ci/tests/__pycache__/test_ci_repair_round.cpython-313.py | ci/tests/__pycache__/test_ci_repair_round.cpython-313.pyc ci/__pycache__/bootstrap.cpython-313.pyc ci/__pycache__/check_version.cpython-313.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:449 |
| ci/tests/__pycache__/test_bootstrap_utf8.cpython-313.py | ci/tests/__pycache__/test_bootstrap_utf8.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_bootstrap_utf8.cpython-313.pyc ci/__pycache__/bootstrap.cpython-313.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:450 |
| lib/phase2/tools/stage2.c | docs/architecture/production_call_paths_stage2.csv engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/configs/stage2.template.json engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/schemas/stage2.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:451 |
| phase3_session/p3_wcs.c | lib/phase3_session/p3_wcs.cpp lib/phase3_session/p3_wcs.h tests/backend/__pycache__/test_p3_wcs.cpython-313-pytest-8.3.5.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:452 |
| tests/backend/__pycache__/test_p3_wcs.cpython-313-pytest-8.3.5.py | ci/tests/__pycache__/test_bootstrap_utf8.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_ci001_failclosed.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_deep_profiles.cpython-313-pytest-8.3.5.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:452 |
| ci/tests/__pycache__/test_deep_profiles.cpython-313-pytest-8.3.5.py | ci/tests/__pycache__/test_deep_profiles.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_bootstrap_utf8.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_ci001_failclosed.cpython-313-pytest-8.3.5.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:453 |
| lib/core/src/module_adapters.c | include/astrocs/core/module_adapters.h lib/core/src/module_adapters.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:455 |
| docs/contracts/TRACEABILITY.csv | contracts/schemas/traceability_matrix.schema.json docs/traceability/TRACEABILITY_LAYERS.csv docs/traceability/TRACEABILITY_MATRIX.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:456 |
| G_GOV_GATE/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:460 |
| C_DOC_CODE_GAP/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:461 |
| C_DOC_CODE_GAP/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:462 |
| D_COMMENT/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:463 |
| A_SCI_DEF/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:464 |
| D_COMMENT/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:465 |
| F_TEST_GAP/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:466 |
| G_GOV_GATE/p0/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:467 |
| G_GOV_GATE/p1/M5a_L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:468 |
| B_STD_MISMATCH/p0/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:469 |
| C_DOC_CODE_GAP/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:470 |
| E_TRACE_BREAK/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:471 |
| I_DOC_HYGIENE/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:472 |
| C_DOC_CODE_GAP/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:473 |
| C_DOC_CODE_GAP/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:474 |
| F_TEST_GAP/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:475 |
| C_DOC_CODE_GAP/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:476 |
| G_GOV_GATE/p0/M5a_L11_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:477 |
| C_DOC_CODE_GAP/p1/M5a_L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:478 |
| A_SCI_DEF/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:479 |
| D_COMMENT/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:480 |
| E_TRACE_BREAK/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:481 |
| A_SCI_DEF/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:482 |
| F_TEST_GAP/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:483 |
| G_GOV_GATE/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:484 |
| B_STD_MISMATCH/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:485 |
| C_DOC_CODE_GAP/p0/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:486 |
| A_SCI_DEF/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:487 |
| A_SCI_DEF/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:488 |
| B_STD_MISMATCH/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:489 |
| E_TRACE_BREAK/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:490 |
| I_DOC_HYGIENE/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:491 |
| I_DOC_HYGIENE/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:492 |
| B_STD_MISMATCH/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:493 |
| A_SCI_DEF/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:494 |
| C_DOC_CODE_GAP/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:495 |
| F_TEST_GAP/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:496 |
| I_DOC_HYGIENE/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:497 |
| F_TEST_GAP/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:498 |
| C_DOC_CODE_GAP/p0/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:499 |
| C_DOC_CODE_GAP/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:500 |
| I_DOC_HYGIENE/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:501 |
| G_GOV_GATE/p0/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:502 |
| G_GOV_GATE/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:503 |
| D_COMMENT/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:504 |
| D_COMMENT/p2/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:505 |
| E_TRACE_BREAK/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:506 |
| A_SCI_DEF/p2/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:507 |
| C_DOC_CODE_GAP/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:508 |
| E_TRACE_BREAK/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:509 |
| C_DOC_CODE_GAP/p2/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:510 |
| A_SCI_DEF/p0/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:511 |
| F_TEST_GAP/p0/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:512 |
| C_DOC_CODE_GAP/p2/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:513 |
| D_COMMENT/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:514 |
| E_TRACE_BREAK/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:515 |
| C_DOC_CODE_GAP/p0/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:516 |
| B_STD_MISMATCH/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:517 |
| H_NUMERIC/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:518 |
| G_GOV_GATE/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:519 |
| D_COMMENT/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:520 |
| A_SCI_DEF/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:521 |
| C_DOC_CODE_GAP/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:522 |
| H_NUMERIC/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:523 |
| C_DOC_CODE_GAP/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:524 |
| G_GOV_GATE/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:525 |
| H_NUMERIC/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:526 |
| F_TEST_GAP/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:527 |
| A_SCI_DEF/p0/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:528 |
| C_DOC_CODE_GAP/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:529 |
| I_DOC_HYGIENE/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:530 |
| G_GOV_GATE/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:531 |
| C_DOC_CODE_GAP/p2/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:532 |
| A_SCI_DEF/p0/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:533 |
| F_TEST_GAP/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:534 |
| I_DOC_HYGIENE/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:535 |
| H_NUMERIC/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:536 |
| I_DOC_HYGIENE/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:537 |
| I_DOC_HYGIENE/p2/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:538 |
| A_SCI_DEF/p2/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:539 |
| G_GOV_GATE/p1/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:540 |
| G_GOV_GATE/p2/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:541 |
| H_NUMERIC/p2/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:542 |
| D_COMMENT/p2/M4_L08_L09.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:543 |
| B_STD_MISMATCH/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:544 |
| A_SCI_DEF/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:545 |
| B_STD_MISMATCH/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:546 |
| C_DOC_CODE_GAP/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:547 |
| F_TEST_GAP/p0/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:548 |
| D_COMMENT/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:549 |
| E_TRACE_BREAK/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:550 |
| F_TEST_GAP/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:551 |
| G_GOV_GATE/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:552 |
| H_NUMERIC/p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:553 |
| F_TEST_GAP/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:554 |
| F_TEST_GAP/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:555 |
| E_TRACE_BREAK/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:556 |
| D_COMMENT/p1/M5a_L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:557 |
| F_TEST_GAP/p1/M5a_L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:558 |
| A_SCI_DEF/p1/M5a_L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:559 |
| I_DOC_HYGIENE/p1/M5a_L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:560 |
| D_COMMENT/p2/M5a_L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:561 |
| I_DOC_HYGIENE/p2/M5a_L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:562 |
| E_TRACE_BREAK/p1/M5a_L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:563 |
| H_NUMERIC/p1/M5a_L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:564 |
| F_TEST_GAP/p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:565 |
| H_NUMERIC/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:566 |
| E_TRACE_BREAK/p2/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:567 |
| G_GOV_GATE/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:568 |
| E_TRACE_BREAK/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:569 |
| D_COMMENT/p2/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:570 |
| I_DOC_HYGIENE/p2/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:571 |
| G_GOV_GATE/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:572 |
| E_TRACE_BREAK/p0/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:573 |
| C_DOC_CODE_GAP/p0/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:574 |
| C_DOC_CODE_GAP/p0/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:575 |
| C_DOC_CODE_GAP/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:576 |
| I_DOC_HYGIENE/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:577 |
| E_TRACE_BREAK/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:578 |
| E_TRACE_BREAK/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:579 |
| B_STD_MISMATCH/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:580 |
| A_SCI_DEF/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:581 |
| E_TRACE_BREAK/p0/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:582 |
| E_TRACE_BREAK/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:583 |
| F_TEST_GAP/p0/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:584 |
| G_GOV_GATE/p0/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:585 |
| G_GOV_GATE/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:586 |
| F_TEST_GAP/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:587 |
| F_TEST_GAP/p0/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:588 |
| G_GOV_GATE/p1/M3_L05_L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:589 |
| F_TEST_GAP/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:590 |
| E_TRACE_BREAK/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:591 |
| H_NUMERIC/p1/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:592 |
| H_NUMERIC/p2/M2b_L03_L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:593 |
| docs/TRACEABILITY.c | contracts/schemas/traceability_matrix.schema.json docs/TRACEABILITY.csv docs/traceability/TRACEABILITY_LAYERS.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:594 |
| docs/traceability/TRACEABILITY_MATRIX.c | contracts/schemas/traceability_matrix.schema.json docs/traceability/TRACEABILITY_MATRIX.csv docs/traceability/TRACEABILITY_MATRIX.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:595 |
| cmake/install_layout.c | cmake/install_layout.cmake | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:596 |
| docs/architecture/PRODUCTION_EXECUTION_INVENTORY.c | docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv tools/arch/__pycache__/build_production_execution_inventory.cpython-313.pyc tools/arch/build_production_execution_inventory.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:597 |
| tools/arch/__pycache__/build_production_execution_inventory.cpython-313.py | ci/__pycache__/bootstrap.cpython-313.pyc ci/__pycache__/check_version.cpython-313.pyc ci/__pycache__/ci_repair_round.cpython-313.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:597 |
| docs/contracts/API_CONTRACTS.c | docs/contracts/API_CONTRACTS.csv tools/quality/contracts/check_api_contracts.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:599 |
| tools/quality/contracts/fixtures/check_api_contracts/invalid_missing_symbol.c | tools/quality/contracts/fixtures/check_api_contracts/invalid_missing_symbol.csv tools/quality/contracts/fixtures/check_doc_symbols/invalid_missing_symbol.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:600 |
| docs/architecture/api_inventory.c | docs/architecture/api_inventory.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:601 |
| cmake/cfitsio_sources.c | cmake/cfitsio_sources.cmake | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:602 |
| docs/traceability/TRACEABILITY_LAYERS.c | docs/traceability/TRACEABILITY_LAYERS.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:603 |
| docs/audit/doc_classification.c | docs/audit/doc_classification.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:604 |
| docs/audit/inventory.c | ci/INVENTORY_REPORT.md docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv docs/architecture/api_inventory.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:605 |
| docs/audit/risk_verification_T012.c | docs/audit/risk_verification_T012.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:606 |
| contracts/API_CONTRACTS.c | docs/contracts/API_CONTRACTS.csv tools/quality/contracts/check_api_contracts.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:607 |
| tests/test_index.c | tests/realdata/__pycache__/test_index_v12.cpython-313-pytest-8.3.5.pyc tests/realdata/test_index_v12.py tests/test_index.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:608 |
| tests/realdata/__pycache__/test_index_v12.cpython-313-pytest-8.3.5.py | ci/tests/__pycache__/test_bootstrap_utf8.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_ci001_failclosed.cpython-313-pytest-8.3.5.pyc ci/tests/__pycache__/test_deep_profiles.cpython-313-pytest-8.3.5.pyc | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:608 |
| _cache/L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:610 |
| lib/phase1/ups/build_upm.cpp | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:611 |
| contracts/schemas/phase3_request_v1.schema.json | ci/checks.schema.json ci/ci_result.schema.json contracts/config/cli_modules_list.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:612 |
| aah3860/aah3860.right.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:613 |
| p2/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:614 |
| p1/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:615 |
| docs.astropy.org/en/stable/api/astropy.modeling.functional_models.Moffat2D.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:616 |
| 13/TROUBLESHOOTING.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:617 |
| _cache/L11.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:618 |
| M5a-G-001/002/003/005.md | astrocs_run_7d4cd130056e.json engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/tasks/P10-005.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/tasks/P11-005.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:619 |
| M5a-G-004/006/007/008/009/010.md | lib/acr/docs/ADR-010-delete-per-kernel-routing.md testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@064010-180S-Green.fts testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@010214-600S-Oiii.fts | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:620 |
| M5a_D-001/002.md | docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/evidence/P12-002/raw_logs/test_photometric_calib_p12_002.log | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:621 |
| M5a_I-001/002.md | docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/evidence/P12-002/raw_logs/test_photometric_calib_p12_002.log | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:622 |
| _cache/L12.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:623 |
| _cache/L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:624 |
| tests/unit/p1_phot_test.cpp | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:625 |
| _cache/L13.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:626 |
| findings/G_GOV_GATE/p0/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:627 |
| findings/D_COMMENT/p1/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:628 |
| findings/D_COMMENT/p2/M6a_L13_L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:629 |
| ../../10_PROTOCOL.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:630 |
| ../../_merge/M3.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:631 |
| README/头文件/integration.json | docs/algorithms/INTEGRATION_ALGORITHMS.md docs/algorithms/PHASE2_INTEGRATION.md docs/science/INTEGRATION.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:632 |
| lib/core/src/astrocs_phase2_writer.cpp | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:633 |
| ALG/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:634 |
| ALG/DATA/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:635 |
| 11.1/module.yaml | contracts/config/cli_modules_list.schema.json contracts/config/module_dll_contract.schema.json contracts/config/module_lifecycle_contract.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:636 |
| docs/architecture/abi/ABI_002_MODULE_ABI.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:637 |
| schemas/phase3_config_v1.schema.json | ci/checks.schema.json ci/ci_result.schema.json contracts/config/cli_modules_list.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:638 |
| docs/standards/DIAGNOSTICS_STANDARD.md | docs/standards/LOGGING_DIAGNOSTICS_STANDARD.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:639 |
| tests/unit/gaia_xpsd_fixture_sp_gen.c | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:640 |
| tools/quality/contracts/fixtures/check_traceability/valid_min.c | tools/quality/contracts/fixtures/check_api_contracts/valid_min.csv tools/quality/contracts/fixtures/check_build_graph/valid_min.json tools/quality/contracts/fixtures/check_comments/valid_min.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:641 |
| docs/science/UNCERTAINTY.md | contracts/data/phase2_uncertainty_rejection_provenance_v1.json docs/science/UNCERTAINTY_AND_COVARIANCE.md tests/unit/p3002_uncertainty_test.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:642 |
| .github/workflows/build.yml | build.sh ci/steps/linux_build_root_graph.sh docs/architecture/BUILD_GRAPH.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:643 |
| tools/check_domain_interfaces.py | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:644 |
| lib/astro_image_io/src/astro_image_io.cpp | docs/modules/astro_image_io.md lib/astro_image_io/astro_image_io.dll lib/astro_image_io/include/astro_image_io.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:645 |
| docs/contracts/SCIENCE_PROFILES.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:646 |
| docs/traceability/TRACEABILITY_MATRIX.md | contracts/schemas/traceability_matrix.schema.json docs/traceability/TRACEABILITY_MATRIX.csv docs/traceability/TRACEABILITY_MATRIX.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:647 |
| counters/p1_stack.json | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:648 |
| docs/RELEASE_AUDIT_2026-09-05.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:649 |
| docs/standards/SCIENCE_CONTRACT_PROCESS.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:650 |
| p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:651 |
| findings/A_SCI_DEF/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:652 |
| findings/A_SCI_DEF/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:653 |
| findings/H_NUMERIC/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:654 |
| findings/H_NUMERIC/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:655 |
| findings/C_DOC_CODE_GAP/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:656 |
| findings/C_DOC_CODE_GAP/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:657 |
| findings/F_TEST_GAP/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:658 |
| findings/G_GOV_GATE/p0/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:659 |
| findings/E_TRACE_BREAK/p1/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:660 |
| findings/D_COMMENT/p2/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:661 |
| findings/I_DOC_HYGIENE/p2/M3b_L06.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:662 |
| findings/G_GOV_GATE/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:663 |
| findings/C_DOC_CODE_GAP/p0/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:664 |
| findings/C_DOC_CODE_GAP/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:665 |
| findings/E_TRACE_BREAK/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:666 |
| findings/E_TRACE_BREAK/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:667 |
| findings/I_DOC_HYGIENE/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:668 |
| findings/I_DOC_HYGIENE/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:669 |
| findings/F_TEST_GAP/p1/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:670 |
| findings/F_TEST_GAP/p2/M5b_L12_L17.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:671 |
| _cache/L14.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:672 |
| _cache/L21.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:673 |
| _cache/L22.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:674 |
| findings/B_STD_MISMATCH/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:675 |
| findings/F_TEST_GAP/p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:676 |
| ../../../_merge/M5a.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:677 |
| _merge/M4.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:678 |
| _merge/M6b.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:679 |
| _merge/M2a.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:680 |
| docs/architecture/ARCH_CONTRACTS.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:681 |
| docs/validation/ACCEPTANCE_GATES.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:682 |
| include/astrocs/io/io_module_api_v1.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:683 |
| _merge/M5b.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:684 |
| _merge/M5a.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:685 |
| _merge/M3b.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:686 |
| modules/services/io/src/hips_input_v1.c | modules/services/io/include/astrocs/io/hips_input_v1.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:687 |
| lib/phase3_session/hips_reader_p3.c | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:688 |
| www.ivoa.net/documents/MOC/20220727/REC-MOC-2.0-20220727.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:689 |
| _cache/L05.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:690 |
| _cache/L07.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:691 |
| module.yaml/types.h/integration.json | docs/algorithms/INTEGRATION_ALGORITHMS.md docs/algorithms/PHASE2_INTEGRATION.md docs/science/INTEGRATION.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:692 |
| contracts/schemas/test_ids.schema.json | ci/checks.schema.json ci/ci_result.schema.json contracts/config/cli_modules_list.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:693 |
| docs/TRACEABILITY.md | contracts/schemas/traceability_matrix.schema.json docs/TRACEABILITY.csv docs/traceability/TRACEABILITY_LAYERS.csv | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:694 |
| docs/README.md | VISUAL_CHECK_README.md docs/README-DOCS.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/templates/MODULE_README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:695 |
| _merge/M7.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:696 |
| p0/M1a_L01_L02.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:697 |
| src/core/psfmatching.c | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:698 |
| findings/E_TRACE_BREAK/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:699 |
| findings/E_TRACE_BREAK/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:700 |
| findings/G_GOV_GATE/p0/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:701 |
| findings/G_GOV_GATE/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:702 |
| findings/G_GOV_GATE/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:703 |
| findings/I_DOC_HYGIENE/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:704 |
| findings/I_DOC_HYGIENE/p2/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:705 |
| findings/C_DOC_CODE_GAP/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:706 |
| findings/F_TEST_GAP/p1/M6b_L16_L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:707 |
| _cache/L16.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:708 |
| _cache/L18.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:709 |
| README/module.yaml/CMakeLists/io_module_api_v1.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:710 |
| lib/astro_image_io/CMakeLists.txt | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/archive/ACR_FOCUSED_CONTROL_PACKAGE_V4/examples/weighted_integration/CMakeLists.reference.txt | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:711 |
| findings/G_GOV_GATE/p1/M7_G_GOV_GATE_p1.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:712 |
| docs/standards/SCIENTIFIC_CONSTANTS.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:713 |
| _cache/L20.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:714 |
| _cache/L19.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:715 |
| docs.astropy.org/en/stable/wcs/sip.h | lib/healpix_db/healpix_drizzle/tests/mini_sip1000.cpp lib/healpix_db/healpix_drizzle/tests/p0_sip_order_guard_test.cpp lib/healpix_db/healpix_drizzle/wcs_sip.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:716 |
| findings/F_TEST_GAP/p0/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:717 |
| findings/F_TEST_GAP/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:718 |
| findings/G_GOV_GATE/p1/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:719 |
| findings/C_DOC_CODE_GAP/p1/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:720 |
| findings/C_DOC_CODE_GAP/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:721 |
| findings/B_STD_MISMATCH/p1/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:722 |
| findings/H_NUMERIC/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:723 |
| findings/I_DOC_HYGIENE/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:724 |
| findings/E_TRACE_BREAK/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:725 |
| findings/A_SCI_DEF/p2/M8_L22_L23.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:726 |
| module.yaml/memory.md/README.md | VISUAL_CHECK_README.md docs/README-DOCS.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/templates/MODULE_README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:727 |
| _cache/L25.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:728 |
| _cache/L27.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:729 |
| _cache/L15.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:730 |
| _merge/M2b.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:731 |
| findings/G_GOV_GATE/p0/M8a_L25_L27.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:732 |
| LICENSES/NOTICE.txt | engineering/control/archive/2026-09-09_superseded_V8.1_CI_CONTROL_20260905/15_SUPERSESSION_NOTICE.md lib/common/healpix/THIRD_PARTY_NOTICE.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:733 |
| _cache/L26.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:734 |
| findings/C_DOC_CODE_GAP/p1/M2a_L04_L10.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:735 |
| docs/DATA_STRUCTURED_CONTRACT_TOOLING.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:736 |
| findings/G_GOV_GATE/p0/M7_G_GOV_GATE_p0.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:737 |
| findings/A_SCI_DEF/p0/M7_A_SCI_DEF_p0.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:738 |
| findings/F_TEST_GAP/p1/M7_F_TEST_GAP_p1.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:739 |
| tools/monitoring/run_cpu_baseline_checks.py | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:740 |
| _merge/M8a.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:741 |
| lib/hips_p2/src/hips_p2_io.cpp | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:742 |
| tools/orchestrator/run_evidence_orchestrator.ps1 | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:743 |
| lib/cosmetic/src/real_ingest.cpp | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:744 |
| module.yaml/types.h | contracts/data/artifact_types.registry.json lib/plate_solve/cpp/ipv/include/ipv_types.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:745 |
| docs/pipeline/integration.json | docs/algorithms/INTEGRATION_ALGORITHMS.md docs/algorithms/PHASE2_INTEGRATION.md docs/science/INTEGRATION.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:746 |
| lib/hips/types.h | contracts/data/artifact_types.registry.json lib/plate_solve/cpp/ipv/include/ipv_types.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:747 |
| lib/photometric_calib/cpp/include/PhotometryCalculator.h | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:748 |
| docs/science/SNR_SCIENCE_DERIVATION.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:749 |
| tasks/02_ABI_BUILD_CLI_TASKS.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:750 |
| ../../lib/snr_estimator/tests/p1noise/CMakeLists.txt | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/archive/ACR_FOCUSED_CONTROL_PACKAGE_V4/examples/weighted_integration/CMakeLists.reference.txt | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:751 |
| tools/redteam_v19r3_sanitizer.sh | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:752 |
| _merge/M3.md | （无同名近似） | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:753 |
| lib/photometric_calib/src/photometry.c | docs/modules/registry/astrocs.phase1.photometry.md docs/science/PHOTOMETRY.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/checklists/G11_WCS_AND_PHOTOMETRY.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:754 |

## 三、符号在被引文件 0 命中、但在其他真源文件存在（32 种）→ 补文件锚点即可，禁止据此撤条
| 引用 | 实际所在 | 首次出现 |
|---|---|---|
| docs/science/CALIBRATION.md::min_samples | docs/API_REFERENCE.md docs/algorithms/NOISE_ESTIMATION.md docs/algorithms/PHASE2_REJECTION.md | 问题扫描/40_OWNER_DECISIONS.md:12 |
| docs/contracts/TEST_MATRIX.md::Drizzle | ASTROCS_PROJECT_CONSTITUTION.md CHANGELOG.md HANDOVER.md | 问题扫描/_cache/L23.md:137 |
| lib/astro_image_io/src/hips/aio_hips_reader.cpp::read_hips_tile | tests/unit/p1001_real_nodes_test.cpp | 问题扫描/_cache/L24.md:357 |
| tools/gen_v19_evidence.py::SKIP_PARTS | tools/gen_v19_source_snapshot.py tools/quality/build_v19r2_package.py tools/quality/build_v19r3_package.py | 问题扫描/_cache/L25.md:87 |
| lib/orchestrator/cpp/include/json_config.h::DrizzleConfig | docs/API_REFERENCE.md docs/ARCHITECTURE.md docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:763 |
| lib/gaia_xpsd_client/src/gaia_client.c::record | CMakeLists.txt ci/ctest_baseline.json ci/tests/test_ci001b_binding.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:764 |
| lib/gaia_xpsd_client/include/astrocs/gaia/types.h::GaiaStar | docs/API_REFERENCE.md docs/contracts/PUBLIC_API.md lib/gaia_xpsd_client/README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:765 |
| lib/phase2/src/rejection.cpp::gather | HANDOVER.md contracts/data/phase2_uncertainty_rejection_provenance_v1.json docs/algorithms/PHASE2_INTEGRATION.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:766 |
| tests/backend/test_p3_projection_oracle.py::car_ | lib/phase3_proj/p3_projection.cpp tests/artifact/test_production_store.py tests/artifact/test_provenance.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:767 |
| tools/quality/check_serial_heavy.py::docstring | docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md runtime/artifact_store/provenance.py tests/backend/test_p2007_joint_gate.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:768 |
| tools/check_module_readmes.py::docstring | docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md runtime/artifact_store/provenance.py tests/backend/test_p2007_joint_gate.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:769 |
| tests/test_index.csv::SCI | ASTROCS_PROJECT_CONSTITUTION.md AstroCS_ENGINEERING_CONSTRAINTS.md CHANGELOG.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:770 |
| docs/contracts/RT-001.md::DOC | .github/workflows/ci-windows.yml AstroCS_ENGINEERING_CONSTRAINTS.md CHANGELOG.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:771 |
| docs/contracts/INDEX.yaml::entries | ci/actions.lock.json ci/reconcile_state.py ci/run.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:772 |
| photometric_calib.h::PhotometricCalibration | CHANGELOG.md docs/API_REFERENCE.md docs/ARCHITECTURE.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:773 |
| tests/unit/p3002_real_nodes_test.cpp::write_props | tests/backend/test_hips_properties.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:774 |
| lib/cosmetic/src/module_entry.cpp::calibrate_frame | docs/API_REFERENCE.md docs/ARCHITECTURE.md docs/algorithms/CALIBRATION_ALGORITHMS.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:775 |
| lib/photometric_calib/cpp/include/photometric_calib.h::PhotometricCalibration | CHANGELOG.md docs/API_REFERENCE.md docs/ARCHITECTURE.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:776 |
| lib/astro_image_io/src/hips/aio_hips_reader.cpp::manifest | ASTROCS_PROJECT_CONSTITUTION.md AstroCS_ENGINEERING_CONSTRAINTS.md CMakeLists.txt | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:777 |
| lib/healpix_db/healpix_drizzle/drizzle_engine.cpp::validate | .github/workflows/ci-linux.yml .github/workflows/ci-windows.yml .github/workflows/fatduck.yml | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:778 |
| docs/architecture/PUBLIC_API.md::P1CAL | lib/calibration/tests/p1cal/CMakeLists.txt lib/calibration/tests/p1cal/p1cal_fixtures.hpp lib/calibration/tests/p1cal/p1cal_oracle.hpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:779 |
| lib/drizzle/include/astrocs/drizzle/types.h::DrizzleConfig | docs/API_REFERENCE.md docs/ARCHITECTURE.md docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:780 |
| include/astrocs/common_abi_v1.h::acs_module_descriptor_v1 | docs/algorithms/CALIBRATION_ALGORITHMS.md include/astrocs/abi/module_api_v1.h lib/calibration/README.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:781 |
| .github/workflows/ci.yml::V19R4 | docs/contracts/DATA_SEMANTICS.md tools/docs_machine_consistency.py tools/quality/build_v19r4_package.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:782 |
| tools/docs_machine_consistency.py::docstring | docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md runtime/artifact_store/provenance.py tests/backend/test_p2007_joint_gate.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:783 |
| tools/quality/contracts/check_traceability.py::TABLE | ci/validate_workflow_binding.py ci/wf_step.py docs/algorithms/HIPS_WRITER.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:784 |
| ci/verify_toolchain.py::docstring | docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md runtime/artifact_store/provenance.py tests/backend/test_p2007_joint_gate.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:785 |
| tools/quality/gen_module_readmes.py::docstring | docs/archive/history/memory_V18R2-V19_operational_log_2026-08-21.md runtime/artifact_store/provenance.py tests/backend/test_p2007_joint_gate.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:786 |
| docs/modules/registry/astrocs.phase2.coverage.md::front | cli/commands.cpp cli/memory_growth.h cli/monitor.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:787 |
| tests/unit/master_flat_median_test.cpp::TEST_F | engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/evidence/P13-001/scripts/test_stage1_batch_runner.py lib/acr/ci/check_acr_dormant.py lib/backend_host/bench_harness.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:788 |
| docs/contracts/DATA_SEMANTICS.md::electron | docs/GLOSSARY.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/03_TARGET_ARCHITECTURE.md engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/CONTROL_V6/AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830/07_SCIENCE_AND_TEST_MATRIX.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:789 |
| lib/astro_image_io/third_party/cfitsio/.github/workflows/ci.yml::V19R4 | docs/contracts/DATA_SEMANTICS.md tools/docs_machine_consistency.py tools/quality/build_v19r4_package.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:790 |

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
| lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::solve_pix2world_iterative | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:802 |
| lib/astro_image_io/src/hips/aio_hips_writer.cpp::write_tile_core | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:803 |
| lib/healpix_db/healpix_drizzle/spherical_overlap.cpp::compute_drop_corners | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:804 |
| lib/phase3_session/p3_wcs.cpp::p3_wcs_validate_descriptor | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:805 |
| runtime/io/fits_core.c::fio_write_hdu | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:806 |
| lib/gaia_xpsd_client/src/module_entry.c::build_result_json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:807 |
| lib/plate_solve/cpp/ipv/include/ipv_types.h::IPVSelectParams | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:808 |
| cli/parser.cpp::kAllowedFlags | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:809 |
| cli/commands.cpp::cmd_benchmark_cpu | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:810 |
| cli/runtime_client.cpp::exit_code_from_error | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:811 |
| tests/unit/gaia_xpsd_fixture_gen.c::xpsd_write_test_file | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:812 |
| cli/commands.cpp::cmd_bench_cpu | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:813 |
| aio_hips_writer.cpp::write_tile_core | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:814 |
| tests/unit/master_flat_median_test.cpp::RejectsNegativeMedianMaster | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:815 |
| gaia_client.c::trace_seq | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:816 |
| lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::build_fits_wcs_from_solution | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:817 |
| lib/healpix_db/healpix_drizzle/healpix_core.h::HealpixGrid | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:818 |
| lib/phase2/src/stage2_common.cpp::p2_stage2_config_from_json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:819 |
| packaging/astrocs.product.json::required_units | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:820 |
| lib/phase3_session/hips_properties.cpp::hips_properties_parse_file | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:821 |
| ipv_wcs.cpp::build_fits_wcs_from_solution | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:822 |
| lib/astro_image_io/tests/p1hips/p1hips_tests_units.cpp::u5_snr | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:823 |
| lib/snr_estimator/cpp/test/noise_model_science_test.cpp::test_variance_field_fit_snr006 | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:824 |
| tools/traceability/check_traceability_matrix.py::_check_refs | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:825 |

## 五、行号越过文件当前末尾（8 种）→ 重取行号或改 path::符号
| 引用 | 现行数 | 首次出现 |
|---|---|---|
| cli/main.cpp:95 | 79 | 问题扫描/_cache/E4.md:68 |
| modules/services/io/include/astrocs/io/fits_stream_v1.h:236 | 228 | 问题扫描/_cache/L10.md:231 |
| docs/contracts/DATA_ARTIFACTS.md:150 | 104 | 问题扫描/_cache/L10.md:247 |
| docs/science/DRIZZLE.md:186 | 161 | 问题扫描/_cache/L16.md:351 |
| docs/science/REJECTION.md:155 | 149 | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:834 |
| docs/development/CONFIG_SCHEMA.md:264 | 86 | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:835 |
| tools/README.md:95 | 84 | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:836 |
| docs/modules/photometric_calib.md:117 | 63 | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:837 |
