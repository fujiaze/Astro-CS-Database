# AUD-001-A2-mosaic

分片：mosaic（Phase2）5 个模块的设计-实现差异审计（coverage(admit) / sampling / upm / rejection / integration）。
只读审计：未修改任何仓库文件；唯一写入本报告。报告外仅执行 `ci/run_checks.py --check <ID>` 与 `tools/quality/check_module_map.py`（按 ci/checks.json 注册定义写 `run/ci/**` 检查产物，run/ 为 gitignore 目录）。

审计基线：工作区 HEAD 未记录（本任务禁 git 写操作，未执行 git 命令）；代码/文档以当前工作区文件为准。
文档对照面：`ASTROCS_DESIGN.md`（最高权威）、`ENGINEERING_SPEC.md`、`docs/plugins/algorithms_phase2/09..13`、`docs/design/UNIFIED_MODEL.md`、`docs/design/PHASE2_DETAILED_DESIGN.md`、`docs/science/{PHASE2_UPM,REJECTION,INTEGRATION,UNCERTAINTY_AND_COVARIANCE,CONTROL_WEIGHT_SNR}.md`、`docs/algorithms/PHASE2_*.md`、`docs/contracts/v6/**`、`contracts/**`。

实现落位事实（先 ls 确认）：5 个模块的生产源码全部位于 `lib/algorithms/coverage/src/`（根 `CMakeLists.txt:436-444` 的 `astrocs_phase2` STATIC 成员）；`lib/algorithms/{sampling,upm,rejection,integration}/` 只有 README.md + module.yaml + memory.md（合同文件，无源码/头/CMake/测试）。`lib/algorithms/integration/v6/` 另有 V6 三模式集成库（未接生产 CLI，见 INT-01）。

## 1. 覆盖清单

| 模块 | 审计结论（含"无差距"） | 差距号 |
|---|---|---|
| coverage（admit） | 输入发现/兼容校验/MOC union/target_order 已实现（coverage.cpp:173-272）；V6 的 support/coverage 分类门与权重 token/模式门已实现（coverage.cpp:291-452）但仅被 V6 集成库消费。**差距**：文档要求的"重叠图/有效面积/信息量/连通分量/coverage 产品"无实现（只做 union MOC）；两个配置项未登记未实现；七件套缺独立 CMake target/entrypoint/ABI；doc 引用的 schema 不存在。 | COV-01/02/03、X-01、X-04 |
| sampling | 控制点采样（P2ControlObservation/P2ControlNode）、background-clean 三阶段、control_variance/control_ivar、V6 空间求值/标量降级门已实现（sampler.h:32-133、187-247）。**差距**：新文档包新增的 `star_mask`/`sky_samples` 数据对象与配置项零实现；七件套仅 2/7。 | SMP-01/02/03/04、X-01、X-04 |
| upm | 加性-only UPM（M(p)+C_f(p)，8×8 control cell 双线性）与 V6 乘加 MA 求解器（`p2_upm_ma_build`）均已实现，含 gauge/秩/条件数/参数协方差/IRLS（upm.h:198-354、upm.cpp:1751-1800）。**差距**：生产链只用加性模型、无 g_k 乘法；b_k 为帧级标量而非文档要求的 `b_k(x)=B_ref(x)+δ_k(x)` 稀疏样条面；稠密背景栅格仍被物化落盘；新配置字段缺失；七件套仅 2/7。 | UPM-01/02/03/04/05/06、X-01、X-04 |
| rejection | 排异框架完整：10 显式方法核、planning AUTO、eligibility、大尺度 grow、V6 六类污染分类（cosmic/satellite/bad_column/moving_source/cloud_gradient/defocus_trail）+ probability + 预测残差方差 `sigma_eff²=sigma_phase1²+J C_θ Jᵀ` 均实现（rejection.h:327-473）。**差距**：生产 mosaic 节点链只调用方向型 `p2_reject_stack_ex`，六类分类/probability 未接线；配置项未登记；七件套仅 2/7。 | REJ-01/02/03、X-01、X-04 |
| integration | 生产 reducer `p2_integrate_pixel` 为加权均值（integrate.cpp:19-79），含 eligibility/五态 status/support canonical reducer/零权合同，无差距地实现了"逐像素加权归约"这一子职责。**差距**：文档要求的三目标产品（GLS、Q/W、psfsw_robust）只存在于未接线的 V6 库；稀疏 SNR 层检测/重建缺失；生产 weight_mode 用被冻结词表取代的 legacy 整数且默认取 baseline pixel_ivar；新配置与产品 schema 缺失；七件套仅 2/7。 | INT-01/02/03/04/05、X-01、X-04 |
| 横向（config/注册表/模块门） | 模块 map 机器门对 5 模块全判 FAIL；config_registry 相对新文档包陈旧；新文档包的 sky 面语义与 FROZEN 科学文档冲突。 | X-01/02/03/04 |

