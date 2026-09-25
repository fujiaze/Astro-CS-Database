
## 4 主题正本清单

一个主题一份正本。"正本"= 该主题的口径、判据、数值语义在此唯一承载；其余文档只允许出现指针或该层特有的展开（推导、端口、门读数），不得复述口径句子本身。

| 主题 | 唯一正本 | 上游（最高设计） | 同主题的其他载体（只留指针） |
|---|---|---|---|
| 科学范围与标度词表 | `docs/science/SCIENCE_SCOPE.md` | §2、§3.3 | `docs/science/UNIFIED_SCIENCE_MODEL.md` 的目标分层节 |
| 统一科学模型与阶段间语义 | `docs/science/UNIFIED_SCIENCE_MODEL.md` | §3.1 | `docs/contracts/DATA_SEMANTICS.md`（字段面）、`docs/design/UNIFIED_MODEL.md`（对象面） |
| 测光定标到星等坐标系（创新点①） | `docs/science/PHOTOMETRY.md` | §2.1 | `docs/plugins/algorithms_phase1/06_photometry.md`、`docs/research/PHOTOMETRY_RESEARCH_PACK.md`、`docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md` |
| 跨帧绝对 SNR 与权重（创新点②） | `docs/science/CONTROL_WEIGHT_SNR.md` | §2.2、§5.3 | `docs/science/PSF_SIGNAL_WEIGHT.md`（并入前者）、`docs/plugins/algorithms_phase1/07_noise_snr.md`、`实验/absolute-snr/**` |
| 加性天光与无接缝叠加（创新点③） | `docs/science/PHASE2_UPM.md` | §2.3、§5.4 | `docs/plugins/algorithms_phase2/11_upm.md`、`docs/algorithms/UPM_SOLVER.md`、`docs/algorithms/PHASE2_UPM_IMPL.md`、`实验/additive-sky-seamless/**` |
| HEALPix 精确面积交叠分配（创新点④） | `docs/science/DRIZZLE.md`（口径）+ `docs/algorithms/DRIZZLE_GEOMETRY.md`（几何与门） | §2.4 | `docs/plugins/algorithms_phase1/08_drizzle.md`、`docs/modules/` 对应页、`实验/healpix-polar/**` |
| 噪声模型与背景方差 | `docs/science/NOISE_MODEL.md` | §2.2、§4.2 | `docs/algorithms/NOISE_ESTIMATION.md`、`docs/contracts/DATA_SEMANTICS.md` |
| 不确定度与协方差传播 | `docs/science/UNCERTAINTY_AND_COVARIANCE.md` | §3.1 | `docs/algorithms/` 各传播节 |
| 点源信息与 PSF 权重 | `docs/science/PSF.md` 与 `docs/science/CONTROL_WEIGHT_SNR.md`（各守各自域） | §2.2、§4.2 | `docs/algorithms/STAR_PSF_ALGORITHMS.md`、`docs/plugins/algorithms_phase1/04_psf.md` |
| 星点检测 | `docs/science/STAR_DETECTION.md` | §4.2 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md`、`docs/plugins/algorithms_phase1/03_star_detection.md` |
| 定标（偏置/暗场/平场） | `docs/science/CALIBRATION.md` | §4.2 | `docs/algorithms/CALIBRATION_ALGORITHMS.md`、`docs/plugins/algorithms_phase1/01_calibration.md` |
| 外观缺陷修正 | `docs/algorithms/COSMETIC_ALGORITHMS.md`，其科学条款上收 `docs/science/CALIBRATION.md` | §4.2 | `docs/plugins/algorithms_phase1/02_cosmetic.md` |
| 逐像素排异与档位 | `docs/science/REJECTION.md`（口径）+ `docs/algorithms/PHASE2_REJECTION.md`（实现级） | §5.5 | `docs/algorithms/REJECTION_ALGORITHMS.md`（并入后者）、`docs/plugins/algorithms_phase2/12_rejection.md`、`docs/architecture/DATA_FLOW.md` |
| 加权积分与叠加 | `docs/science/INTEGRATION.md`（口径）+ `docs/algorithms/PHASE2_INTEGRATION.md`（实现级） | §5.2 | `docs/algorithms/INTEGRATION_ALGORITHMS.md`（并入后者）、`docs/plugins/algorithms_phase2/13_integration.md` |
| 采样与重采样 | `docs/algorithms/PHASE2_SAMPLER.md`、`docs/algorithms/PHASE3_RESAMPLE.md` | §5.2、§6.3 | `docs/algorithms/PHASE3_RSMP_IMPL.md`（并入 PHASE3_RESAMPLE 的实现级节）、`docs/plugins/algorithms_phase3/15_resample.md` |
| 覆盖率与权重面 | `docs/algorithms/PHASE2_COVERAGE.md` | §5.2 | `docs/plugins/algorithms_phase2/09_coverage.md` |
| 天体测量与投影 | `docs/science/ASTROMETRY.md`（口径）+ `docs/science/PHASE3_HIPS_TO_FITS.md`（产品面） | §6.3 | `docs/algorithms/PHASE3_PROJ_IMPL.md`、`docs/plugins/algorithms_phase3/14_projection.md` |
| 板解 | `docs/algorithms/PLATESOLVE.md` | §4.2 | `docs/plugins/algorithms_phase1/05_platesolve.md` |
| Gaia 星表查询 | `docs/algorithms/GAIA_QUERY.md` | §3.3 | `docs/modules/` 对应页、`lib/infrastructure/gaia_xpsd_client/` |
| HiPS 写入与产品落盘形态 | `docs/design/PRODUCT_STORAGE_FORM.md`（形态）+ `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md`（字段词表） | §10 | `docs/algorithms/HIPS_WRITER.md`、`docs/plugins/algorithms_phase2/` |
| ACR 等价性 | `docs/science/ACR_EQUIVALENCE.md` | §3.3、§12 | `docs/research/` |
| 三命令与命令树 | 最高设计 §7.1（条款自身即正本） | §7.1 | `docs/interfaces/cli/`（命令面投影，只留可校验部分） |
| 退出码与错误语义 | `docs/contracts/LOG_AND_ERROR_CONTRACT.md`（说明）+ `lib/infrastructure/cli/exit_codes.h`（数值源） | §7.2 | `docs/interfaces/cli/`、`docs/architecture/ERROR_MODEL.md` |
| 事件流与结构化日志 | `docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md`（格式）+ `docs/contracts/LOG_AND_ERROR_CONTRACT.md`（域→码） | §7.2、§7.3 | `docs/design/LOG_AND_ERROR_SYSTEM.md`、`docs/plugins/infrastructure/21_observability.md` |
| 配置三类与默认值 | `docs/contracts/CONFIG_CONTRACT.md`（语义）+ `eng/packaging/config/defaults.json`、`filters.json`（数值） | §3.2、§7.2 | `docs/development/CONFIG_SCHEMA.md`（并入 contracts 侧）、`docs/plugins/infrastructure/18_cli.md` |
| 资源门与执行档位 | `docs/architecture/EXECUTION_MODEL.md`（模型）+ `eng/contracts/resource_gate_v1.json`（数值） | §9 | `docs/plugins/infrastructure/19_runtime.md`、`20_benchmark.md` |
| CPU 后端、ISA 变体与加载校验 | `docs/architecture/CPU_BACKEND_ARCH.md` + `docs/architecture/cpu/` | §9 | `docs/architecture/ISA_VARIANTS.md`（含位操作变体节）、`docs/standards/C_ABI_STANDARD.md` |
| 线程与线程预算 | `docs/architecture/THREADING_MODEL.md`（含预算节） | §8.2、§9 | `docs/standards/CONCURRENCY_STANDARD.md`、`docs/plugins/infrastructure/19_runtime.md` |
| 命名块管线与块生命周期 | `docs/architecture/PIPELINE.md`（架构）+ `docs/contracts/PIPELINE_BLOCK_CONTRACT.md`（字段） | §8.2 | `eng/contracts/schemas/pipeline_block.schema.json`、`docs/interfaces/` |
| 三阶段调度器 | `docs/contracts/SCHEDULER_CONTRACT.md` | §8.1、§8.3 | `docs/interfaces/`、`docs/modules/` 调度器页 |
| 对外 ABI 与符号面 | `docs/contracts/PUBLIC_API.md`（签名与语义）+ `docs/interfaces/abi/`（协议面） | §8.5 | `docs/standards/C_ABI_STANDARD.md`、`docs/standards/API_STANDARD.md`（纪律）、`docs/architecture/api_inventory.csv`（登记面） |
| I/O 边界与原子性 | `docs/architecture/IO_AND_ATOMICITY.md`（唯一 I/O 边界归属） | §10 | `docs/interfaces/io/`、`docs/standards/IO_STANDARD.md`、`docs/architecture/ASYNC_IO_CONTRACT.md` |
| 模块划分与映射 | `docs/architecture/MODULE_MAP.md`（说明）+ `docs/modules/MODULE_MAP.yaml`（机器面） | §8.4、§8.5 | `docs/modules/**`、`lib/**/module.yaml` |
| 依赖与构建图 | `docs/architecture/BUILD_GRAPH.md` + `DEPENDENCIES.md`（工具链与第三方版本） | §8.4、§11 | `docs/development/`、`eng/cmake/` |
| 兼容性与双平台发行 | `docs/architecture/COMPATIBILITY_POLICY.md` + `docs/standards/RELEASE_STANDARD.md` | §11 | `docs/ci/`（平台档门）、`docs/owner/` |
| 数值与浮点容差 | `docs/standards/NUMERIC_STANDARD.md` | §5.2、§12 | `docs/algorithms/GATES_AND_TOLERANCES.md`、`docs/contracts/TEST_MATRIX.md` |
| 门阈值与容差登记 | `docs/algorithms/GATES_AND_TOLERANCES.md`（科学阈值）+ `docs/ci/03_GATES.md`（门清单） | §12.4 | `docs/standards/`、`docs/quality/`（读数快照出库） |
| 机器门与流水线 | `docs/ci/CI_SPEC.md`（规范）+ `docs/ci/01_CHECKS.md`、`02_PIPELINE.md`、`04_ARTIFACTS.md` | §12 | `eng/ci/checks.json`（机器清单） |
| 科学验证冻结与负例策略 | `docs/standards/`（验证标准）+ `docs/algorithms/GATES_AND_TOLERANCES.md`（判据值） | §12.1、§12.2 | `docs/validation/**`（结果矩阵落 `artifacts/evidence/`） |
| 实验单元与报告八要素 | 最高设计 §12.3（条款） | §12.3 | `实验/**`、`docs/standards/TEST_STANDARD.md` |
| 四层验收与发布门 | `ACCEPTANCE_SPEC.md` §1 | §12.4、§13 | `docs/ci/03_GATES.md`（CI 中档）、`docs/owner/RELEASE_STATUS.md`（投影面） |
| 状态阶梯（状态词） | 最高设计 §12.5（唯一语义源） | §12.5 | `docs/ci/03_GATES.md`、`docs/owner/RELEASE_STATUS.md`、`docs/owner/SCIENCE_OVERVIEW.md` 只引用不扩档 |
| 版本与命名空间 | `docs/VERSIONING.md` + 根 `VERSION`（单值源） | §13 | `docs/owner/`、`eng/ci/check_version.py` |
| 术语与符号 | `docs/GLOSSARY.md`（词表）+ `docs/science/` 各页符号表节（科学符号族） | §0.2、附录 A | 全仓其他文档只引用 |
| 限制与未决 | `docs/KNOWN_LIMITATIONS.md` | §1.3、§12 | `docs/science/**` 的失效条件段以指针上收 |
| 许可证合规与参考实现对照 | `docs/standards/LICENSE_COMPLIANCE.md`（待立：由 `docs/science/**` 末尾的参考代码库块与 `docs/references/SCIENTIFIC_REFERENCES.md §M` 收口而成）+ `eng/packaging/licenses/LICENSE-INDEX.txt`（机器面） | §11、附录 B | `docs/science/**` 末尾的参考代码库块改为指针 |
| 文献与外部标准条目 | `docs/references/SCIENTIFIC_REFERENCES.md`（登记）+ `docs/standards/STANDARDS_REGISTRY.md`（规范版本锚） | 附录 B | `docs/research/**` 各研究包 |
| 文档集索引 | `docs/DOCUMENT_INDEX.yaml` | §0.2 | `docs/contracts/INDEX.yaml`（合同 ID 图，另立登记面） |
| 追溯映射 | `docs/traceability/TRACEABILITY_MATRIX.csv`（与同名 `.json` 为同一事实的人读/机读两态） | §12 | `docs/TRACEABILITY.csv`（并入前者） |
| 排障与诊断 | `docs/diagnostics/TROUBLESHOOTING.md` | §7.3、§12 | `docs/TROUBLESHOOTING.md`（并入前者） |

## 5 四层映射

从最高设计每一机制节出发，沿索引必须能到达该主题在第 2、3 层的正本与第 4 层的机器事实源。缺列即为权威链断点：先在上位补要点，再在下位留指针。

| 最高设计节 | 第 2 层 详细设计 | 第 3 层 具体设计/模块页 | 第 4 层 代码与机器合同 |
|---|---|---|---|
| §1.2 三命令三产品 | `docs/design/PHASE1_DETAILED_DESIGN.md`、`PHASE2_DETAILED_DESIGN.md`、`PHASE3_DETAILED_DESIGN.md`；`docs/architecture/ARCHITECTURE.md` | `docs/interfaces/cli/` | `lib/infrastructure/cli/`、`lib/phase1_session/`、`lib/phase2_session/`、`lib/phase3_session/` |
| §2.1 创新点① | `docs/science/PHOTOMETRY.md` | `docs/algorithms/PHOTOMETRIC_FIT.md`；`docs/plugins/algorithms_phase1/06_photometry.md` | `lib/algorithms/photometry/`、`docs/contracts/unified_object_registry.json` |
| §2.2 创新点② | `docs/science/CONTROL_WEIGHT_SNR.md`（含点源信号权重节）、`docs/science/NOISE_MODEL.md` | `docs/algorithms/NOISE_ESTIMATION.md`；`docs/plugins/algorithms_phase1/07_noise_snr.md` | `lib/algorithms/noise_snr/`、`eng/contracts/schemas/unified/variance.schema.json` |
| §2.3 创新点③ | `docs/science/PHASE2_UPM.md` | `docs/algorithms/UPM_SOLVER.md`、`docs/algorithms/PHASE2_UPM_IMPL.md`；`docs/plugins/algorithms_phase2/11_upm.md` | `lib/algorithms/upm/`、`eng/contracts/schemas/` |
| §2.4 创新点④ | `docs/science/DRIZZLE.md` | `docs/algorithms/DRIZZLE_GEOMETRY.md`；`docs/plugins/algorithms_phase1/08_drizzle.md` | `lib/algorithms/drizzle/`、`lib/algorithms/coverage/` |
| §3.1 数据对象 | `docs/design/UNIFIED_MODEL.md`、`docs/contracts/UNIFIED_OBJECTS.md` | `docs/algorithms/anchors/ANCHOR_CONTRACT.md` | `eng/contracts/schemas/unified/*.schema.json`、`eng/contracts/data/artifact_types.registry.json` |
| §3.2 三类配置 | `docs/contracts/CONFIG_CONTRACT.md` | `docs/development/`（配置操作面） | `eng/contracts/schemas/phase_config_*.schema.json`、`eng/packaging/config/` |
| §3.3 科学量与星表 | `docs/science/ASTROMETRY.md`、`docs/science/ACR_EQUIVALENCE.md` | `docs/algorithms/GAIA_QUERY.md`；`docs/modules/` gaia 页 | `lib/infrastructure/gaia_xpsd_client/` |
| §4 normalize | `docs/design/PHASE1_DETAILED_DESIGN.md` | `docs/plugins/algorithms_phase1/01..08`、`docs/algorithms/CALIBRATION_ALGORITHMS.md`、`COSMETIC_ALGORITHMS.md`、`STAR_DETECTION_ALGORITHMS.md`、`STAR_PSF_ALGORITHMS.md`、`PLATESOLVE.md`、`HIPS_WRITER.md` | `lib/algorithms/{calibration,cosmetic,star_detection,psf,platesolve,photometry,noise_snr,drizzle}/` |
| §5 mosaic | `docs/design/PHASE2_DETAILED_DESIGN.md` | `docs/plugins/algorithms_phase2/09..13`、`docs/algorithms/PHASE2_COVERAGE.md`、`PHASE2_SAMPLER.md`、`PHASE2_UPM_IMPL.md`、`PHASE2_REJECTION.md`、`PHASE2_INTEGRATION.md`、`PHASE2_MOSAIC_WRITE.md` | `lib/algorithms/{coverage,sampling,upm,rejection,integration}/` |
| §6 export | `docs/design/PHASE3_DETAILED_DESIGN.md` | `docs/plugins/algorithms_phase3/14..16`、`docs/algorithms/PHASE3_RESAMPLE.md`、`PHASE3_PROJ_IMPL.md`、`PHASE3_FITS_IMPL.md` | `lib/algorithms/{projection,resample,fits_output}/` |
| §7.1 命令树 | `docs/interfaces/cli/` | `docs/plugins/infrastructure/18_cli.md` | `lib/infrastructure/cli/command_tree.h` |
| §7.2 配置·事件·退出码 | `docs/contracts/CONFIG_CONTRACT.md`、`docs/contracts/LOG_AND_ERROR_CONTRACT.md` | `docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md` | `lib/infrastructure/cli/`、`eng/contracts/schemas/jsonl_event_v1.schema.json` |
| §7.3 错误传播与运行日志 | `docs/design/LOG_AND_ERROR_SYSTEM.md` | `docs/diagnostics/TROUBLESHOOTING.md`、`docs/plugins/infrastructure/21_observability.md` | `lib/infrastructure/observability/` |
| §8.1 唯一入口与阶段调度 | `docs/architecture/ARCHITECTURE.md` | `docs/contracts/SCHEDULER_CONTRACT.md` | `lib/infrastructure/scheduler/` |
| §8.2 命名块与生命周期 | `docs/architecture/PIPELINE.md` | `docs/contracts/PIPELINE_BLOCK_CONTRACT.md`、模块页端口节 | `lib/infrastructure/pipeline/`、`eng/contracts/block_flow/stage_block_flow.json` |
| §8.3 三阶段调度器 | `docs/architecture/EXECUTION_MODEL.md` | `docs/contracts/RT-001.md` | `lib/infrastructure/scheduler/` |
| §8.4 顶层结构（仓库布局） | `docs/architecture/MODULE_MAP.md`、`BUILD_GRAPH.md`、`DEPENDENCY_RULES.md` | `docs/ci/`（布局一致性门） | 根 `CMakeLists.txt`、`eng/cmake/` |
| §8.5 模块与 ABI | `docs/architecture/OWNERSHIP_AND_LIFETIME.md` | `docs/contracts/PUBLIC_API.md`、`docs/interfaces/abi/`、`docs/standards/C_ABI_STANDARD.md` | `lib/include/astrocs/`、`lib/**/module.yaml`、`eng/contracts/data/aio_abi_contract_v1.json` |
| §9 CPU 后端与资源 | `docs/architecture/CPU_BACKEND_ARCH.md`、`THREADING_MODEL.md`、`PERFORMANCE_MODEL.md`、`CACHE_POLICY.md` | `docs/architecture/cpu/`、`docs/standards/BENCHMARK_STANDARD.md` | `lib/infrastructure/benchmark/`、`eng/contracts/resource_gate_v1.json` |
| §10 I/O 与原子产品 | `docs/architecture/IO_AND_ATOMICITY.md`、`docs/design/PRODUCT_STORAGE_FORM.md` | `docs/interfaces/io/`、`docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md`、`docs/contracts/DATA_ARTIFACTS.md` | `lib/infrastructure/aio/`、`eng/contracts/data/artifact_manifest.schema.json` |
| §11 双平台发行 | `docs/architecture/COMPATIBILITY_POLICY.md` | `docs/standards/RELEASE_STANDARD.md`、`docs/operations/` | `eng/packaging/`、`eng/ci/check_version.py` |
| §12.1–12.3 验证与实验单元 | `docs/science/SCIENCE_SCOPE.md`（失效条件） | `docs/standards/`（验证标准）、`docs/algorithms/GATES_AND_TOLERANCES.md` | `eng/tests/`、`实验/<单元>/code/`、`实验/<单元>/results/` |
| §12.4 四层验收 | `ACCEPTANCE_SPEC.md` | `docs/ci/03_GATES.md` | `eng/ci/checks.json`、`eng/ci/run.py` |
| §12.5 状态阶梯 | `docs/owner/RELEASE_STATUS.md`（投影，不扩档） | `docs/ci/03_GATES.md`（CI 中档列） | `eng/ci/`（状态计算） |
| §13 版本与发布权 | `docs/VERSIONING.md` | `docs/standards/RELEASE_STANDARD.md` | 根 `VERSION`、`eng/ci/check_version.py` |
| 附录 A 术语 | `docs/GLOSSARY.md` | 模块页术语节 | — |
| 附录 B 外部标准 | `docs/references/SCIENTIFIC_REFERENCES.md` | `docs/standards/STANDARDS_REGISTRY.md` | — |
