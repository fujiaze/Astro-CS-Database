# QA-V19R7-A1-01 Evidence Index — 机器一致性初扫（M+E）

- task: QA-V19R7-A1-01
- gate: G-QA (A阶段)
- status: DONE
- date: 2026-08-22
- branch: main (ahead of origin/main by 2 + 1 pending)
- tool: tools/docs_machine_consistency.py (已修复 Linux ROOT 自动推导)

## 通过条件
- `python3 tools/docs_machine_consistency.py` 在 vm-bj Linux 上可运行（退出码 0/1 均可，JSON 已生成）
- `reports/v19r7_quality/machine_consistency_before.json` 已落盘且可复现
- `broken_before.log` 已落盘
- broken 数已计入 `audit_stats.json`

## 实际结果
- 退出码: 0 (PASS)
- 检查项: 9/9 PASS, broken: 0
- 检查清单:
  1. config_weight_mode_ivar — PASS
  2. frame_id_contract_exact (DATA-FRAME-ID-001) — PASS
  3. error_taxonomy_exit_codes (ERROR_MODEL 全集合 == orchestrator.h) — PASS
  4. integration_status_full_set — PASS
  5. rejection_status_full_set — PASS
  6. stage_ids_docs_vs_orchestrator — PASS
  7. snr_constants (1.4826022185 / 0.7316728) — PASS
  8. product_contracts (signal/support/variance/ivar) — PASS
  9. drizzle_variance_formula — PASS

## 产出清单
- `tools/docs_machine_consistency.py` — 修复 ROOT 自动推导（仅改 ROOT 计算，保留10项检查逻辑）
- `reports/v19r7_quality/machine_consistency_before.json` — 机器一致性 JSON
- `reports/v19r7_quality/broken_before.log` — broken 明细（0 broken）
- `reports/v19r3/evidence/quality/docs_consistency.json` — 工具原路径输出（同内容副本）

## 修复说明（A1-01 唯一改动）
- 旧: `ROOT = r"F:\\Astro dev\\Astro CS Normalization Database"` 硬编码 Windows 路径 → Linux FileNotFoundError
- 新: `_deduce_root()` 自动推导：`__file__` 上两级校验含 docs/ 与 lib/，回退 cwd，再回退 parent；reports 输出到当前项目的 `reports/v19r7_quality/`
- 影响: 仅工具路径最小修复，不改 lib/* 业务代码

## 关联
- MASTER_TASK_REGISTER: QA-V19R7-A1-01 TODO→DONE
- Commit: `chore(qa): fix machine_consistency linux path + before scan [QA-V19R7-A1-01/02]`