## 2. 差距明细

| # | 模块 | 差距类型 | 级别 | 文档条款引用 | 现状复现路径（文件:行 / 命令+输出） | 影响面 | 建议归属 |
|---|---|---|---|---|---|---|---|
| SMP-01 | sampling | 缺口 | P0 | `ASTROCS_DESIGN.md:246`（天光亮度平面：星点掩膜外每帧取稀疏背景采样点）；`docs/plugins/algorithms_phase2/10_sampling.md:5,18,20,42-46`；`docs/design/UNIFIED_MODEL.md:46-48` | `grep -rn "sky_sample\|sky_plane\|sky_sample_spacing\|B_ref" lib/ contracts/ config/` 仅命中 `lib/infrastructure/hips_browser/healpix_browser_qt/tests/trace_browser_lineage.cpp:3,20`（无关测试 tsv），无实现；`sampler.h:78-133` 输出仅 P2ControlNode/P2ControlObservation，无 star_mask/sky_samples 字段；`sampler.cpp:471-540` 入口签名无对应出参；`grep -rn "star_mask" lib/algorithms/{coverage,sampling,upm} contracts/schemas config/config_registry.json` 仅 `config_registry.json:820 bright_star_mask`（旧字段登记，未实现） | UPM 无天光面输入；加性背景只能靠控制点 median 吸收；违反最高设计 §4.4；下游天空背景/接缝与光污染鲁棒性无实现 | AUD→新任务（sampling 天光采样实现） |
| SMP-02 | sampling | 缺口 | P1 | `10_sampling.md:63-68`（spacing/sky_sample_spacing/bright_star_mask/min_control_points/min_sky_samples/local_estimator） | `config/config_registry.json:801-845` 只登记 spacing/bright_star_mask/min_control_points，且 `doc line` 指向 30/31/32（新文档 30-32 行已是 §4.2 mermaid）；无 sky_sample_spacing/min_sky_samples/local_estimator；`P2SamplerConfig`（sampler.h:33-58）无对应字段 | 新配置项无合同、无实现、无默认；文档-配置漂移 | DOC-002 + sampling 实现任务 |
| SMP-03 | sampling | 缺口 | P1 | `10_sampling.md:26-29`（掩膜排除：检测目录星点 PSF 膨胀/饱和溢出/坏点宇宙线/亮星光晕/高结构/移动源标记） | 实现仅 `sampler.cpp:866` SNR catalogue veto（`background_catalog_veto`）+ patch clipping；无独立 star_mask 对象/天球坐标掩膜；`sampler.cpp:580-608` 只读 SNR catalogue 质量位，未读 star detection 目录 | 高结构区误入控制点拟合；移动源标记缺失 | sampling 实现任务 |
| SMP-04 | sampling | 违规 | P1 | `ENGINEERING_SPEC.md:33-43`（§4 七件套） | `python3 tools/quality/check_module_map.py` → `sampling NOT_IMPLEMENTED ... missing_public_header missing_implementation missing_cmake_target missing_co_located_tests product_unit_missing`；`ls lib/algorithms/sampling` = memory.md/module.yaml/README.md（无 include/src/tests/CMakeLists.txt）；`module.yaml:58 entrypoint: MISSING` | 模块不可独立构建/不可调度；entrypoint 未接 | BLD-001 / P2-SAMP-IMPL |
| UPM-01 | upm | 违规/缺口 | P0 | `ASTROCS_DESIGN.md:213`（upm 联合相对模型 g·s+b）、`:246`（b_k(x) 稀疏样条/不建稠密栅格）；`11_upm.md:26-27,43-51`；`PHASE2_DETAILED_DESIGN.md:26` | 生产模型为纯加性：`upm.cpp:4-27`（y_ik = M(p_k)+C_i(p_k)，无 g_k）、`upm.cpp:67-100 Model{... C[frame][control]}`；V6 MA 求解器把 b_k 定为**帧级标量**：`upm.h:202-211` 明示"本 API 只实现帧级 b_k；空间加性场 b_k(x) 属 OPEN-P2S-02"、`upm.cpp:1751-1773` 参数布局 [s][g][b]、`upm.cpp:1793-1794/1818-1819` b 列 Jacobian=1.0（常数）；全仓无 B_ref/δ_k/spline | 违反最高设计 §4.4；光污染/梯度场只能被常数或 8×8 cell 吸收，接缝与背景平滑不达标；g 与 b 分离语义在生产链缺失 | AUD→SCI-001 裁决 + 新任务（天光面实现） |
| UPM-02 | upm | 违规 | P0 | `ASTROCS_DESIGN.md:213`；`11_upm.md:26,30`（g_k 乘法与 b_k 加性不得互相代替） | 生产 mosaic 节点链 `lib/infrastructure/scheduler/src/module_adapters.cpp:20-22`（upm-fit→`p2_upm_build_geo`；upm-apply→`p2_upm_open/p2_upm_calibrate_block`）；`module_adapters.cpp:4155`、`4331`；`p2_upm_calibrate_block` 语义为 raw−C_f(p)（`upm.cpp:1274+`、`PHASE2_UPM.md:48-50`）。`grep -rn "p2_upm_ma_build" lib/ --include=*.cpp` 仅 `lib/algorithms/integration/v6/src/phase2_integrate.cpp:1627`（V6 库，未接 CLI） | 逐帧乘法光度响应 g_k 在生产链不生效；UPM 归一到公共通量尺度的能力缺失 | AUD→E2E-001 前必须修（P2-UPM-INT） |
| UPM-03 | upm | 违规 | P1 | `ASTROCS_DESIGN.md:246`（"不构建稠密背景栅格"）；`11_upm.md:44` | `upm.cpp:1426-1551 p2_upm_materialize_dense_n` 把每帧每 tile 的 C_i(p) 求值成 512×512 稠密 double 落盘（`astrocs-upm-dense-v2`，`aio_upm.cpp:223/407`）；`lib/algorithms/coverage/tools/stage2.cpp:482` 调用之（默认 CLI 链 module_adapters 未调用） | 与"面值按需现场求值"冲突；大天区磁盘/内存开销；工具链与设计口径不一致 | PERF-001 / P2-UPM-IMPL |
| UPM-04 | upm | 缺口/漂移 | P1 | `11_upm.md:74-80`（bkg_model=spline、bkg_spline_spacing、frame_gradient_order、roughness_penalty） | `config/config_registry.json:861-911` 仍记旧字段 `bkg_model_order`（default 0，doc line 37）；新文档 37 行已是 §4.2 mermaid，§5 配置表在 74-80；无 bkg_model/bkg_spline_spacing/frame_gradient_order/roughness_penalty 登记；`P2UpmBuildConfig`（upm.h:71-96）无对应字段 | 新天光面模型无配置面；配置注册表指向旧文档残留 | DOC-002 + P2-UPM-IMPL |
| UPM-05 | upm | 违规 | P1 | `ENGINEERING_SPEC.md:33-43` | 模块 map：`upm NOT_IMPLEMENTED ... missing_public_header missing_implementation missing_cmake_target missing_co_located_tests product_unit_missing`；`ls lib/algorithms/upm` 仅合同文件；`module.yaml:69 entrypoint: MISSING`、`:66-67 dll_target 不存在` | 模块不可独立构建/调度 | BLD-001 / P2-UPM-IMPL |
| UPM-06 | upm | 过时 | P2 | `ENGINEERING_SPEC.md:19`（注释禁止堆历史/复述）；模块元数据自洽性 | `module.yaml:2-3` 称 upm.cpp 1565 行/upm.h 184 行，实测 `wc -l` = 2459/358；`module.yaml:109-125 source_symbols` 未含 `p2_upm_ma_*`（9 个）、`p2_upm_convergence`、`p2_upm_control_variance`；README.md:9-11 同样陈旧 | 模块清单/追溯面失真 | DOC-002 |
| REJ-01 | rejection | 缺口 | P1 | `ASTROCS_DESIGN.md:244`（排异是污染状态估计）；`12_rejection.md:21-25`（六类分型 + reason/probability）；`PHASE2_DETAILED_DESIGN.md:37-43` | 生产 op 用 `module_adapters.cpp:4534 p2_reject_stack_ex`（方向型 reasons accepted/low/high，`rejection.h:73-78`），无 reason_class/probability；六类分类入口 `p2_reject_classify`（`rejection.h:343-444`）仅被 `lib/algorithms/integration/v6/src/phase2_integrate.cpp:1739` 与 `tests/unit/v6_p2_rej` 调用 | 生产马赛克不含污染分类与概率，移动源独立层保留在生产链未生效 | AUD→P2-REJ-INT |
| REJ-02 | rejection | 缺口 | P1 | `12_rejection.md:29-34`（rejection_classes/sigma_gate/max_iter/keep_moving_sources） | `config/config_registry.json:912-984`：rejection_classes finding=gap、sigma_gate/max_iter unregistered、keep_moving_sources finding=gap；实现侧 `P2RejectClassifyConfig.keep_moving_source`（rejection.h:406）未从 CLI 配置透传（`module_adapters.cpp` 无 keep_moving_source 命中） | 排异类别开关/阈值无合同面；移动源保留不可配 | DOC-002 + P2-REJ-INT |
| REJ-03 | rejection | 违规 | P1 | `ENGINEERING_SPEC.md:33-43` | 模块 map：`rejection NOT_IMPLEMENTED ... missing_public_header missing_implementation missing_cmake_target missing_co_located_tests product_unit_missing`；`ls lib/algorithms/rejection` 仅合同文件（+CLASSIFY_V1_PROFILE.md） | 模块不可独立构建/调度 | BLD-001 / P2-REJ-IMPL |
| INT-01 | integration | 违规/缺口 | P0 | `ASTROCS_DESIGN.md:205`（不同科学目标明确最优统计量）；`13_integration.md:5,18-28,42-59`（surface GLS / Q-W / psfsw_robust 三目标产品）；`PHASE2_DETAILED_DESIGN.md:45-83` | 生产 reducer `integrate.cpp:19-79` 仅 `signal=Σwv/Σw`；`integrate.h:5-11` 仅声明 `stack.support_x_snr2.v1`/`stack.equal.v1`，无 variance/covariance/effective PSF 输出；完整三模式实现在 `lib/algorithms/integration/v6/src/phase2_integrate.cpp`（run_point_information:847/run_surface_gls:1046/run_psfsw_robust:1224），`grep` 调用者仅 `tests/integration/v6_p2/v6_p2_integrate_test.cpp` | 生产马赛克不满足设计 §4.3 的点源/扩展源目标统计量；W_info/Q/W/covariance/effective PSF 均不产出 | AUD→E2E-001 前必须修（P2-INT-INT） |
| INT-02 | integration | 缺口 | P0 | `ASTROCS_DESIGN.md:225-240`（自动检测稀疏 SNR 层；实际 SNR=帧级×帧内，否则帧级；标准行为非可选）；`13_integration.md:18-22,32-38` | `grep -rn "sparse_snr_layer" lib/` → `NO_MATCH_IN_LIB`（仅 config/contracts 文档面）；生产 integrate 用逐帧 ivar（`module_adapters.cpp:4917-4932`）或等权（mode=1），无帧级 SNR 读取、无帧内层检测、无重建算子/manifest 记录 | 违反最高设计 §4.3；SNR→逆方差权重链路与"帧级×帧内"语义缺失 | AUD→P2-INT-INT |
| INT-03 | integration | 违规 | P0 | `docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md:9-18`（生产={point_information,surface_gls,psfsw_robust}；legacy 整数{0,1,2}被取代，`legacy_integer_allowed=false`，0 必拒）；`13_integration.md:65` | 生产 CLI/session 用**整数** weight_mode 且默认 2：`lib/infrastructure/cli/session_commands.h:133 {"weight_mode","2"}`、`module_adapters.cpp:4656-4669`（默认 2，仅允许 1/2）、`lib/infrastructure/cli/v6_mode_gate.h:71-75`（1\|2 路由为 baseline）；canonical 字符串枚举 `algorithm_weight_mode`（`contracts/schemas/phase_config_mosaic.schema.json:122-129`，enum=三生产模式）`grep -rn "algorithm_weight_mode" lib/` **零消费**（仅 config/templates 与 registry）。ivar 读链本身闭合：`module_adapters.cpp:4689-4736` 打开 `AIO_HIPS_RD_IVAR`、缺失 fail-closed；写入侧 `aio_hips_writer.cpp:901/1314/1627` 产 ivar | 生产默认权重是被冻结词表降级为"文档基线"的 pixel_ivar；canonical 三生产模式无配置入口；双词表并存 | AUD→SCI-001/W6 裁决 + CLI/config 修复 |
| INT-04 | integration | 缺口 | P1 | `13_integration.md:61-68`（target_product/correlation_approx/baseline_compare） | `config/config_registry.json:1000-1019` target_product finding=gap；correlation_approx/baseline_compare 无登记；`P2PixelStack`/`P2PixelResult`（integrate.h:36-63）无这些参数 | 相关噪声近似与基线比较无配置面 | DOC-002 + P2-INT-INT |
| INT-05 | integration | 违规 | P1 | `ENGINEERING_SPEC.md:33-43` | 模块 map：`integration NOT_IMPLEMENTED ... missing_cmake_target missing_co_located_tests missing_implementation product_unit_missing`；`ls lib/algorithms/integration` 顶层无 src/include/tests/CMakeLists.txt（仅 v6/ 子目录自带 CMake） | 模块不可独立构建/调度 | BLD-001 / P2-INT-IMPL |
| COV-01 | coverage | 缺口 | P1 | `09_coverage.md:5,17,22-24`（重叠图/有效面积/信息量/连通分量/coverage 产品）；`PHASE2_DETAILED_DESIGN.md:17-19`；`ASTROCS_DESIGN.md:211` | `coverage.h:46-54 P2CoverageResult` 仅 inputs/union_cells/target_order/status；`coverage.cpp:173-272 p2_coverage_build` 只做 union MOC+target_order；`grep -rn "effective_area\|information\|overlap_graph" lib/algorithms/coverage` 仅命中 weight-mode 字符串（:425 等）；连通分量只在 UPM 的 frame-control 二分图（`upm.cpp:82-86,1969`），非 coverage 的帧间重叠图 | 下游 sampling/upm/integration 无有效面积/信息量/帧间重叠图输入；断图语义只在 UPM 侧部分承载 | AUD→P2-COV-IMPL |
| COV-02 | coverage | 缺口 | P1 | `09_coverage.md:28-31`（min_overlap/connected_components） | `config/config_registry.json:765-799`：min_overlap registration=none/finding=unregistered；connected_components finding=gap（"插件默认 true；未进任何配置类"）；实现无对应字段 | 覆盖门与连通分量开关无合同面 | DOC-002 + P2-COV-IMPL |
| COV-03 | coverage | 违规 | P1 | `ENGINEERING_SPEC.md:33-43` | 模块 map：`coverage NOT_IMPLEMENTED ... missing_cmake_target header_missing_abi_version missing_implementation product_unit_missing`；`module.yaml:44-47 dll_target/entrypoint` 为"尚未存在/MISSING"；`coverage.h` 无 abi_version/struct_size | 模块不可独立构建/调度；C ABI 版本化缺失 | BLD-001 / P2-COV-IMPL |
| X-01 | 横向（5 模块） | 缺口 | P1 | `09_coverage.md:18`、`10_sampling.md:22`、`11_upm.md:19`、`12_rejection.md:17`、`13_integration.md:28`（各模块参考 schema） | `find contracts -name "*coverage_output*" -o -name "*control_points*" -o -name "*upm_output*" -o -name "*rejection_output*" -o -name "*mosaic_product*"` → 无输出；模块 map 逐模块 FAIL `dangling_schema_link: contracts/schemas/{coverage_output,control_points,upm_output,rejection_output,integration_output}.schema.json`；`contracts/schemas/` 无 `star_mask/sky_samples/sky_plane` schema（UNIFIED_MODEL:46-48 新对象无合同） | "contracts/schemas/ 唯一事实源"缺口；端口引用无效 DATA 合同（ENGINEERING_SPEC §4 第 7 项） | DOC-002 + SCHEMA 任务 |
| X-02 | 横向（config/注册表） | 漂移/过时 | P1 | `ENGINEERING_SPEC.md:111`（改代码/测试同步订正 ci/checks.json）、`ACCEPTANCE_SPEC.md` 文档-代码一致 | `config/config_registry.json` 明显按旧文档包生成：module 10_sampling 条目 `doc line`=30/31/32（:806,821,836），module 11_upm 条目 `doc line`=37/38/39（:864,885,900）且字段名 `bkg_model_order`（:865）；旧文档包残留于 `run/incoming/AstroCS文档集/docs/plugins/algorithms_phase2/11_upm.md:37 | bkg_model_order | 0`；新文档同位置已是 §4.2 mermaid / §4.4 SNR 目标 | 配置注册表与权威文档不一致；新字段无登记；可能误导实现 | DOC-002（DOC-001 残留清理复核） |
| X-03 | 横向（文档-文档） | UNRESOLVED | — | `ASTROCS_DESIGN.md:213,246` + `10_sampling.md` + `11_upm.md` + `UNIFIED_MODEL.md:46-48` **对** `docs/science/PHASE2_UPM.md:7-8,34,48-50,153`（FROZEN，纯加性 C_f(p)、"无 WCS/天球参与"）+ `docs/algorithms/UPM_SOLVER.md`/`PHASE2_UPM_IMPL.md`（`grep b_k` 零命中） | 新文档包只在 `docs/design/UNIFIED_MODEL.md` 与插件 10/11 引入 star_mask/sky_samples/sky_plane/B_ref；`grep -rln "sky_samples\|sky_plane\|star_mask\|B_ref" docs/algorithms/ docs/science/ docs/design/ docs/contracts/` 仅命中 `docs/design/UNIFIED_MODEL.md`；FROZEN 科学文档仍定义加性-only 且明确"无 WCS/天球参与" | 实现（加性-only，无天球）与 FROZEN 科学文档一致，与最高设计 §4.4/插件 11 冲突；谁权威无法由本审计裁决 | SCI-001 + 负责人裁决 |
| X-04 | 横向（5 模块） | 违规 | P1 | `ENGINEERING_SPEC.md:127`（检查器覆盖：模块 manifest/注册表/构建 target/产品清单一致、端口引用有效 DATA 合同） | `python3 ci/run_checks.py --check CHK-MODULE-MANIFEST` → `verdict=FAIL entries=1 steps=8 pass=7 fail=1`；`python3 tools/quality/check_module_map.py --json-out run/ci/module-map/module_map.json` 对 5 模块全 FAIL（见各模块行） | 机器门红；发布前必须处置 | BLD-001（并按模块归属 P2-*-IMPL） |

