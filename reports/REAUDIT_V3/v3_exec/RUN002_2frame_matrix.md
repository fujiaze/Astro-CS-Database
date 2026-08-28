# RUN-002 —— 每板块 2 帧 A/B/C/D 小矩阵（真实数据，Fatduck Windows）

> 规格（03_TASK_SPECIFICATIONS RUN-002）：每板块 2 帧运行 A/B/C/D 小矩阵；先验证配置
> 语义映射（特别是 `weight_mode`）；小矩阵任何**未解释**科学差异或接缝回归则停止，禁止全量。
> 执行：2026-08-28 16:57–17:30 CST（Fatduck 在线窗口内）；stage1×6 帧 + stage2×5 runs 全部
> exit=0。审核人指令："这些数据都在fatduck上，你可以远程测试"。

## 1 输入与执行矩阵

- **输入**：`testdata\Galaxy_Center_T4` 每板块前 2 帧 Red（panel1/2/3 × 2 = 6 帧），经
  **stage1 真实全链路**（orchestrator 2.0，Calibrate→PlateSolve→Photometric(GaiaDR3SP)→SNR→
  Drizzle→HiPS_verify）生成 per-frame HiPS ×6，**6/6 exit=0**。
- **矩阵**（stage2，同一批 6 帧 HiPS）：

| run | 锚/代码 | exe SHA256(前16) | weight_mode 配置 | 运行时映射 | wall |
|---|---|---|---|---|---|
| A2R | A=b38b446e6 (V16 配置语法) | 2570aca7ae941cc5 | auto | (legacy 0, 旧诊断无此字段) | 314s |
| B2R | B=83471979a (V16) | 480aa7469a703828 | auto | (legacy 0) | 330s |
| C2R | C=535e7387 (**V17 迁移后语法**) | 647cb2aef73998af | ivar | **2** | 199s |
| D2R | D=7737f539 (V17) | 7355a61f4e827a0a | ivar | **2** | 171s |
| Cleg2R | C=535e7387 (V17) | 647cb2aef73998af | support_x_snr2 | **0** | 204s |

## 2 配置代际语义映射验证 ✓（本任务核心）

1. **代码级**（git show 三锚 stage2_common.cpp）：A/B `auto|support_x_snr2→0`（R5 冻结 legacy）；
   C/D `auto|ivar→2`、`support_x_snr2→0`（SNR-008 退休）。
2. **运行级**：C/D diagnostics 显式输出 `weight_mode: 2`、Cleg2R 输出 `weight_mode: 0` ——
   与配置意图一致。
3. **V17 迁移闭环**：C/D 拒绝 V16 `rejection.{low,high,max_iterations,min_samples}`（报错并指向
   `tools/migrate_stage2_config.py`）→ 用官方迁移工具转换（typed `rejection.winsorized_sigma.
   {lower_sigma,upper_sigma,max_iterations}` + `underdetermined_n`）→ 重跑 exit=0。A/B 保留 V16
   原语法（其代际无 typed schema）。**同一科学意图（winsorized 4/3/min2）两代语法各自生效**。
4. **交叉验证**：C2R(ivar) vs Cleg2R(legacy) 同 exe 同输入 → signal 相对差 12.3%
   （99% 像素），support **完全一致** —— weight_mode 只影响权重、不影响 support/validity
   语义，自洽。

## 3 科学差异判定（全部可解释，无未解释差异）

数值对比（Norder3 全部 tiles，634,194 signal 像素，astropy 逐像素）：

| 对比 | max_rel_diff | 差异像素 | 判定 |
|---|---|---|---|
| **C2R vs D2R** | **0.000e+00** | **0 / 634,194** | **D 相对 C 零数值回归** ✓ |
| C2R vs Cleg2R | 1.233e-01 | 627,765/634,194 | ivar vs legacy 权重语义变更本身（预发布科学定义） |
| A2R vs B2R | 5.648e-01 | 624,593/634,194 | A→B 背景洁净 UPM 采样语义演进（B commit 记录的预期变更） |
| support 三对 | 0.000e+00 | 0 | support 语义跨锚一致 |

- **UPM 模型**：`upm_sparse.json` 与 `model_hash` 在 C2R/D2R/Cleg2R **位级一致**
  （`7ee8da43def46880` / `369c65b6f4493015c5c6…`）→ 535e7387→7737f539 间代码演进对模型零影响。
- **结构指标**：C2R/D2R 完全一致（integrated 24,171,613、controls 11,040、obs 18,273、
  tiles 690）；A2R controls 9,856（旧采样）；B2R obs 18,273 = C/D（B 的采样门控语义被 C/D 继承）。
- **像素深度口径澄清**：A/B 诊断的 `integrated_pixels`(160.1M) = C/D 的 `pixels_depth_ge_2`
  (160.1M)；C/D 新增 `underdetermined_pixels`(137.6M) 为显式欠定指标（A/B 无此概念）。
  86% 欠定是**每 tile 仅 2 帧小样本的预期几何效应**（B 的洁净采样要求 control ≥2 clean obs，
  2R 下 obs/control≈1.65）；V2 32R 全量实测欠定率 1.9%。非回归，记录为小矩阵限制。

## 4 结论与放行

- **RUN-002 判 PASS**：配置语义映射（weight_mode 两代语法）验证成立；全部差异均已解释；
  **D 候选相对 C 起点数值逐位一致**；无接缝回归信号（support 一致 + 同语义 mosaic 一致）。
- **放行 RUN-003..006（32R 全量）**。
- 限制：2R 小矩阵欠定率高（86%）为样本量效应；panel3 第 2 帧与 panel1/2 同规格
  （各板块均 2 帧，无缺帧）；stage1 6 帧为本次新生成（Fatduck 上 V2 旧 per-frame HiPS
  已清理），生产配置模板 `lib/orchestrator/configs/stage1_gc_panel*_Red.json` 未改动。
- 产物：`Fatduck:...\run\temp\reaudit_v3\run002\{s1,s2,out_*.mosaic.hips}` + 本地
  `run/reaudit_v3/run002/diag/*_diagnostics.json`（5 份）+ 各 run 日志。
