# 鲁棒流量校准 - 双程序架构设计

> 文档版本: 1.0
> 日期: 2026-07-10

---

## 1. 设计动机

### 1.1 单体架构的问题

初始 MVP 设计将光谱积分与梯度拟合耦合在单个流水线中，且用黑体近似替代真实光谱。存在以下问题：

- 光谱积分逻辑与图像处理逻辑耦合，无法独立测试和复用
- 修改滤光片/QE 配置需要重新运行整个流水线
- 黑体近似精度不足，无法利用 Gaia DR3SP 光谱数据

### 1.2 双程序解耦

重构为两个独立程序：

1. **光谱积分器** (spectrum_integrator)：输入天区坐标+滤光片/QE，输出 F_syn JSON
2. **梯度估算器** (gradient_estimator)：输入图像+F_syn JSON，输出校正图像

两程序通过 JSON 文件通信，互不依赖，可独立运行和测试。

---

## 2. 双程序架构图

```
                         data/response_curves/
                         ┌──────────────────────┐
                         │ filters.json (43条)   │
                         │ qe_curves.json (12条) │
                         └──────┬───────────────┘
                                │
                    ┌───────────┴───────────┐
                    │                       │
                    ▼                       ▼
┌───────────────────────────┐     ┌───────────────────────────┐
│    光谱积分器              │     │    梯度估算器              │
│    Spectrum Integrator    │     │    Gradient Estimator     │
├───────────────────────────┤     ├───────────────────────────┤
│                           │     │                           │
│  GaiaSpectrumClient       │     │  FSynLoader               │
│  (gaia_client.dll)        │     │  (加载 F_syn JSON)         │
│                           │     │                           │
│  CurveLoader              │     │  StarMatcher              │
│  (加载 filter/QE 曲线)     │     │  (KDTree 匹配 + MAD 清洗)  │
│                           │     │                           │
│  SyntheticPhotometry      │     │  GradientFitter           │
│  (Akima + Simpson 积分)    │     │  (IRLS + Tukey + LOOCV)   │
│                           │     │                           │
│  SpectrumIntegrator       │     │  ImageCorrector           │
│  (批量积分)                │     │  (I_cal=(I-S)/M)          │
│                           │     │                           │
├───────────────────────────┤     ├───────────────────────────┤
│  输入:                    │     │  输入:                    │
│  - ra, dec, radius        │     │  - FITS/XISF 图像          │
│  - filter, qe             │     │  - F_syn JSON              │
│  - Gaia DR3SP 数据库      │     │  - WCS (FITS头或JSON)       │
│                           │     │                           │
│  输出:                    │     │  输出:                    │
│  - F_syn JSON ────────────┼────▶│  - 校正后 FITS 图像        │
│                           │     │  - 质量报告 JSON           │
│                           │     │  - 残差 CSV               │
└───────────────────────────┘     └───────────────────────────┘
```

---

## 3. 光谱积分器

### 3.1 输入

| 参数 | 类型 | 说明 |
|------|------|------|
| ra | float | 视场中心赤经 (度) |
| dec | float | 视场中心赤纬 (度) |
| radius | float | 锥形搜索半径 (度) |
| mag_low | float | 星等下限 |
| mag_high | float | 星等上限 |
| filter | str | 滤光片名称 |
| qe | str (可选) | QE 曲线名称 |

### 3.2 处理流程

1. `CurveLoader` 加载滤光片透过率 T(λ) 和 QE 曲线 Q(λ)
2. `GaiaSpectrumClient` 锥形搜索 Gaia DR3SP，获取星表 + 光谱数据 (uint8[343])
3. `SpectrumIntegrator.integrate_batch()` 对每颗星计算 `F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ`
4. `SpectrumIntegrator.save_results()` 输出 JSON

### 3.3 输出 JSON 格式

```json
{
  "filter_name": "Baader R",
  "qe_name": "GSENSE2020BSI",
  "wl_step": 0.1,
  "spectrum_source": "gaia_bp_rp",
  "n_stars": 1523,
  "ra_center": 266.4168,
  "dec_center": -28.9833,
  "radius_deg": 0.5,
  "stars": [
    {
      "source_id": 1234567890,
      "ra": 10.5,
      "dec": 20.3,
      "mag_g": 12.5,
      "f_syn": 12345.678
    }
  ]
}
```

---

## 4. 梯度估算器

### 4.1 输入

| 参数 | 类型 | 说明 |
|------|------|------|
| image | FITS/XISF | 输入天文图像 |
| fsyn | JSON | 光谱积分器输出的 F_syn JSON |
| wcs | WCS | 来自 FITS 头或 plate_solve JSON |
| match_radius | float (可选) | 星-图匹配半径 (像素，默认 3.0) |
| outlier_sigma | float (可选) | MAD 离群点阈值 (默认 3.0) |
| max_order | int (可选) | 多项式最大阶数 (默认 5) |

