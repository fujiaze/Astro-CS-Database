> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/owner/PIPELINE_OVERVIEW.md（GOV-004 建立）

# PIPELINE_OVERVIEW.md — AstroCS 处理链总览（L0 负责人层）

> 目标版本: 0.10.0-alpha.2  V6 重构
> 权威: `docs/contracts/ARCH-001.md`、`docs/refactor/P1/P2/P3_SYMBOL_MAP.md`、`CLI_COMMAND_LAYER.md`
> 本文件只汇总权威结构, 不重复维护实现细节。

## 1. 处理链 (唯一生产路径)

```
phase1 (校准/星点/噪声) → phase2 (coverage/UPM/排异/积分) → phase3 (HiPS→FITS)
        run preset (CLI-002) 驱动; 逐 phase Artifact 哈希链传递
```

## 2. 三阶段职责

| 阶段 | 输入 | 输出 | 模块 (astrocs.*) |
|---|---|---|---|
| Phase1 | 光帧 + 母版 | 校准帧 + 星表 | phase1.calibration / stars / wcs / photometry / noise |
| Phase2 | 校准帧 HiPS | mosaic + UPM surface + 排异诊断 | phase2.coverage/sampler/upm/rejection/integrate/block |
| Phase3 | HiPS | 平面 WCS FITS | phase3.wcs / resample / output |

## 3. 唯一生产路径 (G7 gate)
- CLI kRules 表驱动 (CLI-001), 不直连科学内核。
- 旧路径全部退出: drizzle 测试 wrapper (LEG-001), Orchestrator (LEG-002),
  AIO PipelineEngine 调度 (LEG-003), old Stage2 (LEG-004), ACR dormant (LEG-004)。
- 生产仅纯 CPU backend; workers 由 Runtime lease ≥2 (P2-002)。

## 4. 不变性
- 同一时间一个任务 IN_PROGRESS (控制包状态机)。
- 每任务一提交一推送; 仅 main。
- 未过硬门不得称完成。
