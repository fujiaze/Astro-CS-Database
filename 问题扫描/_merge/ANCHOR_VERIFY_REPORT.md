# 锚点机械核验报告 v2

- 生成器：_tools/verify_anchors.js；复跑 node 问题扫描/_tools/verify_anchors.js
- 本次扫描 168 份审计 md、6424 次锚点引用：路径存在性 / 行号是否越过文件末尾 / path::符号 是否真在该文件内
- 免报前缀：run build out artifacts evidence reports 工程控制 GaiaDR3* BASS* __pycache__ .pytest_cache 与本审计目录自身

## 一、路径在真源不存在（156 种）
| 引用 | 首次出现 |
|---|---|
| _cache/Lxx.md | 问题扫描/00_README.md:34 |
| _merge/Mx.md | 问题扫描/00_README.md:36 |
| README/module.yaml | 问题扫描/10_PROTOCOL.md:37 |
| Makefile/build.ps1 | 问题扫描/_cache/L01.md:21 |
| docs/05_STAR_DETECT_PSF_DEDUP_SPEC.md | 问题扫描/_cache/L01.md:253 |
| D.spherical-projection/D.h | 问题扫描/_cache/L02.md:6 |
| 83/module.yaml | 问题扫描/_cache/L02.md:57 |
| schemas/phase3_request_v1.schema.json | 问题扫描/_cache/L02.md:228 |
| src/module_entry.cpp | 问题扫描/_cache/L03.md:22 |
| CMakeLists.txt/tests/unit/CMakeLists.txt | 问题扫描/_cache/L04.md:69 |
| test_drizzle_oracle.py/parallel.py | 问题扫描/_cache/L04.md:237 |
| descriptor/integration.json | 问题扫描/_cache/L05.md:192 |
| lib/phase1/stars/star_measure_io.cpp | 问题扫描/_cache/L06.md:16 |
| _cache/L06.md | 问题扫描/_cache/L06.md:400 |
| lib/photometric_calib/flux_calibrator/python/star_matcher.py | 问题扫描/_cache/L07.md:268 |
| README/module.yaml/memory.md | 问题扫描/_cache/L08.md:9 |
| docs/interfaces/data/DATA-001_ARTIFACT_CONTRACT.md | 问题扫描/_cache/L10.md:187 |
| contracts/module.yaml | 问题扫描/_cache/L10.md:432 |
| cmake/astrocs.product.windows.json | 问题扫描/_cache/L12.md:10 |
| lib/orchestrator/cpp/CMakeLists.txt | 问题扫描/_cache/L12.md:54 |
| tests/abi/run_abi_checks.sh | 问题扫描/_cache/L12.md:167 |
| tools/check_cli_protocol.py | 问题扫描/_cache/L12.md:194 |
| docs/owner/PHASE_OVERVIEW.md | 问题扫描/_cache/L17.md:11 |
| module_version/module.yaml | 问题扫描/_cache/L17.md:96 |
| docs/architecture/cpu/ARCH_CONTRACTS.md | 问题扫描/_cache/L18.md:31 |
| docs/TRACEABILITY_family.json | 问题扫描/_cache/L18.md:73 |
| modules/services/io/include/astrocs/io/io_module_api_v1.h | 问题扫描/_cache/L18.md:216 |
| docs/history/README.md | 问题扫描/_cache/L18.md:229 |
| docs/history/v19/ARCHITECTURE.md | 问题扫描/_cache/L18.md:230 |
| docs/architecture/CPU_ADAPTIVE_V1.md | 问题扫描/_cache/L18.md:316 |
| www.itl.nist.gov/div898/handbook/eda/section3/eda35h3.h | 问题扫描/_cache/L20.md:18 |
| docs/interfaces/io/IO_002.md | 问题扫描/_cache/L21.md:9 |
| module.yaml/README.md/memory.md | 问题扫描/_cache/L22.md:168 |
| usr/lib/python3.13/unittest/loader.py | 问题扫描/_cache/L23.md:7 |
| unittest/loader.py | 问题扫描/_cache/L23.md:87 |
| tests/cpu/baseline/run_provider_baseline_checks.py | 问题扫描/_cache/L23.md:160 |
| tests/realdata/test_realdata_index_v12.py | 问题扫描/_cache/L23.md:247 |
| gitlab.com/free-astro/siril/-/raw/master/README.md | 问题扫描/_cache/L25.md:10 |
| www.gnu.org/software/gsl/doc/html/gpl.h | 问题扫描/_cache/L25.md:11 |
| _cache/F00_FRONT_SPOTCHECKS.md | 问题扫描/_cache/L26.md:12 |
| docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md | 问题扫描/_cache/L27.md:155 |
| docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md | 问题扫描/_cache/L27.md:156 |
| docs/superpowers/plans/2026-07-13-cpp-qt-browser.md | 问题扫描/_cache/L27.md:157 |
| tasks/SCI-F3-001.md | 问题扫描/_cache/L27.md:168 |
| ../10_PROTOCOL.md | 问题扫描/_cache/README.md:4 |
| ../20_AGENT_PLAN.md | 问题扫描/_cache/README.md:7 |
| ../_cache/F00_FRONT_SPOTCHECKS.md | 问题扫描/_merge/00_COORDINATION.md:4 |
| docs/TRACEABILITY.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:190 |
| docs/traceability/TRACEABILITY_MATRIX.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:191 |
| cmake/install_layout.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:192 |
| docs/architecture/PRODUCTION_EXECUTION_INVENTORY.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:193 |
| docs/contracts/API_CONTRACTS.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:194 |
| tools/quality/contracts/fixtures/check_api_contracts/invalid_missing_symbol.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:195 |
| docs/architecture/api_inventory.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:196 |
| cmake/cfitsio_sources.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:197 |
| docs/traceability/TRACEABILITY_LAYERS.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:198 |
| docs/audit/doc_classification.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:199 |
| docs/audit/inventory.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:200 |
| docs/audit/risk_verification_T012.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:201 |
| contracts/API_CONTRACTS.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:202 |
| tests/test_index.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:203 |
| _cache/L01.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:204 |
| _cache/L02.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:205 |
| lib/phase1/ups/build_upm.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:206 |
| contracts/schemas/phase3_request_v1.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:207 |
| aah3860/aah3860.right.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:209 |
| p2/M1a_L01_L02.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:210 |
| p1/M1a_L01_L02.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:211 |
| docs.astropy.org/en/stable/api/astropy.modeling.functional_models.Moffat2D.h | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:212 |
| 13/TROUBLESHOOTING.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:214 |
| _cache/L11.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:216 |
| M5a-G-001/002/003/005.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:217 |
| M5a-G-004/006/007/008/009/010.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:218 |
| M5a_D-001/002.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:219 |
| M5a_I-001/002.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:220 |
| _cache/L12.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:221 |
| _cache/L17.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:222 |
| tests/unit/p1_phot_test.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:223 |
| _cache/L13.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:224 |
| _merge/00_COORDINATION.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:225 |
| findings/G_GOV_GATE/p0/M6a_L13_L14.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:226 |
| findings/D_COMMENT/p1/M6a_L13_L14.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:227 |
| findings/D_COMMENT/p2/M6a_L13_L14.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:228 |
| ../../10_PROTOCOL.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:229 |
| ../../_merge/M3.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:230 |
| README/头文件/integration.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:231 |
| lib/core/src/astrocs_phase2_writer.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:232 |
| ALG/module.yaml | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:235 |
| ALG/DATA/module.yaml | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:236 |
| 11.1/module.yaml | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:237 |
| docs/architecture/abi/ABI_002_MODULE_ABI.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:239 |
| schemas/phase3_config_v1.schema.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:242 |
| docs/standards/DIAGNOSTICS_STANDARD.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:244 |
| tests/unit/gaia_xpsd_fixture_sp_gen.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:246 |
| tools/quality/contracts/fixtures/check_traceability/valid_min.c | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:247 |
| docs/science/UNCERTAINTY.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:249 |
| .github/workflows/build.yml | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:250 |
| tools/check_domain_interfaces.py | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:251 |
| lib/astro_image_io/src/astro_image_io.cpp | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:252 |
| docs/contracts/SCIENCE_PROFILES.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:253 |
| docs/traceability/TRACEABILITY_MATRIX.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:254 |
| counters/p1_stack.json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:255 |
| docs/RELEASE_AUDIT_2026-09-05.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:257 |
| docs/standards/SCIENCE_CONTRACT_PROCESS.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:258 |
| p1/M2a_L04_L10.md | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:260 |
| A_SCI_DEF/p0/M1a_L01_L02.md | 问题扫描/_merge/M1a.md:240 |
| H_NUMERIC/p1/M1a_L01_L02.md | 问题扫描/_merge/M1a.md:247 |
| I_DOC_HYGIENE/p2/M1a_L01_L02.md | 问题扫描/_merge/M1a.md:248 |
| findings/A_SCI_DEF/p0/M3b_L06.md | 问题扫描/_merge/M3b.md:91 |
| findings/A_SCI_DEF/p1/M3b_L06.md | 问题扫描/_merge/M3b.md:92 |
| findings/H_NUMERIC/p0/M3b_L06.md | 问题扫描/_merge/M3b.md:93 |
| findings/H_NUMERIC/p1/M3b_L06.md | 问题扫描/_merge/M3b.md:94 |
| findings/C_DOC_CODE_GAP/p0/M3b_L06.md | 问题扫描/_merge/M3b.md:95 |
| findings/C_DOC_CODE_GAP/p1/M3b_L06.md | 问题扫描/_merge/M3b.md:96 |
| findings/F_TEST_GAP/p0/M3b_L06.md | 问题扫描/_merge/M3b.md:97 |
| findings/G_GOV_GATE/p0/M3b_L06.md | 问题扫描/_merge/M3b.md:98 |
| findings/E_TRACE_BREAK/p1/M3b_L06.md | 问题扫描/_merge/M3b.md:99 |
| findings/D_COMMENT/p2/M3b_L06.md | 问题扫描/_merge/M3b.md:100 |
| findings/I_DOC_HYGIENE/p2/M3b_L06.md | 问题扫描/_merge/M3b.md:101 |
| findings/G_GOV_GATE/p0/M5a_L11_L17.md | 问题扫描/_merge/M5a.md:121 |
| G_GOV_GATE/p1/M5a_L11.md | 问题扫描/_merge/M5a.md:121 |
| C_DOC_CODE_GAP/p1/M5a_L11.md | 问题扫描/_merge/M5a.md:121 |
| D_COMMENT/p1/M5a_L11.md | 问题扫描/_merge/M5a.md:121 |
| D_COMMENT/p2/M5a_L11.md | 问题扫描/_merge/M5a.md:121 |
| E_TRACE_BREAK/p1/M5a_L11.md | 问题扫描/_merge/M5a.md:121 |
| F_TEST_GAP/p1/M5a_L11.md | 问题扫描/_merge/M5a.md:121 |
| H_NUMERIC/p1/M5a_L11.md | 问题扫描/_merge/M5a.md:121 |
| I_DOC_HYGIENE/p1/M5a_L11.md | 问题扫描/_merge/M5a.md:121 |
| I_DOC_HYGIENE/p2/M5a_L11.md | 问题扫描/_merge/M5a.md:121 |
| A_SCI_DEF/p1/M5a_L11.md | 问题扫描/_merge/M5a.md:121 |
| findings/G_GOV_GATE/p0/M5b_L12_L17.md | 问题扫描/_merge/M5b.md:6 |
| findings/G_GOV_GATE/p1/M5b_L12_L17.md | 问题扫描/_merge/M5b.md:7 |
| findings/C_DOC_CODE_GAP/p0/M5b_L12_L17.md | 问题扫描/_merge/M5b.md:8 |
| findings/C_DOC_CODE_GAP/p1/M5b_L12_L17.md | 问题扫描/_merge/M5b.md:9 |
| findings/E_TRACE_BREAK/p1/M5b_L12_L17.md | 问题扫描/_merge/M5b.md:10 |
| findings/E_TRACE_BREAK/p2/M5b_L12_L17.md | 问题扫描/_merge/M5b.md:11 |
| findings/I_DOC_HYGIENE/p1/M5b_L12_L17.md | 问题扫描/_merge/M5b.md:12 |
| findings/I_DOC_HYGIENE/p2/M5b_L12_L17.md | 问题扫描/_merge/M5b.md:13 |
| findings/F_TEST_GAP/p1/M5b_L12_L17.md | 问题扫描/_merge/M5b.md:14 |
| findings/F_TEST_GAP/p2/M5b_L12_L17.md | 问题扫描/_merge/M5b.md:15 |
| _cache/L14.md | 问题扫描/_merge/M6a.md:4 |
| _cache/L21.md | 问题扫描/_merge/M8a.md:6 |
| _cache/L22.md | 问题扫描/_merge/M8a.md:7 |
| _merge/M1a.md | 问题扫描/findings/A_SCI_DEF/p0/M1a_L01_L02.md:26 |
| findings/B_STD_MISMATCH/p0/M1a_L01_L02.md | 问题扫描/findings/A_SCI_DEF/p0/M1a_L01_L02.md:46 |
| findings/F_TEST_GAP/p0/M1a_L01_L02.md | 问题扫描/findings/A_SCI_DEF/p0/M1a_L01_L02.md:46 |
| ../../../_merge/M5a.md | 问题扫描/findings/A_SCI_DEF/p1/M5a_L11.md:2 |
| _merge/M4.md | 问题扫描/findings/C_DOC_CODE_GAP/p0/M4_L08_L09.md:25 |
| _merge/M6b.md | 问题扫描/findings/E_TRACE_BREAK/p0/M6b_L16_L18.md:4 |
| _merge/M2a.md | 问题扫描/findings/E_TRACE_BREAK/p1/M2a_L04_L10.md:42 |
| docs/architecture/ARCH_CONTRACTS.md | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.md:67 |
| docs/validation/ACCEPTANCE_GATES.md | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.md:67 |
| include/astrocs/io/io_module_api_v1.h | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.md:146 |
| _merge/M5b.md | 问题扫描/findings/G_GOV_GATE/p0/M5b_L12_L17.md:5 |
| _merge/M5a.md | 问题扫描/findings/G_GOV_GATE/p1/M5a_L11.md:8 |
| _merge/M3b.md | 问题扫描/findings/H_NUMERIC/p1/M3b_L06.md:22 |

