# lib/phase2 模块记忆

## 模块目标

Phase2（控制包 `AstroCS_Phase2_Implementation_Control_Package_V1`，
SHA `34A532A2451C8746BEF7B5DA05C3C4C7D15201D66A9D5F6AB5F8F291BE2EB308`）：
多个 Phase1 单帧 HiPS 在最大覆盖并集 Ω 上建立**一个** UnifiedPhotometricModel
（UPM，联合加性校准 + SNR-aware 权重 + robust loss），按内存动态分块把所有覆盖帧
校准到统一模型，逐球面像素执行迭代排异（7 种）与 SNR/support/quality 加权叠加，
最终经唯一 AIO 输出标准 IVOA HiPS 马赛克。

正式撤销：HICS 动态数据库、每帧独立梯度产品、只在重叠区建控制点、Phase2 乘性
photometric scale、新 runtime I/O DLL。

## 已完成（本模块，2026-08-10）

### W0-W10（前序会话，gate 12/12 基线）
- W0 盘点 / W1 Wiki 同步 / W2 接口冻结（`工程控制/docs/PHASE2_IMPLEMENTATION/`、
  `工程控制/docs/PHASE2_INTERFACE_FREEZE/`）；
- W4 UPM CPU reference（Huber IRLS + snr2_normalized + 弱零锚）；
- W6 block planner；W7 sigma/winsorized/averaged-sigma/ESD；
- W9 ACR synthetic `synthetic.mosaic_reject.fp64acc` legacy launcher；
- W10 robustness（NaN/min-samples/zero-memory/all-rejected）。

### 本会话补全（真实链闭合）
- **W3 coverage 真实实现**（`src/coverage.cpp`）：AIO reader 读取每帧
  properties/tiles，兼容校验（hips_frame/obs_filter/tile_width/version），
  MOC union（NESTED parent 聚合），target_order = min(输入 max order)。
- **W4 control sampler**（`include/astro/phase2/sampler.h` +
  `src/sampler.cpp`）：union 内 control cell 网格（默认 8×8/tile），patch
  estimator（support>0 + finite 过滤，median 位置 + MAD 尺度），SNR 来自
  Phase1 SNR Catalogue 邻近星点（不重新检测星点），保留负值。
- **W4 UPM 完整化**：自包含 SHA-256（FIPS 180-4，`src/sha256.cpp`）、
  frame-control 二分图连通分量、真实内容哈希、JSON sparse 持久化
  （`p2_upm_save/open`，format `astrocs-upm-v1`）。
- **W5 dense cache**：头部 JSON（source_hash/target_order/precision/frame
  count/checksum）+ 二进制 controls/frame 块；`p2_upm_dense_info` /
  `p2_upm_dense_read_block` 做 stale 校验（source hash 不匹配返回 2）。
- **W7 补全**：LinearFit（残差 MAD 尺度稳健版）与 RCR（Maples et al. 2018
  论文独立实现，Chauvenet 判据，weighted/unweighted）。
- **W8 stage2 正式入口**（`tools/stage2.cpp` → `astrocs-stage2.exe`）：
  单 JSON 参数驱动 DISCOVER → VALIDATE → COVERAGE_UNION → CONTROL_SAMPLE →
  UPM_FIT → UPM_PERSIST → BLOCK_PLAN → BLOCK_CALIBRATE → REJECT_INTEGRATE →
  HIPS_WRITE → HIPS_VERIFY；输出 signal/support 两个 Image HiPS；
  support_out = max(accepted support)（覆盖并集保守下界）；
  fallback=none（样本不足不做 rejection，单覆盖稳定输出）。
- **W11 真实马赛克**：crop 三片（T2/T3/t4_crop，51 tiles，6.99M px 全
  fallback）+ T4 crop×full 重叠验证（285 tiles，61.59M px，重叠区 4.02M px
  真实 2 样本加权叠加）+ 最终完整三片（312 tiles，64.56M px）。

## 验证结果（2026-08-10）

- 合成 gate：18/18 PASS（原 12 + coverage 2 + sampler 1 + upm roundtrip 1 +
  linear-fit 1 + RCR 1）。
- 真实 coverage：T2/T3/t4_crop union=51，target_order=7，filter=Red。
- 真实 UPM（t4_crop×full）：15338 controls / 16418 obs，1 分量，frame offset
  crop=0（参考帧）、full=-0.001057，iterations=95，objective=1.7e-6。
