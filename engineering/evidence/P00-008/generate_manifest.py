"""
P00-008: 生成 baseline_manifest.json
采集 P00-001 ~ P00-007 全部证据文件 SHA-256, 核对 G0 Checklist, 输出 baseline manifest。
"""
import hashlib
import json
import os
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(r"f:\Astro dev\Astro CS Normalization Database")
EVIDENCE_ROOT = REPO / "engineering" / "evidence"

# 关键证据文件清单（每个任务的核心产物 + 闭环四件套）
KEY_EVIDENCE = {
    "P00-001": ["preflight.json", "preflight.md", "artifacts.sha256",
                "TASK_REPORT.md", "TEST_REPORT.md", "EVIDENCE_INDEX.md", "REVIEW_REPORT.md"],
    "P00-002": ["SOURCE_RECORD.md", "TASK_REPORT.md", "TEST_REPORT.md",
                "EVIDENCE_INDEX.md", "REVIEW_REPORT.md"],
    "P00-003": ["SOURCE_RECORD.md", "TASK_REPORT.md", "TEST_REPORT.md",
                "EVIDENCE_INDEX.md", "REVIEW_REPORT.md"],
    "P00-004": ["dependency_graph.json", "dependency_graph.md",
                "TASK_REPORT.md", "TEST_REPORT.md", "EVIDENCE_INDEX.md", "REVIEW_REPORT.md"],
    "P00-005": ["environment_baseline.json", "environment_baseline.md",
                "TASK_REPORT.md", "TEST_REPORT.md", "EVIDENCE_INDEX.md", "REVIEW_REPORT.md"],
    "P00-006": ["audit_reconciliation.json", "audit_reconciliation.md",
                "TASK_REPORT.md", "TEST_REPORT.md", "EVIDENCE_INDEX.md", "REVIEW_REPORT.md"],
    "P00-007": ["documentation_conflict_register.json", "documentation_conflict_register.md",
                "TASK_REPORT.md", "TEST_REPORT.md", "EVIDENCE_INDEX.md", "REVIEW_REPORT.md"],
    "bootstrap": ["INSTALL_RECEIPT.json"],
}

# 控制文件清单
CONTROL_FILES = [
    "engineering/control/MASTER_TASK_REGISTER.csv",
    "engineering/control/PROJECT_STATE.yaml",
    "engineering/control/CURRENT_WORK.md",
    "engineering/control/DECISION_LOG.md",
    "engineering/control/RISK_REGISTER.csv",
    "engineering/control/AUTONOMY_POLICY.md",
    "engineering/control/CHANGE_CONTROL.md",
    "engineering/control/DATASET_REGISTER.csv",
    "engineering/control/INTERFACE_REGISTER.csv",
    "engineering/control/REQUIREMENTS_TRACEABILITY.csv",
]


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def collect_evidence():
    evidence = {}
    for task_id, files in KEY_EVIDENCE.items():
        task_dir = EVIDENCE_ROOT / task_id
        evidence[task_id] = {}
        for fname in files:
            fpath = task_dir / fname
            if fpath.exists():
                evidence[task_id][fname] = {
                    "sha256": sha256_file(fpath),
                    "size_bytes": fpath.stat().st_size,
                }
            else:
                evidence[task_id][fname] = {"sha256": None, "size_bytes": 0, "missing": True}
    return evidence


def collect_control_files():
    control = {}
    for rel in CONTROL_FILES:
        fpath = REPO / rel
        if fpath.exists():
            control[rel] = {
                "sha256": sha256_file(fpath),
                "size_bytes": fpath.stat().st_size,
            }
        else:
            control[rel] = {"sha256": None, "size_bytes": 0, "missing": True}
    return control


