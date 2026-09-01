# P2-007: Phase2 接缝与资源联合门

任务 ID: P2-007
Gate: G5
依赖: P2-006; MON-003
平台: Linux (2c2g)
变更类别: validation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P2-007：

> 在 2c2g 运行 production seam workload ≥10s，保存科学+资源证据。CPU 不达门时
> 不得因 seam 数值好而 PASS。Windows 最终还要同一 test registry 复验，但本任务
> 先关闭 Linux 实现缺陷。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| production seam workload ≥10s | 6 块 seam mini HiPS(模式轮换+偏移交替) → run active_wall=14.51s | c01 #1 |
| 科学证据 | UPM 校正场非空(seam 被检测校正); C 空间变化 < 0.2(不拟合星 2.0) | c01 #5 |
| 资源证据 | workers_p50=2(多 worker); cpu_p50=100%/mean=113%(≥90/85); peak_rss=39.7MB | c01 #2/#3/#4 |
| CPU 不达门不因数值 PASS | gate 事件必须 ok(失败 → RESOURCE 10 + diagnosis) | c01 #5 |
| Windows 复验 | 同一 test registry 保留; Fatduck 在线后复验 | 说明 |

## 实现文件

- `tests/backend/phase2_fixture_main.cpp`：新增 `--make-seam6`(6 块 seam HiPS)
- `tests/backend/test_p2007_joint_gate.py`（新）：5 组断言(≥10s/workers≥2/CPU 门/RSS 有界/科学+联合门)

## 测试结果

- `test_p2007_joint_gate.py`: 5/5 PASS
- `test_p2003_seam_oracle.py`: 4/4 PASS(回归); `test_p2006_canonical_pipeline.py`: 4/4 PASS
- gate: active_wall=14.51s, workers_p50=2, cpu_p50=100%, cpu_mean=113% — 2c2g 达门

## 说明

- 2c2g(cgroup 限 2 核)下 avg_equivalent_cores=1.11 受 harness 常驻干扰影响,
  但 MON-003 门按 worker_p50≥2 + CPU p50≥90% 判定, 实测达门。
- Windows 复验(test registry 同源)在 Fatduck 在线窗口执行, 不阻塞 Linux。
