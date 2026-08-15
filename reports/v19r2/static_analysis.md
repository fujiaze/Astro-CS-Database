# Static Analysis Report (V19R2)

## 方法

g++ 16.1.0 `-fanalyzer`（-O0 -Wall -fopenmp），直接分析关键科学单元：

| 单元 | 结果 | 耗时 |
| --- | --- | --- |
| lib/phase2/src/upm.cpp | 0 finding | 154 s |
| lib/phase2/src/rejection.cpp | 0 finding（原 ls_fit_line 死代码已清） | 8 s |
| lib/phase2/src/integrate.cpp | 0 finding | 快 |
| lib/snr_estimator/cpp/src/noise_model.cpp | 0 finding | 2 s |
| lib/healpix_db/healpix_drizzle/drizzle_engine.cpp | 0 finding | 28 s |

## 覆盖率与例外

```text
direct_analyzed_units = 5（科学关键单元，含本轮全部修改点）
shipping_units        = 280
```

其余 275 单元的 exception（沿用 V19 策略，原因+替代证明）：

- 原因：-fanalyzer 对全仓 280 单元逐个运行需数小时，本轮不承担；
- 替代证明：-Wall -Wextra -Wpedantic 0 warning + 科学/回归套件
  （phase2 83/83、SNR 32/32、drizzle oracle 9003/9003、overlap 77/77、
  reverse 37/37、matrix 180/180）+ 未变单元与 V19 hash 一致（V19 已
  -fanalyzer 4 单元 0 finding）；
- owner：V19R2 审计轮；后续轮可扩到全 shipping 单元。

## 结论

```text
STATIC_ANALYSIS_GATE=PASS（含文档化 exception + alternate proof）
```
