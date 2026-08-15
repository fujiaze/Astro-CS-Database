# PR#1 UPM Frame Binding — Merge Gate Summary

日期：2026-08-15
分支：fix/upm-frame-order（本地修复分支，基于 PR head f2407f2）

## P0 PR FETCH

- remote: https://github.com/fujiaze/Astro-CS-Database.git
- PR head SHA: f2407f2a47efabb18acfc6f50e1ca49177bdd2d5
- PR base / merge-base: 87173cf72b948f5d534857b9af6d3abc79c83bd6
- changed files: lib/phase2/src/upm.cpp, lib/phase2/tests/synthetic_gate.cpp
- diff.patch SHA256: 见 diff_sha256.txt

## P1 DIFF REVIEW（PR_UPM_GATE §9 checklist）

1. writer 从参数行 index 域序列化 ID：PASS（frame_id_by_index）
2. reader 同时重建 frame_id_by_index 与 frame_index：PASS
3. 无 std::map 遍历推断参数顺序：PASS（save 已改为 frame_id_by_index）
4. 重复 ID 拒绝：PASS（门禁修复新增校验，见下）
5. 行列维度检查：PASS（门禁修复新增）
6. sparse/dense 行为一致：PASS（UpmPersistSparseDenseBinding）
7. 旧模型文件显式处理：PASS（v1/v2 同路径；畸形拒绝）
8. 修复覆盖所有生产 load/apply 路径：PASS（唯一入口 p2_upm_open；
   stage2.cpp / calibrated_pair_diag.cpp 均经此）
9. 测试使用公共生产 API：PASS（p2_upm_build/save/open/calibrate_block）
10. 未引入第二套绑定实现：PASS

## 门禁修复（PR 原始提交不满足 PR-UPM-010，按门禁在 PR 分支补齐）

p2_upm_open 新增 DATA-UPM-MODEL-001 校验：
- frames 必须存在且为数组；每项必须为无符号整数；重复 ID 拒绝
- controls 必须存在为数组；字段类型非法拒绝
- cell_index / frame_component / component_ref_frame 长度与类型校验
- C 行数必须等于 frame 数；control 索引越界拒绝（原为静默忽略）
- 全部字段访问包 try/catch：损坏文件稳定返回 1，异常不越 C ABI

p2_upm_save 新增 ALG-UPM-FRAME-BIND-001 不变量：
- frame_id_by_index.size() == C.size()，否则拒绝写盘

## P2 SCIENCE TEST（PR-UPM-001..010）

测试位置：lib/phase2/tests/synthetic_gate.cpp

| Gate | 用例 | 结果 |
| --- | --- | --- |
| PR-UPM-001 | OpenSavePreservesFrameParameterBinding（PR 自带） | PASS |
| PR-UPM-002 | UpmPersistAllPermutations（24/24 排列） | PASS |
| PR-UPM-003 | UpmPersistRandomStableIds（100 种子） | PASS |
| PR-UPM-004 | UpmPersistSparseDenseBinding（稀疏路径） | PASS |
| PR-UPM-005 | UpmPersistSparseDenseBinding（稠密路径） | PASS |
| PR-UPM-006 | UpmPersistRoundtripChainNoDrift（M0..M3） | PASS |
| PR-UPM-007 | UpmPersistInsertionOrderIndependent | PASS |
| PR-UPM-008 | OpenSavePreservesFrameParameterBinding + seam 逐帧应用 | PASS |
| PR-UPM-009 | UpmPersistMosaicSeamEquivalence | PASS |
| PR-UPM-010 | UpmPersistInvalidModelRejected（8 类破坏） | PASS |

回归：phase2_synthetic_gate 全量 82/82 PASS（分支 V18R3 基线 + 本次修改）。

## BACKWARD COMPAT DECISION（PR_UPM_GATE §7）

方案 A（安全迁移）：所有 build 路径（p2_upm_build / p2_upm_build_geo）的
frame index 均来自 std::set（升序），历史 writer 的 frames 列表（map 遍历）
与 C 行序一致，旧文件自洽 → 可直接读取；新校验兜底，畸形/歧义文件显式拒绝，
绝不猜测重建。文档落点：docs/architecture/COMPATIBILITY_POLICY.md、
docs/algorithms/UPM_SOLVER.md、docs/modules/phase2.md、CHANGELOG.md（V19R2 S2/S7 完成）。

## 合并门

- PR_DIFF_REVIEW=PASS
- PR_UPM_001_010=PASS
- UPM_BINDING_TRACEABILITY=PASS（docs/TRACEABILITY.csv 已建 13 行）
- BACKWARD_COMPAT_DECISION=PASS（方案 A）
- KNOWN_P0=0 / KNOWN_P1=0（本 PR 范围内）
