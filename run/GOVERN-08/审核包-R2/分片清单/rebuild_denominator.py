#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GOVERN-08 · 重建收敛分母与片划分（负责人裁定 ①② 执行件）

用法（仓内持久、可复跑；不写 /tmp）:
    cd "/workspace/Astro CS Database"
    python3 "run/GOVERN-08/审核包-R2/分片清单/rebuild_denominator.py"

产出（均写在本脚本同目录 = 仓内持久路径）:
    逐份判定.csv    逐文件判定结果（3,554 行）
    片清单.yaml      逐片成员与行数（机器可读）
    分母实测.json    分母 / 车道 / 完成度实测汇总
    历史交付件覆盖.csv  14 份轮1v3 交付件 → 新片映射

纪律:
  - 零 git 写（不 add/commit/checkout/reset/stash）
  - 不编译、不跑测试、不跑实验脚本；只做 git ls-files + wc -l 级计数
  - 中文路径一律 git -c core.quotepath=false
"""

import json
import os
import re
import subprocess
from collections import OrderedDict, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
SCOPE = ["docs", "实验", "lib", "eng"]

# ================================================================= 判定式 G1
#
# 负责人裁定 ①：排除生成物，按人工产物重算分母。
# 裁定理由：生成物没有人工语义、不构成阅读对象；其正确性由生成器与判据保证。
#
# 判定式 G1（生成物）:
#     G1(f)  ⟺  P1(f) ∧ P2(f) ∧ ¬P3(f)
#
#   P1 产出面：路径命中「运行记录面」正则闭集 RUN_PLANES
#              （这些目录由运行器/门禁写出，是「跑出来的读数」）
#   P2 数据形态：扩展名 ∈ DATA_EXT（纯读数载体；不含 .md/.py/.cpp/.h/.sh/.yaml 源码与人读件）
#   P3 手写面豁免：路径命中 HAND_WRITTEN（合同 schema / fixture / 配置库 / 工具自述 / 场景定义）
#
# 关键区分（本判定式的立身之本）:
#   「机器校验」(被检查器读) ≠ 「机器生成」(被运行器写出)。
#   例：eng/contracts/data/clause_registry.json 被 eng/tests/.../oracle.py 校验，
#       但它由人手写、承载合同语义 ⇒ P3 豁免 ⇒ HUMAN。
#   反例：eng/contracts/ledgers/ 下机读台账由生成器写出 ⇒ P1 命中 ⇒ GENERATED。
#
# P3 的存在正是为了不把「登记面自己写的声明」当成判定式：
#   docs/DOCUMENT_INDEX.yaml:24「机器源与台账不进 docs，落在 eng/contracts/ 与 artifacts/evidence/」
#   docs/engineering/DOCUMENT_GOVERNANCE.md:24「docs/ 下只有人读文档」
# —— 这两句是**声明**，不是谓词；据此把 eng/contracts/** 一刀切成生成物是错的。

RUN_PLANES = [
    r"(^|/)results(/|$)",                       # 实验/验收结果面
    r"(^|/)gates(/|$)",                         # 门禁运行记录
    r"(^|/)probes(/|$)",                        # 探针采样
    r"(^|/)timeseries(/|$)",                    # 时序采样
    r"(^|/)worker_balance(/|$)",                # worker 均衡采样
    r"(^|/)real16_bitwise_cmp(/|$)",
    r"^eng/tests/validation/",                  # release02 验证运行面
    r"^eng/contracts/ledgers/",                 # 机读台账（死键/缺口等）
    r"^实验/engineering-evidence/l2_performance/",
    r"^实验/engineering-evidence/compress-01/",
    r"^实验/engineering-evidence/v6/",
    r"^实验/engineering-evidence/v19r7-quality/",
    r"^lib/algorithms/photometry/data/response_curves/",  # 仪器响应实测数据表
]
RUN_PLANE_RE = [re.compile(p) for p in RUN_PLANES]

DATA_EXT = {"json", "jsonl", "csv", "out", "log", "tsv"}

# P3 手写面豁免（优先级高于 P1）
HAND_WRITTEN = [
    r"^eng/contracts/(schemas|data|config)/",   # 合同 schema / 合同数据 / 合同配置
    r"^eng/packaging/config/",                  # 配置库（filters.json 等，自带 authority 指针）
    r"(^|/)fixtures(/|$)",                      # 门禁 fixture（人手造以触发特定分支）
    r"^eng/tools/",                             # 工具与其自述/自检样例
    r"(^|/)synthetic/scenes(/|$)",              # 合成场景定义（带中文科学意图 description）
]
HAND_WRITTEN_RE = [re.compile(p) for p in HAND_WRITTEN]

# S3 第三方 vendor（单列，不并入生成物）
VENDOR_DIR = {"third_party", "thirdparty", "vendor", "external"}

# 单车道一轮实测容量（台账 run/GOVERN-08/审核包/审稿/轮次台账.md:15-16 反推值）
LANE_CAPACITY = 11000

# 尾域合并阈值（SRS-2）：层行数低于此值并入同根 TAIL 层，避免碎片车道
TAIL_POOL = 2000

# ================================================================= 工具


def sh(cmd):
    return subprocess.run(cmd, cwd=REPO, shell=True,
                          capture_output=True, text=True, check=True).stdout


def tracked(scope):
    out = sh("git -c core.quotepath=false ls-files -- " + " ".join(scope))
    return [l for l in out.split("\n") if l]


def line_counts(paths):
    """逐份行数。xargs 分批后对每批 '总计' 求和（只看最后一批会漏算）。"""
    per, B = {}, 400
    for i in range(0, len(paths), B):
        batch = paths[i:i + B]
        quoted = " ".join("'" + p.replace("'", "'\\''") + "'" for p in batch)
        for line in sh("wc -l -- " + quoted).split("\n"):
            m = re.match(r"^\s*(\d+)\s+(.*)$", line)
            if m and m.group(2) not in ("总计", "total"):
                per[m.group(2)] = int(m.group(1))
    return per


def ext_of(path):
    name = os.path.basename(path)
    return name.rsplit(".", 1)[1].lower() if "." in name else ""


def classify(path):
    """→ (tier, reason)。tier ∈ HUMAN / GENERATED / VENDOR"""
    ext = ext_of(path)
    parts = set(path.split("/")[:-1])
    if parts & VENDOR_DIR:
        return "VENDOR", "P0 vendor 目录: " + "/".join(sorted(parts & VENDOR_DIR))
    for r in HAND_WRITTEN_RE:
        if r.search(path):
            return "HUMAN", "P3 手写面豁免: " + r.pattern
    if ext in DATA_EXT:
        for r in RUN_PLANE_RE:
            if r.search(path):
                return "GENERATED", "P1 产出面 %s ∧ P2 数据形态 .%s" % (r.pattern, ext)
    return "HUMAN", "默认保留（非产出面或非数据形态）"


def layer_of(path):
    """抗漂移分层锚：片身份挂在目录分层这条结构轴上，不挂在总行数上。"""
    p = path.split("/")
    if p[0] == "docs":
        if len(p) > 2 and p[1] in ("science", "engineering", "detail"):
            return "DOC-" + p[1][:3].upper(), "docs/" + p[1]
        return "DOC-ROOT", "docs/(根)"
    if p[0] == "lib":
        if len(p) >= 3 and p[1] in ("algorithms", "infrastructure", "shared"):
            return "%s-%s" % (p[1][:3].upper(), p[2]), "lib/%s/%s" % (p[1], p[2])
        return "LIB-OTH", "lib/(其他)"
    if p[0] == "eng":
        # 只在「eng/<子目录>」成立一层；eng/README.md 这类根级件归 ENG-ROOT
        if len(p) >= 3 and "." not in p[1]:
            return "ENG-" + p[1], "eng/" + p[1]
        return "ENG-ROOT", "eng/(根)"
    if p[0] == "实验":
        # 只有「实验/<单元目录>」成立一层；实验/裁决台账.md 这类根级件归 EXP-ROOT
        if len(p) >= 3 and "." not in p[1]:
            return "EXP-" + p[1], "实验/" + p[1]
        return "EXP-ROOT", "实验/(根)"
    return "OTHER", p[0]


def yaml_quote(s):
    return '"' + str(s).replace("\\", "\\\\").replace('"', '\\"') + '"'


def main():
    files = tracked(SCOPE)
    per = line_counts(files)

    rows = []
    for f in files:
        tier, reason = classify(f)
        code, lname = layer_of(f)
        rows.append(OrderedDict(path=f, lines=per.get(f, 0), ext=ext_of(f),
                                layer=code, layer_name=lname, tier=tier, reason=reason))

    by_tier = defaultdict(lambda: [0, 0])
    for r in rows:
        by_tier[r["tier"]][0] += 1
        by_tier[r["tier"]][1] += r["lines"]
    total_files, total_lines = len(files), sum(r["lines"] for r in rows)
    assert sum(v[0] for v in by_tier.values()) == total_files
    assert sum(v[1] for v in by_tier.values()) == total_lines

    # ---- 分母（人工产物 = 主体 - 生成物）-------------------------------
    human = [r for r in rows if r["tier"] == "HUMAN"]
    gen = [r for r in rows if r["tier"] == "GENERATED"]
    vend = [r for r in rows if r["tier"] == "VENDOR"]
    H_F, H_L = len(human), sum(r["lines"] for r in human)

    # ---- 抗漂移切片 SRS-1 ---------------------------------------------
    # 每层片数 n = ceil(层行数 / C)，再把层内文件按 LPT（最长优先）均衡分到 n 片。
    # 不跨层。C = 实测单车道容量。
    # 抗漂移点：片数只依赖【各层自己的行数】，不依赖全仓总量；
    #           某层变大只增加该层的 n，其它层的片号与成员完全不动。
    # 均衡点：ceil 保证片数最少，LPT 消除「尾片只剩几十行」的碎片。
    def pack_layer(mem, cap):
        L = sum(m["lines"] for m in mem)
        n = max(1, -(-L // cap))
        bins = [[] for _ in range(n)]
        loads = [0] * n
        for m in sorted(mem, key=lambda x: (-x["lines"], x["path"])):
            i = min(range(n), key=lambda j: (loads[j], j))
            bins[i].append(m)
            loads[i] += m["lines"]
        return [(b, sum(x["lines"] for x in b)) for b in bins if b]

    by_layer = defaultdict(list)
    for r in human:
        by_layer[r["layer"]].append(r)

    # SRS-2 尾域合并：层行数 < TAIL_POOL 的层并入同根的 TAIL-<root>，
    # 消除「一条车道只读 23 行」的碎片。合并只在跨过 TAIL_POOL 阈值时才发生，
    # 故不破坏抗漂移（主干层的片号与成员完全不动）。
    pooled, layers, lname = {}, {}, {}
    for code in sorted(by_layer):
        mem = by_layer[code]
        L = sum(m["lines"] for m in mem)
        root = mem[0]["path"].split("/")[0]
        if L < TAIL_POOL:
            tcode = "TAIL-" + {"docs": "DOC", "lib": "LIB",
                               "eng": "ENG", "实验": "EXP"}.get(root, "X")
            pooled.setdefault(tcode, []).extend(mem)
        else:
            layers[code] = mem
            lname[code] = mem[0]["layer_name"]
    ROOT_CN = {"docs": "docs", "lib": "lib", "eng": "eng", "实验": "实验"}
    for code, mem in pooled.items():
        if mem:
            layers[code] = mem
            lname[code] = "尾域合并（%s/ 下层行数 < %d 的小层）" % (
                ROOT_CN.get(mem[0]["path"].split("/")[0], "?"), TAIL_POOL)

    slices = []
    for code in sorted(layers):
        for mem, lines in pack_layer(layers[code], LANE_CAPACITY):
            slices.append((code, mem, lines))

    per_layer_seq = defaultdict(int)
    slice_objs = []
    for code, mem, lines in slices:
        per_layer_seq[code] += 1
        sid = "%s-%03d" % (code, per_layer_seq[code])
        oversize = any(m["lines"] > LANE_CAPACITY for m in mem)
        slice_objs.append(OrderedDict(
            id=sid, layer=code, layer_name=lname.get(code, code),
            files=len(mem), target_lines=LANE_CAPACITY, actual_lines=lines,
            oversize=oversize,
            basis="SRS-1 层内 LPT 均衡装箱（n=ceil(层行数/%d)，不跨层）" % LANE_CAPACITY,
            members=[m["path"] for m in mem]))

    file2slice = {m: s["id"] for s in slice_objs for m in s["members"]}

    # ---- 历史交付件 → 新片映射 ----------------------------------------
    HIST = OrderedDict([
        ("s000", ["lib/infrastructure/scheduler/src/module_adapters.cpp"]),
        ("s001", ["lib/algorithms/coverage/tests/synthetic_gate.cpp"]),
        ("s002", ["lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp",
                  "docs/engineering/UNRESOLVED_REGISTER.md"]),
        ("s003", ["docs/science/DATA_SEMANTICS.md",
                  "lib/infrastructure/gaia_xpsd_client/src/gaia_client.c",
                  "lib/algorithms/coverage/src/upm.cpp"]),
        ("s004", ["lib/algorithms/coverage/src/rejection.cpp",
                  "lib/infrastructure/aio/src/hips/aio_hips_writer.cpp",
                  "lib/infrastructure/cli/commands.cpp"]),
        ("s005", ["lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
                  "lib/infrastructure/acr/scheduler/dispatcher.cpp",
                  "lib/algorithms/star_detection/src/sdet_api.cpp",
                  "lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp"]),
        ("s006", ["lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp",
                  "lib/infrastructure/hips_browser/healpix_browser_qt/core/gl_renderer.cpp",
                  "docs/engineering/PUBLIC_API.md",
                  "lib/algorithms/coverage/src/sky_plane.cpp",
                  "lib/algorithms/coverage/tools/stage2.cpp"]),
        ("s007", ["实验/absolute-snr/results/b7_absolute_snr_recon.json",
                  "实验/absolute-snr/results/b7_absolute_snr_recon_seed20260922.json",
                  "lib/infrastructure/pipeline/orchestrator/cpp/tests/test_orchestrator_cli.cpp",
                  "lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp",
                  "eng/tests/unit/CMakeLists.txt"]),
    ])
    DELIVERABLE_MAP = [("轮1v3-片%02d" % i, k) for i, k in
                       zip(range(1, 9), ["s000", "s001", "s002", "s003", "s004", "s005", "s006", "s007"])] \
        + [("轮1v3-片%02d" % i, k) for i, k in
           zip(range(9, 15), ["s000", "s001", "s002", "s003", "s004", "s005"])]

    tier_of = {r["path"]: r["tier"] for r in rows}
    line_of = {r["path"]: r["lines"] for r in rows}

    # 历史片内按新判定式重判：其中有多少是生成物
    hist_rows, covered_slices, covered_lines, covered_files = [], set(), 0, set()
    for manifest, mem in HIST.items():
        gen_here = [m for m in mem if tier_of.get(m) == "GENERATED"]
        hum_here = [m for m in mem if tier_of.get(m) == "HUMAN"]
        slices_here = sorted({file2slice[m] for m in mem if m in file2slice})
        hist_rows.append(OrderedDict(
            manifest=manifest, members=len(mem),
            human=len(hum_here), generated=len(gen_here),
            human_lines=sum(line_of.get(m, 0) for m in hum_here),
            generated_lines=sum(line_of.get(m, 0) for m in gen_here),
            new_slices=slices_here))
        for m in mem:
            if tier_of.get(m) == "HUMAN" and m in file2slice:
                covered_slices.add(file2slice[m])
                covered_lines += line_of.get(m, 0)
                covered_files.add(m)

    summary = OrderedDict()
    summary["head"] = sh("git rev-parse HEAD").strip()
    summary["scope"] = SCOPE
    summary["corpus"] = {"files": total_files, "lines": total_lines}
    summary["tiers"] = {k: {"files": v[0], "lines": v[1]} for k, v in sorted(by_tier.items())}
    summary["denominator_human"] = {
        "files": H_F, "lines": H_L,
        "by_root": {root: {"files": sum(1 for r in human if r["path"].split("/")[0] == root),
                           "lines": sum(r["lines"] for r in human if r["path"].split("/")[0] == root)}
                    for root in SCOPE}}
    summary["lanes"] = {
        "lane_capacity_lines": LANE_CAPACITY,
        "passes": 10,
        "lanes_per_pass": len(slice_objs),
        "lanes_total_10pass": len(slice_objs) * 10,
        "ceil_by_lines": -(-H_L // LANE_CAPACITY)}
    summary["slices"] = {
        "count": len(slice_objs),
        "layers": len({s["layer"] for s in slice_objs}),
        "max_actual": max(s["actual_lines"] for s in slice_objs),
        "min_actual": min(s["actual_lines"] for s in slice_objs),
        "oversize": [s["id"] for s in slice_objs if s["oversize"]]}
    summary["historical"] = {
        "deliverables": len(DELIVERABLE_MAP),
        "distinct_manifests": len(HIST),
        "duplicate_manifests": len(DELIVERABLE_MAP) - len(HIST),
        "dup_detail": [OrderedDict(deliverable=d, manifest=m) for d, m in DELIVERABLE_MAP[8:]],
        "new_slices_covered": sorted(covered_slices),
        "new_slices_covered_count": len(covered_slices),
        "human_files_covered": len(covered_files),
        "human_lines_covered": covered_lines}
    summary["completion"] = {
        "rule": "既往产出不计入遍数（台账 轮次台账.md:46）",
        "completed_passes": 0, "passes_required": 10,
        "slices_covered": len(covered_slices), "slices_total": len(slice_objs),
        "lines_covered": covered_lines, "lines_total": H_L,
        "files_covered": len(covered_files), "files_total": H_F}

    with open(os.path.join(HERE, "分母实测.json"), "w", encoding="utf-8") as fh:
        json.dump(summary, fh, ensure_ascii=False, indent=2)

    with open(os.path.join(HERE, "逐份判定.csv"), "w", encoding="utf-8") as fh:
        fh.write("path,lines,ext,layer,tier,slice,reason\n")
        for r in rows:
            fh.write("%s,%d,%s,%s,%s,%s,%s\n" % (
                r["path"], r["lines"], r["ext"], r["layer"], r["tier"],
                file2slice.get(r["path"], ""), r["reason"]))

    with open(os.path.join(HERE, "历史交付件覆盖.csv"), "w", encoding="utf-8") as fh:
        fh.write("deliverable,manifest,new_slices\n")
        for d, m in DELIVERABLE_MAP:
            r = next(x for x in hist_rows if x["manifest"] == m)
            fh.write("%s,%s,%s\n" % (d, m, ";".join(r["new_slices"])))

    # 产出文件名不与并行车道撞车：AGENTS.md §7「后到者不回滚覆盖」。
    # 并行车道已于 11:21 占用「片清单.yaml」（GEN-2 判定式，74 片）。
    # 本车道成果另存 G1 命名，前台裁定后再决定采哪一份。
    with open(os.path.join(HERE, "片清单-G1.yaml"), "w", encoding="utf-8") as fh:
        fh.write("# GOVERN-08 收敛片划分 v1 —— 仓内持久清单（不得写在 /tmp）\n")
        fh.write("# 生成命令: python3 \"run/GOVERN-08/审核包-R2/分片清单/rebuild_denominator.py\"\n")
        fh.write("# HEAD: %s\n" % summary["head"])
        fh.write("# 分母: 人工产物 %d 份 / %d 行（已排除生成物与第三方 vendor）\n" % (H_F, H_L))
        fh.write("# 划分规则 SRS-1: 层内贪心装箱，不跨层，单片上限 = 实测单车道容量 %d 行\n" % LANE_CAPACITY)
        fh.write("分母:\n  人工产物:\n    份数: %d\n    行数: %d\n" % (H_F, H_L))
        fh.write("  按根目录:\n")
        for root, v in summary["denominator_human"]["by_root"].items():
            fh.write("    %s: {份数: %d, 行数: %d}\n" % (root, v["files"], v["lines"]))
        fh.write("  生成物: {份数: %d, 行数: %d}\n" % (len(gen), sum(r['lines'] for r in gen)))
        fh.write("  第三方vendor: {份数: %d, 行数: %d}\n" % (len(vend), sum(r['lines'] for r in vend)))
        fh.write("\n车道:\n  单车道容量行: %d\n  一遍车道数: %d\n  十遍车道数: %d\n"
                 % (LANE_CAPACITY, len(slice_objs), len(slice_objs) * 10))
        fh.write("\n片清单:\n")
        for s in slice_objs:
            fh.write("  - 片号: %s\n" % s["id"])
            fh.write("    层: %s\n" % yaml_quote(s["layer_name"]))
            fh.write("    成员份数: %d\n" % s["files"])
            fh.write("    目标行数: %d\n" % s["target_lines"])
            fh.write("    实际行数: %d\n" % s["actual_lines"])
            fh.write("    超容量: %s\n" % ("true" if s["oversize"] else "false"))
            fh.write("    划片依据: %s\n" % yaml_quote(s["basis"]))
            fh.write("    成员文件:\n")
            for m in s["members"]:
                fh.write("      - %s\n" % yaml_quote(m))

    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
