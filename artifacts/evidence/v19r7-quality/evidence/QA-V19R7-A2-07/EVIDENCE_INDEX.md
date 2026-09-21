# QA-V19R7-A2-07 Evidence Index — Standards 域审计（H+E, grep 抽样）

- task: QA-V19R7-A2-07 | Spec §4.1 A2-07 (13 项标准)
- gate: G-QA A Gate
- status: DONE (只读, grep 抽样)
- date: 2026-08-21 | branch: main | auditor: resident:code
- baseline: V19R6R2-W1 HEAD 05d7d6b

## 通过条件
- `reports/v19r7_quality/audit_findings_standards.md` 已落盘，含概述/方法/发现表(STD-001..012)/统计，给出文件:行号样例
- 13 项标准 CODE/COMMENT/NUMERIC/C_ABI/CONCURRENCY/ERROR/IO/LOGGING/TEST/BENCHMARK/DOCUMENTATION/API/RELEASE 逐项抽样
- 不改 docs/lib（只读）

## 实际结果
- 审计文档: 13 份 docs/standards/*.md 全文
- 审计代码: 581 源码文件 (873 全仓含 third_party/archive, 已排除第三方后实审 581) — grep 关键词抽样
- 前置扫描: standards_violations.json — checks=13, CODE 3 / COMMENT 4 命中, 其余 info hits
- 发现: 10 分级项 — P0 0 / P1 3 / P2 7 (STD-011/012 无违规)
- 关键样例行: 12 行精确 file:line
- 机器一致性: pass (checks=9 pass=true), 文件审计: total 873

## 产出清单
- `reports/v19r7_quality/audit_findings_standards.md` — 主分表（STD-001..012, P0 0/P1 3/P2 7, 含 file:line 样例）
- 本 `EVIDENCE_INDEX.md`
- 对偶索引: `evidence/QA-V19R7-A2-07/EVIDENCE_INDEX.md`
- 前置证据: `reports/v19r7_quality/standards_violations.json` (A1-02 生成)

## 关键发现映射
| # | 标准 | 样例位置 | 级别 |
|---|---|---|---|
| STD-001 | COMMENT MUST 禁 V 版本号 | representative_probe.cpp:2 V19R4 ; test_spherical_overlap.cpp:1025 F-V19R2 ; synthetic_gate.cpp:5322 F-V19R2 | P1 |
| STD-002 | COMMENT 禁废话 | checkpoint.cpp:406 遍历数组 | P1 |
| STD-003 | CODE/NUMERIC 裸 1e-6 | hiss_correctness_test.cpp:155,187,269 ; test_query_pixel.cpp:613 | P1 |
| STD-004..010 | CODE/C_ABI/CONCURRENCY/NUMERIC/IO/LOGGING/TEST | 见 standards 分表 | P2 |

## 关联
- 上游: A1-02 standards_violations.json
- 下游: B3-02..B3-10, B4-03..B4-28 逐模块修复
- Commit: `docs(qa): audit findings standards [A2-07]` (待提交)

## 校验
- 文件存在且非空；grep 可复现：`grep -rn "V19R\\d" lib --include="*.cpp" --include="*.h" | grep -v third_party`

## 局限
- grep 抽样非 clang-tidy 全量；NUMERIC/CONCURRENCY 未做 TSan/ASan 运行时；建议 C 阶段 WSL ASan/UBSan 矩阵复核。
