# DOC-006: 统一注释命名与代码可读性

任务 ID: DOC-006
Gate: G7
依赖: DOC-005
平台: Linux
变更类别: quality

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` DOC-006：

> 按模块小范围整理，禁止全仓格式化。拆薄 CLI；删除任务号/审核轮次/旧版本故事/
> 逐行复述/虚假 thread-safe 注释；保留数学原因、单位、边界、所有权、线程约束、
> 非显然优化和合同 ID。修复模糊变量/死代码/危险 test shell。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 删除任务号注释 | 全 lib/ cli/ 扫描 `(P[123]-00[0-9]`/`— P[123]-00[0-9]` 任务号残留 = 0(保留 ALG-/SCI-/TEST-/DATA-/API-/MOD-/ARCH- 合同 ID) | c02 |
| 保留数学原因/单位/所有权/合同 ID | 清理仅删任务号, 保留技术含义(如 "并行由 std::thread + Runtime lease 驱动") | c01/c02 |
| 无虚假 thread-safe 注释 | p1_session.h "threadsafe:no(handle 级)" 为诚实标注(非虚假) | c01 |
| 注释卫生 | check_comment_hygiene PASS(501 扫描 0 违规) | c01 |
| 编译/测试回归 | make 通过; ctest 56/56 PASS | 回归 |

## 实现文件(仅注释, 无逻辑变更)

- `lib/phase2/src/sampler.cpp`、`lib/phase2/src/upm.cpp`
- `lib/phase2/include/astro/phase2/upm.h`、`sampler.h`
- `lib/phase2_session/p2_session.cpp`
- `lib/phase3_session/p3_session.cpp/.h`、`p3_resample.cpp/.h`、`p3_output.cpp/.h`、`p3_wcs.cpp`
- `lib/phase1/wcs/wcs_tan.h`、`lib/phase1/noise/noise_model.h`
- `lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp`

## 测试结果

- c01: comment hygiene 0 违规; c02: 任务号注释 0 残留
- ctest 56/56 PASS

## 说明

- 本次仅注释文本清理(任务号 → 保留含义), 无任何逻辑/签名变更; 编译与测试全绿。
- 合同 ID(ALG-P3-001 等)按规格保留, 任务号(P3-001)删除。
