#!/usr/bin/env python3
"""TRACE-001 六层追溯检查器 (SCI→ALG→ARCH→API→CODE→TEST)。
规则:
 R1 表头与模板逐字一致;
 R2 claim_id 格式 ^(SCI|ALG|ARCH|API|CODE|TEST)-[A-Z0-9]{2,8}-\\d{3}$ 且唯一;
 R3 链完整性: 行层级的所有上层字段必须非空(SCI 只需 science, ALG 需 science+algorithm, ...TEST 全链);
 R4 引用存在: 文档/测试/源码路径必须存在; source_symbol/api_symbol 需 path::symbol 且文件内可见该符号;
 R5 oracle_id 格式 ORC-<DOM>-NNN (非空时);
 R6 域覆盖: 每个域名必须同时有 SCI 行与 TEST 行 (断链=缺端点);
 R7 status ∈ {ACTIVE, RETIRED}; RETIRED 行豁免 R3/R4 但仍计入唯一性。
用法: python3 eng/tools/check_traceability.py <csv...>   exit 0 = PASS。

退役 (RETIRED, 2026-09-16, 负责人裁决) —— 本脚本对应的 CI 检查项 TRACEABILITY-CODE 已退役：
  1. 新 CI 规范 docs/ci/01_CHECKS.md 不含任何追溯要求（TRACEABILITY / 追溯 零命中）；
  2. ASTROCS_DESIGN.md 与 ENGINEERING_SPEC.md 同样零命中「追溯」；
  3. 本检查的唯一默认输入 artifacts/evidence/prerelease-v5/tables/TRACEABILITY.csv 位于**构建产物目录**，
     从来不是权威落位；该表已随 artifacts/ 按负责人裁决删除（commit b1290525「不归档、不保留」）；
  4. 表内容锚在 docs/VERSIONING.md 的版本串匹配上，而新设计 §12 明令版本信息下线 ⇒ 口径被新世代废止；
  5. 内容未丢：git show b1290525^:artifacts/evidence/prerelease-v5/tables/TRACEABILITY.csv 可取回。
  保留可复跑性：传入显式 CSV 仍按 R1-R7 全量校验；无参调用不再回退被删快照 ——
  打印 RETIRED 说明并 exit 2（fail-closed，不伪装绿）。登记见 docs/ci/01_CHECKS.md §2 退役记录。
"""
import csv, os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# 历史默认表（已随 artifacts/ 删除；退役后本脚本不再持有默认输入路径常量，避免悬空引用）。
HEADER = ["claim_id", "science_doc", "science_anchor", "algorithm_doc", "algorithm_anchor",
          "architecture_doc", "api_symbol", "source_symbol", "test_id", "oracle_id",
          "unit", "precision_contract", "status"]
LAYERS = ["SCI", "ALG", "ARCH", "API", "CODE", "TEST"]
# 每层要求非空的列(累积): SCI 行只需 science 两列; TEST 行需全链
CHAIN_COLS = {
    "SCI": ["science_doc", "science_anchor"],
    "ALG": ["science_doc", "science_anchor", "algorithm_doc", "algorithm_anchor"],
    "ARCH": ["science_doc", "science_anchor", "algorithm_doc", "algorithm_anchor", "architecture_doc"],
    "API": ["science_doc", "science_anchor", "algorithm_doc", "algorithm_anchor", "architecture_doc", "api_symbol"],
    "CODE": ["science_doc", "science_anchor", "algorithm_doc", "algorithm_anchor", "architecture_doc", "api_symbol", "source_symbol"],
    "TEST": ["science_doc", "science_anchor", "algorithm_doc", "algorithm_anchor", "architecture_doc", "api_symbol", "source_symbol", "test_id"],
}
CLAIM_RE = re.compile(r"^(SCI|ALG|ARCH|API|CODE|TEST)-([A-Z0-9]{2,8})-\d{3}$")
# oracle_id 两种合法形态: ORC-<DOM>-NNN (TRACE-001 主形态) 或 SYN-NNN (V5 独立合成 Oracle 实际稳定 ID, 见 08 §1/§SYN-00x)
ORC_RE = re.compile(r"^(?:ORC-[A-Z0-9]{2,8}-\d{3}|SYN-\d{3})$")

