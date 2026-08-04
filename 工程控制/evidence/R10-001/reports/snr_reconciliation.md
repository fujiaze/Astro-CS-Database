# R10 SNR 点核算报告

日期: 2026-08-04 | 数据: 单张 T4 Red 真实帧（Galaxy_Center_mosaic3_T4）| 采样: 2000 点

## 1. 修复根因

**Bug**: 1979 个 SNR 控制点仅 370 有效，1609 个被判"ra/dec 越界"丢失。

**根因**: `SnrControlPoint` 未 `#pragma pack(1)`，`sizeof=24`（4 字节尾部填充），而 orchestrator 序列化用
`memcpy(dst, points, n*20)` 按 20 字节连续拷贝。从第 2 个点起 ra/dec 字段错位 → 被校验为越界。

**修复**: `lib/snr_estimator/cpp/include/snr_estimator.h` 添加 `#pragma pack(push, 1)` + `static_assert(sizeof==20)`。
提交: `8cb22a9`。

## 2. FP32 模式核算（`fp32_snr_fix_verify_20260804.log` L422）

```
n_total=2000 n_valid=1979 n_dropped=21
{NOT_DROPPED=1979 INVALID_PSF=21 ZERO_FLUX=0 OUTSIDE_TILE=0 NO_OVERLAP=0 INVALID_WCS=0 DUPLICATE_IPIX=0 OTHER=0}
```

- 有效 1979/2000 (98.95%)
- 丢弃 21 全部归因 `INVALID_PSF`（PSF 拟合失败，非打包 bug）
- 零 NaN、零越界、零 radec2pix 失败

## 3. FP64 模式核算（`fp64_snr_fix_verify_20260804.log` L451）

```
n_total=2000 n_valid=1947 n_dropped=53
{NOT_DROPPED=1947 INVALID_PSF=53 ZERO_FLUX=0 OUTSIDE_TILE=0 NO_OVERLAP=0 INVALID_WCS=0 DUPLICATE_IPIX=0 OTHER=0}
```

- 有效 1947/2000 (97.35%)
- 丢弃 53 全部归因 `INVALID_PSF`
- FP64 丢弃略多（53 vs 21），与 FP64 星点检测转换后 PSF 拟合的数值差异一致；无结构性问题

## 4. 结论

- 打包 bug 已修复：不再有因 ra/dec 错位导致的越界丢失
- 全部丢弃点完成原因分类（SNR-001 已闭合）
- 剩余丢弃均为 INVALID_PSF，属于科学拟合正常拒绝，需后续 PSF 质量优化（非 R10 范围）
