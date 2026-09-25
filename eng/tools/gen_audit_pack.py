# NON_PRODUCTION_TOOL_ONLY
# This file is NOT part of the production pipeline.
# It is a development/testing/research tool only.
# The production pipeline uses orchestrator.exe <stage1.json> exclusively.

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 ACSD v1.1 审计包。"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

RETIRED_NOTICE = (
    "GEN_AUDIT_PACK_RETIRED: 本工具（ACSD v1.1 审计包生成（git bundle + 文档/证据汇总 + AUDIT_MANIFEST））已退役；任意调用 exit 2（fail-closed，不伪装绿）。\n"
    "  依据: ENGINEERING_SPEC.md §8（不允许「静默坏掉 / 僵尸入口」）；独立审查《一页纸》S1-2"
    "（结论不得写成源码字面量，结论字段必须从证据源读取，读不到写 NOT_VERIFIED）。\n"
    "  退役原因: ① 输出根 audit/AstroCS-v1.1-audit-pack/（且不在根清单允许目录内）；输入 engineering/**、dist/** 均已不存在；"
    "② verification.regression_tests=「352/352 PASS」、tasks_done=「31/31」、gates_passed=「G0-G8 (9/9)」、verdict=「PASS」 全为字面量，无法从证据源复算 ⇒ 再跑一次就产出假绿（且会把在册汇总覆空仍打印 OK）。\n"
    "  活动替代: 无；V1.1 世代审计包已作废。\n"
    "  复原命令: git show 822b9c5391a14cc36979a7c550984f6ce363c713:eng/tools/gen_audit_pack.py\n"
    "  退役后行为: 直接调用打印本说明并 exit 2；原实现保留在本文件下半（按复原命令取回）。"
)

if __name__ == "__main__":
    print(RETIRED_NOTICE, file=sys.stderr)
    sys.exit(2)

def _deduce_root() -> Path:
    # auto-deduce project root: walk up until docs/ and lib/ found (Linux-portable)
    try:
        p = Path(__file__).resolve()
        for cand in [p.parent, *list(p.parents)]:
            if (cand / "docs").is_dir() and (cand / "lib").is_dir():
                return cand
    except Exception:
        pass
    cwd = Path.cwd()
    if (cwd / "docs").is_dir() and (cwd / "lib").is_dir():
        return cwd
    return Path(__file__).resolve().parents[2] if len(Path(__file__).resolve().parents) >=3 else Path(__file__).resolve().parent

REPO = _deduce_root()
OUT = REPO / "audit" / "AstroCS-v1.1-audit-pack"
OUT.mkdir(parents=True, exist_ok=True)

def run(cmd, cwd=REPO):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return r.returncode, r.stdout, r.stderr

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()

# 1. git bundle（完整历史）
print("[1/6] 生成 git bundle...")
bundle = OUT / "astrocs-v1.1-full.bundle"
rc, so, se = run(["git", "bundle", "create", str(bundle), "--all"])
print(f"  bundle: {bundle.name} rc={rc} {se.strip() if se else ''}")

# 2. git log 全量
print("[2/6] 导出 git log...")
rc, so, _ = run(["git", "log", "--oneline"])
(OUT / "git_log_all.txt").write_text(so, encoding="utf-8")
rc, so2, _ = run(["git", "log", "--stat", "--date=iso"])
(OUT / "git_log_detailed.txt").write_text(so2, encoding="utf-8")
print(f"  {len(so.splitlines())} commits")

# 3. 汇总关键文档
print("[3/6] 汇总关键文档...")
docs = {
    "HANDOVER.md": "engineering/evidence/P08-002/HANDOVER.md",
    "final_handover.json": "engineering/evidence/P08-002/final_handover.json",
    "MASTER_TASK_REGISTER.csv": "engineering/control/MASTER_TASK_REGISTER.csv",
    "PROJECT_STATE.yaml": "engineering/control/PROJECT_STATE.yaml",
    "CURRENT_TASK.md": "engineering/control/CURRENT_TASK.md",
    "README.md": "README.md",
    "hiss_format_v1.md": "engineering/contracts/hiss_format_v1.md",
    "hcsd_format_v1.md": "engineering/contracts/hcsd_format_v1.md",
    "cli_command_schema_v1.json": "engineering/contracts/cli_command_schema_v1.json",
    "cli_event_schema_v1.json": "engineering/contracts/cli_event_schema_v1.json",
    "error_code_registry.csv": "engineering/contracts/error_code_registry.csv",
    "config_parameter_registry.csv": "engineering/contracts/config_parameter_registry.csv",
    "VERSION.txt": "dist/AstroCS-CLI-v1/VERSION.txt",
    "SHA256SUMS.txt": "dist/AstroCS-CLI-v1/SHA256SUMS.txt",
    "dist_README.txt": "dist/AstroCS-CLI-v1/README.txt",
}
doc_dir = OUT / "documents"
doc_dir.mkdir(exist_ok=True)
for name, src in docs.items():
    sp = REPO / src
    if sp.exists():
        shutil.copy2(sp, doc_dir / name)
        print(f"  + {name}")
    else:
        print(f"  ! 缺失: {src}")

