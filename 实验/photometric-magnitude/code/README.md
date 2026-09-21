# SCI-A · code（固定种子一键复跑）

固定随机种子：`20260921`（`scia_common.SCIA_SEED`，所有 RNG 由 `scia_common.rng(tag)`
以 SHA256 派生，跨机器/跨进程可复现）。

## 一键复跑

```bash
# 全量（含最慢的 step6 单元消除族，总计约 10–20 分钟，取决于 CPU）
bash 实验/photometric-magnitude/code/run_all.sh

# 快速（跳过 step6）
bash 实验/photometric-magnitude/code/run_all.sh quick
```

日志落 `run/SCI-401/logs/step*.log`，结果落 `实验/photometric-magnitude/results/*.json`，
中间产物（仿真帧、缓存）落 `run/SCI-401/`（不入库）。

## 文件清单

| 文件 | 作用 |
|---|---|
| `scia_common.py` | 核心：确定性 RNG、通带/QE 装载、Akima 子样条 + 复合 Simpson 积分（与 `spectrum_integrator.cpp` 同约定）、IRLS/Tukey 零点估计、Moffat PSF 渲染与 4 参数拟合、精确 Fisher PSF 拟合方差、预算/判据、盲检测、星表匹配、WCS 工具 |
| `scia_sim.py` | 仪器/真值模型与前向仿真：逐像素 Poisson（源+天光+暗流）→ 高斯读出 → 增益 → 饱和 → 量化；椭圆 Moffat 注入；星等→通量的低阶空间增益 `m(x,y)`；解析合成帧 |
| `scia_calib.py` | 星表引导测光、独立孔径测光、PSF vs 孔径系统项、逐帧零点标定（IRLS）、逐项误差预算 |
| `scia_pipeline.py` | 流水线级公共件：仿真帧装载、质量选择（有效域语义）、预算组装、`apply_photometry`、低阶空间增益设计矩阵 |
| `scia_gaia.py` | Gaia DR3SP XP 本地星表读取（`lib/infrastructure/gaia_xpsd_client`）+ 锥形查询缓存 |
| `gaia_xp_dump.c` | 独立 C 夹具：直接链接仓库内 `gaia_client.c`（只读）导出 XP 锥形样本为 CSV |
| `step0_fetch_refs.sh` | 唯一需要网络的步骤：取 SVO HST/WFC3_UVIS2 F657N/F673N/F502N 总系统透过率曲线并打印 SHA256 |
| `step1_analytic.py` | 纯解析代数合成（600 星，SED 取真实 XP）：Oracle、收敛性、负例、噪声标定 |
| `step2_hst_sim.py` | HST M16 F657N drz → 24×24 分块平均模板 → 物理前向仿真帧 A（透明 1.0）/ B（0.62）/ C（扩展星场 500 星） |
| `step3_forward_vs_photflam.py` | XP 合成通量 vs HST PHOTFLAM 绝对定标对拍（三滤镜、跨星色、WCS 二轮精化） |
| `step4_guided_vs_blind.py` | 星表引导检测 vs 全图盲检测：匹配率、虚警、算力、粗 WCS 鲁棒性 |
| `step5_calibration_gate.py` | 逐帧标定 + 误差预算双边界判据 + 帧间独立 |
| `step6_apply_and_units.py` | `apply photometry` 落像素与下游消费；物理单位消除 + 反推不可辨识退化族 |
| `step7_negatives.py` | 6 条非退化负例（N0–N5） |
| `step8_real_frame.py` | testdata 真实帧（底参照）：帧内仪器参数、引导 vs 盲检、单帧可自算预算 |
| `step9_collect.py` | 汇总判据表 → `results/GATES.md` / `results/gates.json` |

## 依赖

本机实测环境（`python3 -c "import sys,numpy,scipy,astropy; print(...)"`）：
**Python 3.13.5 / numpy 2.2.4 / scipy 1.15.3 / astropy 7.0.1**。
未使用 photutils / sep（避免引入未登记依赖）；`results/` 中引用的 photutils 3.0.0 / sep 1.4.1
行号来自**文献与上游源码核对**，不是本机安装版本（见 `run/SCI-401/lit/verified_refs.md`）。
`gaia_xp_dump.c` 用 `gcc` 编译，输出到 `run/SCI-401/bin/`。

## 约定与陷阱（轮次 1 审稿后补记）

- `scia_common.jdump` 会把 **NaN/Inf 统一写成 `null`**（`allow_nan=False`），保证产物是合法 JSON。
- `scia_calib.calibrate` 返回的 `m_coeffs` 是**星等残差**的多项式 `poly(x,y)`；
  归一化修正场是 `m(x,y) = 10^{+0.4·poly(x,y)} = 1/m_gain`（**不是** `10^{-0.4·poly}`）。
  `step6` 有方向自检字段 `m_correction_improves` 防回归。
- `budget_from_frame` 的 `sigma_fit_robust` **只合成逐像素噪声**；系统项由 `Budget.sigma_ceiling`
  在平方和中各计一次（不要重复折进 MC）。
- `step8_real_frame.py` 读 FITS 时**不要**再减 `BZERO`：astropy 对 uint16+BZERO 已给物理值
  （该 bug 曾导致归档结果与交付代码不自洽，见 `results/REVIEW.md` R1）。
- `run_all.sh quick` 跳过 step6；`step9_collect.py` 会把缺失项标 `NOT_RUN` 而不是崩溃。