def symbol_exists(ref):
    """path::symbol => 文件存在且符号可见; 纯路径 => 存在。"""
    if "::" in ref:
        path, sym = ref.split("::", 1)
        full = os.path.join(REPO, path)
        if not os.path.isfile(full):
            return False, f"文件不存在: {path}"
        text = open(full, encoding="utf-8", errors="replace").read()
        if f"def {sym}" in text or f"function {sym}" in text or re.search(rf"\b{re.escape(sym)}\b", text):
            return True, ""
        return False, f"符号 {sym} 不在 {path}"
    full = os.path.join(REPO, ref)
    return os.path.isfile(full), ("" if os.path.isfile(full) else f"路径不存在: {ref}")

def check_table(path, errors):
    with open(path, encoding="utf-8", newline="") as f:
        rows = list(csv.reader(f))
    if not rows or rows[0] != HEADER:
        errors.append(f"{os.path.basename(path)}: R1 表头与模板不一致")
        return {}
    reg = {}
    for ln, r in enumerate(rows[1:], 2):
        if not any(x.strip() for x in r):
            continue
        if len(r) != len(HEADER):
            errors.append(f"{os.path.basename(path)}:{ln}: 列数 {len(r)} != {len(HEADER)}")
            continue
        row = dict(zip(HEADER, r))
        cid = row["claim_id"].strip()
        m = CLAIM_RE.match(cid)
        if not m:
            errors.append(f"{os.path.basename(path)}:{ln}: R2 claim_id 非法: {cid!r}")
            continue
        if cid in reg:
            errors.append(f"{os.path.basename(path)}:{ln}: R2 claim_id 重复: {cid}")
            continue
        reg[cid] = (ln, row)
        layer = m.group(1)
        if row["status"] not in ("ACTIVE", "RETIRED"):
            errors.append(f"{os.path.basename(path)}:{ln}: R7 status 非法: {row['status']!r}")
        if row["status"] == "RETIRED":
            continue
        for col in CHAIN_COLS[layer]:
            if not row[col].strip():
                errors.append(f"{os.path.basename(path)}:{ln}: R3 断链: {layer} 行 {col} 为空 ({cid})")
        for col in ("science_doc", "algorithm_doc", "architecture_doc"):
            v = row[col].strip()
            if v and not os.path.isfile(os.path.join(REPO, v.split("#")[0])):
                errors.append(f"{os.path.basename(path)}:{ln}: R4 文档不存在: {v} ({cid})")
        for col in ("api_symbol", "source_symbol"):
            v = row[col].strip()
            if v:
                ok, why = symbol_exists(v)
                if not ok:
                    errors.append(f"{os.path.basename(path)}:{ln}: R4 引用不存在: {why} ({cid})")
        t = row["test_id"].strip()
        if t and not symbol_exists(t)[0]:
            errors.append(f"{os.path.basename(path)}:{ln}: R4 测试不存在: {t} ({cid})")
        o = row["oracle_id"].strip()
        if o and not ORC_RE.match(o):
            errors.append(f"{os.path.basename(path)}:{ln}: R5 oracle_id 非法: {o!r} ({cid})")
        for col in ("unit", "precision_contract"):
            if not row[col].strip():
                errors.append(f"{os.path.basename(path)}:{ln}: {col} 为空 ({cid})")
    # R6 域覆盖: 每域必须有 SCI 端点与 TEST 端点
    domains = {}
    for cid, (ln, row) in reg.items():
        dom = cid.split("-")[1]
        domains.setdefault(dom, set()).add(cid.split("-")[0])
    for dom, layers in sorted(domains.items()):
        if "SCI" not in layers or "TEST" not in layers:
            errors.append(f"R6 域 {dom} 断链: 现有层 {sorted(layers)}, 需含 SCI 与 TEST 端点")
    return reg

def main():
    if len(sys.argv) <= 1:
        print("TRACEABILITY_RETIRED: 本检查项 TRACEABILITY-CODE 已于 2026-09-16 按负责人裁决退役；"
              "默认输入 artifacts/evidence/prerelease-v5/tables/TRACEABILITY.csv 已随 artifacts/ 删除（b1290525）。"
              "可复跑用法: python3 eng/tools/check_traceability.py <claims.csv>（登记见 docs/ci/01_CHECKS.md）。")
        return 2
    errors = []
    tables = sys.argv[1:]
    total = 0
    for path in tables:
        reg = check_table(path, errors)
        total += len(reg)
    if errors:
        print(f"TRACEABILITY_FAIL ({len(errors)}):")
        for e in errors[:25]:
            print(" ", e)
        return 1
    print(f"TRACEABILITY_PASS claims={total} tables={[os.path.basename(p) for p in tables]}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
