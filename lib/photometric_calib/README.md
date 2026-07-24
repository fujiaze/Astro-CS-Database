# Photometric Calibration (Flux Calibration)

版本：v2.0 C++ DLL | Python ctypes封装 | 2026-07-12

## 模块职责

天文图像测光定标模块。将仪器流量校准到标准测光系统，使用 Gaia DR3/SP 光谱合成参考流量，输出与 Gaia 参考星系统一致的校正图像。

## GitHub仓库

- 仓库地址：https://github.com/fujiaze/Flux-calibration
- 默认分支：master
- 最新commit：1a6fd32

## 功能列表

- C++ DLL 实现（`pc_calibrate_simple` 函数，单一入口完成全流程）
- TAN+SIP WCS 投影（无 AP/BP 时用 3 次牛顿迭代反解前向 SIP）
- 暴力最近邻匹配 PSF 星和 Gaia 星（距离 < 3px）
- MAD 离群清洗（`sigma = MAD/0.6745`，sigma=0 时跳过）
- 简化算法：`scale = median(F_syn / F_instr)`，`I_cal = I * scale`
- OpenMP 16 线程并行（`I_cal = I * scale` 像素级计算）
- 全静态链接 DLL（697KB，仅依赖 KERNEL32/msvcrt 系统 DLL）

## 算法原理

天文图像中，像素值可分解为 `I = I_star × M + S`，其中 M 为乘性梯度（残留渐晕、大气消光、色差），S 为加性梯度（月光、光害、大气辉光）。

**v2.0 简化策略**：去掉乘性梯度曲面拟合（M_map）和加性天光校正（S_map），仅做全局 scale 校正。天光作为低频残差保留，一致性由后续马赛克背景匹配模块处理。

**合成测光**：对每颗 Gaia 参考星，用其 Gaia DR3/SP 光谱数据 + 滤光片透过率 + CCD QE 正向计算合成流量：

```
F_syn = ∫ S(λ) · T(λ) · Q(λ) · λ dλ
```

**校正流程**（C++ DLL 内部）：
1. WCS 投影 Gaia 星到像素坐标（TAN+SIP）
2. 暴力最近邻匹配 PSF 星和 Gaia 星（距离 < 3px）
3. MAD 离群清洗（`r = log10(F_instr / F_syn)`，sigma=3.0）
4. `scale = median(F_syn / F_instr)`
5. `I_cal = I * scale`

详细算法设计见 [docs/algorithm.md](docs/algorithm.md)。

## 目录结构

```
photometric_calib/
├── cpp/                              # C++ DLL 源码
│   ├── include/
│   │   └── photometric_calib.h       # C API 声明 (pc_calibrate_simple)
│   ├── src/
│   │   ├── pc_api.cpp                # C API 包装层
│   │   ├── star_matcher.cpp/.h       # 暴力最近邻匹配 + MAD 清洗
│   │   ├── image_corrector.cpp/.h    # I_cal = I*scale 校正
│   │   └── wcs_transform.cpp/.h      # TAN+SIP 投影
│   ├── test/
│   │   └── test_photometric_calib.py # 测试 (4/4 通过)
│   ├── Makefile
│   └── build.ps1
├── python/
│   ├── photometric_calib.py          # ctypes 封装 (PhotometricCalib 类)
│   ├── pc_logger.py                  # 日志系统
│   ├── sed_builder.py                # SED 构造器（spectrum_integrator 引用）
│   └── synthetic_photometry.py       # 合成测光（spectrum_integrator 引用）
├── flux_calibrator/
│   └── python/
│       ├── pipeline_adapter.py       # 管线适配器（调用 C++ DLL）
│       ├── fsyn_loader.py            # F_syn JSON 加载器
│       ├── test_pipeline_adapter.py
│       └── test_synthetic.py
├── spectrum_integrator/python/       # 光谱积分器（活跃版本）
├── shared/python/                    # 共享数据结构与日志
├── archive/                          # 归档代码
│   ├── estimator.py                  # 旧版 GradientEstimator（含梯度拟合）
│   ├── gradient_fitter.py            # 旧版梯度曲面拟合器
│   ├── old_gradient_tools/
│   ├── old_monolithic/
│   └── old_python_photometric/
├── docs/
│   ├── algorithm.md                  # 算法内核详设
│   └── architecture.md               # 架构设计
└── data/response_curves/
    ├── filters.json                  # 43 条滤光片透过率曲线
    └── qe_curves.json                # 12 条 CCD QE 曲线
```

## 依赖列表

### C++ 依赖
- C++17，OpenMP
- MinGW64 g++（MSYS2）
- `astro_image_io.dll`（PipelineFrame 接口，管线适配器层使用）
- Gaia DR3/SP 光谱数据（通过 `fsyn_loader.py` 加载 F_syn JSON）

### Python 依赖
- Python 3.10+
- numpy, ctypes
- astropy（WCS 测试用例）

### 外部 C++ DLL 依赖（光谱积分器）

