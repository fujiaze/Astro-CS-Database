#!/usr/bin/env python3
"""从权威注册表派生「阶段块流规格」eng/contracts/block_flow/stage_block_flow.json。

唯一事实源：lib/infrastructure/pipeline/module_ports.registry.json（冻结端口绑定表）。
本脚本**不发明**任何块名：块名 = 注册表端口名。

生命周期与阶段边界是本规格的**新增语义**（注册表不含），按以下可判定规则派生：
  - 消费者数 >= 2 ⇒ STAGE（必须活到阶段末，供后续节点复用）
  - 消费者数 == 0 且是阶段终产物 ⇒ EXTERNAL_OUT
  - 消费者数 == 0 且非终产物 ⇒ STAGE（终态产物，随阶段发布）
  - 消费者数 == 1 ⇒ SHORT（最后一次消费即销毁）
  - 无生产者 ⇒ EXTERNAL_IN
人工复核后可覆盖，但覆盖会被 check_block_flow_spec.py 的 R7 复核。
"""
import io
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
REG = os.path.join(REPO, "lib/infrastructure/pipeline/module_ports.registry.json")
OUT = os.path.join(REPO, "eng/contracts/block_flow/stage_block_flow.json")

PHASE_TO_STAGE = {"phase1": "normalize", "phase2": "mosaic", "phase3": "export"}
# 阶段终产物（外部输出）——按各阶段 CLI 合同：normalize→fits，mosaic→mosaic，export→verified
# 各阶段对外发布的磁盘产品（最高设计 §8.1 三命令合同）：
#   normalize → fits（标准化帧）
#   mosaic    → mosaic（马赛克树）
#   export    → fits（投影 FITS 产品）+ verified（校验报告）
STAGE_TERMINALS = {"normalize": {"fits"}, "mosaic": {"mosaic"},
                   "export": {"fits", "verified"}}


def main():
    reg = json.load(io.open(REG, encoding="utf-8"))
    nodes = []
    for m in reg["modules"]:
        for op in m["operations"]:
            ins = [p["name"] for p in op["ports"] if p["direction"] == "input"]
            outs = [p["name"] for p in op["ports"] if p["direction"] == "output"]
            nodes.append({
                "module_id": m["module_id"],
                "stage": PHASE_TO_STAGE[m["phase"]],
                "operation": op["operation"],
                "entry": op["entry"],
                "reads": ins,
                "writes": outs,
            })
    nodes.sort(key=lambda n: (n["stage"], reg_index(reg, n["module_id"])))

    # 块级统计（**按阶段作用域**：阶段间只通过磁盘产品交换，同名块在不同阶段是不同块，
    # 见 AGENTS §6「不串三阶段」与最高设计 §8.1）
    produced = {}
    consumers = {}
    for n in nodes:
        st = n["stage"]
        for b in n["writes"]:
            produced.setdefault((st, b), []).append(n["module_id"])
        for b in n["reads"]:
            consumers.setdefault((st, b), []).append(n["module_id"])

    blocks = []
    for stage in ("normalize", "mosaic", "export"):
        stage_blocks = set()
        for n in nodes:
            if n["stage"] != stage:
                continue
            stage_blocks.update(n["reads"])
            stage_blocks.update(n["writes"])
        for b in sorted(stage_blocks):
            key = (stage, b)
            nc = len(consumers.get(key, []))
            np_ = len(produced.get(key, []))
            if np_ == 0:
                lc = "EXTERNAL_IN"          # 本阶段无生产者 ⇒ 来自上游磁盘产品
            elif b in STAGE_TERMINALS[stage]:
                lc = "EXTERNAL_OUT"         # 阶段终产物：必须发布（也可被本阶段内部消费）
            elif nc >= 2:
                lc = "STAGE"                # 多消费者 ⇒ 必须活到阶段末
            elif nc == 0:
                lc = "STAGE"                # 终态中间产物：随阶段持久化
            else:
                lc = "SHORT"                # 单消费者 ⇒ 最后一次消费即销毁
            blocks.append({
                "stage": stage, "block": b, "lifecycle": lc,
                "produced_by": produced.get(key, []), "consumed_by": consumers.get(key, []),
            })

    doc = {
        "schema": "astrocs.stage-block-flow/v1",
        "derived_from": "lib/infrastructure/pipeline/module_ports.registry.json",
        "generated_by": "eng/tools/quality/gen_block_flow_spec.py",
        "note": "块名 = 注册表端口名（不发明新名）。生命周期与阶段边界为本规格新增语义，规则见脚本 docstring。",
        "nodes": nodes,
        "blocks": blocks,
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    io.open(OUT, "w", encoding="utf-8").write(
        json.dumps(doc, ensure_ascii=False, indent=2) + "\n")
    print("wrote %s: %d nodes, %d block declarations" % (OUT, len(nodes), len(blocks)))
    return 0


def reg_index(reg, module_id):
    for i, m in enumerate(reg["modules"]):
        if m["module_id"] == module_id:
            return i
    return 999


if __name__ == "__main__":
    sys.exit(main())