- 真实马赛克 Hipsgen：signal/support LINT（IVOA 1.0 compatible）、
  CHECKCODE/CHECK、CHECKDATASUM（418/418 files）全过（overlap 与 full 两套）。
- 输出数值：与输入同 tile median 相对差 0.9%–4.7%（重叠区加权叠加所致，合理）。
- 性能：285 tiles 26s（单线程 CPU reference，未接 GPU kernel）。

## 接口清单（冻结）

- coverage：`p2_coverage_build` / `p2_coverage_free`
- sampler：`p2_sample_controls`
- upm：`p2_upm_build/save/open/info/calibrate_block/materialize_dense/
  dense_info/dense_read_block/close`
- rejection：`p2_reject_stack`（None/Sigma/Winsorized/AveragedSigma/
  LinearFit/GeneralizedESD/RCR）
- block：`p2_block_plan`
- integrate：`p2_integrate_pixel`
- acr：`astro::compute::phase2::register_phase2_acr_kernels()`

## 未完成 / 已知限制

- W9 ACR 仅 legacy CPU launcher 注册，无 CUDA/GPU kernel（profile 后按热点接入，
  CPU reference 是权威科学语义）。
- Oracle 矩阵（Astropy/NIST/Siril/IRAF 对照）只做了 NIST ESD 与 Rosner 风格
  单侧离群验证，未全量跑 synthetic matrix（N=2..500 × 7 种污染）。
- Aladin GUI smoke 未做（无 GUI 环境，Phase1 同样记录）。
- 输出仅 signal/support；weight/rejection_count 诊断产品未输出（可选诊断）。
- stage2 逐 tile 处理、单线程；未做 tile 内 micro-chunk 分片路径实测
  （block planner 已实现估算与标记）。
- 根目录 `memory.md` 尚未追加 Phase2 进度（待后续会话同步）。

## 2026-08-14 V15 Final Semantic Closure（HEAD 8a772ca）

- rejection 语义冻结：canonical IDs + typed params + planning 层 auto
  （WBPP 2.9.1 本机源码 bestRejectionMethod：n<6 percentile / 6-15
  winsorized / >15 linear_fit）+ eligibility 分层 + per-sample reason +
  UNDERDETERMINED；RJ-001..008 全修复（ESD 双 sqrt、NONE NaN、valid 掩码、
  low/high 阈值、status/reason 分离、参数 typed 化、support/quality 消费、
  sigma 改名 robust_mad_clip）；
- 生产默认 method=auto + profile=wbpp_current；旧 low/high/max_iterations
  deprecation adapter；schema/template/parser 单源（config_consistency PASS）；
- 卫星线门：20 exposure 受控注入（真实 t4_crop 底图）recall=1.0000、
  mosaic bg/star bias=0；n<=2 生产 run 61.6M px 全部 UNDERDETERMINED；
- sampler：catalogue 全扫描 → dec 排序索引 + 帧 median 预计算（10min→9.2s）；
  null-config 未初始化 bug 修复（p2_sampler_default_config）；
- 全量 gate 59/59（41.8s clean-tree）；oracle（Astropy/NIST/Siril harness/
  rcr 2.4.7/WBPP policy）全 PASS；六轮自审 + clean-tree 终验 PASS；
- 审核包：AstroCS_Review_FinalSemanticClosure_V15.zip（SHA
  26219370FE0F8758B5482648B3682D85BD90CE4711A62403C87339D479D1A03F，342KB）。

## 2026-08-14 V16 Final Closure AuditFix（HEAD 1145a28）

- profile 拆分：wbpp_current（integration-group 一次解析）vs
  astrocs_adaptive（tile nominal-depth，独立命名不冒充 WBPP）；
- RejectionNormalizationPolicy（none/median_center/median_scale；decision
  作用 working、mask 回原始科学值积分；percentile 负值安全必须
  median_center；rcr 必须 none；违规 INVALID_CONFIGURATION）；
- MinMax 一次性固定 rank（(3,5)→42 精确）；max_iterations 删除；
- eligibility 单路径：p2_collect_candidate_stack（strided）CPU/ACR/compat
  同一 policy core；depth 诊断互斥（depth_0/1/ge_2）；
