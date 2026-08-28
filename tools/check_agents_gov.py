#!/usr/bin/env python3
"""GOV-001 checker: 核验 AGENTS.md 含 V5 全部治理要素且无冲突旧条款。exit 0=PASS。"""
import sys

PATH = "AGENTS.md"
REQUIRED = [
    ("main-only",    ["只在 main 原子提交并立即 push", "禁止分支及破坏性 Git"]),
    ("amd64",        ["仅支持 amd64"]),
    ("节点",         ["vm-bj", "Fatduck"]),
    ("cpu-only",     ["ACR 暂不接入", "纯 CPU 自适应 backend"]),
    ("单入口",       ["仅一个 astrocs CLI", "Phase1/2/3 由 CLI 调用"]),
    ("资源门禁",     ["重计算自动监控", "低利用率或异常内存增长为失败"]),
    ("无硬编码",     ["ISA、workers、block 由逐内核 benchmark 选择", "禁止硬编码"]),
    ("alpha/发布",   ["未经最终外部审核不得宣称发布", "AWAITING_EXTERNAL_RELEASE_REVIEW"]),
    ("状态机",       ["NOT_STARTED -> IN_PROGRESS", "REVIEW_PENDING", "waiver"]),
    ("不停工",       ["不设等待外部批准的停止点", "Fatduck 离线不中止 Linux 可执行任务"]),
]
FORBIDDEN = []

def main():
    text = open(PATH, encoding="utf-8").read()
    missing = []
    for name, needles in REQUIRED:
        ok = all(n in text for n in needles)
        print(f"[{'OK' if ok else 'MISS'}] {name}")
        if not ok:
            missing.append(name)
    # Git Bash/pwsh 只允许出现在禁止性条款中, 不得作为默认环境旧规则
    bad = []
    for i, line in enumerate(text.splitlines(), 1):
        if ("Git Bash" in line or "pwsh" in line) and "禁止" not in line:
            bad.append(f"第{i}行出现 Git Bash/pwsh 但非禁止性条款")
    for f in bad:
        print(f"[CONFLICT] {f}")
    if missing or bad:
        print(f"GOV_CHECK_FAIL missing={missing} conflicts={bad}")
        return 1
    print("GOV_CHECK_PASS 10/10 要素齐备, 无冲突条款")
    return 0

if __name__ == "__main__":
    sys.exit(main())
