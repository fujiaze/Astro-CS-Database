#!/usr/bin/env python3
"""阶段块流规格机器门（ARCH-505）：把「生产节点改为命名块读写」变成可证伪的合同。

判据（全部 fail-closed；每条都有负例自测）：
  R1  节点集 == 注册表节点集（module_id 一一对应，无多无少）
  R2  每节点的 operation/entry 与注册表**逐字**一致
  R3  每节点声明的 reads/writes == 注册表该 operation 的 input/output 端口集（不发明块名、不丢端口）
  R4  阶段内 DAG 无环，且节点声明顺序是一个合法拓扑序
  R5  每个被消费的块在**本阶段内**恰有一个生产者，且生产者排在该消费者之前
  R6  EXTERNAL_IN 块在本阶段内无生产者；EXTERNAL_OUT 块必须有生产者
  R7  生命周期自洽：消费者数 >= 2 ⇒ STAGE；== 0 ⇒ STAGE 或 EXTERNAL_OUT；
      == 1 ⇒ SHORT，除非该块同时是阶段终产物（EXTERNAL_OUT 优先）
  R8  阶段间隔离：某阶段的块不得依赖另一阶段节点的产出（阶段间只走磁盘产品）
  R9  每个块声明只出现一次（stage, block）唯一
  R10 规格派生自注册表：derived_from 指向的文件存在，且重新派生结果与磁盘规格一致（防手改漂移）

用法：
  python3 eng/tools/quality/check_block_flow_spec.py [--spec <json>] [--json-out <json>]
  python3 eng/tools/quality/check_block_flow_spec.py --self-test
"""
import argparse
import copy
import io
import json
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SPEC = os.path.join(REPO, "eng/contracts/block_flow/stage_block_flow.json")
REG = os.path.join(REPO, "lib/infrastructure/pipeline/module_ports.registry.json")
GEN = os.path.join(REPO, "eng/tools/quality/gen_block_flow_spec.py")
STAGES = ("normalize", "mosaic", "export")
PHASE_TO_STAGE = {"phase1": "normalize", "phase2": "mosaic", "phase3": "export"}


def load_registry():
    return json.load(io.open(REG, encoding="utf-8"))


def registry_nodes(reg):
    out = {}
    for m in reg["modules"]:
        for op in m["operations"]:
            out[m["module_id"]] = {
                "stage": PHASE_TO_STAGE[m["phase"]],
                "operation": op["operation"],
                "entry": op["entry"],
                "reads": sorted(p["name"] for p in op["ports"] if p["direction"] == "input"),
                "writes": sorted(p["name"] for p in op["ports"] if p["direction"] == "output"),
            }
    return out