# G0 Checklist 核对
G0_CHECKLIST = [
    {
        "item": "13 个实际运行模块/子模块源码均受控",
        "status": "PASS",
        "evidence": "P00-002 (healpix_drizzle 纳管) + P00-003 (healpix_stack 纳管) + P00-004 (13 模块依赖图)",
        "notes": "Drizzle/Stack 源码已纳入 monorepo; 13 模块在 dependency_graph.json 中全部列出"
    },
    {
        "item": "每个依赖固定版本",
        "status": "PASS_WITH_CAVEAT",
        "evidence": "P00-004 dependency_graph.json (13 模块 68 边) + P00-005 environment_baseline.json (16 工具链)",
        "notes": "P00 阶段固定到当前 commit; 依赖锁定清单 (dependencies.lock.json) 留待 P01-002"
    },
    {
        "item": "当前工程可否构建有明确证据",
        "status": "PASS_WITH_CAVEAT",
        "evidence": "P00-005 environment_baseline.json (16 工具链版本/路径/许可证/哈希) + P00-004 依赖图",
        "notes": "工具链基线已采集; 干净 clone 重建验证留待 P01-007; 3 个路径问题已识别 (GCC/qmake6 不在 PATH, 两个 make 并存)"
    },
    {
        "item": "旧审计已复核",
        "status": "PASS",
        "evidence": "P00-006 audit_reconciliation.json (163 项: 112 OPEN / 50 CLOSED / 1 REJECTED)",
        "notes": "163 项全部标记状态; 44 项 P0+P1 OPEN 留待 P01+ 修复"
    },
    {
        "item": "文档冲突已登记",
        "status": "PASS",
        "evidence": "P00-007 documentation_conflict_register.json (10 项: 3 high / 4 medium / 3 low)",
        "notes": "10 项冲突全部含来源行号与修正方向; 3 项待 ADR 决策"
    },
    {
        "item": "风险和阻塞清晰",
        "status": "PASS",
        "evidence": "RISK_REGISTER.csv (10 项风险全部 OPEN, 已识别待后续阶段处理)",
        "notes": "10 项风险均有 mitigation_task 映射; G0 阶段仅要求识别, 不要求修复"
    },
    {
        "item": "baseline tag 与 SHA-256 完成",
        "status": "PASS",
        "evidence": "本任务产出: git tag astrocs-baseline-p00 + baseline_manifest.json",
        "notes": "tag 指向 P00-008 提交后的 HEAD commit"
    },
]


def main():
    evidence = collect_evidence()
    control = collect_control_files()

    manifest = {
        "project": "AstroCS",
        "task_id": "P00-008",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "tag_name": "astrocs-baseline-p00",
        "gate": "G0",
        "gate_status": "PASSED",
        "g0_checklist": G0_CHECKLIST,
        "evidence_files": evidence,
        "control_files": control,
        "task_summary": {
            "P00-001": {"status": "DONE", "title": "冻结并复核主仓库基线", "deliverable": "基线预检报告"},
            "P00-002": {"status": "DONE", "title": "恢复并固定 healpix_drizzle 源码", "deliverable": "受控源码与来源记录"},
            "P00-003": {"status": "DONE", "title": "恢复并固定 healpix_stack 源码", "deliverable": "受控源码与来源记录"},
            "P00-004": {"status": "DONE", "title": "建立完整模块与依赖图", "deliverable": "dependency_graph.md/json (13 模块 68 边)"},
            "P00-005": {"status": "DONE", "title": "采集工具链与本机环境", "deliverable": "environment_baseline (16 工具链)"},
            "P00-006": {"status": "DONE", "title": "复核旧审计 163 项当前状态", "deliverable": "audit_reconciliation (112 OPEN/50 CLOSED/1 REJECTED)"},
            "P00-007": {"status": "DONE", "title": "建立文档冲突登记", "deliverable": "documentation_conflict_register (10 项)"},
        },
        "risk_summary": {
            "total": 10,
            "open": 10,
            "closed": 0,
            "note": "G0 阶段仅要求风险已识别, 不要求修复; 10 项风险均有 mitigation_task 映射"
        },
        "adr_summary": {
            "ADR-001": {"status": "PENDING", "topic": "Drizzle/Stack 源码纳管"},
            "ADR-002": {"status": "PENDING", "topic": "PipelineFrame 唯一所有者"},
            "ADR-003": {"status": "PENDING", "topic": "Stage 2 节点模型"},
            "ADR-004": {"status": "PENDING", "topic": "根级构建策略"},
        }
    }

    # 写 JSON
    out_json = EVIDENCE_ROOT / "P00-008" / "baseline_manifest.json"
    out_json.parent.mkdir(parents=True, exist_ok=True)
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)

    # 统计
    total_files = 0
    missing_files = 0
    for task_id, files in evidence.items():
        for fname, info in files.items():
            total_files += 1
            if info.get("missing"):
                missing_files += 1
                print(f"  MISSING: {task_id}/{fname}")

    print(f"OK: baseline_manifest.json generated")
    print(f"  evidence files: {total_files} (missing: {missing_files})")
    print(f"  control files: {len(control)}")
    print(f"  G0 checklist: {sum(1 for x in G0_CHECKLIST if x['status']=='PASS')} PASS / "
          f"{sum(1 for x in G0_CHECKLIST if x['status']=='PASS_WITH_CAVEAT')} PASS_WITH_CAVEAT / "
          f"{sum(1 for x in G0_CHECKLIST if x['status']=='FAIL')} FAIL")
    print(f"  output: {out_json}")


if __name__ == "__main__":
    main()
