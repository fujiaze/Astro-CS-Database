#!/usr/bin/env python3
"""排查台账数据库 CLI（数据不入库：run/<排查>/ledger-raw.json；本工具入库）

流水线：MIMO 挖疑 → MIMO 复核真伪 → 确证入台账 → fixer 修理
→ reviewer 确认 → close 关闭条目 → 前台推送。

fix_state 状态机：OPEN → IN_FIX → FIXED_REVIEWED → CLOSED。
条目只增不改编号、不删除（历史可追溯）；CLOSED 仅为状态标记。

用法：
  ledger.py stats
  ledger.py add --src S31 --sev 红 --surf ② --loc "file.cpp:100" --desc "..." [--fix "..."] [--note "..."] [--verify 确证]
  ledger.py update P-160 --verify 确证 --fix-state IN_FIX --note "FIX-04 派单"
  ledger.py close P-160 --note "reviewer 批准 review-FIX-04"
  ledger.py list --open | --fix-state OPEN | --sev 红 | --surf ② | --since 2026-09-25 --until 2026-09-28 | --grep 关键词 | --p P-160
  ledger.py show P-160
  ledger.py --self-test
"""

import argparse, json, os, re, sys, tempfile, datetime

VERIFY_LEVELS = ("确证", "部分确证", "待确证")
FIX_STATES = ("OPEN", "IN_FIX", "FIXED_REVIEWED", "CLOSED")
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
P_RE = re.compile(r"P-(\d+)")


def _today():
    return datetime.date.today().isoformat()


def _fail(msg, code=2):
    print(f"LEDGER_ERROR: {msg}", file=sys.stderr)
    sys.exit(code)


def load(db):
    """读库；损坏即 fail-closed（rc=2），不静默降级。"""
    if not os.path.exists(db):
        return {"entries": []}
    try:
        data = json.load(open(db, encoding="utf-8"))
    except Exception as e:
        _fail(f"台账 JSON 不可解析（fail-closed）：{db}: {e}")
    if not isinstance(data, dict) or not isinstance(data.get("entries"), list):
        _fail(f"台账结构不符（缺 entries 数组）：{db}")
    return data


def save(db, data):
    tmp = db + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
    os.replace(tmp, db)


def _next_p(data):
    mx = max((int(P_RE.fullmatch(e["p"]).group(1)) for e in data["entries"] if P_RE.fullmatch(e.get("p", ""))), default=0)
    return f"P-{mx + 1:03d}"


def cmd_add(data, a):
    if a.verify and a.verify not in VERIFY_LEVELS:
        _fail(f"verify 必须是 {VERIFY_LEVELS} 之一")
    e = {"p": _next_p(data)}
    for k in ("src", "sev", "surf", "loc", "desc", "fix", "note"):
        if getattr(a, k):
            e[k] = getattr(a, k)
    e["verify"] = a.verify or "待确证"
    e["fix_state"] = "OPEN"
    e["added"] = _today()
    data["entries"].append(e)
    print(f"added {e['p']}（共 {len(data['entries'])} 条）")


def _find(data, p):
    for e in data["entries"]:
        if e.get("p") == p:
            return e
    _fail(f"条目不存在：{p}")


def cmd_update(data, a):
    e = _find(data, a.p)
    for k in ("sev", "surf", "loc", "desc", "fix", "note"):
        if getattr(a, k, None) is not None:
            e[k] = getattr(a, k)
    if a.verify:
        if a.verify not in VERIFY_LEVELS:
            _fail(f"verify 必须是 {VERIFY_LEVELS} 之一")
        e["verify"] = a.verify
    if a.fix_state:
        if a.fix_state not in FIX_STATES:
            _fail(f"fix_state 必须是 {FIX_STATES} 之一")
        e["fix_state"] = a.fix_state
    e["updated"] = _today()
    print(f"updated {a.p}: fix_state={e.get('fix_state')}, verify={e.get('verify')}")


def cmd_close(data, a):
    e = _find(data, a.p)
    if e.get("fix_state") == "CLOSED":
        print(f"{a.p} 已是 CLOSED（不重复关闭）")
        return
    e["fix_state"] = "CLOSED"
    e["closed"] = _today()
    if a.note:
        e["closed_note"] = a.note
    e["updated"] = _today()
    print(f"closed {a.p}")


def _match(e, a):
    if a.p and e.get("p") != a.p:
        return False
    if a.fix_state and e.get("fix_state", "OPEN") != a.fix_state:
        return False
    if a.open and e.get("fix_state", "OPEN") == "CLOSED":
        return False
    if a.sev and e.get("sev") != a.sev:
        return False
    if a.surf and e.get("surf") != a.surf:
        return False
    if a.verify and e.get("verify") != a.verify:
        return False
    if a.since or a.until:
        dates = [d for d in (e.get("added"), e.get("updated"), e.get("closed")) if d and DATE_RE.fullmatch(d)]
        if a.since and (not dates or not any(d >= a.since for d in dates)):
            return False
        if a.until and (not dates or not any(d <= a.until for d in dates)):
            return False
    if a.grep:
        hay = " ".join(str(e.get(k, "")) for k in ("p", "src", "loc", "desc", "fix", "note"))
        if a.grep.lower() not in hay.lower():
            return False
    return True


def cmd_list(data, a):
    rows = [e for e in data["entries"] if _match(e, a)]
    if a.format == "json":
        print(json.dumps(rows, ensure_ascii=False, indent=1))
        return
    for e in rows:
        body = (e.get("loc", "") + "  " + e.get("desc", "")).replace("\n", " ")[:96]
        print(f"{e['p']:6} {e.get('fix_state','OPEN'):17} {e.get('verify','—'):5} {e.get('sev','—'):3} {e.get('surf','—'):2} {body}")
    print(f"— {len(rows)} / {len(data['entries'])} 条")


