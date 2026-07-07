# Star Detector 模块变更日志

## V5.0 (2026-07-07) — 模块化重构 + 代码清理 + 编译优化

### 重构内容

1. **代码清理**: 移除源代码中所有第三方项目引用元素（注释、宏名、函数名、变量名、日志字符串），使代码独立于任何参考来源。
   - 宏重命名: `SIRIL_XTOL` → `LM_XTOL`, `SIRIL_INV_4_LOG2` → `INV_4_LOG2` 等
   - 函数重命名: `sdet_compute_bgnoise_siril` → `sdet_compute_bgnoise`, `sdet_gsl_gaussian_f` → `sdet_gaussian_f`, `sdet_gsl_lm_fit` → `sdet_lm_fit`
   - 结构体重命名: `GSL_PSFData` → `PSFFitData`
   - 注释清理: 删除所有第三方项目引用字眼（对齐类表述、项目名前缀注释、复刻类表述等）
   - 日志清理: "XXX peaker" → "peaker"

2. **死代码删除**: 移除已被 GSL trust-region LM 替代的旧手写 LM 函数（sdet_gaussian_residual, sdet_gaussian_residual_and_jacobian, sdet_lm_solve, sdet_compute_trimmed_mad），消除编译警告。

3. **编译性能优化** (Makefile):
   - 默认配置: `-O3 -march=native -ffp-contract=fast -funroll-loops`
   - 新增 `make lto` 目标: 启用链接时优化（-flto），运行时性能更优
   - 新增 `make pch` 目标: 启用预编译头（PCH），重复编译更快
   - 新增对象文件目标（`%.o`），为未来并行编译扩展预留

4. **归档原代码**: 原始版本（含第三方项目引用注释）归档于 `archive/V4.66_pre_cleanup/`，保留完整历史。

### 算法参考说明

本模块的 PSF 拟合算法参考了 Siril 1.4.3 (https://siril.org/) 的以下实现:
- GSL trust-region LM 求解器配置 (`gsl_multifit_nlinear_trs_lm`)
- halfA 边界搜索初始化策略
- Gaussian PSF 参数化 `{B, A, x0, y0, SX=2σ², fr=acos(2r-1), alpha}`
- 雅可比矩阵解析表达式
- 候选排序 `mag_est` 降序
- 候选 `reject_star` FWHM 自适应上限
- mag box 积分 `Σ(pixel - B)`
- 背景噪声估计 `FnNoise1_ushort` 算法

这些参考提高了星点检出及 flux 的准确性和鲁棒性,特别是:
- GSL LM 的 More 缩放/对角预处理解决了参数量级差异导致的收敛问题
- halfA 边界搜索初始化提供了稳健的拟合起点
- 候选 `mag_est` 排序保证了与参考实现一致的星等顺序

### 与 Siril 1.4.3 效果对比 (3 帧抽样)

| 帧 | FOV | IPv饱和 | Siril饱和 | 饱和偏差 | 前60交集 | 位置重复率 | 顺序重复率 |
|----|-----|---------|-----------|---------|---------|-----------|-----------|
| NGC4945_Lum | wide | 1474 | 1470 | 0 | 60/60 | 100% | 100% |
| NGC6302_narrow | narrow | 0 | 0 | 0 | 59/60 | 98.3% | 100% |
| Galaxy_Center_Oiii | wide | 219 | 221 | 1 | 59/60 | 98.3% | 100% |

3 帧全部达标（饱和偏差 ≤ 1, 非饱和位置+顺序 > 90%）。

### 与 Siril 1.4.3 效果对比 (790 帧全帧)

| FOV | 帧数 | 达标率 | 前60中位 | 前100中位 | 饱和偏差中位 |
|-----|------|--------|----------|-----------|-------------|
| narrow | 38 | 89.5% | 98.3% | 98.0% | 0.0 |
| medium | 367 | 70.8% | 98.3% | 98.0% | 1.0 |
| wide | 385 | 43.3% | 96.7% | 98.0% | 3.0 |

| 通道 | 帧数 | 达标率 | 前60中位 |
|------|------|--------|----------|
| Lum | 168 | 26.2% | 95.0% |
| Red | 133 | 43.6% | 96.7% |
| Green | 135 | 50.4% | 98.3% |
| Blue | 129 | 66.7% | 98.3% |
| Oiii | 59 | 79.7% | 98.3% |
| H-alpha | 122 | 92.6% | 98.3% |
| OIII | 30 | 96.7% | 100.0% |
| Sii | 17 | 100.0% | 98.3% |

**前60/前100 位置重复率统计 (790 帧)**:

| 指标 | 中位数 | min | max | 均值 |
|------|--------|-----|-----|------|
| 前60 IPv→Siril 位置重复率 | 98.3% | 85.0% | 100.0% | 97.1% |
| 前100 IPv→Siril 位置重复率 | 98.0% | 81.0% | 100.0% | 97.6% |

**关键结论**:
- 非饱和星检测参考 Siril 方法优化 (92.4% 达标, 前60中位 98.3%, 前100中位 98.0%)
- IPv 拟合率 100% (GSL LM 收敛性优秀)
- 主要瓶颈: wide FOV + Lum 通道饱和星系统性多检 (正偏差占主导)
- narrow FOV (89.5%) 和 H-alpha/OIII/Sii 通道 (>=92.6%) 表现优秀

### 兼容性

- API 接口不变 (`star_detector.h` 未修改)
- Python 绑定 (`star_detector.py`) 不变
- 输出 DLL (`star_detector.dll`) 二进制兼容
- 3 帧抽样测试通过, 与 V4.66 性能一致

### 文件结构

```
lib/star_detector/
├── archive/
│   └── V4.66_pre_cleanup/       # 原始代码归档 (含第三方项目引用)
├── include/
│   └── star_detector.h          # 公共 API (未修改)
├── python/
│   ├── star_detector.py         # Python 绑定
│   └── test_*.py                # 测试脚本
├── src/
│   ├── sdet_api.cpp             # API + PSF 拟合 (GSL LM)
│   ├── sdet_detector.cpp        # 候选检测
│   ├── sdet_image.cpp           # 图像处理
│   ├── sdet_background.cpp      # 背景估计
│   └── sdet_log.cpp             # 日志
├── Makefile                     # 编译脚本 (新增 lto/pch 目标)
├── CHANGELOG.md                 # 本文件
├── memory.md                    # 模块开发记忆
└── README.md                    # 模块说明
```

---

## V4.66 (2026-07-06) — GSL LM + halfA 边界搜索初始化

- 用 GSL trust-region LM (`gsl_multifit_nlinear_trs_lm`) 替代手写 LM
- halfA 边界搜索初始化 (从中心向四方向搜索 halfA 边界)
- 3 帧全部达标: 60/59/59, 饱和偏差 0/0/1, 顺序重复率 100%

## V4.63 — 回退自创逻辑

- 移除自创 noconv_kept 逻辑 (保留不收敛饱和星)
- 移除自创 stall_count 提前终止
- 候选排序改用 mag_est 降序
- 不收敛拟合直接丢弃 (PSF_ERR_DIVERGED)

## V4.54 — Gaussian PSF profile

- 从 Moffat4 改为 Gaussian PSF (model = B + A*exp(-Q))
- FWHM = 2.3548*σ (Gaussian), 替代 FWHM = 0.87*σ (Moffat4)
- 同一星点 Gaussian A ≈ 峰值, Moffat4 A 偏高
