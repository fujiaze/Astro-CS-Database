# Round 0 — Contract（V18R2 Resource-Driven）

日期：2026-08-15 ｜ 控制包：AstroCS_ResourceDriven_Performance_CodeClosure_
Control_Package_V18R2（SHA256 99D261E280A9984110191734528DAC8795818E41C79D6111C466BB61D60130ED）

## 冻结基线（禁止重跑）

```text
before 3× 完整 16 帧：129.7 / 126.1 / 126.65 s（V17 代码）
```

## 方法（取代旧 V18）

```text
单帧/最多 2 帧资源剖析（CPU/线程/内存/磁盘 IO/等待）
→ 源码复杂度审计（hotpath_complexity.csv）
→ 定点优化（source evidence → targeted profile → implement → oracle → 单帧）
→ 全部完成后仅 1 次完整 16 帧最终验证
```

## 禁止

- 反复批量跑基准；subset 冒充 complete；
- 改科学算法（冻结语义）；BASS/2×2/3×3 真实数据（留 V19）；
- 为了刷百分比乱改算法。

## 完成定义

```text
PERFORMANCE_AND_CODE_CLOSURE=PASS
FINAL_DATA_VALIDATION=PENDING_V19
```
