# Robust Flux Calibration

基于 Gaia DR3/SP 光谱数据消除天文图像空间缓变梯度，输出与 Gaia 星表系统一致的校正图像。

## 版本说明

**v1.0** — Python 开发原型。后续将 C++ 重写。

## 算法原理概述

天文图像中，像素值可分解为 `I = I_star × M + S`，其中 M 为乘性梯度（残留渐晕、大气消光、色差），S 为加性梯度（月光、光害、大气辉光）。

**双通道分离**：PSF 拟合对每颗星输出两个独立观测量——星通量 `F_instr`（编码乘性梯度 M）和局部背景 `B`（编码加性梯度 S），天然解耦不同物理性质的梯度。

**合成测光**：对每颗 Gaia 参考星，用其Gaia DR3/SP 光谱数据 + 滤光片透过率 + CCD QE 正向计算合成流量：

```
F_syn = ∫ S(λ) · T(λ) · Q(λ) · λ dλ
```

**乘性梯度拟合**：定义比值 `r = log10(F_instr / F_syn)`，用 IRLS + Tukey biweight 稳健回归拟合 2D 多项式曲面。LOOCV 自动选择最优阶数，无需人工调参。

**校正**：`I_cal = (I - S) / M`，再乘以全局缩放因子归一化到 Gaia 参考星系统。

详细算法设计见 [docs/algorithm.md](docs/algorithm.md)。

## 双程序架构

```
┌─────────────────────┐     F_syn JSON     ┌─────────────────────┐
│  光谱积分器          │ ──────────────────▶ │  梯度估算器          │
│  Spectrum Integrator │                    │  Gradient Estimator │
├─────────────────────┤                    ├─────────────────────┤
│ Gaia 光谱数据      │                    │ FITS 图像 + WCS      │
│ + 滤光片 + QE        │                    │ + F_syn JSON         │
│ → F_syn JSON        │                    │ → 校正图像 + 报告     │
└─────────────────────┘                    └─────────────────────┘
```

两个程序完全解耦，通过 JSON 文件通信。详细架构设计见 [docs/architecture.md](docs/architecture.md)。

## 目录结构

```
Robust-Flux-Calibration/
├── README.md
├── .gitignore
├── docs/
│   ├── algorithm.md                   # 算法内核详设
│   └── architecture.md                # 双程序架构设计
├── data/response_curves/
│   ├── filters.json                   # 43 条滤光片透过率曲线
│   └── qe_curves.json                 # 12 条 CCD QE 曲线
├── spectrum_integrator/python/        # 光谱积分器 (6 .py)
├── gradient_estimator/python/         # 梯度估算器 (10 .py)
├── shared/python/                     # 共享组件 (3 .py)
└── archive/old_monolithic/            # 旧单体代码，不再维护 (8 .py)
```

## 安装与依赖

### Python 依赖

- Python 3.10+
- numpy, scipy, astropy, Pillow

### 外部 C++ DLL 依赖

本仓库不含 DLL，需自行编译或从以下仓库获取：