# 4. 证据报告汇总（四份标准报告 x 关键任务）
print("[4/6] 汇总证据报告...")
evid_dir = OUT / "evidence_reports"
evid_dir.mkdir(exist_ok=True)
key_tasks = ["P05-002", "P06-002", "P06-003", "P07-001", "P07-002", "P08-001", "P08-002"]
for task in key_tasks:
    tdir = REPO / "engineering" / "evidence" / task
    if not tdir.exists():
        continue
    for rep in ["TASK_REPORT.md", "TEST_REPORT.md", "EVIDENCE_INDEX.md", "REVIEW_REPORT.md"]:
        src = tdir / rep
        if src.exists():
            dst = evid_dir / f"{task}_{rep}"
            shutil.copy2(src, dst)
print(f"  {len(list(evid_dir.iterdir()))} 份报告")

# 5. 生成审计清单
print("[5/6] 生成审计清单...")
rc, commit, _ = run(["git", "rev-parse", "HEAD"])
rc, branch, _ = run(["git", "rev-parse", "--abbrev-ref", "HEAD"])
rc, count_out, _ = run(["git", "rev-list", "--count", "HEAD"])

manifest = {
    "package": "AstroCS-v1.1-audit-pack",
    "version": "v1.1.0",
    "generated_at": datetime.now().isoformat(),
    "git": {
        "commit": commit.strip(),
        "branch": branch.strip(),
        "total_commits": int(count_out.strip()) if count_out.strip().isdigit() else 0,
        "remote": "https://github.com/fujiaze/Astro-CS-Database",
    },
    "contents": {
        "git_bundle": "astrocs-v1.1-full.bundle (完整 git 历史, 可 git clone)",
        "git_log_all.txt": "所有 commit oneline 列表",
        "git_log_detailed.txt": "详细 commit 日志 (含文件变更统计)",
        "documents/": "关键契约与控制文档 (15 个)",
        "evidence_reports/": "关键任务四份标准报告 (7 任务 x 4 = 28 份)",
    },
    "verification": {
        "regression_tests": "352/352 PASS",
        "tasks_done": "31/31",
        "gates_passed": "G0-G8 (9/9)",
        "verdict": "PASS",
    },
    "bundle_usage": "git clone astrocs-v1.1-full.bundle AstroCS-clone",
}

# 计算所有文件 SHA-256
file_hashes = []
for f in sorted(OUT.rglob("*")):
    if f.is_file() and f.name != "AUDIT_MANIFEST.json":
        file_hashes.append({
            "path": str(f.relative_to(OUT)),
            "sha256": sha256_file(f),
            "size": f.stat().st_size,
        })
manifest["files"] = file_hashes
manifest["total_files"] = len(file_hashes)

(OUT / "AUDIT_MANIFEST.json").write_text(
    json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

# 人类可读清单
lines = [
    "ACSD v1.1 审计包清单",
    "=" * 60,
    f"版本: {manifest['version']}",
    f"生成时间: {manifest['generated_at']}",
    f"Git commit: {manifest['git']['commit']}",
    f"分支: {manifest['git']['branch']}",
    f"总提交数: {manifest['git']['total_commits']}",
    f"远程: {manifest['git']['remote']}",
    "",
    "验证状态:",
    f"  回归测试: {manifest['verification']['regression_tests']}",
    f"  任务完成: {manifest['verification']['tasks_done']}",
    f"  Gate 通过: {manifest['verification']['gates_passed']}",
    f"  裁决: {manifest['verification']['verdict']}",
    "",
    "包内容:",
    f"  1. {manifest['contents']['git_bundle']}",
    f"  2. {manifest['contents']['git_log_all.txt']}",
    f"  3. {manifest['contents']['git_log_detailed.txt']}",
    f"  4. {manifest['contents']['documents/']}",
    f"  5. {manifest['contents']['evidence_reports/']}",
    "",
    f"文件清单 ({manifest['total_files']} 个):",
    "-" * 60,
]
for fh in file_hashes:
    lines.append(f"  {fh['sha256'][:16]}  {fh['size']:>10}  {fh['path']}")
lines += [
    "-" * 60,
    "",
    "使用 git bundle 克隆:",
    f"  git clone astrocs-v1.1-full.bundle AstroCS-clone",
    "",
]
(OUT / "AUDIT_MANIFEST.txt").write_text("\n".join(lines), encoding="utf-8")
print(f"  清单: {manifest['total_files']} 个文件")

# 6. 汇总
print("[6/6] 完成!")
total_size = sum(f.stat().st_size for f in OUT.rglob("*") if f.is_file())
print(f"  路径: {OUT}")
print(f"  总大小: {total_size / 1024 / 1024:.1f} MB")
print(f"  bundle: {bundle.stat().st_size / 1024 / 1024:.1f} MB")
