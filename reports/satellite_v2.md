# Satellite Gate V2（真实 16-exposure，V17 True Final Freeze）

## 队列（真实 Phase1→Phase2）

```text
dataset   : NGC1727_T2_flying_dutchman（Chilescope T2, 4096²）
filter    : H-alpha（Baader 7nm 映射），EXPTIME=1200s
frames    : 16 个独立 exposure（全部 platesolve 成功，0 失败）
Phase1    : orchestrator stage1（xisf 主文件经 Siril 1.4.4 转 FITS；
            calibration standard + WCS + PSF + photometric + SNR +
            drizzle nside=65536 → per-exposure HiPS order 7）
Phase2    : astrocs-stage2，profile=wbpp_2_9_1（group nominal=16 →
            单次解析 linear_fit），normalization=astrocs_median_center_v1
trail     : 第 8 帧受控注入大圆卫星线（amp=0.05，1907 像素，3 tile）
```

## 四组输出（V17 重跑，canonical config）

```text
mosaic_truth     : 16 帧无 trail + method=none        （真实参考）
mosaic_clean     : 16 帧无 trail + auto rejection
mosaic_trail     : trail 帧 + auto rejection
mosaic_trail_none: trail 帧 + method=none             （注入强度确认）
```

## 指标（evidence/real_rejection_metrics.json）

| 指标 | 值 | 判定 |
| --- | --- | --- |
| trail rejection recall（真实 kernel） | **1.0000**（1907/1907） | PASS |
| injection@mask（trail_none vs truth） | median +0.0033（=0.05/16），p95 0.0063 | 注入确认 |
| clean vs truth：背景 bias median / p95 | 0.000 / 6.04e-05 | 无净损伤 |
| clean vs truth：背景 std ratio | 0.9991 | 无噪声退化 |
| clean vs truth：星点通量 rel bias | 0.0000（69 星像素） | 无星损 |
| trail vs truth：背景 bias median / p95 | 0.000 / 6.04e-05 | trail 完全抑制 |
| observed pixel any-rejection rate | 6.58%（1200 采样） | 如实报告（真实数据） |
| observed sample rejection rate | **0.54%**（100/18448） | 如实报告（真实 1200s H-alpha 栈） |
| underdetermined | 10766 px（depth≤2 真实覆盖边缘） | 明确诊断 |

> V16 曾报 9.45% clean sample false reject。根因已查明：V16 的
> `rejection_cli --plan` 未读取 config 的 normalization 字段（零初始化=
> NONE），kernel 级测量与 stage2 生产的 median_center 语义不一致。
> V17 修复 CLI 后，真实 16 帧 observed sample rejection rate = 0.54%，
> 与 stage2 diagnostics 语义一致。真实数据无 ground truth，一律只叫
> observed（不叫 false reject）。

## 受控 clean truth（true FPR 唯一测量）

零 outlier 合成 20 帧（known sky + 8 PSF 星 + faint extended structure +
独立 Gaussian）：true sample FPR = 1.88%（30,000 samples），且与 frozen
Siril 1.4.3 LinearFit harness 同源 case **100% 一致**（8,000 decisions
逐样本相同；Siril 自身同样拒 1.8375%）——该 FPR 是 frozen reference 行为，
不是 AstroCS 过拒。星点通量 rel bias −0.07%、PSF FWHM +0.01%、
faint structure −0.29%、背景噪声效率 1.045；注入 recall：
thin satellite 1.0 / compact cosmic 1.0 / hot streak 1.0。

## 与 V15/V16 的差异（V17 修复）

- support 过滤真正读取 `support/`（V15 用 signal 代替的 bug 已修）；
- truth baseline 用无 trail 干净副本（V15 逻辑漏洞已修）；
- kernel 级测量 honor normalization（V16 CLI 漏读 → 9.45% 假象；
  V17 修复后 0.54%）；
- 真实 16 帧只报 observed rate；true FPR 由受控 clean truth 提供；
- profile/normalization 全部 canonical 命名（wbpp_2_9_1 /
  astrocs_median_center_v1）。

## 结论

```text
trail 显著抑制（recall=1.0）
clean preservation PASS（背景/星点 bias≈0，std ratio≈1）
observed sample rejection rate = 0.54%（真实数据，如实报告）
true FPR（受控零离群）= 1.88%，与 frozen Siril reference 逐样本一致
resolved method = linear_fit（WBPP 2.9.1 n>15 政策，group-level 一次解析）
SATELLITE_REJECTION_GATE = PASS（真实 16 exposure + 受控 clean truth）
```

## 诚实性

- 16 帧为真实同一 panel/filter 独立 exposure（非复制同一图）；
- per-pixel 判定全部来自生产 rejection kernel（rejection_cli →
  p2_reject_stack_ex，plan 与 stage2 同语义），无 Python 镜像；
- 15+ 真实队列达成（16 帧全部 Phase1 成功）；
- 合成 20 帧门保留作为受控对抗证据，非唯一证据。
