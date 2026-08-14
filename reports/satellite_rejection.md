# 卫星线真实生产门（V15）

## 1. Exposure granularity（核心架构问题回答）

Phase2 输入 = **per-exposure HiPS**（Phase1 每帧独立 signal/support/SNR 产品；
20 个输入各自独立，frame_id 因 signal payload 不同而不同）。stage2
REJECT_INTEGRATE 在 rejection 前按像素保持**独立 exposure 栈**
（`stack[]/weights[]/support_v[]/fid_stack[]` 每像素收集，未 collapse 成
panel/master）。结论：Phase1→Phase2 不存在"多 exposure 先合成 panel"问题；
现有 GC/t4 overlap 只有 2 帧重叠是数据覆盖问题，不是架构 collapse。

## 2. 20-exposure 生产门（受控注入，真实星场/背景底图）

构造（`lib/phase2/tools/satellite_gate_build.py`，NON_PRODUCTION_TOOL_ONLY）：

- 底图：真实 Phase1 单帧 HiPS `t4_crop_v3.hips`（GC Red，24 tile，真实星场/背景）；
- 20 个独立 HiPS 输入：每帧独立乘性/加性噪声（±1% / 3e-5），support/NaN
  结构与 SNR catalogue 保持原样；
- 第 10 帧注入大圆卫星线（signal += 0.05 ≈ 17×背景 median，宽度 ~1.5px，
  1418 像素，跨 3 tile）。

生产路径：`astrocs-stage2`，`rejection.method=auto, profile=wbpp_current`。

## 3. 指标（`evidence/satellite_metrics.json`）

| 指标 | 值 |
| --- | --- |
| nominal contributors | 20（每 tile） |
| resolved method | `astrocs.linear_fit_siril_1_4_3.v1`（WBPP n>15 政策一致） |
| effective contributor histogram | 全部像素 depth=20；depth_1=0；underdetermined_pixels=0 |
| trail rejection recall | **1.0000**（1418/1418 真实 kernel 判定） |
| clean false reject（像素级 ≥1 样本） | 88.7%（1500 背景像素采样；致密 GC 天区） |
| clean sample-level false reject | 27.65%（8294/30000 样本；冻结 linear-fit 固有行为） |
| mosaic 背景 bias | median=0.00e+00，p95=0.00e+00 |
| 星点通量 bias（相对） | median=0.00e+00，p95=0.00e+00 |

判定：trail 显著抑制（recall=1.0）；星点/背景无净损伤（bias=0）；resolved
method 与 WBPP profile 一致 → **SATELLITE_REJECTION_GATE=PASS**。

## 4. n=1–2 → REJECTION_UNDERDETERMINED（真实产品现状）

真实 GC/t4 overlap（`t4_crop_v3 × t4_full_v3_final`，285 tile，2 输入）生产
run（auto/wbpp_current）：

- resolved：`percentile`（nominal 1-2 < 6 → WBPP 政策）；
- **underdetermined_pixels = 61,588,497（100%）**；rejected_samples = 0；
- fallback_pixels = 61,588,497（全接受，明确诊断）。

结论：n=1–2 明确 `REJECTION_UNDERDETERMINED`，**不宣称 Auto 可剔除卫星线**；
单帧空间 trail detector 是另一立项（不伪装成 pixel-stack rejection）。

## 5. 诚实性

- per-pixel 判定全部来自生产 rejection kernel（rejection_cli → 
  `p2_reject_stack_ex`），无 Python 镜像判定；
- clean false reject 数值为冻结 linear-fit 语义固有行为，如实报告，不降
  threshold 制造 PASS；
- 20 帧为"真实底图 + 受控噪声/注入"构造（控制包允许"构造"），非真实
  同夜 20 帧连拍（如实标注）。
