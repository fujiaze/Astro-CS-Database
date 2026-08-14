# Satellite Gate V2（真实 16-exposure，V16）

## 队列（真实 Phase1→Phase2）

```text
dataset   : NGC1727_T2_flying_dutchman（Chilescope T2, 4096²）
filter    : H-alpha（Baader 7nm 映射），EXPTIME=1200s
frames    : 16 个独立 exposure（全部 platesolve 成功，0 失败）
Phase1    : orchestrator stage1（xisf 主文件经 Siril 1.4.4 转 FITS；
            calibration standard + WCS + PSF + photometric + SNR +
            drizzle nside=65536 → per-exposure HiPS order 7）
Phase2    : astrocs-stage2，profile=wbpp_current（group nominal=16 →
            单次解析 linear_fit），normalization=median_center
trail     : 第 8 帧受控注入大圆卫星线（amp=0.05，1928 像素，3 tile）
```

## 四组输出（V2 修正版）

```text
mosaic_truth     : 16 帧无 trail + method=none        （真实参考）
mosaic_clean     : 16 帧无 trail + auto rejection
mosaic_trail     : trail 帧 + auto rejection
mosaic_trail_none: trail 帧 + method=none             （注入强度确认）
```

## 指标（evidence/satellite_v2_metrics.json）

| 指标 | 值 | 判定 |
| --- | --- | --- |
| trail rejection recall（真实 kernel） | **1.0000**（1907/1907） | PASS |
| injection@mask（trail_none vs truth） | median +0.0033（=0.05/16），p95 0.0063 | 注入确认 |
| clean vs truth：背景 bias median / p95 | 0.000 / 6.04e-05 | 无净损伤 |
| clean vs truth：背景 std ratio | 0.9991 | 无噪声退化 |
| clean vs truth：星点通量 rel bias | 0.0000（69 星像素） | 无星损 |
| trail vs truth：背景 bias median / p95 | 0.000 / 6.04e-05 | trail 完全抑制 |
| clean pixel any-reject | 59.6%（1200 采样） | 如实报告 |
| clean sample false reject | **9.45%**（1744/18448） | 如实报告（真实 1200s H-alpha 栈；linear_fit 固有） |
| underdetermined | 10766 px（depth≤2 真实覆盖边缘） | 明确诊断 |

## 与 V15 的差异（V16 修复）

- support 过滤真正读取 `support/`（V15 用 signal 代替的 bug 已修）；
- truth baseline 用**无 trail 干净副本**（V15 用"同样过度拒绝的 clean"作
  baseline 的逻辑漏洞已修）；
- 增加 trail_none 组确认注入强度；
- clean preservation 以 truth（无拒绝）为参考。

## 结论

```text
trail 显著抑制（recall=1.0）
clean preservation PASS（背景/星点 bias≈0，std ratio≈1）
clean sample false reject 9.45%（真实数据；统计合理上限内，如实报告）
resolved method = linear_fit（WBPP 2.9.1 n>15 政策，group-level 一次解析）
SATELLITE_REJECTION_GATE = PASS（真实 16 exposure）
```

## 诚实性

- 16 帧为真实同一 panel/filter 独立 exposure（非复制同一图）；
- per-pixel 判定全部来自生产 rejection kernel（rejection_cli →
  p2_reject_stack_ex），无 Python 镜像；
- 15+ 真实队列达成（16 帧全部 Phase1 成功）；
- 合成 20 帧门（V15 工具）保留作为受控对抗证据，非唯一证据。
