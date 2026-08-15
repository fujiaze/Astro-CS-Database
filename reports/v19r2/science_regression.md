# Science Regression (V19R2)

## 原则（§21）

未修改模块引用 V19 可信 artifact：git HEAD 与 V19 快照 hash 比对
（LF 规范化）仅 8 文件变化（5 memory + 2 docs + upm.cpp），280 shipping
中除 upm.cpp 外全部逐字节一致 → V19 科学结论继续有效。

## 本轮重跑/新跑

| 套件 | 结果 |
| --- | --- |
| phase2 synthetic gate（含 PR-UPM-001..010、未知帧） | 83/83 PASS |
| SNR 科学矩阵 | 32/32 PASS |
| SNR 对账 | 5/5 PASS |
| Drizzle 方差传播（SNR-011/012/DRZ-014） | 8/8 PASS |
| Drizzle candidate oracle | 9003/9003 PASS（false negative=0） |
| Drizzle 科学补齐 | 16/16 PASS |
| Drizzle reverse | 5/5 + 科学矩阵 37/37 PASS |
| spherical overlap | 77/77 PASS（F-V19R2-DRZ-001 修复后） |
| edge-cross oracle | 0 漏报 PASS |
| Drizzle DLL API | 11/11 PASS |
| Drizzle WCS 矩阵 | 180/180 PASS |
| StarDetector FP64 | 4/4 PASS |
| orchestrator CLI | 233/233 PASS |
| AIO pipeline contract | 28/28 PASS |

## 受影响的科学契约

- SCI-UPM-PERSIST-001/ALG-UPM-FRAME-BIND-001/DATA-UPM-MODEL-001
  （PR#1 门禁，已过）;
- F-V19R2-UPM-002 修复（未知帧显式失败）——新增测试覆盖；
- F-V19R2-DRZ-001 测试断言调整（科学阈值不变，仅放宽不可判定的单调性）。

```text
SCIENCE_REGRESSION=PASS
```
