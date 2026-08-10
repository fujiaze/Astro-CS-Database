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
