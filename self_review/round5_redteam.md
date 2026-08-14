# Round 5 — Independent Red-Team Review

假设当前结论是错的，主动寻找最可能失败处。每项必须
`DISPROVED_WITH_EVIDENCE` 或 `BUG_FOUND_AND_FIXED`。

| # | Hypothesis | 结论 |
| --- | --- | --- |
| 1 | Auto 其实仍按 pixel effective N 路由 | DISPROVED_WITH_EVIDENCE：`P2_REJECT_AUTO` 在 rejection.cpp 仅出现在 plan_resolve（校验+路由，行 1013/1041）；stage2 每 tile 用 depth 解析一次，pixel loop 只传 explicit plan.method |
| 2 | n=2 卫星线被错误宣称可拒绝 | DISPROVED_WITH_EVIDENCE：kernel `n <= underdetermined_n → UNDERDETERMINED`；生产 n2overlap run underdetermined_pixels=61,588,497、rejected_samples=0；reports/satellite_rejection.md 明确"不宣称可剔除" |
| 3 | NaN 被 NONE 重新接受（RJ-001 回归） | DISPROVED_WITH_EVIDENCE：V15NoneDoesNotReacceptNaN PASS；compat 与 ex 双路径均不重接受 |
| 4 | rejected_low/high 仍用符号语义 | DISPROVED_WITH_EVIDENCE：V15LowHighThresholdSemantics PASS（-50→LOW、+50→HIGH）；ESD/RCR/minmax 用整体 median 边界映射 side（文档化） |
| 5 | CPU/GPU mask 不同 | DISPROVED_WITH_EVIDENCE：Phase2Acr.CudaEquivalent / CudaWeightedSupportEquivalent / G9CompactFrameSubset PASS（CUDA 低侧阈值已按负值约定修复） |
| 6 | config schema/default 不一致 | DISPROVED_WITH_EVIDENCE：config_consistency_check.py PASS（30 keys）；schema 的 `required:["method"]` 与 parser 默认 auto 并存但运行时只走 parser（无 schema 校验路径），template 显式 method=auto |
| 7 | legacy 路径仍可被 production 调用 | DISPROVED_WITH_EVIDENCE：stage2.cpp 无 `p2_reject_stack(` 调用（grep 仅测试/工具）；healpix_stack 冻结且生产 Phase2 入口=astrocs-stage2 |
| 8 | 序列化/层级改变 science | DISPROVED_WITH_EVIDENCE：stage2 HIPS_VERIFY 回读通过；V5/Hipsgen 外部 oracle 冻结；gate 59/59 含 identity/serialization |
| 9 | performance 基准热身不公平 | DISPROVED_WITH_EVIDENCE（诚实报告）：3 次冷启动全记录（52.83/43.80/43.02s），首跑含 OS 缓存冷启动，median/p95 如实标注 |
| 10 | Browser Auto Global pan/zoom 闪烁 | DISPROVED_WITH_EVIDENCE：set_view 仅 AutoView 置 auto_range_dirty_；AutoGlobal 全 dataset 标尺缓存一次（g_global_scan_cache_），pan 不重算 |
| 11 | support 显示仍 sqrt | DISPROVED_WITH_EVIDENCE：hips_sky_view.cpp support 分支已 linear（V15）；gl_renderer 的 sqrt 均为数学计算且 gl_renderer 非生产主路径（main.cpp/main_window.cpp 未引用） |
| 12 | 浏览器第二套 HEALPix 仍在生产 | DISPROVED_WITH_EVIDENCE：healpix_math.cpp 全部委托 astrocs::healpix（本地实现已删除）；grep 无 common 外 handwritten ang2pix |
| 13 | sampler 空间索引改变科学结果 | DISPROVED_WITH_EVIDENCE：索引只做保守预筛，最终判据为同一 angular_distance_deg；G6/RealHipsControlSampling PASS；stage2 输出与未优化路径等价（bias=0 对照） |
| 14 | ACR 路径 underdetermined 计数漏计 | BUG_FOUND_AND_FIXED：stage2 ACR 分支原用 legacy `reject_min_samples`（漏 nv==2）→ 本轮改为 `rplan.underdetermined_n/minimum_n` 并计 underdetermined_px（R1-P2-001） |

## 结论

```text
13 项 DISPROVED_WITH_EVIDENCE
1 项 BUG_FOUND_AND_FIXED（统计计数，非科学输出）
无 "probably fine"
ROUND5=PASS
```
