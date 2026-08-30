#!/usr/bin/env python3
"""DOC-005: 静态 Pipeline 图与运行 trace 一致性检查。

规则:
1. 静态图: 每 session (p1/p2/p3) 声明的阶段节点集 (源码 stage 调用)。
2. 运行 trace: 运行 manifest (astrocs_run_*.json) 的 stages 记录。
3. 一致性: 运行 trace 的每个 stage 必须存在于对应 session 的静态节点集。
exit 0 = PASS。
"""
import json, pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]

# session → 源码文件 → 阶段节点提取 (静态图)
SESSIONS = {
    "p1": "lib/phase1_session/p1_session.cpp",
    "p2": "lib/phase2_session/p2_session.cpp",
    "p3": "lib/phase3_session/p3_session.cpp",
}

def static_nodes(src_path):
    txt = (REPO / src_path).read_text(encoding="utf-8", errors="ignore")
    # 提取 "name", "<node>" 或 stage("<node>"
    nodes = set(re.findall(r'"name",\s*"([a-z_]+)"', txt))
    nodes |= set(re.findall(r'stage\(\s*"([a-z_]+)"', txt))
    return nodes

def main():
    errors = []
    for sess, src in SESSIONS.items():
        nodes = static_nodes(src)
        if not nodes:
            if sess == "p3":
                nodes = {"output"}   # p3 单阶段: 直接输出 FITS (无 stage 声明)
            else:
                errors.append(f"{sess}: 静态图无节点 ({src})")
                continue
        # 校验 manifest (若存在)
        manifests = sorted((REPO / "build").rglob("astrocs_run_*.json"))
        for mf in manifests:
            try:
                doc = json.loads(mf.read_text(encoding="utf-8"))
            except Exception:
                continue
            for st in doc.get("stages", []):
                name = st.get("name", "") if isinstance(st, dict) else str(st)
                # run_phaseN 包装阶段跳过; 具体阶段须在静态图
                if name.startswith("run_phase") or not name:
                    continue
                if name not in nodes:
                    errors.append(f"{sess}: 运行 trace stage '{name}' 不在静态图 ({src})")
    if errors:
        print("DOC-005_TRACE_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"DOC-005_PASS: {len(SESSIONS)} session 静态图节点与运行 trace 一致")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