## 二、行号越过文件当前末尾（4 种）
| 引用 | 现行数 | 首次出现 |
|---|---|---|
| modules/services/io/include/astrocs/io/fits_stream_v1.h:236 | 228 | 问题扫描/_cache/L10.md:231 |
| docs/contracts/DATA_ARTIFACTS.md:150 | 104 | 问题扫描/_cache/L10.md:247 |
| docs/science/DRIZZLE.md:186 | 161 | 问题扫描/_cache/L16.md:351 |
| docs/science/REJECTION.md:155 | 149 | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.md:13 |

## 三、path::符号 的符号在被引文件内 0 命中（31 种）
| 引用 | 首次出现 |
|---|---|
| tests/io/test_fits_stream_contract.py::test_hips_rewriter_drops_bad_keyword | 问题扫描/_cache/F00_FRONT_SPOTCHECKS.md:107 |
| test_monitor_contract.py::_compile_probe | 问题扫描/_cache/L23.md:29 |
| docs/contracts/TEST_MATRIX.md::Drizzle | 问题扫描/_cache/L23.md:137 |
| tests/backend/test_isa_avx.py::TestISA002AvxSubset | 问题扫描/_cache/L23.md:262 |
| tests/monitoring/test_monitor_contract.py::_compile_probe | 问题扫描/_cache/L23.md:372 |
| lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp::parseBlockIndex | 问题扫描/_cache/L24.md:214 |
| tools/gen_v19_evidence.py::SKIP_PARTS | 问题扫描/_cache/L25.md:87 |
| orchestrator.cpp::compute_pixel_scale_arcsec | 问题扫描/_cache/L26.md:243 |
| lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp::solve_pix2world_iterative | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:278 |
| lib/astro_image_io/src/hips/aio_hips_writer.cpp::write_tile_core | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:279 |
| lib/healpix_db/healpix_drizzle/spherical_overlap.cpp::compute_drop_corners | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:280 |
| lib/orchestrator/cpp/include/json_config.h::DrizzleConfig | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:281 |
| lib/phase3_session/p3_wcs.cpp::p3_wcs_validate_descriptor | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:282 |
| lib/gaia_xpsd_client/src/gaia_client.c::record | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:283 |
| runtime/io/fits_core.c::fio_write_hdu | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:284 |
| lib/gaia_xpsd_client/src/module_entry.c::build_result_json | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:285 |
| lib/plate_solve/cpp/ipv/include/ipv_types.h::IPVSelectParams | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:286 |
| lib/gaia_xpsd_client/include/astrocs/gaia/types.h::GaiaStar | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:287 |
| lib/phase2/src/rejection.cpp::gather | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:288 |
| cli/parser.cpp::kAllowedFlags | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:289 |
| cli/commands.cpp::cmd_benchmark_cpu | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:290 |
| cli/runtime_client.cpp::exit_code_from_error | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:291 |
| tests/backend/test_p3_projection_oracle.py::car_ | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:292 |
| tests/unit/gaia_xpsd_fixture_gen.c::xpsd_write_test_file | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:293 |
| tools/quality/check_serial_heavy.py::docstring | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:294 |
| cli/commands.cpp::cmd_bench_cpu | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:295 |
| tools/check_module_readmes.py::docstring | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:296 |
| tests/test_index.csv::SCI | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:297 |
| docs/contracts/RT-001.md::DOC | 问题扫描/_merge/ANCHOR_VERIFY_REPORT.md:298 |
| aio_hips_writer.cpp::write_tile_core | 问题扫描/findings/E_TRACE_BREAK/p1/M2a_L04_L10.md:12 |
| docs/contracts/INDEX.yaml::entries | 问题扫描/findings/E_TRACE_BREAK/p1/M6b_L16_L18.md:41 |

## 处置纪律
一、多为上游项目相对路径、旧名或影子树，逐条判定改锚还是撤条；二、必须改为 path::符号；三、符号 0 命中是最强假阳信号：或定义在他处（补文件），或根本不存在（撤条）。