## 3. UNRESOLVED

1. **X-03 文档-文档冲突（天光面语义）**：`ASTROCS_DESIGN.md:246` 与插件 `10_sampling.md`/`11_upm.md`/`UNIFIED_MODEL.md:46-48` 要求 `b_k(x)=B_ref(x)+δ_k(x)` 稀疏样条天光面、star_mask、sky_samples、SNR 加权最小 RMS；而 FROZEN 的 `docs/science/PHASE2_UPM.md`（:7-8 目的、:34 有效域、:48-50 连续定义、:153 §3a）定义**纯加性** `calibrated=raw−C_f(p)` 且明确"UPM 在像素域 control cell 上工作，**无 WCS/天球参与**"，`docs/algorithms/UPM_SOLVER.md`/`PHASE2_UPM_IMPL.md` 全文无 `b_k`。两边都是权威链内文档，本审计不自行裁决；需要 SCI-001 按 AGENTS.md §8 走变更 claim 或负责人裁决，才能确定 UPM-01/UPM-02 的最终归属与是否改文档。
2. **INT-03 词表冲突的裁决归属**：`02_WEIGHT_MODE_VOCABULARY.md:9-18` 冻结 legacy 整数为被取代且 `legacy_integer_allowed=false`，但 `session_commands.h:133`/`module_adapters.cpp:4656-4669` 把整数 1/2 当生产可选（默认 2），`v6_mode_gate.h:149-171` 又把 1\|2 标为 baseline。这是"文档已撤销仍存在"（过时）还是"实现违规"，取决于 W6 迁移是否已完成；登记待裁决。
3. **X-02 旧文档包残留**：`run/incoming/AstroCS文档集/` 保留旧插件文档（与当前 docs/ 内容不同，diff 见复核命令），且 `config/config_registry.json` 明显引用旧文档行号。DOC-001 的"残留清理"验收门是否应覆盖 `run/incoming/`（gitignore 目录）未在文档中明确，登记待裁决。

