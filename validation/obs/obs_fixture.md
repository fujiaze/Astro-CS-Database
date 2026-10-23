# 最小可跑夹具（obs 作用域）

本文件给出 `fit_psf` / `measure_science_sources` / `bind_wcs` / `bind_observation_model`
所需的**最小字段集**，供独立验证照搬重建（只 include `<acsd/contracts.hpp>`，自实现
Storage/Resources/Cancellation）。机器可读的同源版本在 `obs_fixture.hpp`。

## 1. 最小可跑的 FootprintSet

```cpp
auto footprints = std::make_shared<acsd::FootprintSet>();
footprints->index = index;                             // acsd::Index::Kind::native，size = width*height
footprints->width = width;                             // 与 integrated_values 的边长一致
footprints->height = height;
footprints->detector_from_sample = {1.0, 0.0, 0.0, 1.0};   // Mat2：样本局部 -> 探测器
footprints->offset = {0.0, 0.0};
footprints->local_cell.lower = {-0.5, -0.5};           // ★ 像元格以样本为中心
footprints->local_cell.upper = {0.5, 0.5};
footprints->detector_frame = "frame-0";
// 下面这些字段不需要：
//   wcs                  帧模型由入口参数单独传入
//   pixel_response       PSF 形状拟合不读响应；首版测光用单波段分段常数像元响应
//   astrometry           天测不确定度不参与 PSF 形状拟合
//   inline_footprints    规则探测器的足迹就是每个样本的 local_cell
//   sample_response_gain 每个样本的增益相同
```

**两个容易错的地方**

1. `local_cell` 不赋值时是退化盒 `{0,0}×{0,0}`（零面积）→ 每个样本的像元积分为 0 →
   模板列全零 → 位置/形状导数（模板的导数）也全零 → 设计矩阵只剩背景列，**秩 1**。
   实测症状：奇异值 `3.464102, 0, 0, …`，而 √12 = 3.4641016…，即「12 个拟合行上的
   全 1 背景列」——这正是该根因的定量指纹。
2. 像元格必须**以样本为中心**（`[-0.5, 0.5]`），因为源位置用的是 `PixelCenter0`
   约定（样本中心）。写成 `[0,1]²` 不会让模板变 0，但会让足迹相对位置整体平移
   半个像元。

### 样本网格 → 探测器的映射

`detector_from_sample` 是 2×2 矩阵，`offset` 是平移。样本索引 r 的中心在
`(r % width, r / width)`；像元格的四个角点经同一个仿射映射给出该样本在探测器平面的
边界。规则探测器取单位矩阵与零平移。

### PixelResponse

**不需要**。`fit_psf` 与 `measure_science_sources` 不读 `pixel_response`：首版 PSF 的
像元响应是**单波段分段常数**的光子响应（PixelResponse ≠ spectral Response），而形状
拟合只用到足迹几何与 `sample_response_gain`（可空）。

## 2. 源行（source rows）如何定义

本模块里没有独立命名的 “source_rows” 字段：**拟合行 = 窗口内的原生样本**，由几何选出：

* 对每个源，取其 `PixelCenter0` 位置；
* 窗口半径 = `max(request.window_radius_pixel, initial_width_pixel · sqrt(2 ln(1/tail)))`
  （后半项来自解析尾界：Gaussian 尾部质量 exp(−R²/2σ²)）；
* 选出足迹矩形与该位置距离 ≤ 窗口半径的原生样本，作为该源的拟合行。

因此：

* **行数 > 列数**是硬要求。列数 = `psf_parameter_count(spatial_degree, sources)`
  （见 `obs_fixture.hpp`）：形状项 `3·(d+1)(d+2)/2` + 每源 3（x、y、流量）+ 1（背景）。
  常数空间（d=0）+ 8 颗星 = 3 + 24 + 1 = **28 列**，所以拟合行必须 > 28。
* **最小星数**：星数没有下限，但每颗星都要贡献独立的方向。空间二次（d=2）时
  形状项就有 9 列，1 颗星是 9+3+1 = 13 列，**秩亏** → `not_identifiable`（这是要求的
  行为：不暗降阶）。要拟合 d=2 至少需要足够的、位置分散的星。
* 星位彼此**不要**共线（否则 σx/σy 退化），并相距 ≥ 3 像元。

## 3. 目录星（CatalogueStar）的最小构造

```cpp
acsd::CatalogueStar star;
star.source_id = 1000;                       // 身份：测光结果沿用 catalogue 的 source_id
star.ra_rad = ...; star.dec_rad = ...;       // 位置
star.epoch_year = 2016.0;                    // 与 observation.context.epoch_julian_year 同历元
star.brightness_mag = 10.0;                  // 测光链路用；PSF 形状拟合不用
star.xp_basis = ...; star.xp_coefficients = ...;  // 只有光度解算（phot 模块）用
```

* **位置的最省事做法**：用 `acsd::forward(context, wcs, PixelCenter0{x, y}, budget)` 得到
  SkyICRS 存进星表；测光时 `acsd::inverse(context, wcs, sky, budget)` 应当精确回到
  `(x, y)`。**不要**自己用度数偏移手算 RA/Dec 再反解：度偏移先转弧度再加到度值上，
  会让星位偏 1.965 像元（1 − 1/57.2958 的因子），通量随之变成 0。
* **天测噪声不在星表上**：它在 `AstrometricSolution`（`parameter_noise` / `influence`）
  上；`bind_wcs(context, observation, wcs, solution)` 会在解算没有误差身份时把
  `astrometric_parameter_error_is_unknown` 记为 omitted（未知不填零）。

## 4. 配对的 NativeObservation

```cpp
observation->integrated_values = <f64 数组>;   // 形状 {SIDE, SIDE} 可以，内部按扁平向量读
observation->native_index = index;
observation->footprints = footprints;
observation->frame = frame_id;
observation->unit = unit_luminosity();          // 与数组的单位一致
observation->context.epoch_julian_year = 2016.0;
// 噪声不是 fit_psf 的前置条件；测光的方差需要噪声模型（未知项会让方差保持 null）
```

## 5. 实测基线（validation/obs/，160 项检查 0 失败）

| 项 | 结果 |
|---|---|
| Gaussian 像元积分 vs 解析 erf | 最大差 0；整帧收集质量 0.999998791422（<1，未归一） |
| Moffat / Airy vs 独立求积 | 5.4e-11 / 5.43e-10 |
| 合成真值测光 | flux = 100.0000000000（真值 100） |
| F01 反例：窄 [100,100] / 宽 [99,101] 的通量包络半宽 | 2.78533e-12 / 15.602 |
| PSF 形状拟合 | σ = 1.599726（真值 1.6） |
| 128×128 帧 | fit_psf 0.003 s；measure_science_sources 0.001 s |