- averaged_sigma 改名 astrocs.averaged_sigma.v1（IRAF exact=NOT_CLAIMED）；
- WBPP Light 默认参数对齐（linearFit 5/3.5、percentile 0.2/0.1）；
  large-scale rejection 默认 off → unsupported（feature matrix 如实）；
- 真实 16-exposure E2E：NGC1727 T2 H-alpha 1200s × 16（Phase1 全成功，
  order 7 HiPS）→ Phase2 wbpp_current（nominal=16→linear_fit 单次）
  → 卫星门 V2 recall=1.0000、背景/星点无净损伤、sample false reject
  9.45%（真实数据）；
- ScratchVec heap-mode 修复（n>64 崩溃）；gate 65/65、oracle 全 PASS；
- 审核包：AstroCS_Review_FinalClosure_V16.zip（SHA
  E02B64137B18FCD00AA71C733B79A1BB05CB7CC0AAB7C1870966061EB22D7350，
  111 项清单 0 坏，含 canonical_core + repo_source_manifest.csv）。

## 2026-08-15 V19 ivar 科学权重 (eb48cef/04ffaa0)
- UPM: w = quality x support^p x ivar (obs->ivar>0 优先, 否则 1/unc^2);
  legacy snr^2/(1+snr^2) 仅 use_ivar_weight=0 (ablation, SNR-015)
- stage2 weight_mode 默认 ivar(2): 逐像素 ivar 产品; support 只作 validity;
  缺产品 -> support 回退 + ivar_product_missing 计数
- ACR kernel: mode2=support x ivar; mode0 legacy
- sampler: 读帧 ivar 产品控制 leaf -> obs.ivar
- gate 74/74; G5 UPM-calibrated ivar bias=-0.0008 var=0.0359 (opt 0.036)

## 2026-08-16 V19R3 Traceable Foundation Correction（HEAD 3131680）

- UPM 科学权重冻结（SCI-UPM-WEIGHT-001）：production raw_w = quality ×
  control_ivar（=1/control_variance）；control_variance = k_corr×(π/2)×
  σ_bg²/N_retained（ALG-UPM-CONTROL-IVAR-001）；k_corr=1.4 由 Drizzle
  synthetic MC 校准（UPMW-005：pixfrac=0.8、2000 实现、实证 1.3883、
  N_eff≈181/251）；N_retained 用 clipping 后样本；obs.ivar 弃用诊断
- integration：零权重合同（0 合法不贡献）、P2PixelStack.weight_mode 删除
  （policy/reducer 分离）、ivar 产品缺失默认硬科学错误（显式 fallback 开关）
- ACR：weight_mode=ivar 生产强制 CPU（ACR-IVAR-001），kernel wmode=2 禁用
- Drizzle：bounded target-ipix geometry cache（LRU 8192、run generation
  清空、hit≈91.7%）+ tgt_b/tgt_g/gcache 计数
- 质量门：fresh audit 791/791（carry=0）、clang --analyze 100%（4 CUDA
  例外）、WSL ASan/UBSan 9/9（修复 akima heap-overflow P1）、traceability
  63 contracts（50/50+50/50）、docs 全集合 8/8、comment hygiene 0
- 审核包 AstroCS_Review_TraceableFoundationCorrection_V19R3.zip
  SHA256=2593d6673809b2c22f7012f5305c88821c7742b494774ea5ef0687e680454409
- 状态：PRE_RELEASE_ENGINEERING_FOUNDATION=PASS；FINAL_REAL_DATA_VALIDATION=PENDING

## P2-COV-DOC 冻结（2026-09-07，SA-P2-S20）

- 范围：astrocs.p2.coverage（matrix P2-COV 行）合同冻结——coverage
  生产源=本目录 src/coverage.cpp（239 行）+ include/astro/phase2/
  coverage.h（59 行），legacy 即本目录（legacy_paths="lib/phase2
  coverage sources"）；sampler/upm/rejection/integrate 归 P2-SAMP/
  P2-UPM/P2-REJ/P2-INT 各自 DOC。本任务 lib/phase2 生产源零 diff
  （不改码），仅文档/manifest 落位。
