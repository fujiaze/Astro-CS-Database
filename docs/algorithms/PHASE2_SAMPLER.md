# Phase2 Sampler

关联：SCI-UPM-*；模块：lib/phase2/src/sampler.cpp。

## 输入

coverage union MOC + 帧 SNR catalogue。

## 输出

control cell 观测（value/uncertainty/snr/control_variance/
control_ivar/support/quality；ivar 仅诊断）。

## Preconditions

target_order 已定；cell 网格 8×8。

## Postconditions

- patch estimator：support>0 + finite 过滤，median 位置 + MAD 尺度；
- SNR 来自 catalogue 邻近星点（不重新检测）；snr_available=0 时不伪装 1.0。

## V19R3 control estimator 方差（ALG-UPM-CONTROL-IVAR-001 /
DATA-UPM-CONTROL-UNC-001）

```text
control_variance = control_k_corr × (π/2) × sigma_bg² / N_retained
control_ivar     = 1 / control_variance
uncertainty      = sqrt(control_variance)   # SE(patch median)
```

- N_retained = clipping 后保留样本（不是 n_total）；
- sigma_bg = MAD 尺度（clipping 收敛集）；
- control_k_corr 默认 1.4（UPMW-005 MC 校准：pixfrac=0.8 下实证
  1.3883，N_eff≈181<251）；显式配置覆盖，<=0 回退冻结默认；
  per-frame 覆盖 frames[frame_id].kcorr>0 ? per-frame : cfg.control_k_corr（sampler.cpp:672），缺省回退 1.4；
- obs.ivar 保留为单 leaf Phase1 ivar 诊断（V19R3 弃用，不进科学权重）。

## Invariants

保留负值；cell 无 obs 时标记几何节点（不进数据分量）。

## 复杂度

O(cells + catalogue log n)（dec 排序索引 + 帧 median 预计算）。

## 并行模型

默认串行（hotfix：`P2_ENABLE_OPENMP=OFF`，`0xC0000005` 回退）；tile 级缓存保留（每 cell 覆盖帧的 `signal/support` 仅读一次，消除 `64×` 重读）与 `progress` 日志保留；`P2_ENABLE_OPENMP=ON` 可显式开启 `OpenMP` 并行（`cfitsio` 读 `critical(aio_read)` 串行化）。

## 数值风险

MAD=0；patch 样本不足 → min_samples 拒绝。

## ID

ALG-P2-SAMPLE-*；TEST-P2-SAMPLE-*；UPMW-004/005/007（median SE /
Drizzle 相关 MC / patch estimator vs truth）。
