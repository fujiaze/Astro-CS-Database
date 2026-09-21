# before_stackn32 —— stack_n=1 交付版备份（STACKN32-001）

本目录保存 **`stack_n = 1`（读出噪声 3.1 e⁻）时代**的场景配方，供前后对照与复现。
**不得删除**：任务硬要求"必须保留前后两套数字，不得只留新版"。

## 内容

| 路径 | 内容 |
|---|---|
| `scenes/m16_{nebula_core,starfield,dark_lowsnr,band_matrix}.json` | 4 个 M16 场景配方的 **stack_n=1 版**（改动前逐字节副本） |

> 大产物（FITS 帧、实验 result JSON）不在本目录，而在
> `run/RELEASE-02/paper/data/STACKN32/before_stackn32/`（119 MB，含交付数据集副本、
> M16-SCENE 交付帧副本、M16FIX/P9 的全部 stack_n=1 数字）。

## 代码侧改动（逐条 before → after）

1. `实验/shared/synthetic/scenes/m16_*.json`（4 个）
   - `"stack_n": 1` → `"stack_n": 32`
   - `calibration.$comment` 追加 STACKN32-001 说明；新增 `calibration.stack_n_rule`
2. `实验/shared/synthetic/m16_scene.py`
   - 模块 docstring 的 `stack_n` 字段说明追加默认值 32 与 √32×3.1 = 17.536 e⁻
   - `M16_DEFAULTS["stack_n"]`: `1` → `32`（原 171 行）
   - `render_m16_frame` 内 `stack_n = int(sc.get("stack_n", 1))` → `... , 32))`（原 498 行）
   - **噪声链公式、读出噪声字段 `detector.read_noise_e = 3.1`、满阱、曝光、平场、天光路径一字未改**
3. `实验/absolute-snr/code/reverse_verify/p7_noise/p7lib.py`
   - `det_of()` 的兜底默认 `scene.get("stack_n", 1)` → `..., 32)`（无行为影响：场景总是显式给值）
4. `实验/shared/data/synthetic/generate.py`
   - **无 `stack_n` 硬编码**（已逐行核对：该文件从不读写 `stack_n`）；只在 docstring 追加 STACKN32-001 说明
5. `实验/shared/data/synthetic/datasets.json`
   - 新增顶层 `$stack_n` 登记项（纯文档，无行为影响）
6. `实验/absolute-snr/code/reverse_verify/p7_noise/exp{1,3,4}_*.py`
   - 只改**注释/说明字符串**（原文"不改场景 JSON 默认值"已过时）；**判据、阈值、臂设计一字未改**
   - exp1 的 `design.stack_n_rationale` 与 exp4 的 `criteria.P2_note` 同步订正（见 STACKN32-001 §5.4）
7. `run/RELEASE-02/paper/data/M16FIX/src/a6_before_repro.py`
   - 新增 `A6_OUT_SUFFIX` 环境变量（默认空 ⇒ 原行为不变），用于把 2×2 分解写到 `a6_*_stackn32/`
     而不覆盖 stack_n=1 的历史产物

## 未改（红线）

- **任何判据阈值 / 容差**（A6 的 0.05 / 0.30 / 5× / 0.02；方差闭合的 5% / 2% / 3√2σ_v；
  回归的 1e-9 / [0.5,2] / 0.30；P7 的 R1–R5 / S1–S6 / P1–P6）—— **一字未改**；
- `lib/** docs/** eng/tests/** ci/** eng/contracts/** eng/packaging/config/**` —— 未触碰；
- 未跑 `ci/run_checks.py`、未跑 ninja/cmake；**零 git 写**。