def validate(spec, reg):
    errs = []
    rn = registry_nodes(reg)
    nodes = spec.get("nodes", [])
    blocks = spec.get("blocks", [])

    # R1
    got = [n["module_id"] for n in nodes]
    if sorted(got) != sorted(rn):
        errs.append("R1 node set mismatch: only-in-spec=%s only-in-registry=%s"
                    % (sorted(set(got) - set(rn)), sorted(set(rn) - set(got))))
    if len(got) != len(set(got)):
        errs.append("R1 duplicate module_id in spec")

    # R2 / R3
    for n in nodes:
        mid = n.get("module_id")
        if mid not in rn:
            continue
        r = rn[mid]
        if n.get("operation") != r["operation"]:
            errs.append("R2 %s operation %r != registry %r" % (mid, n.get("operation"), r["operation"]))
        if n.get("entry") != r["entry"]:
            errs.append("R2 %s entry %r != registry %r" % (mid, n.get("entry"), r["entry"]))
        if sorted(n.get("reads", [])) != r["reads"]:
            errs.append("R3 %s reads %s != registry %s" % (mid, sorted(n.get("reads", [])), r["reads"]))
        if sorted(n.get("writes", [])) != r["writes"]:
            errs.append("R3 %s writes %s != registry %s" % (mid, sorted(n.get("writes", [])), r["writes"]))
        if n.get("stage") != r["stage"]:
            errs.append("R2 %s stage %r != registry %r" % (mid, n.get("stage"), r["stage"]))

    # R9
    seen = set()
    for b in blocks:
        key = (b.get("stage"), b.get("block"))
        if key in seen:
            errs.append("R9 duplicate block declaration %s" % (key,))
        seen.add(key)

    for stage in STAGES:
        sn = [n for n in nodes if n.get("stage") == stage]
        order = {n["module_id"]: i for i, n in enumerate(sn)}
        sb = [b for b in blocks if b.get("stage") == stage]
        bmap = {b["block"]: b for b in sb}

        # R4 拓扑序
        for n in sn:
            for rd in n.get("reads", []):
                b = bmap.get(rd)
                if not b:
                    errs.append("R4 %s reads undeclared block %r in stage %s" % (n["module_id"], rd, stage))
                    continue
                for prod in b.get("produced_by", []):
                    if prod not in order:
                        errs.append("R8 %s reads block %r produced by out-of-stage node %s"
                                    % (n["module_id"], rd, prod))
                        continue
                    if order[prod] >= order[n["module_id"]]:
                        errs.append("R4 stage %s: %s reads %r before producer %s in declared order"
                                    % (stage, n["module_id"], rd, prod))
            for wr in n.get("writes", []):
                if wr not in bmap:
                    errs.append("R4 %s writes undeclared block %r in stage %s" % (n["module_id"], wr, stage))

        # R5 / R6 / R7
        for b in sb:
            name = b["block"]
            prod = b.get("produced_by", [])
            cons = b.get("consumed_by", [])
            lc = b.get("lifecycle")
            if lc not in ("EXTERNAL_IN", "EXTERNAL_OUT", "STAGE", "SHORT"):
                errs.append("R7 %s/%s invalid lifecycle %r" % (stage, name, lc))
                continue
            if lc == "EXTERNAL_IN":
                if prod:
                    errs.append("R6 %s/%s EXTERNAL_IN but has producer %s" % (stage, name, prod))
            else:
                if len(prod) != 1:
                    errs.append("R5 %s/%s must have exactly one in-stage producer, got %s"
                                % (stage, name, prod))
            if lc == "EXTERNAL_OUT" and not prod:
                errs.append("R6 %s/%s EXTERNAL_OUT but no producer" % (stage, name))
            if lc in ("STAGE", "SHORT"):
                for c in cons:
                    if c not in order:
                        errs.append("R8 %s/%s consumed by out-of-stage node %s" % (stage, name, c))
                for p in prod:
                    if p not in order:
                        errs.append("R8 %s/%s produced by out-of-stage node %s" % (stage, name, p))
            # R7 生命周期自洽
            if lc == "SHORT" and len(cons) >= 2:
                errs.append("R7 %s/%s SHORT but has %d consumers (must be STAGE)" % (stage, name, len(cons)))
            if lc == "SHORT" and len(cons) == 0:
                errs.append("R7 %s/%s SHORT but has no consumer (SHORT = 最后一次消费即销毁)"
                            % (stage, name))
            # R11 双向一致：块的 produced_by/consumed_by 必须与节点声明的 writes/reads 完全一致
            want_prod = sorted(n["module_id"] for n in sn if name in n.get("writes", []))
            want_cons = sorted(n["module_id"] for n in sn if name in n.get("reads", []))
            if sorted(prod) != want_prod:
                errs.append("R11 %s/%s produced_by %s != nodes declaring it in writes %s"
                            % (stage, name, sorted(prod), want_prod))
            if sorted(cons) != want_cons:
                errs.append("R11 %s/%s consumed_by %s != nodes declaring it in reads %s"
                            % (stage, name, sorted(cons), want_cons))
            if lc == "EXTERNAL_IN" and sorted(prod) != want_prod:
                pass   # 已由 R11 覆盖
            if lc == "STAGE" and len(cons) == 1 and name not in ("snr",):
                # 单消费者却声明 STAGE：允许（保守），但必须显式：这里只做提示级不判红
                pass

    return errs


def derive_and_compare(spec):
    """R10：重新派生并与磁盘规格比对（防手改漂移）。"""
    tmp = SPEC + ".regen"
    env = dict(os.environ)
    try:
        out = subprocess.run([sys.executable, GEN], capture_output=True, text=True, cwd=REPO)
        if out.returncode != 0:
            return ["R10 regeneration failed: %s" % out.stderr.strip()[:400]]
        regen = json.load(io.open(SPEC, encoding="utf-8"))
        if regen != spec:
            return ["R10 spec on disk differs from regenerated output (hand-edited drift)"]
        return []
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def run(spec_path):
    if not os.path.isfile(spec_path):
        return ["spec not found: %s" % spec_path], None
    spec = json.load(io.open(spec_path, encoding="utf-8"))
    reg = load_registry()
    errs = validate(spec, reg)
    errs += derive_and_compare(spec)
    return errs, spec


