# Round 3 — Science / Oracle / Adversarial Review

## Oracle 独立性（V15 修复后）

| 对照 | 独立来源 | 结果 |
| --- | --- | --- |
| robust_mad_clip | Astropy `sigma_clip(cenfunc=median, stdfunc=mad_std)` | agree=200/200 PASS |
| Generalized ESD | NIST/Rosner 54 点（scipy t 独立实现） | 3 outliers + masking case PASS |
| Linear Fit | **未修改 Siril 1.4.3 官方源码 harness**（rejection_float.c + siril_fit_linear.c） | 7 case 全部逐元素 agree + permutation invariant PASS |
| RCR | 官方 rcr 2.4.7（nickk124/robust-outlier-rejection a8a29a6） | 10 case rejected-set exact PASS |
| Winsorized | SciPy winsorize primitive 数值断言（仅原语；Siril 为权威参考） | PASS |
| WBPP Auto | 本机 WBPP 2.9.1 源码 bestRejectionMethod | 6 档 nominal 路由一致 PASS |

Python 镜像（winsorized_mirror_smoke）已明确标注 `NOT_AN_ORACLE`（不冒充
独立来源）；恒真断言已修复（V15）。

## Adversarial / Metamorphic（证据）

| 类别 | 覆盖 | 证据 |
| --- | --- | --- |
| empty / n=0 | status=MIN_SAMPLES | R2MinSamples / ex kernel |
| one/two samples | n=1-2 → UNDERDETERMINED（真实生产 61.6M px） | V15SatelliteN2Underdetermined + n2overlap run |
| zero variance | 无拒绝（全接受） | oracle edge_matrix |
| NaN / ±Inf | INVALID_INPUT 防御（全 accepted，不伪称拒绝） | oracle edge_matrix |
| valid=false / support=0 / quality | eligibility 过滤 + 计数 | V15FilterAllPolicies |
| one bright / one dark / two-sided | rejected_low/high 阈值语义 | V15LowHighThresholdSemantics |
| satellite trail（20 帧） | recall=1.0000（1418/1418 生产 kernel） | satellite_metrics.json |
| cosmic-ray 式单帧离群 | 20 帧栈 linear_fit 拒绝 | V15SatelliteTrail20Frames |
| dense field / star | 星点 flux bias=0 | satellite_metrics.json |
| tile seam / order boundary | geometry truth（browser）+ UPM seam gates | test_geometry_truth PASS |
| permutation（frame order） | 全方法 mask 不变 | G6PermutationInvariance / V15ExPermutationInvarianceTyped / linear_fit+rcr oracle permutation |
| serialization round-trip | UPM save/open + hash | G2PersistenceAndHashSensitivity PASS |
| 同科学不同调度（CPU/ACR） | legacy launcher / CUDA 等价 | Phase2Acr.* PASS |
| renaming/path move 不改变 identity | frame_id 复制/改名不变 | G3StableFrameIdentity PASS |

## 结论

```text
所有科学判定来自生产实现或独立外部来源；无同公式 Python mirror 冒充 oracle；
adversarial matrix 无未解科学不一致。
ROUND3=PASS
```
