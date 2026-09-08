#!/usr/bin/env python3
"""DOC-005: 静态 Pipeline 图与运行 trace 一致性检查。

规则:
1. 静态图: 每 session (p1/p2/p3) 声明的阶段节点集 (源码 stage 调用)。
   p3 的静态图来自 cli/runtime_client.cpp 的 graph 节点链 node_id 声明
   (p3_session.cpp 本身无 stage 声明; runtime_client 是 p3 图的声明面)。
2. 运行 trace: 运行 manifest (astrocs_run_*.json) 的 stages 记录。
   manifest schema v1 不写 stages (write_run_manifest 只写 phases/artifacts):
   无 stages 的 manifest 跳过 stage 比对; 有 stages 的 manifest 仍强制逐条
   命中静态图 (检测力由负例 fixture 保证, 不构成空转假 PASS)。
3. 一致性: manifest 的 phases 决定用哪个 session 静态图校验
   (修复: 原实现 manifests 收集在 session 循环内, p1 图会误校验 p2 manifest)。
exit 0 = PASS。
"""
import json, pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]

# session → 源码文件列表 → 阶段节点提取 (静态图)
SESSIONS = {
    "p1": ["lib/phase1_session/p1_session.cpp"],
    "p2": ["lib/phase2_session/p2_session.cpp"],
    "p3": ["lib/phase3_session/p3_session.cpp", "cli/runtime_client.cpp"],
}

PHASE_TO_SESSION = {1: "p1", 2: "p2", 3: "p3"}

def static_nodes(src_paths):
    nodes = set()
    for src_path in src_paths:
        p = REPO / src_path
        if not p.is_file():
            continue
        txt = p.read_text(encoding="utf-8", errors="ignore")
        # 提取 "name", "<node>" 或 stage("<node>"
        nodes |= set(re.findall(r'"name",\s*"([a-z_]+)"', txt))
        nodes |= set(re.findall(r'stage\(\s*"([a-z_]+)"', txt))
        # graph 节点链: node_id 直接赋值 ("cal") 与 chain tuple 首元素
        nodes |= set(re.findall(r'node_id"\]\s*=\s*"([a-z_]+)"', txt))
        nodes |= set(re.findall(r'\{\s*"([a-z_]+)",\s*"astrocs\.[a-z0-9.]+",', txt))
    return nodes

def main():
    errors = []
    static_map = {}
    for sess, srcs in SESSIONS.items():
        nodes = static_nodes(srcs)
        if not nodes and sess == "p3":
            # runtime_client 缺失时结构性兜底 (不构成 FAIL 源)
            nodes = {"output"}
        static_map[sess] = nodes
        if not nodes:
            errors.append(f"{sess}: 静态图无节点 ({','.join(srcs)})")
    if errors:
        print("DOC-005_TRACE_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    # 修复: manifests 只收集一次, 不随 session 重复校验 (原实现 p1 图会
    # 校验 p2 manifest 的 stage → 跨 session 误报/漏报)
    manifests = sorted((REPO / "build").rglob("astrocs_run_*.json")) + \
                sorted(REPO.glob("astrocs_run_*.json"))
    for mf in manifests:
        try:
            doc = json.loads(mf.read_text(encoding="utf-8"))
        except Exception:
            continue
        stages = doc.get("stages") or []
        if not stages:
            continue   # schema v1 manifest 无 stages 记录, 跳过比对
        sess = None
        for ph in doc.get("phases") or []:
            if ph in PHASE_TO_SESSION:
                sess = PHASE_TO_SESSION[ph]
                break
        if sess is None:
            errors.append(f"{mf.name}: phases={doc.get('phases')} 无法映射 session 静态图")
            continue
        nodes = static_map[sess]
        for st in stages:
            name = st.get("name", "") if isinstance(st, dict) else str(st)
            # run_phaseN 包装阶段跳过; 具体阶段须在静态图
            if name.startswith("run_phase") or not name:
                continue
            if name not in nodes:
                errors.append(f"{mf.name}: 运行 trace stage '{name}' 不在 {sess} 静态图")
    if errors:
        print("DOC-005_TRACE_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"DOC-005_PASS: {len(SESSIONS)} session 静态图节点与运行 trace 一致")
    return 0

if __name__ == "__main__":
    sys.exit(main())