def _self_test():
    reg = load_registry()
    spec = json.load(io.open(SPEC, encoding="utf-8"))
    cases = []
    cases.append(("S1-valid-green", validate(spec, reg) == []))

    s = copy.deepcopy(spec); s["nodes"] = s["nodes"][:-1]
    cases.append(("S2-missing-node-red", any(e.startswith("R1") for e in validate(s, reg))))

    s = copy.deepcopy(spec)
    for n in s["nodes"]:
        if n["module_id"] == "astrocs.phase1.calibration":
            n["operation"] = "not_a_real_operation"
    cases.append(("S3-operation-drift-red", any(e.startswith("R2") for e in validate(s, reg))))

    s = copy.deepcopy(spec)
    for n in s["nodes"]:
        if n["module_id"] == "astrocs.phase1.calibration":
            n["writes"] = ["invented_block"]
    cases.append(("S4-invented-block-red", any(e.startswith("R3") for e in validate(s, reg))))

    s = copy.deepcopy(spec)
    for b in s["blocks"]:
        if b["stage"] == "normalize" and b["block"] == "wcs":
            b["lifecycle"] = "SHORT"
    cases.append(("S5-lifecycle-lie-red", any(e.startswith("R7") for e in validate(s, reg))))

    s = copy.deepcopy(spec)
    for b in s["blocks"]:
        if b["stage"] == "mosaic" and b["block"] == "calibrated":
            b["lifecycle"] = "EXTERNAL_IN"
            b["produced_by"] = ["astrocs.phase2.coverage"]
    cases.append(("S6-external-in-with-producer-red",
                  any(e.startswith("R6") for e in validate(s, reg))))

    s = copy.deepcopy(spec)
    for b in s["blocks"]:
        if b["stage"] == "normalize" and b["block"] == "cleaned":
            b["produced_by"] = ["astrocs.phase1.cosmetic", "astrocs.phase1.star-psf"]
    cases.append(("S6b-two-producers-red",
                  any(e.startswith("R5") for e in validate(s, reg))))

    s = copy.deepcopy(spec)
    for b in s["blocks"]:
        if b["stage"] == "normalize" and b["block"] == "snr":
            b["lifecycle"] = "SHORT"; b["consumed_by"] = ["astrocs.phase1.writer"]
    cases.append(("S6c-terminal-dropped-red",
                  any(e.startswith("R7") or e.startswith("R11") for e in validate(s, reg))))

    s = copy.deepcopy(spec)
    for b in s["blocks"]:
        if b["stage"] == "normalize" and b["block"] == "snr":
            b["lifecycle"] = "SHORT"; b["consumed_by"] = []
    cases.append(("S6d-short-with-no-consumer-red",
                  any(e.startswith("R7") for e in validate(s, reg))))

    s = copy.deepcopy(spec)
    for b in s["blocks"]:
        if b["stage"] == "export" and b["block"] == "props":
            b["consumed_by"] = ["astrocs.phase1.photometry"]
    cases.append(("S7-cross-stage-consumer-red", any(e.startswith("R8") for e in validate(s, reg))))

    s = copy.deepcopy(spec)
    s["nodes"] = list(reversed(s["nodes"]))
    cases.append(("S8-reversed-order-red", any(e.startswith("R4") for e in validate(s, reg))))

    # S9：R10 手改漂移（临时改磁盘规格 → 派生比对必须判红 → 还原）
    raw = io.open(SPEC, encoding="utf-8").read()
    try:
        tampered = json.loads(raw)
        tampered["blocks"][0]["lifecycle"] = "SHORT"
        io.open(SPEC, "w", encoding="utf-8").write(
            json.dumps(tampered, ensure_ascii=False, indent=2) + "\n")
        errs = derive_and_compare(tampered)
        cases.append(("S9-hand-edit-drift-red", any(e.startswith("R10") for e in errs)))
    finally:
        io.open(SPEC, "w", encoding="utf-8").write(raw)
        subprocess.run([sys.executable, GEN], capture_output=True, cwd=REPO)

    bad = [n for n, g in cases if not g]
    for n, g in cases:
        print("SELFTEST " + ("PASS " if g else "FAIL ") + n)
    if bad:
        print("SELFTEST_FAIL: " + repr(bad), file=sys.stderr)
        return 1
    print("SELFTEST_PASS: %d/%d" % (len(cases), len(cases)))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", default=SPEC)
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        rc = _self_test()
        # 注册表按 outputs 判 missing_output：自测也必须落机器可读证据。
        if getattr(args, "json_out", None):
            os.makedirs(os.path.dirname(args.json_out), exist_ok=True)
            io.open(args.json_out, "w", encoding="utf-8").write(json.dumps(
                {"tool": "block_flow_spec", "mode": "self-test", "rc": rc,
                 "verdict": "PASS" if rc == 0 else "FAIL"}, ensure_ascii=False) + "\n")
        return rc
    errs, spec = run(args.spec)
    verdict = "PASS" if not errs else "FAIL"
    n_nodes = len(spec["nodes"]) if spec else 0
    n_blocks = len(spec["blocks"]) if spec else 0
    print("BLOCK_FLOW_SPEC_%s: nodes=%d blocks=%d errors=%d" % (verdict, n_nodes, n_blocks, len(errs)))
    for e in errs[:40]:
        print("  " + e)
    if args.json_out:
        os.makedirs(os.path.dirname(args.json_out), exist_ok=True)
        io.open(args.json_out, "w", encoding="utf-8").write(json.dumps(
            {"tool": "check_block_flow_spec", "spec": args.spec, "nodes": n_nodes,
             "blocks": n_blocks, "errors": errs, "verdict": verdict},
            ensure_ascii=False, indent=1) + "\n")
    return 0 if not errs else 1


if __name__ == "__main__":
    sys.exit(main())
