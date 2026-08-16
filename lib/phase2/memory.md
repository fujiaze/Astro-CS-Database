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