- 产物：README.md r1 重写（原 42 行构建说明保留 §13）+ module.yaml
  （CONTRACT_READY，entrypoint=MISSING）+ 本段；ALG-COV-001=
  docs/algorithms/PHASE2_COVERAGE.md（§2 逐公式行号锚 + §11.3
  DISP-COV-001..005 + §11.4 TEST-COV-DESIGN-001）；DATA-COV-001=
  DATA_SEMANTICS.md §19；API-COV-001=PUBLIC_API.md；registry 页
  docs/modules/registry/astrocs.phase2.coverage.md 事实修订 + 矩阵行
  MOD-astrocs-phase2-coverage 更新（SCI/ALG/SRC/TEST 转 Verified）。
- SCI 层：零改动——SCI-P2-COV-001 指向既有 FROZEN 共享 SCI
  （PHASE2_UPM.md §1 覆盖并集 / INTEGRATION.md §5 support「覆盖并集
  保守下界」/ SCIENCE_SCOPE.md §处理链第 5 步），状态声明=
  PHASE2_COVERAGE.md §11.5（P1-WCS-DOC SCI-WCS-001=共享
  ASTROMETRY.md 先例）。
- 合同红线（matrix 专项，显式负向条款）：coverage/support/validity
  三概念分离；**no use as implicit scientific weight**——union cell/
  n_tiles/覆盖帧数为几何登记量，禁入任何权重式（w_UPM 唯一冻结式
  PHASE2_UPM.md §5，support 仅 eligibility 语义）。
- 关键缺陷登记（不改码，整改归 P2-COV-IMPL/INT）：DISP-COV-001
  "no inputs" 分支 status 不一致（:154-157）、002 frame_id 基名截断
  （:113-118）、003 空 filter 静默放行（:182/:186）、004
  intersection/depth/missing-tiles 产品缺失（matrix 四语义仅 union
  落地；覆盖度几何≠UPM geometric_reliability 权重因子——该乘数恒
  1.0 为 R3-A 已登记缺陷，修正归 P2-UPM 域）、005 extern "C" 内
  include（:47-51）+ 两阶段全量重扫 + ThreadLease 未接线。
- 验收基线（本任务自检）：check_traceability_matrix rc=0 errors=0
  warns=4（基线不变）；check_contract_graph rc=0 contracts=55；
  check_doc_index PASS；pytest tests/traceability 4 passed；生产源
  diff=0。日志 run/local/agent_p2_cov_doc/（不提交）。

### P2-002（2026-09-10，控制包 ASTROCS-CONSTITUTION-ALIGNMENT-V1）

- §30.2/§30.3 白名单内落地：contracts/data/
  phase2_uncertainty_rejection_provenance_v1.json（AIO 子产品位分配
  NREJ=32/NUSED=64 冻结登记 + §30.3 五键冻结表 + 诊断平面不入 science
  planes（F-UNC-003 零断链，schema/validator 零修改）+ pending AIO 通道
  登记）+ tests/unit/p2002_unc_rej_prov_test.cpp（kernel 语义直调（正确
  gather 契约）+ 集成投影对拍 + §30.3 五键对拍 + unavailable 显式登记 +
  确定性/1v4 parity + ASTROCS_P2002_FAULT=proj|prov 故障注入必败）。
- §30.1（P2-001 已落地）不动；本任务生产源零修改（scientific_change=
  false，零代码 diff 于 lib/phase2）。
- findings 移交（域外 lib/core，白名单外不改）：F-P2-002-01（P0，
  p2_op_reject 的 p2_collect_candidate_stack 调用 value_stride=
  sizeof(double) 违反 gather 契约（应为每帧元素跨度）+ 3 元素 vals 缓冲
  以 pixel 为基址越界读，depth≥3 时 kernel 收垃圾栈、rejection bins
  失真且非确定；ASAN heap-buffer-overflow 实证于 rejection.cpp:1185；
  depth=2 时 kernel 不触发故 P2-001 未暴露）；F-P2-002-02（P1，
  integrate 不剔除 kernel 拒绝样本——逐样本 reason 未持久化，
  n_ineligible 恒等式在部分拒绝场景不成立）；F-P2-002-03（AIO 域，
  P2-001 F1/F2 维持：writer int32 子产品位 32/64 通道与 ASTROCS_*
  properties 键通道未实现，§30.2/§30.3 HiPS 产品面 pending，现由
  integrated bins + p2_final.json 诊断面承载）。

