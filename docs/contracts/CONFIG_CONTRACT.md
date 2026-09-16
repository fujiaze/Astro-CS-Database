# 配置合同（全局默认值 · 滤镜库 · 三命令 phase_config · cpu_profile · run_manifest）

> ID: CFG-001　状态: 建立（2026-09-16）　任务: 工程控制/PROJECT-GOVERNANCE-01/tasks/CFG-001.md
> 权限边界：本文件只承载**配置语义与索引**（文件域 docs/contracts/**）；schema 本体在 `contracts/schemas/`，配置数据在 `config/`。
> 本文件**不新增**任何科学定义、默认数值或容差；所有数值只能转录，缺权威数值者显式 `pending_authority`。

## 0 权威链

| 事项 | 权威 |
|---|---|
| 输入合同（config + inputs 数据块、程序根 config/） | `ASTROCS_DESIGN.md` §3.3（:111-146） |
| 运行前预检（绿/橙/红、error 强制阻断） | `ASTROCS_DESIGN.md` §3.5（:166-191） |
| 配置挂载 / 模板 / 机器输出 / 退出码 | `ASTROCS_DESIGN.md` §6.1-§6.3（:277-340） |
| 三类配置严格分离、benchmark 独占 cpu_profile | `docs/design/UNIFIED_MODEL.md` §3（:54-66） |
| CLI 预检与模板职责 | `docs/plugins/infrastructure/18_cli.md` §3-§5 |
| 科学红线（默认容差不可改、cpu_profile 不进科学配置） | `ENGINEERING_SPEC.md` §3（:23-28） |
| 字段名族与跨类禁止名锚点 | `docs/contracts/config_separation_anchors.json`（DATA-001，只读） |

## 1 三类配置（现场清单）

| 类 | 文件 | 写入者 | 语义 | 禁止 |
|---|---|---|---|---|
| phase_config | `contracts/schemas/phase_config_normalize.schema.json`、`contracts/schemas/phase_config_mosaic.schema.json`、`contracts/schemas/phase_config_export.schema.json`；模板 `config/templates/normalize.phase_config.json`、`config/templates/mosaic.phase_config.json`、`config/templates/export.phase_config.json` | 用户 / `cli --template` | 科学参数 · 输入路径 · output_dir · 算法选择；可跨机器复现 | 任何硬件字段（isa/isa_level/workers/worker_count/block/block_size/cpu_model/cpu_vendor/thread_budget/affinity_mask） |
| cpu_profile | `contracts/schemas/cpu_profile.schema.json` | **仅 benchmark** | ISA/workers/block + CPU/OS/软件版本/provider hash 机器绑定 | phase_config 的 `sci_*`/`algorithm_*` 字段 |
| run_manifest | `contracts/schemas/run_manifest.schema.json` | 运行时（每次运行冻结） | 源码 SHA / 配置哈希 / 输入输出哈希 / 工具链版本 | 科学参数与硬件调优字段 |

- DATA-001 锚点里保留的 phase_config 聚合 schema 路径，由本任务实现为**三份 phase 专属 schema**：`contracts/schemas/phase_config_normalize.schema.json`、`contracts/schemas/phase_config_mosaic.schema.json`、`contracts/schemas/phase_config_export.schema.json`；**未**创建同名聚合文件，以免留下第二份等价定义（UNIFIED_MODEL §3、裁决四口径）。回归锁：`tests/config/test_cfg001_contracts.py::TestPhaseConfigFamily::test_no_aggregate_second_definition`。

## 2 `config/defaults.json`（44 字段；`astrocs.config-defaults/v1`）

- 结构：`fields[]`，每项 `{key, value, unit, constraint, authority_status, source, source_ref, pending_task, note}`；
  `authority_status ∈ {sourced, owner_adjudicated, pending_authority}`。
- 机器门（`TestDefaultsContract`）：字段数 == 带 unit 数 == 带 source 或 pending_authority 数，来源不明字段 == 0；
  且**非 pending 字段的 unit 不得为 unspecified**；每个 `source_ref` 的文件:行必须真实存在，11 个关键锚点逐行核对 token。
- 分组与来源（只转录，不改数值）：

| 组 | 字段数 | 权威锚点 |
|---|---|---|
| calibration（暗场-亮场曝光容差） | 1 | 负责人裁决 2026-09-16（5 s）；待落 `docs/science/CALIBRATION.md`（SCI-RES-01/R-003 定义 + R-004 落文档） |
| detection（σ 检测阈值） | 1 | `docs/science/STAR_DETECTION.md:18` |
| psf（默认模型/β） | 2 | `docs/science/PSF.md:7`、`:92` |
| noise（噪声模型默认配置） | 8 | `docs/algorithms/NOISE_ESTIMATION.md:134`（另见 :11/:20/:36/:100/:151/:154） |
| rejection（排异阈值表） | 18 | `docs/science/REJECTION.md:131` |
| photometry（mag_tolerance / Tukey c / IRLS / 最小星数） | 6 | `docs/science/PHOTOMETRY.md:21,24,38,39` |
| weight（默认权重模式） | 1 | `docs/science/PSF_SIGNAL_WEIGHT.md:12`、`:28` |
| precision（默认精度） | 1 | `docs/science/SCIENCE_SCOPE.md:53` |
| upm（k_corr） | 1 | `docs/science/PHASE2_UPM.md:100` |
| hips（tile 宽） | 1 | `docs/science/PHASE3_HIPS_TO_FITS.md:27` |
| drizzle（pixfrac） | 1 | 语义 `docs/science/DRIZZLE.md:21,27,31`；**数值 pending** |
| sparse_snr / scalar_gate | 3 | **数值 pending** |

- **pending_authority 四项（禁止编造，值必须为 null）**：

| key | unit | 归属任务 | 依据 |
|---|---|---|---|
| `drizzle.pixfrac` | `1` | SCI-RES-01/R-005 | 语义权威 `docs/science/DRIZZLE.md`（(0,1]、half=0.5·pixfrac）与 `docs/algorithms/DRIZZLE_GEOMETRY.md`（:57,:61,:102 严格校验不夹逼）；**数值默认 0.8 无科学权威出处**——现实现默认 0.8 见 `lib/infrastructure/pipeline/orchestrator/configs/stage1.template.json:41`（实现模板，非科学权威），旧文档 `docs/development/CONFIG_SCHEMA.md:84` 同值。GAP-023 |
| `sparse_snr.density` | `点/度²` | SCI-RES-01/R-001 + 负责人批准 | ASTROCS_DESIGN §3.3 点名；`docs/science/**`、`docs/algorithms/**` 全库无数值（GAP-024）；单位见 `docs/plugins/algorithms_phase1/07_noise_snr.md:65` |
| `scalar_gate.rd` | unspecified | SCI-RES-01/R-002 + 负责人批准 | GAP-024；`07_noise_snr.md:61` 字段名，默认/单位列均为 —— |
| `scalar_gate.trend` | unspecified | SCI-RES-01/R-002 + 负责人批准 | 同上（`07_noise_snr.md:62`） |

- `calibration.dark_light_exposure_tolerance = 5 s`：负责人已裁决值，按裁决**不标** pending；文档侧仅有 `K=t_light/t_dark` 语义（`docs/science/CALIBRATION.md:19,21,90`），数值待 SCI-RES-01/R-004 落 `docs/science/CALIBRATION.md`。
- 负空间（未收录，避免越权）：`docs/plugins/algorithms_phase1/07_noise_snr.md:63-65` 的 `psfsw_enable`/`sparse_snr_layer` 等 plugin 级默认**不进** defaults.json——本任务要求 source 指向 docs/science 或 docs/algorithms，plugin 文档不在该 source 规则内（见 §8 未决项）。

## 3 三命令 phase_config 与模板

结构 = `ASTROCS_DESIGN.md` §3.3 的 JSON 数据块：`{phase_name, config, inputs}`；`config`/`inputs` 两级 `additionalProperties:false`。

| phase | 模板 | config 必填 | 可选算法选择（逐项权威） | inputs 项 |
|---|---|---|---|---|
| normalize | `config/templates/normalize.phase_config.json` | output_dir, precision | `algorithm_psf_model`（`docs/science/PSF.md:7,:81,:105`，当前唯一实现 Moffat4）；`sparse_snr_layer`（`ASTROCS_DESIGN.md` §3.4:161-164）；**`algorithm_drizzle_pixfrac`**（字段收录；语义/值域权威 `docs/science/DRIZZLE.md:21,:27,:31` + `docs/algorithms/DRIZZLE_GEOMETRY.md:57,:61,:102`，schema 机器强制 `0 < pixfrac <= 1`；**不声明数值默认**——默认值槽在 defaults.json 的 `drizzle.pixfrac`，状态 pending_authority/SCI-RES-01/R-005） | `{light, bias?, dark?, flat?, cosmetic?, filter}`，filter 必须命中滤镜库 |
| mosaic | `config/templates/mosaic.phase_config.json` | output_dir, precision | `algorithm_weight_mode`（`docs/plugins/algorithms_phase2/13_integration.md:67`；点源默认语义 `docs/science/PSF_SIGNAL_WEIGHT.md:28`）、`algorithm_rejection_method`（`docs/science/REJECTION.md:131-132`）、`algorithm_upm_gauge`（`docs/plugins/algorithms_phase2/11_upm.md:36`） | `{product, filter?}` |
| export | `config/templates/export.phase_config.json` | output_dir, precision, output_mode, wcs | `output_mode`（`ASTROCS_DESIGN.md` §5.3:267；默认 `surface_brightness` 见 `docs/plugins/algorithms_phase3/16_fits_output.md:41`）、`wcs.projection`（§5.3:264-266 首批 8 种 + 缺省 TAN；`14_projection.md:31`）、`wcs.{rotation_deg, crpix_px}`（`14_projection.md:32,35`） | `{product}` |

- `precision` **必填**（`ASTROCS_DESIGN.md` §3.3:145「precision（FP32/FP64）显式声明」），值域 {fp32, fp64}；模板填 `fp64`（`docs/science/SCIENCE_SCOPE.md:53` 默认 FP64）。
- export 几何字段名与值域取自科学权威：`center_deg`/[`s_out_deg`]/`width_px`/`height_px`（`docs/science/PHASE3_HIPS_TO_FITS.md:29-31`；约束 `:48`：abs(dec) ≤ 85°、W_out/H_out ∈ [1,20000]、s_out > 0）。原 `projection` 插件文档的 `mode` 在 phase_config 中命名为 `output_mode`，以避开 legacy cpu_profile v1 的 `mode` 字段名（UNIFIED_MODEL §3 禁止同名异义，见 §7 门表）。
- **模板示例值声明**：模板中的 `path/to/...` 路径、`filter: "Baader R"`、export 的 `center_deg: [0,0]` 与 `s_out_deg: 0.001`、`width_px/height_px: 512` 都是**示例占位**（用户必须按观测改写），**不是**科学默认值；除 `precision`/`output_mode`/`projection`/`rotation_deg` 等有权威默认者外，模板不主张任何数值默认。`width_px/height_px = 512` 与 HiPS tile 默认（`PHASE3_HIPS_TO_FITS.md:27`）同值，仅作可运行的示例几何。
- 硬约束：`config`/`inputs` 两级拒绝任何未登记字段 ⇒ cpu_profile 的 `workers`/`isa`/`block_size` 混入必失败（负例 ④）。
- 内存/流式预算类字段（`docs/plugins/algorithms_phase3/16_fits_output.md:38-39` 的 `band_height`/`tile_cache_mb`）**不进** phase_config：它们不可跨机器复现，属实现策略，按 UNIFIED_MODEL §3 不得写入科学配置（见 §8 未决项）。

## 4 `config/filters.json`（`astrocs.filter-library/v1`）

- **逐字转录** `lib/algorithms/photometry/data/response_curves/filters.json`（45 条；字段 `name/channel/wavelength_nm/value/n_points`），未重采样、未插值、未改数值；机器门逐条与源文件比对（`TestFiltersLibrary::test_verbatim_transcription`）。
- **provenance**：指向 `lib/algorithms/photometry/cpp/test/filter_qe_provenance.json`；其 source 自述 `unverified: original curve source not recorded in repository`，本库 **45/45 如实标注** `status=unverified`、`verified=false`、`url=null`、`gap_id=GAP-025`；曲线统计量（`curve_stats`）由机器门与曲线逐条对账。**补 provenance 属下一轮工程包专项**（原出处/版本/获取方式/校验和）。
- **未知滤镜 → error**：`lookup.unknown_filter = "error"`（`ASTROCS_DESIGN.md` §3.3:144、§3.5 红级）；三份 phase_config schema 的 `filter` 字段为 45 键 `enum`，并由机器门锁定 `enum == config/filters.json 的 filters 键`（`test_filter_enum_equals_library_keys`）。
- **不含每滤镜零点**（本任务裁决口径）：零点 `location` 是**逐次运行估计量**且满足**零点平移不变量**——`docs/science/PHOTOMETRY.md:7`（估计零点 location、尺度因子 scale…）与 `:73`（零点平移不变量：F_instr 同乘 k ⇒ location 增 log10 k，scale 除 k，sigma_residual 不变）；`docs/algorithms/PHOTOMETRIC_FIT.md:9` 明确 `zero_point` 字段因「无定义式、结构体无字段」被删除（P5-SNR 订正，负责人授权）。故滤镜库**不出现**任何零点列/占位/null 字段；机器门 `test_no_zero_point_column_anywhere` 对**键路径与全文**双向断言（大小写不敏感）。若将来需要每滤镜零点，属**新科学定义**，须负责人批准后另立任务。
- 未决（不当场选口径）：`ASTROCS_DESIGN.md` §3.3 的示例串 `"bader r"` 与本库键 `"Baader R"` 不同形 ⇒ 滤镜名匹配的大小写/归一化语义无权威定义；本库只声明 `match=exact`、`case_sensitive=true`，并把该语义登记为待裁决项（§8）。

## 5 `contracts/schemas/cpu_profile.schema.json`（迁移后：单一文件，两分支）

- **单一事实源**：不再有第二份等价定义；文件以 `oneOf` 定义
  - `$defs.legacy_v1`：BENCH-004 旧 JSON（`schema_version=1`，`kernels` 数组，`hardware/build/memory_benchmark/verdict`）；
  - `$defs.profile_v2`：**当前实现** `schema="astrocs.cpu-profile/v2"`（`lib/infrastructure/benchmark/backend_host/profile_gen_v2.cpp:558-610`、`verify_profile_v2` :645-707）。
- **绑定**：CPU（`host.vendor/family/model/stepping/xcr0`）· OS（`host.os_abi`）· 软件版本（`build.astrocs_version/source_commit/runtime_build_id`）· provider hash（`build.provider_build_ids`、`benchmark_binary_sha256`、`kernels.*.self_test_sha256`）· ISA/workers/block（`kernels.*.provider ∈ {baseline,avx2,avx512}`、`workers ≥ 1`、`block ≥ 1`）。身份绑定字段与 `check_profile_identity_v1`（`lib/infrastructure/benchmark/backend_host/cpu_routing.cpp:216-300`）逐项一致。
- **只有 benchmark 可写**：`x-astrocs-writer = "benchmark"`、`x-astrocs-not-writable-by = ["用户","cli --template","phase_config"]`；机器门 `TestCpuProfileMigration::test_writer_is_benchmark_only` + §7 的跨类不相交门。
- **兼容面（既有检查项不失效）**：顶层 `required` 取两分支共有键 `[build, kernels]`，顶层 `properties` 同时登记两分支顶层键，完整 v1 必填集落在 `$defs.legacy_v1.required`；`properties.kernels.items.required` 保持原访问路径（`tests/backend/test_cpu_profile.py:74-79`、`tools/validate_cpu_profile.py:41-70`）。
- **已知取舍（如实登记，需负责人知悉）**：顶层 `properties.kernels.type = ["array","object"]` 以同时容纳 v1 数组与 v2 对象，因此 `tools/validate_cpu_profile.py:65` 的 `rule.get('type')=='array'` 分支对该字段不再生效；v1 kernel 项约束仍由本文件 `items.required` 与 `$defs.legacy_v1` 强制，并由负例 `cpu_profile_v1_missing_required.json` 复跑证明仍能红。
- **v1 文本/实现漂移（`x-astrocs-v1-drift` 登记，5 条）**：`hardware.feature_bits`（原文本 array／实测 integer）、`hardware.xcr0`（原文本 string／实测 integer）、`build.backend_sha256`（原文本 object／实测 string）、`kernels[].block_size`（原文本 min 1／实测 0）、`kernels[].oracle_status`（canonical 大写 PASS／实测小写 pass，读取侧归一）。本文件按**实测口径**兼容两义，canonical 值不变；不新增也不删除既有检查项。

## 6 `contracts/schemas/run_manifest.schema.json`

- 冻结源码 SHA（`software_sha`，40hex）· 配置哈希（`config_hash`，sha256 64hex）· 输入/输出哈希（`manifest_input_hashes`/`manifest_output_hashes`，逐项 path+sha256，minItems 1、uniqueItems）· 工具链版本（`toolchain_version` 必填，`toolchain_compiler`/`toolchain_cmake` 可选）· `run_id` · `created_utc`。
- 字段名族取自 DATA-001 锚点（`^run_id$`、`^software_sha$`、`^config_hash$`、`^manifest_(input|output)_hashes$`、`^toolchain_[a-z0-9_]+$`）；`additionalProperties:false` 拒绝 `workers`/`isa`/`block` 等（负例复跑）。`input_hashes` 与 DATA-OBJ-PROVENANCE-001.provenance.input_hashes 同义（锚点 note）。

## 7 机器门清单（复跑命令）

```bash
# 主门（本任务新增，零第三方依赖）
timeout 300 python3 -m unittest discover -s tests/config -t tests/config
# 单例：模板通过 schema / 负例必败
timeout 60 python3 tests/config/run_validation.py contracts/schemas/phase_config_normalize.schema.json config/templates/normalize.phase_config.json
timeout 60 python3 tests/config/run_validation.py contracts/schemas/phase_config_normalize.schema.json tests/config/fixtures/negative/hardware_fields_in_phase_config.json
```

| 门 | 断言 | 测试 |
|---|---|---|
| 三模板通过对应 schema | 逐模板 `validate()==[]` 且 `phase_name` 相符 | `TestPhaseConfigFamily::test_templates_pass_their_schema` |
| 四个负例必败 | 未知滤镜/缺 output_dir/precision 越界/硬件字段混入，各自命中预期错误串 | `TestNegativeFixturesMustFail::test_negative_01..04` |
| defaults 计数 | 字段数 == 带 unit 数 == 带 source 或 pending 数；来源不明 == 0 | `TestDefaultsContract::test_counts_and_no_unknown_source` |
| 转录保真 | 11 个关键 source 锚点 (文件:行:token) 逐行成立；pending 值必为 null | `test_every_source_ref_resolves_and_key_anchors_hold`、`test_pending_items_are_the_adjudicated_gap_set` |
| 跨类不相交 | phase_config ∩ cpu_profile(v2) == ∅；∩ run_manifest == ∅；∩ cpu_profile(全体) == {precision}（登记） | `TestCrossClassDisjointness` |
| 滤镜库 | 45/45 逐字一致 + provenance unverified/GAP-025 + 无零点键 + enum==库键 | `TestFiltersLibrary` |
| cpu_profile | 单一定义、benchmark-only、v1/v2 双分支可绿、缺必有字段可红 | `TestCpuProfileMigration`、`test_cpu_profile_v1_missing_required_still_fails` |

## 8 未决项与归属（不在本任务实现）

1. **plugin 级默认值的入册口径**：`weight_mode=point_information`、`upm.gauge=reference_frame`、`output_mode=surface_brightness` 等默认来自 `docs/plugins/**`，而本任务 source 规则限定 docs/science|docs/algorithms ⇒ 目前只出现在模板与 §3 表，未进 defaults.json。归属：负责人裁决（放宽 source 规则或把 plugin 默认提升进 SCI/ALG 文档）。
2. **per-module knobs 的逐次覆盖**：`min_overlap`/`spacing`/`sigma_gate`/`max_iter`/`band_height`/`tile_cache_mb` 等未进 phase_config（`additionalProperties:false` 会拒绝）。归属：负责人裁决后另立任务（不得由 Agent 自选字段）。
3. **滤镜名匹配语义**：`"bader r"`（设计示例）vs `"Baader R"`（库键）的大小写/归一化语义无权威定义。归属：负责人裁决 + CLI-002 实现。
4. **cpu_profile `host.os_abi` 值域**：只约束非空字符串（schema 内 `x-astrocs-pending-authority` 已登记）；枚举值域待权威面冻结。
5. **legacy v1 同名登记**：`kernels[].precision`（v1）与 phase_config `precision` 同名同义（fp32/fp64，非异义冲突），已用机器门把交集锁定为 `{precision}`；如需彻底消除，需迁移 v1 producer（lib/**，本任务文件域外）。
6. **登记面（本任务文件域外，须由调度线/前台补齐）**：`docs/DOCUMENT_INDEX.yaml` 登记本文件（否则 DOC-INDEX 提交后转红）；`tests/test_index.csv` 登记 `tests/config`；`ci/checks.json` 若需新增 `UT-CONFIG` 检查项（ENGINEERING_SPEC §7 要求改测试后同步订正 checks.json，但 ci/** 不在本任务文件域）；`docs/contracts/INDEX.yaml`/UNIFIED_OBJECTS.md 登记本任务的 schema 与默认值索引（同文件正被 DATA-001 线并发修改，避免踩踏）。
7. **ENGINEERING_SPEC §7 根目录清单缺 `config/`**：已知文档缺口，已由控制包登记下一轮；本任务**不改**该文档。