def cmd_stats(data):
    es = data["entries"]
    def c(key, default="—"):
        out = {}
        for e in es:
            k = e.get(key, default)
            out[k] = out.get(k, 0) + 1
        return dict(sorted(out.items(), key=lambda kv: -kv[1]))
    print(f"总计 {len(es)} 条")
    print("fix_state:", json.dumps(c("fix_state", "OPEN"), ensure_ascii=False))
    print("verify   :", json.dumps(c("verify"), ensure_ascii=False))
    print("sev      :", json.dumps(c("sev"), ensure_ascii=False))
    print("面       :", json.dumps(c("surf"), ensure_ascii=False))


def self_test():
    """能红能绿：临时库全操作断言；损坏库必须 fail-closed。"""
    n = [0]
    def T(name, cond):
        if not cond:
            _fail(f"self-test 失败：{name}")
        n[0] += 1
    with tempfile.TemporaryDirectory() as d:
        db = os.path.join(d, "t.json")
        json.dump({"entries": [
            {"p": "P-001", "sev": "红", "verify": "确证", "loc": "a.md:1", "desc": "x", "fix_state": "OPEN"},
            {"p": "P-002", "sev": "黄", "verify": "待确证", "loc": "b.md:2", "desc": "y", "fix_state": "CLOSED"},
        ]}, open(db, "w", encoding="utf-8"))
        data = load(db)
        T("load", len(data["entries"]) == 2)
        class A: pass
        a = A()
        for k in ("p", "src", "sev", "surf", "loc", "desc", "fix", "note", "verify", "fix_state", "since", "until", "grep", "format", "open"):
            setattr(a, k, None)
        a.src, a.sev, a.surf, a.loc, a.desc, a.fix, a.note, a.verify = "S99", "红", "①", "c.md:3", "z", "f", "n", "确证"
        cmd_add(data, a)
        T("add 递增编号", data["entries"][-1]["p"] == "P-003")
        T("add 状态 OPEN", data["entries"][-1]["fix_state"] == "OPEN")
        u = A()
        for k in ("p", "sev", "surf", "loc", "desc", "fix", "note", "verify", "fix_state"):
            setattr(u, k, None)
        u.p, u.note, u.fix_state = "P-003", "FIX-99 派单", "IN_FIX"
        cmd_update(data, u)
        T("update", data["entries"][-1]["fix_state"] == "IN_FIX")
        cc = A(); cc.p, cc.note = "P-003", "reviewer 批准"
        cmd_close(data, cc)
        T("close", data["entries"][-1]["fix_state"] == "CLOSED" and data["entries"][-1].get("closed"))
        cmd_close(data, cc)  # 幂等
        q = A()
        for k in ("p", "fix_state", "sev", "surf", "verify", "since", "until", "grep", "format", "open"):
            setattr(q, k, None)
        q.open = True
        T("list --open 排除 CLOSED", all(e.get("fix_state") != "CLOSED" for e in data["entries"] if _match(e, q)))
        q.open, q.grep = None, "z"
        T("grep 命中", len([e for e in data["entries"] if _match(e, q)]) == 1)
        q.grep, q.since, q.until = None, "2000-01-01", "2099-12-31"
        # 无日期的遗留条目（P-001/P-002）正确地被日期窗排除，仅 P-003 命中
        T("日期窗（无日期条目被排除）", len([e for e in data["entries"] if _match(e, q)]) == 1)
        q.since, q.until = None, None
        bad = os.path.join(d, "bad.json")
        open(bad, "w", encoding="utf-8").write("{not json")
        try:
            load(bad)
            _fail("self-test 失败：损坏库未 fail-closed")
        except SystemExit:
            n[0] += 1
    print(f"SELF-TEST PASS: {n[0]}/{n[0]}")


def main():
    ap = argparse.ArgumentParser(description="排查台账数据库 CLI")
    ap.add_argument("--db", default=os.path.join("run", "全仓-01", "ledger-raw.json"))
    ap.add_argument("--self-test", action="store_true")
    sub = ap.add_subparsers(dest="cmd")
    sub.add_parser("stats")
    s_add = sub.add_parser("add")
    for k in ("src", "sev", "surf", "loc", "desc", "fix", "note", "verify"):
        s_add.add_argument(f"--{k}")
    s_up = sub.add_parser("update"); s_up.add_argument("p")
    for k in ("sev", "surf", "loc", "desc", "fix", "note", "verify", "fix-state"):
        s_up.add_argument(f"--{k}")
    s_cl = sub.add_parser("close"); s_cl.add_argument("p"); s_cl.add_argument("--note")
    s_ls = sub.add_parser("list")
    s_ls.add_argument("--open", action="store_true")
    for k in ("fix-state", "sev", "surf", "verify", "since", "until", "grep", "p"):
        s_ls.add_argument(f"--{k}")
    s_ls.add_argument("--format", choices=("table", "json"), default="table")
    s_sh = sub.add_parser("show"); s_sh.add_argument("p")
    a = ap.parse_args()
    if a.self_test:
        self_test(); return
    if not a.cmd:
        ap.print_help(); return
    data = load(a.db)
    if a.cmd == "stats":
        cmd_stats(data)
    elif a.cmd == "add":
        cmd_add(data, a); save(a.db, data)
    elif a.cmd == "update":
        cmd_update(data, a); save(a.db, data)
    elif a.cmd == "close":
        cmd_close(data, a); save(a.db, data)
    elif a.cmd == "list":
        cmd_list(data, a)
    elif a.cmd == "show":
        cmd_show(data, a)


if __name__ == "__main__":
    try:
        main()
    except BrokenPipeError:
        # 管道截断（如 | head）非错误：静默收口
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
        sys.exit(0)