| DLL | 仓库 | 用途 |
|-----|------|------|
| `gaia_client.dll` | [Gaia-DR3-DR3SP-Client-C](https://github.com/fujiaze/Gaia-DR3-DR3SP-Client-C) | Gaia DR3SP 光谱数据库访问 |
| `astro_image_io.dll` | [Astro-Image-IO-C](https://github.com/fujiaze/Astro-Image-IO-C) | FITS/XISF 图像读写 |
| `star_detector.dll` | [Star-Detector-Cpp](https://github.com/fujiaze/Star-Detector-Cpp) | 星点检测 |
| `dynamic_psf.dll` | [Dynamic-PSF-Cpp](https://github.com/fujiaze/Dynamic-PSF-Cpp) | Moffat4 PSF 拟合 |

## 编译说明

```bash
cd cpp
make                  # 生成 photometric_calib.dll (697KB)
```

或使用 PowerShell 脚本：

```powershell
cd cpp
powershell -ExecutionPolicy Bypass -File build.ps1
```

编译选项：`-O2 -std=c++17 -fopenmp -fPIC -Wall -shared -static -lm`

编译产出 `photometric_calib.dll`（697KB），全静态链接，仅依赖 KERNEL32/msvcrt 系统 DLL。

## 使用示例

### 1. 管线适配器调用（推荐）

通过 `pipeline_adapter.py` 将 C++ DLL 包装为 `PipelineStageHandler`，从 `PipelineFrame` 命名块读取数据并输出校正图像：

```python
from pipeline_adapter import PhotometricParams, register_photometric_handler

params = PhotometricParams(
    f_syn_path="f_syn_results.json",   # F_syn JSON 文件路径
    log_dir="logs/photometric",         # 日志目录
    match_radius_px=3.0,                # 匹配半径（仅日志，C++固定3.0）
    outlier_sigma=3.0,                  # MAD阈值（仅日志，C++固定3.0）
)
register_photometric_handler(engine, params)
```

处理器流程：
1. 从 `data` 块读取图像像素
2. 从 `header` KV 块读取 WCS/SIP 参数（CRVAL/CRPIX/CD/SIP）
3. 构造 Gaia 星数组（优先从 `f_syn_path` JSON 加载，回退到 `gaia_cat` 块）
4. 从 `psf` 块读取 PSF 拟合结果（[status, B, flux, cx, cy, fwhm]）
5. 调用 C++ `pc_calibrate_simple` 全局 scale 校正
6. 替换 `data` 块为校正后图像
7. 添加 `photo_stats` KV 块（N_MATCHED, SCALE_FACTOR）

### 2. 直接调用 C++ DLL 封装

```python
from photometric_calib import PhotometricCalib
import numpy as np

pc = PhotometricCalib()  # 自动查找 cpp/photometric_calib.dll

out_img, n_matched, scale = pc.calibrate_simple(
    pixels=image,                      # float32 [H, W]
    gaia_ra=ra, gaia_dec=dec,          # float64 [n_gaia]
    gaia_mag=mag, gaia_fsyn=fsyn,      # float64 [n_gaia]
    psf_cx=cx, psf_cy=cy,              # float64 [n_psf]
    psf_flux=flux, psf_status=status,  # float64 / int32 [n_psf]
    crval1=crval1, crval2=crval2,
    crpix1=crpix1, crpix2=crpix2,
    cd11=cd11, cd12=cd12, cd21=cd21, cd22=cd22,
    sip_order=sip_order,               # 0=无SIP
    sip_a=sip_a, sip_b=sip_b,          # float64 [36] (i*6+j 索引)
    sip_ap=sip_ap, sip_bp=sip_bp,      # 可选, None 时 C++ 用迭代法
)
# out_img: float32 [H, W] 校正后图像
# n_matched: MAD 清洗后匹配星数
# scale: 全局 scale 因子
```

### 3. 光谱积分器（生成 F_syn JSON）

```bash
cd spectrum_integrator/python

python run_integrator.py --ra 266.4168 --dec -28.9833 --radius 0.5 \
    --mag-low 8 --mag-high 16 --filter "Baader R" \
    --output f_syn_results.json
```

## 测试

### C++ DLL 测试

```bash
cd cpp/test
python test_photometric_calib.py
```

预期输出：4/4 通过
1. 基本测光校准（10 星 TAN 投影，scale=10.0）
2. MAD 离群清洗（20 星注入 1 离群，保留 19）
3. 无 Gaia 星退化（scale=1.0）
4. SIP WCS 投影（二阶 SIP，10 星匹配）

### Python 封装自测

```bash
cd python
python photometric_calib.py
```

## 性能数据

**测试日期**：2026-07-12
**环境**：16 线程 CPU + 64GB 内存，Windows，workers=2 并行（子进程级，DLL 安全）
**测试集**：7 个数据集 × 6 种滤光片，共 45 帧代表帧（每个 panel/filter 组合选文件最大的一帧）

### 总体结果

| 指标 | 数值 |
|------|------|
| 总帧数 | 45 |
| 成功 | 45 |
| 失败 | 0 |
| 成功率 | 100% |
| 总耗时 | 520.53 秒（8.7 分钟） |
| 平均每帧 | 11.6 秒 |
| Step3 星数 (min/avg/max) | 126 / 863 / 2002 |
| Step4 匹配星数 (min/avg/max) | 118 / 826 / 1979 |

### 分数据集统计

| 数据集 | 帧数 | 滤光片 | 星数范围 | 匹配星数范围 |
|--------|------|--------|----------|--------------|
| Galaxy_Center_T4 | 12 | B/G/R/Ha (3 panels) | 970 – 2002 | 922 – 1979 |
| Victory_Nebula_T4 | 8 | B/G/R/Lum (2 panels) | 1050 – 1399 | 936 – 1246 |
| NGC247_T2 | 6 | B/G/R/Ha/Lum/OIII | 230 – 252 | 202 – 240 |
| LDN43_T2 | 5 | B/G/R/Ha/Lum | 126 – 144 | 118 – 138 |
| NGC1727_T2 | 5 | B/G/R/Ha/OIII | 1325 – 1574 | 1305 – 1559 |
| NGC55_T3 | 5 | B/G/R/Ha/Lum | 181 – 249 | 176 – 243 |
| NGC83_cluster_T3 | 4 | B/G/R/Lum | 201 – 215 | 181 – 205 |

### 说明

- **星数差异**：银河中心区域（Galaxy_Center_T4）星密度最高（最高 2002 颗），暗星云区域（LDN43_T2）星密度最低（最低 126 颗），均能成功定标。
- **代表帧策略**：每个 panel/filter 组合选文件最大（通常曝光长、星点足）的一帧执行 Step3+Step4，得到定标系数后可应用到该组其他帧。
- **并行策略**：workers=2（子进程级并行），DLL 线程安全约束下最高稳定并行数。串行（workers=1）最稳定，workers=2 可加速但可能触发 DLL 堆冲突。

## 版本历史

- **v2.0 (2026-07-12)**：C++ DLL 重写，去掉梯度拟合（M_map 曲面），简化为全局 scale 校正
  - 新建 `cpp/` 目录，实现 `pc_calibrate_simple` C API
  - `python/photometric_calib.py` 提供 ctypes 封装（PhotometricCalib 类）
  - `flux_calibrator/python/pipeline_adapter.py` 重写为调用 C++ DLL
  - Python 版 `estimator.py` 和 `gradient_fitter.py` 归档到 `archive/`
  - 编译产出 `photometric_calib.dll`（697KB，全静态链接，OpenMP 16 线程）
  - 测试 4/4 通过
- **v1.1 (2026-07-12)**：封存天光校正（S_map），仅保留乘性流量定标（M_map）
  - 校正公式由 `I_cal = (I - S_map) / M_map` 改为 `I_cal = I / M_map`
  - 模块重命名：`gradient_estimator` → `flux_calibrator`
- **v1.0**：Python 开发原型，完整天光 + 流量定标（含 IRLS + Tukey biweight 稳健回归 + LOOCV 选阶的梯度曲面拟合）

## 关键决策

### 去掉梯度拟合，简化为全局 scale 校正

v2.0 去掉乘性梯度曲面拟合（M_map）和加性天光校正（S_map），改为全局 scale 校正：
- 校正公式从 `I_cal = I / M_map` 变为 `I_cal = I * scale`
- `scale = median(F_syn / F_instr)`，单一标量校正全图
- Python 版 `estimator.py`（GradientEstimator）和 `gradient_fitter.py`（梯度曲面拟合器）已归档到 `archive/`

### 天光校正封存（v1.1）

S_map 加性梯度天光校正存在根本缺陷：
1. PSF 拟合的局部背景 B 值包含 ISL+DGL+气辉+黄道光+星云目标信号，在银河等区域缓变星云信号会被当作天光误减
2. 采样稀疏（仅星点位置），多项式在星点之间无物理约束
3. 无物理先验，无法区分"缓变天光"与"缓变星云"

天光一致性留给后续马赛克背景匹配模块处理（多帧共享最小残差低频背景）。

### r 定义为 log10(F_instr / F_syn)

v1.0 开发过程中发现并修复的关键 bug：r 必须定义为 `log10(F_instr / F_syn)` 而非 `log10(F_syn / F_instr)`。

图像模型 `I = I_star × M + S` 中 M 为渐晕因子，`F_instr = I_star × M`，`F_syn = I_star`，故 `log10(M) = log10(F_instr / F_syn)`。若 r 定义反了，`M_map = 1/M_true`，校正结果为 `I_star × M_true²`（错误）。

### C++ DLL 设计要点

- WCS：CRPIX 1-based，像素 0-based，`dx = x - (CRPIX - 1)`
- SIP 系数按 `i*6+j` 索引（长度 36 扁平数组）
- 无 AP/BP 时用 3 次牛顿迭代反解前向 SIP
- 暴力最近邻（Gaia 星通常 < 10000，无需 nanoflann）
- MAD：`sigma = MAD/0.6745`，sigma=0 时跳过清洗（与 Python 版一致）
- 全静态链接，仅依赖系统 DLL，便于部署

## 许可证

MIT