| DLL | 仓库 | 用途 |
|-----|------|------|
| `gaia_client.dll` | [Gaia-DR3-DR3SP-Client-C](https://github.com/fujiaze/Gaia-DR3-DR3SP-Client-C) | Gaia DR3SP 光谱数据库访问 |
| `astro_image_io.dll` | [Astro-Image-IO-C](https://github.com/fujiaze/Astro-Image-IO-C) | FITS/XISF 图像读写 |
| `star_detector.dll` | [Star-Detector-Cpp](https://github.com/fujiaze/Star-Detector-Cpp) | 星点检测 |
| `dynamic_psf.dll` | [Dynamic-PSF-Cpp](https://github.com/fujiaze/Dynamic-PSF-Cpp) | Moffat4 PSF 拟合 |

编译后将 DLL 放到 PATH 可访问的目录，或与 Python 脚本同级目录。

### Gaia DR3SP 数据库

需单独部署 Gaia DR3SP 光谱数据库（gdr3sp-1.0.0-01~20.xpsd，2.2 亿星）。详见 Gaia-DR3-DR3SP-Client-C 仓库。

## 快速开始

### 1. 光谱积分器

计算视场内 Gaia 参考星的合成流量 F_syn：

```bash
cd Python/spectrum_integrator/python

python run_integrator.py --ra 266.4168 --dec -28.9833 --radius 0.5 \
    --mag-low 8 --mag-high 16 --filter "Baader R" \
    --output f_syn_results.json
```

### 2. 梯度估算器

用 F_syn 结果校正天文图像：

```bash
cd Python/gradient_estimator/python

python run_estimator.py --image light.fits \
    --fsyn f_syn_results.json \
    --output calibrated.fits \
    --report quality_report.json
```

## 模块说明

### 光谱积分器 (spectrum_integrator)

| 模块 | 功能 |
|------|------|
| `gaia_spectrum_client.py` | GaiaSpectrumClient — 封装 gaia_client.dll，锥形搜索+光谱获取 |
| `integrator.py` | SpectrumIntegrator — 批量合成流量计算 F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ |
| `synthetic_photometry.py` | SyntheticPhotometry — Akima 插值 + Simpson 1/3 积分 |
| `curve_loader.py` | CurveLoader — 加载滤光片/QE 曲线 JSON |
| `run_integrator.py` | CLI 入口 |

### 梯度估算器 (gradient_estimator)

| 模块 | 功能 |
|------|------|
| `fsyn_loader.py` | FSynLoader — 加载 F_syn JSON 结果 |
| `star_matcher.py` | StarMatcher — Gaia 星 ↔ PSF 星空间匹配 (KDTree + MAD 清洗) |
| `gradient_fitter.py` | GradientFitter — IRLS + Tukey biweight 稳健回归 + LOOCV 选阶 |
| `image_corrector.py` | ImageCorrector — 图像校正 I_cal=(I-S)/M + 归一化 |
| `estimator.py` | GradientEstimator — 主程序，串联定标全流程 |
| `wcs_transform.py` | WCSTransform — WCS 坐标转换 (TAN+SIP) |
| `run_estimator.py` | CLI 入口 |
| `test_synthetic.py` | 合成数据端到端验证 |

### 共享组件 (shared)

| 模块 | 功能 |
|------|------|
| `data_types.py` | GaiaSpectrumStarPy, FSynResult 数据结构 |
| `pc_logger.py` | 统一日志系统 |

## CLI 参数详解

### run_integrator.py

| 参数 | 必填 | 默认值 | 说明 |
|------|------|--------|------|
| `--ra` | 是 | — | 视场中心赤经 (度) |
| `--dec` | 是 | — | 视场中心赤纬 (度) |
| `--radius` | 否 | 0.5 | 锥形搜索半径 (度) |
| `--mag-low` | 否 | 8 | 星等下限 |
| `--mag-high` | 否 | 16 | 星等上限 |
| `--filter` | 是 | — | 滤光片名称 (用 --list-filters 查看) |
| `--qe` | 否 | — | QE 曲线名称 (用 --list-qe 查看) |
| `--gaia-data` | 否 | 自动 | Gaia DR3SP 数据目录 |
| `--output` | 否 | f_syn_results.json | 输出 JSON 路径 |
| `--list-filters` | — | — | 列出所有可用滤光片 |
| `--list-qe` | — | — | 列出所有可用 QE 曲线 |

### run_estimator.py

| 参数 | 必填 | 默认值 | 说明 |
|------|------|--------|------|
| `--image` | 是 | — | 输入 FITS/XISF 图像路径 |
| `--fsyn` | 是 | — | F_syn JSON 文件路径 |
| `--output` | 否 | calibrated.fits | 校正后图像输出路径 |
| `--report` | 否 | quality_report.json | 质量报告 JSON 输出路径 |
| `--residual-dir` | 否 | logs | 残差 CSV 输出目录 |
| `--match-radius` | 否 | 3.0 | 星-图匹配半径 (像素) |
| `--outlier-sigma` | 否 | 3.0 | MAD 离群点清洗 sigma 阈值 |
| `--max-order` | 否 | 5 | 多项式最大阶数 |
| `--wcs-json` | 否 | — | plate_solve WCS JSON (图像无 WCS 时使用) |

## 测试

合成数据端到端验证（1024×1024 图像，30 颗星，已知梯度）：

```bash
cd Python/gradient_estimator/python
python test_synthetic.py
```

预期输出：3/3 通过（梯度恢复、质量报告、残差 CSV）。

## 关键设计决策

### r 定义为 log10(F_instr / F_syn)

v1.0 开发过程中发现并修复的关键 bug：r 必须定义为 `log10(F_instr / F_syn)` 而非 `log10(F_syn / F_instr)`。

图像模型 `I = I_star × M + S` 中 M 为渐晕因子，`F_instr = I_star × M`，`F_syn = I_star`，故 `log10(M) = log10(F_instr / F_syn)`。若 r 定义反了，`M_map = 1/M_true`，校正结果为 `I_star × M_true²`（错误）。

### 不含 DLL

本仓库为纯 Python 原型，不含预编译 DLL。4 个 C++ 依赖仓库独立维护，避免二进制文件膨胀和版本不同步。

## 许可证

MIT