## 4. 复核命令清单

```bash
# 0) 实现落位确认（5 模块源码均在 coverage/src）
find lib/algorithms/coverage lib/algorithms/sampling lib/algorithms/upm \
     lib/algorithms/rejection lib/algorithms/integration -maxdepth 2 -type f | sort
wc -l lib/algorithms/coverage/src/{coverage,sampler,upm,rejection,integrate}.cpp \
      lib/algorithms/coverage/include/astro/phase2/{coverage,sampler,upm,rejection,integrate}.h

# 1) 新语义零实现证据（SMP-01 / X-01）
grep -rn "sky_sample\|sky_plane\|sky_sample_spacing\|B_ref" lib/ contracts/ config/
find contracts -name "*star_mask*" -o -name "*sky_sample*" -o -name "*sky_plane*"
grep -rn "star_mask" lib/algorithms/coverage lib/algorithms/sampling lib/algorithms/upm \
     contracts/schemas config/config_registry.json

# 2) 天光面语义零命中（UPM-01 / X-03）
grep -rln "sky_samples\|sky_plane\|star_mask\|B_ref" docs/algorithms/ docs/science/ docs/design/ docs/contracts/
grep -rn "b_k" docs/algorithms/PHASE2_UPM_IMPL.md docs/algorithms/UPM_SOLVER.md docs/science/PHASE2_UPM.md
sed -n '198,212p' lib/algorithms/coverage/include/astro/phase2/upm.h   # 只实现帧级 b_k
sed -n '1751,1800p' lib/algorithms/coverage/src/upm.cpp                # theta=[s][g][b]

# 3) 生产链只走加性 UPM / 加权均值 / 方向型排异（UPM-02 / INT-01 / REJ-01）
grep -n "p2_upm_build_geo\|p2_upm_calibrate_block\|p2_integrate_pixel\|p2_reject_stack_ex" \
     lib/infrastructure/scheduler/src/module_adapters.cpp
grep -rn "p2_upm_ma_build" lib/ --include=*.cpp
grep -rn "run_point_information\|run_surface_gls\|run_psfsw_robust" lib/ tests/ --include=*.cpp

# 4) 稀疏 SNR 层未消费（INT-02）
grep -rn "sparse_snr_layer" lib/ || echo NO_MATCH_IN_LIB

# 5) weight_mode 词表冲突（INT-03）
sed -n '1,20p' docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md
sed -n '129,139p' lib/infrastructure/cli/session_commands.h
sed -n '4656,4669p' lib/infrastructure/scheduler/src/module_adapters.cpp
grep -rn "algorithm_weight_mode" lib/
sed -n '122,129p' contracts/schemas/phase_config_mosaic.schema.json

# 6) 七件套 / 模块门（X-04，含 coverage/sampling/upm/rejection/integration 逐条 FAIL）
python3 tools/quality/check_module_map.py --json-out run/ci/module-map/module_map.json
python3 ci/run_checks.py --check CHK-MODULE-MANIFEST
python3 -c "import json;d=json.load(open('run/ci/module-map/module_map.json'));[print(m['target_dir'],m['status'],[f['code'] for f in m['findings']]) for m in d['modules'] if m['target_dir'] in ('lib/algorithms/coverage','lib/algorithms/sampling','lib/algorithms/upm','lib/algorithms/rejection','lib/algorithms/integration')]"

# 7) config_registry 陈旧（X-02）
sed -n '801,845p' config/config_registry.json      # 10_sampling 指向旧行号 30-32
sed -n '861,911p' config/config_registry.json      # 11_upm 旧字段 bkg_model_order
sed -n '30,40p' docs/plugins/algorithms_phase2/11_upm.md
sed -n '37p' 'run/incoming/AstroCS文档集/docs/plugins/algorithms_phase2/11_upm.md'
diff 'run/incoming/AstroCS文档集/docs/plugins/algorithms_phase2/11_upm.md' docs/plugins/algorithms_phase2/11_upm.md
python3 ci/run_checks.py --check CHK-REGISTRY-DOC-SYNC

# 8) 文档引用的产品 schema 缺失（X-01）
grep -n "schema.json" docs/plugins/algorithms_phase2/09_coverage.md docs/plugins/algorithms_phase2/10_sampling.md \
      docs/plugins/algorithms_phase2/11_upm.md docs/plugins/algorithms_phase2/12_rejection.md \
      docs/plugins/algorithms_phase2/13_integration.md
```

> 说明：第 6 条中的 `check_module_map.py` 与 `ci/run_checks.py` 会按其注册定义写 `run/ci/**`（gitignore 运行产物）；本审计未写任何受版本控制文件。