### 4.2 处理流程

1. `FSynLoader` 加载 F_syn JSON
2. 星检测 + PSF 拟合（内置或外部输入）：
   - `StarDetector.detect_ex()` 检测星点
   - `DynamicPSF.fit_batch()` 拟合 PSF，获取 flux (F_instr) 和 B (背景)
3. `StarMatcher.match_and_clean()`：
   - WCS 投影 Gaia 星到像素坐标
   - KDTree 最近邻匹配 PSF 星
   - 排除饱和星和拟合失败星
   - MAD sigma 裁剪离群点
4. `GradientFitter.fit_multiplicative()`：拟合 `r = log10(F_instr/F_syn)` 曲面
5. `GradientFitter.fit_additive()`：拟合 `B` 背景曲面
6. `ImageCorrector.correct_and_normalize()`：`I_cal = (I-S)/M` + 归一化

### 4.3 输出

| 输出 | 格式 | 说明 |
|------|------|------|
| 校正图像 | FITS (float32) | `I_final = I_cal × scale` |
| 质量报告 | JSON | 匹配星数、阶数、LOOCV误差、残差统计、scale_factor |
| 残差 | CSV | mult_residuals.csv, add_residuals.csv |

### 4.4 质量报告 JSON 格式

```json
{
  "n_matched": 150,
  "n_excluded": 3,
  "n_used": 147,
  "mult_order": 3,
  "mult_loocv_error": 0.00123,
  "mult_residual_median": 0.0001,
  "mult_residual_std": 0.008,
  "add_order": 2,
  "add_loocv_error": 0.456,
  "add_residual_median": 0.02,
  "add_residual_std": 3.2,
  "scale_factor": 1.000000
}
```

---

## 5. 共享组件

### 5.1 data_types.py

两程序共享的数据结构：

- `GaiaSpectrumStarPy`: Gaia 光谱星数据 (ra, dec, mag_g, source_id, spectrum[343])
- `FSynResult`: F_syn 结果数据

### 5.2 pc_logger.py

统一日志系统，同时输出到文件和控制台：
- 日志目录：`Python/logs/`
- 格式：`[时间] [级别] 消息`
- UTF-8 编码

---

## 6. JSON 通信接口

F_syn JSON 是两程序的唯一通信接口。Schema：

| 字段 | 类型 | 说明 |
|------|------|------|
| filter_name | str | 滤光片名称 |
| qe_name | str (可选) | QE 曲线名称 |
| wl_step | float | 积分步长 (nm) |
| spectrum_source | str | 光谱来源 ("gaia_bp_rp") |
| n_stars | int | 星数 |
| ra_center | float | 视场中心赤经 |
| dec_center | float | 视场中心赤纬 |
| radius_deg | float | 搜索半径 |
| stars | array | 星列表 |
| stars[].source_id | int | Gaia source_id |
| stars[].ra | float | 赤经 |
| stars[].dec | float | 赤纬 |
| stars[].mag_g | float | G 星等 |
| stars[].f_syn | float | 合成流量 |

---

## 7. 外部依赖关系

```
┌─────────────────────────────────────────────────────┐
│              Robust Flux Calibration                │
│                 (Python 原型)                       │
├─────────────────────────────────────────────────────┤
│                                                     │
│  spectrum_integrator          gradient_estimator    │
│  ┌─────────────────┐          ┌─────────────────┐   │
│  │                 │          │                 │   │
│  │  gaia_client ───┼──┐       │  astro_image_io ┼──┤
│  │  .dll           │  │       │  .dll           │  │
│  │                 │  │       │                 │  │
│  │                 │  │       │  star_detector ─┼──┤
│  │                 │  │       │  .dll           │  │
│  │                 │  │       │                 │  │
│  │                 │  │       │  dynamic_psf ───┼──┤
│  │                 │  │       │  .dll           │  │
│  └─────────────────┘  │       └─────────────────┘  │
│                       │                             │
└───────────────────────┼─────────────────────────────┘
                        │
                        ▼
              ┌──────────────────┐
              │  Gaia DR3SP      │
              │  数据库           │
              │  (2.2亿星)       │
              └──────────────────┘
```

| DLL | 仓库 | 使用方 |
|-----|------|--------|
| gaia_client.dll | [Gaia-DR3-DR3SP-Client-C](https://github.com/fujiaze/Gaia-DR3-DR3SP-Client-C) | spectrum_integrator |
| astro_image_io.dll | [Astro-Image-IO-C](https://github.com/fujiaze/Astro-Image-IO-C) | gradient_estimator |
| star_detector.dll | [Star-Detector-Cpp](https://github.com/fujiaze/Star-Detector-Cpp) | gradient_estimator |
| dynamic_psf.dll | [Dynamic-PSF-Cpp](https://github.com/fujiaze/Dynamic-PSF-Cpp) | gradient_estimator |
