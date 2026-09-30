# Performance Baseline

> 上游：ASTROCS_DESIGN.md §9（CPU 后端与资源）

同一机器、同一数据、同一 config，每 benchmark ≥3 次，记录 median/p95。
基准数据：

```text
Phase1 小真实帧 / 代表性完整帧
Phase2 t4 overlap / GC 3-panel
Browser GC wide / pan / zoom / STF
```

完整数值见 `实验/engineering-evidence/**`（实测类证据的唯一留档区，登记 = `docs/ASTROCS_DESIGN.md` §10（I/O 与原子产品）（:151））。原引 `evidence/performance/*.json`（V14 交付）**在本仓不存在**（死指针，本行原句已订正）⇒ 下文 V14 / V18R2 读数为**历史读数、原始 JSON 未入库**，只作历史参照，不作现行基线证据；补做现行基线须重跑并按 `docs/ASTROCS_DESIGN.md` §10（I/O 与原子产品） 落 `实验/engineering-evidence/`。

V14 首轮结果：

```text
Phase1 panel1    65.0s（3 runs）
Phase2 GC        292.0s -> 234.6s（-20%）
Phase2 t4         87.9s ->  70.8s（-19%）
Browser shot      2.43s；zoom 0.30s/f；pan 0.22s/f
```

规则：

- 优化前后 science 输出 hash/数值等价；
- 无 >5% 无解释总体回退；
- 未安全优化项标注 `NO_SAFE_OPTIMIZATION_FOUND`。

---

## V18R2 资源驱动轮（性能基线）

> **口径诚实边界**：本节数值为**单次读数**，不满足上文「每 benchmark ≥3 次、记 median/p95」的自订口径 ⇒ 只作历史参照，不构成现行基线结论；原始读数工件未登记入库。

```text
Phase1            ~67.35 s/frame
Drizzle           ~64 s/frame（16-frame batch）
资源驱动轮        126.65 s -> 67.35 s
RSS               37.5 GB -> 1.2 GB
PLATESOLVE        15 s -> 0.16 s
```

规则同前：优化前后 science 输出 hash/数值等价；无 >5% 无解释总体回退；未安全优化项标注 `NO_SAFE_OPTIMIZATION_FOUND`。