### SCI-F2-001（2026-09-12，控制包 ASTROCS-CONSTITUTION-ALIGNMENT-V1 rev30）

- 目标：F-P2-002-02（STD-F2）integrate 剔除 kernel 拒绝样本，使 §30.2
  n_ineligible = depth − nused − nrej 恒等式在部分拒绝场景成立。
- **缺陷本体在写白名单（lib/phase2/ + tests/）之外**，未越界修改：
  `lib/core/src/module_adapters.cpp` 两处——
  (1) `:2511-2545`（p2_op_reject）把 kernel 的逐样本 reason 塌缩为**像素级**
  accepted(u8)"任一接受即 1"（`:2532-2536`），nrej 为像素级计数（`:2537`）；
  (2) `:2765`（p2_op_integrate）把该**像素级**标志套用到该像素的**每一个**
  样本——部分拒绝像素内被拒样本仍进积分 ⇒ nused 含被拒样本 ⇒
  nused + nrej > depth ⇒ n_ineligible < 0（违反 §30.2 完备划分）。
- **lib/phase2 两条生产路径已正确**（逐样本剔除，即本任务目标行为）：
  `tools/stage2.cpp:1462-1467 / 1515-1522` 与
  `src/acr_kernels.cpp:184-193` 均按 kernel reason 写**每样本** acc[] 后
  再过 p2_integrate_pixel。⇒ 违规面唯一 = lib/core 节点链（Stage2 CLI 与
  ACR 均不产出 §30.2 的 nused/nrej/variance/ivar 产品）。
- 本任务 in-scope 交付（白名单内，source 面零改动于 lib/phase2/src、
  include、tools）：tests/unit/p2002_unc_rej_prov_test.cpp 新增 2c/2d/2e 三节
  + 订正 2 节被缺陷行为编码的旧期望——（a）§30.2 恒等式逐像素断言
  （n_ineligible == 0，depth 由 candidates bins 机器佐证，非测试自述）；
  （b）§30.1 ivar_mosaic = Σ ivar_i 必须只含**入栈样本**（nrej>0 像素
  W = ivar_sum − ivar_F3），与 nused/nrej 三面一致性；
  （c）lib/phase2 库面对照：逐样本掩码 → 恒等式成立（GREEN），像素级塌缩
  → n_ineligible < 0（负向对照，证明缺陷不在本域）；
  （d）depth=3 部分拒绝 fixture 1/4 worker bitwise parity；
  （e）ASTROCS_P2002_FAULT=identity 等价缺陷注入必败。
- 判定基线（主树 = 缺陷在位）：RED，759 条 CHECK 失败，其中 252 像素
  （3 帧 × 每 32px 网格离群点）三面同时违反：
  n_ineligible = 3 − 3 − 1 = −1、nused=3 而 nrej=1、wsum=3.5 而应为 2.5。
  `run/scif2001/logs/red_run.log`。
- 补丁证明（git archive 影子树 + 本任务测试，**未落盘主树**）：
  `run/scif2001/minimal_patch_module_adapters.diff`（214 行，+108 行）
  逐样本掩码持久化（`p2_rejection_sample_mask.bin` + tiles[].depth/
  frame_slots/sample_mask_offset）+ integrate 逐样本消费（缺失即 fail-closed）
  → 本测试 0 失败 GREEN，受影响回归 9/9 PASS。不动 SCI 公式、默认容差、
  kernel、plan、产品合同（nused/nrej 语义与 dtype 不变）。
- **finding F-SCI-F2-001-01（P1，写域归属缺口）**：
  `lib/core/src/module_adapters.cpp` 同时承载两个独立 P1 缺陷
  （本条的 §30.2 逐样本剔除塌缩；FD-R1-012 的 p1 链就地重写撕裂读，约 12%
  假红），却**不在任何在册任务 write_scope 内**（FD-R1-013 已登记覆盖缺口；
  FD-R1-009 裁定前台不扩写域）。⇒ 需负责人新增以该文件为写域的原子任务
  或授权现有线承接；补丁已备好可直接采用。
- 交付后 main 的 CTEST-P2002-UNC-REJ-PROV（waivable=false）将转红——
  这是门应有的行为（P1/数据完整性不可 waiver，**未**登记进 known-failures
  基线，禁 waiver）。修复落地即自动转绿。
