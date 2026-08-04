# R10 科学正确性报告

日期: 2026-08-04 | 范围: 合成数据 bit-exact + 单张真实帧逐 Gate

## 1. 科学约束遵守

| 约束 | 状态 | 证据 |
| --- | --- | --- |
| signal=累计通量（不除面积） | ✅ | HISS 写入日志 add_tile 通量语义 |
| support=面积比 [0,255] uint8 | ✅ | add_tile 子块 type=2 布局 |
| Tile 叶像素数=4^d | ✅ | nside=65536, tile_nside=128, n_leaf=262144=4^9 |
| 球面重叠用 Girard 定理 | ✅ | spherical_overlap 实现（PRECISE 模式） |
| pixfrac∈(0,1] | ✅ | pixfrac=1.0 通过 Schema 校验 |
| 自动 NSIDE 上限 2^22 | ✅ | calculate_nside 用 HEALPIX_SCALE_PER_NSIDE_ARCSEC 推导 |
| PlateSolve 向量匹配 gnomonic + Y 轴反转 | ✅ | ipv_solver 实现 |
| RANSAC+Umeyama SVD、尺度 ±10% | ✅ | ipv_ransac/ipv_itertrans 实现 |
| 星检测串行 | ✅ | 无并行改造 |

## 2. R10 关键 Bug 修复的科学影响

### Bug 1: NSIDE 计算常数错误
- 修复: 常数 1186.18 → π/(3·NSIDE²)=211034.6
- 效果: 单帧自动选择 nside=65536（6.3"/px 匹配源像素），不再错误选 512

### Bug 2: HEALPix 边细分不收敛
- 修复: 阈值 `hp_res_rad*1e-12` → `hp_res_rad*1e-6`，边用角距离法
- 效果: nside=65536 drizzle 完成（428s），不再 332B 次操作超时

### Bug 3: 跨 DLL 精度上下文不共享
- 修复: `aio_set_precision_mode(int)`
- 效果: FP64 模式 AIO 真正按 float64 读 data 块

### Bug 4: SnrControlPoint 打包
- 修复: `#pragma pack(1)` + static_assert(sizeof==20)
- 效果: SNR 控制点 ra/dec 不再错位，1979/1947 点有效

### Bug 5: FP64 PlateSolve 星点检测
- 修复: FP64→FP32 显式转换缓冲
- 效果: FP64 模式星点检测正常（median>5）

## 3. 合成数据验证

`synthetic_hiss_precision.log`（229 行）:

- FP32 roundtrip bit-exact
- FP64 roundtrip bit-exact（`read_tile_signal_f64`，16/16 值与原 double 逐位一致）
- 总计 5 组测试 63 项断言，0 失败

## 4. 单帧逐 Gate

T4 Red 一帧（4500x3600, BIN-1, 180s），FP32 与 FP64 各跑全链:

| Gate | FP32 | FP64 |
| --- | --- | --- |
| READ_FITS | dtype=0 FP32 | dtype=1 FP64（data_f64 不降级） |
| CALIBRATE | 图像+Masters FP32 | 图像+Masters FP64 |
| PLATESOLVE | 星点检测 float32 | 显式转 float32 供检测；WCS double |
| PSF | mode=FP32 | dpsf_fit_batch_d (FP64) |
| PHOTOMETRIC | n_matched=1712 | n_matched=1684 (f64 API) |
| SNR | 1979 有效 | 1947 有效（全部归因 INVALID_PSF） |
| DRIZZLE | float32, 累计器 FP32 | float64, 累计器 FP64 |
| HISS_VERIFY | 285/285, FP32 | 285/285, FP64 |

## 5. 边界声明

- 单帧验证 ≠ 全量验证：仅一张 T4 Red 帧，不代表 710 帧全量回归
- 浏览器性能未做 GUI 实测（browser_verify 为 CLI 侧验证）
- 未宣称整个 Phase1 已完成
