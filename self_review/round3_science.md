# Round 3 — Science / Oracle / Adversarial Review（V16）

## Oracle（全部实际运行，无 NOT_RUN）

| 对照 | 来源 | 结果 |
| --- | --- | --- |
| robust_mad_clip | Astropy sigma_clip(mad_std) | agree=200/200 PASS |
| ESD | NIST/Rosner 54 + masking | PASS |
| LinearFit | **未修改 Siril 1.4.3 源码 harness** | 7 case exact + permutation PASS |
| RCR | 官方 rcr 2.4.7 | 10 case exact PASS |
| Winsorized | SciPy 原语数值断言 | PASS |
| WBPP Auto | 本机 2.9.1 bestRejectionMethod | 6 档 PASS |
| MinMax | PixInsight 论坛示例 (3,5)→42 | V16MinMaxFixedCountExact PASS |
| Averaged Sigma | 公式定义（IRAF exact=NOT_CLAIMED；oracle_matrix 如实标注，不再 NOT_RUN） | 矩阵行为 PASS |

## Adversarial（V16 新增/复跑）

```text
negative median percentile（非对称 fraction）→ 方向正确 PASS
percentile+none / rcr+median_center → INVALID_CONFIGURATION PASS
n=200 sigma 栈（ScratchVec heap path）→ oracle/matrix PASS（修复后）
真实 16 帧 trail（1907 像素）→ recall=1.0000 PASS
clean vs truth preservation（背景 std ratio 0.9991）→ PASS
CPU/ACR 等价、permutation、serialization、identity → 65/65 gate PASS
```

## 独立来源政策

- 无同公式 Python mirror 冒充 oracle（winsorized_mirror_smoke 标注
  NOT_AN_ORACLE）；
- 卫星门 per-pixel 判定全部来自生产 kernel；
- 真实 E2E 数据为真实 raw→Phase1→Phase2（NGC1727 H-alpha 16 exposure）。

```text
ROUND3=PASS
```
