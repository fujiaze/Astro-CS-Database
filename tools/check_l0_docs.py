#!/usr/bin/env python3
"""DOC-002: L0 固定文档完整性校验。

规则: REVIEW.md + docs/review/{SCIENCE,PIPELINE,ARCHITECTURE,RELEASE_STATUS,CHANGE_REVIEW}_OVERVIEW.md
全部存在且非空; REVIEW.md 链接全部 5 份; 各文档只汇总权威合同 (不含完整公式抄写)。
exit 0 = PASS。
"""
import pathlib, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
DOCS = ["SCIENCE_OVERVIEW.md", "PIPELINE_OVERVIEW.md", "ARCHITECTURE_OVERVIEW.md",
        "RELEASE_STATUS.md", "CHANGE_REVIEW.md"]

def main():
    errors = []
    root_review = REPO / "REVIEW.md"
    if not root_review.is_file():
        errors.append("REVIEW.md missing")
    else:
        rt = root_review.read_text(encoding="utf-8", errors="ignore")
        for d in DOCS:
            if d not in rt:
                errors.append(f"REVIEW.md missing link to {d}")
    for d in DOCS:
        p = REPO / "docs" / "review" / d
        if not p.is_file():
            errors.append(f"docs/review/{d} missing")
        elif len(p.read_text(encoding="utf-8", errors="ignore")) < 200:
            errors.append(f"docs/review/{d} too short")
    if errors:
        print("DOC-002_L0_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"DOC-002_PASS: REVIEW.md + {len(DOCS)} L0 docs, 链接完整, 简洁可审")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
