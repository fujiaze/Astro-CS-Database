#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GOVERN-08 · 权威合并版分母与片划分构建器（判定式 A1）

用法（仓内持久、可复跑；不写 /tmp）:
    cd "/workspace/Astro CS Database"
    python3 "run/GOVERN-08/审核包-R2/分片清单/authoritative_build.py"

产出（均写在本脚本同目录 = 仓内持久路径）:
    逐份判定-权威版.csv        逐文件判定结果
    片清单-权威版.yaml         权威片划分（机器可读）
    分母实测-权威版.json       分母 / 车道 / 完成度 / 复核汇总

A1 = 判定式 G1（车道 A）为主体 + 对 G1 与 GEN-2（车道 B）56 份分歧文件的逐条裁定。
裁定表 RULINGS 全部逐条带仓内证据指针；除这 56 份外两车道判定完全一致
（634,924 行 + 325,105 行第三方），故 A1 不改动其余文件。

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

# ================================================================= 判定式 A1
#
# A1(f) = VENDOR(f)  else  OVERRIDE(f)  else  [ ¬P3(f) ∧ P1(f) ∧ P2(f) ]
#
# P1 产出面 / P2 数据形态 / P3 手写面豁免 —— 全部沿用车道 A 的判定式 G1。
#
# A1 的唯一增补是对「G1 与 GEN-2 判不同」的 56 份文件做逐条裁定（表 RULINGS）。
# 裁定所依据的统一原则：
#
#   某文件判 GENERATED，当且仅当它的**内容可由仓内运行器按固定输入重新产出**，
#   其正确性由生成器与判据保证，读它不产生新的人工语义。
#   否则判 HUMAN（人工产物），因为它是不可再生的正本、输入数据或人工裁决记录。
#
# 该原则同时解释两车道的判据：G1 的「产出面」、GEN-2 的「自述 generated_by /
# derived_from」都是「可再产出」的证据；反过来，管道输入数据表与人工决策台账
# 不可再产出，必须留在人工侧。

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
    # ⚠ 本权威版**删除** G1 的 `^lib/algorithms/photometry/data/response_curves/`
    #   这一条产出面。G1 把它列作产出面，但 G1 自己的注释写的是「仪器响应实测数据表」，
    #   而实测证据显示它是 P1 的**输入**数据表而非运行读数（见 RULINGS 中该两条）。
    #   删除后这两份改由 RULINGS 显式判 HUMAN，避免「删了面却无人管」。
]
RUN_PLANE_RE = [re.compile(p) for p in RUN_PLANES]

DATA_EXT = {"json", "jsonl", "csv", "out", "log", "tsv"}

HAND_WRITTEN = [
    r"^eng/contracts/(schemas|data|config)/",   # 合同 schema / 合同数据 / 合同配置
    r"^eng/packaging/config/",                  # 配置库（filters.json 等，自带 authority 指针）
    r"(^|/)fixtures(/|$)",                      # 门禁 fixture（人手造以触发特定分支）
    r"^eng/tools/",                             # 工具与其自述/自检样例
    r"(^|/)synthetic/scenes(/|$)",              # 合成场景定义（带中文科学意图 description）
]
HAND_WRITTEN_RE = [re.compile(p) for p in HAND_WRITTEN]

VENDOR_DIR = {"third_party", "thirdparty", "vendor", "external"}

# ------------------------------------------------------------------ 裁定表
# 每条 = (路径, 判定, 归属车道, 依据)
# 归属车道记的是「本条采纳哪一车道的答案」，两车道一致处不进本表。

_R = "响应/滤镜数据"
_E = "实验产出面"
_V = "验证运行面"
_S = "snr_design 运行审计"
_T = "锁与依赖"
_SPLIT = "；".join

RULINGS = OrderedDict()


def _rule(path, verdict, side, basis):
    RULINGS[path] = (verdict, side, basis)


# —— 甲组：G1 判 HUMAN，本权威版判 GENERATED（采纳 GEN-2）————————————

# 1) 自述「逐字转录」且转录保真有机器门兜底 ⇒ 内容可由源件再产出
_rule("eng/packaging/config/filters.json", "GENERATED", "GEN-2",
      "本件自带 transcription{source_path: lib/algorithms/photometry/data/response_curves/filters.json, "
      "mode: verbatim}；docs/engineering/CONFIG_CONTRACT.md:104 记「逐字转录…未重采样、未插值、未改数值」"
      "且机器门 TestFiltersLibrary::test_verbatim_transcription 逐条与源件比对 ⇒ 内容可再产出。G1 的 P3 "
      "^eng/packaging/config/ 把它当手写面豁免，误。")

# 2) snr_design 运行审计：带墙钟运行时事实
for _p, _n in [("audit_exp3_physical.json", 1076), ("audit_exp1245.json", None),
               ("audit_sim_validation.json", None), ("audit_mosaic_shape.json", None),
               ("audit_sp0.json", None)]:
    _rule("实验/absolute-snr/code/reverse_verify/snr_design/audit/" + _p, "GENERATED", "GEN-2",
          ("文件内带墙钟键 elapsed_s（如 audit_exp3_physical.json:%d）；实验/README.md:12 定墙钟类字段为"
           "「运行时事实…重型验证的成本证据」⇒ 运行读数。" % _n) if _n else
          "文件内带墙钟键 elapsed_s/runtime_s；实验/README.md:12 定墙钟类字段为「运行时事实」⇒ 运行读数。")
_rule("实验/absolute-snr/code/reverse_verify/snr_design/exp2_sparse_snr_reconstruction.json",
      "GENERATED", "GEN-2", "带墙钟键；实验/README.md:12 ⇒ 运行读数。")
_rule("实验/absolute-snr/code/reverse_verify/snr_design/exp3_multiframe_weight_penalty.json",
      "GENERATED", "GEN-2", "带墙钟键；实验/README.md:12 ⇒ 运行读数。")

# 3) 自述生成器脚本名
_rule("实验/shared/data/real/m16_scene_index.json", "GENERATED", "GEN-2",
      "本件自带 generated_by: [实验/shared/synthetic/m16_mask.py, m16_scene.py, "
      "exp_a6_seeing_aperture.py] 与 generated_at ⇒ 内容可由具名脚本再产出。")
_rule("eng/contracts/block_flow/stage_block_flow.json", "GENERATED", "GEN-2",
      "本件自带 generated_by: eng/tools/quality/gen_block_flow_spec.py 与 "
      "derived_from: lib/infrastructure/pipeline/module_ports.registry.json ⇒ 可再产出。"
      "G1 的 P1 未覆盖 eng/contracts/block_flow/、P3 也未命中 ⇒ 漏判。")

# 4) 实验/engineering-evidence 面（实验/README.md:8 明列的证据留档面）
#    G1 只列了该树的 4 条子路径，漏掉其余子目录。
_EV = "实验/README.md:8 定 engineering-evidence/ 为「测量结果 / 基准数值 / 质量实测 / 审计时点证据」留档面。"
for _p in ["release-01/VIS-001/l4/chunks_gc/tiles_index.json",
           "release-01/VIS-001/l4/chunks_m42/tiles_index.json",
           "compress-01/MANIFEST.sha256",
           "science/kcorr_matrix.json",
           "release-01/VIS-001/l3/gc_stretch.json",
           "release-01/VIS-001/l4/gc_stretch.json",
           "release-01/VIS-001/l4/m42_stretch.json",
           "release-01/VIS-001/l3/m42_stretch.json",
           "prerelease-v5/ISA-001/MEASUREMENTS.csv",
           "prerelease-v5/ISA-002/MEASUREMENTS.csv",
           "prerelease-v5/ISA-003/MEASUREMENTS.csv"]:
    _rule("实验/engineering-evidence/" + _p, "GENERATED", "GEN-2", _EV)

# 5) eng/tests/validation/release02 面（README:3/5/6 定为可复跑实验资产）
#    G1 的 P2 扩展名白名单没有 .txt/.sha256，把运行器捕获的 stdout/stderr 与摘要
#    漏在人工侧。GEN-2 的 O3 面把它们收进来了。
_V2 = ("eng/tests/validation/release02/README.md:3「可复跑实验资产」、:5「由 run/RELEASE-02/<name>/ 收集而来」、"
       ":6「大体积中间数组…可由脚本重新生成」⇒ 面内读数为可再产出。G1 的 P2 白名单无 .txt/.sha256 ⇒ 漏判。")
for _p in ["fix_p1_photometry_apply/qf_oracle_stderr.txt",
           "phot_verify/evidence_wiring.txt",
           "unc_propagation/oracle_output.txt",
           "fix_p2b_variance_oracle/wc_selfcheck.txt",
           "fix_p2b_variance_oracle/oracle_output.txt",
           "fix_p2b_variance_oracle/oracle_output_red.txt",
           "fix_p2b_variance_oracle/realdata_evidence.txt",
           "fix_p1_photometry_apply/oracle_output.txt",
           "fix_p1_photometry_apply/oracle_stderr.txt",
           "fix_p2a_seam_oracle/oracle_out.txt",
           "fix_p1_photometry_apply/qf_oracle_output.txt",
           "fix_p1_photometry_apply/ma_cmd.txt",
           "fix_p1_photometry_apply/p1001_cmd.txt"]:
    _rule("eng/tests/validation/release02/" + _p, "GENERATED", "GEN-2", _V2)

# 6) results 面内的 .txt / .sha256（G1 的 P2 白名单漏掉这两种载体）
_RES = ("results/ 是实验结果面（实验/README.md:7「结果与判据表（results/）」）；本件是面内读数，"
        "G1 的 P2 白名单无 .txt/.sha256 ⇒ 漏判。")
for _p in ["实验/healpix-polar/results/SNAPSHOT.sha256",
           "实验/healpix-polar/results/audit/route1/e1_leaf_area_and_scale.txt",
           "实验/healpix-polar/results/audit/route1/e2_polar_pixel_limit.txt",
           "实验/healpix-polar/results/audit/route1/e3_circumradius_scan.txt",
           "实验/healpix-polar/results/audit/route1/e4_flux_conservation.txt",
           "实验/healpix-polar/results/audit/route1/e5_projection_budgets.txt",
           "实验/healpix-polar/results/audit/route1/e6_lhuilier_vos.txt",
           "实验/m42-realdata/results/SNAPSHOT.sha256"]:
    _rule(_p, "GENERATED", "GEN-2", _RES)

# —— 乙组：GEN-2 判 GENERATED，本权威版判 HUMAN（采纳 G1 / 或改为采纳 GEN-2）——

# 7) 决定性一组：响应曲线是 P1 的**输入**数据表，不是运行读数
_RC = ("docs 证据：lib/algorithms/photometry/docs/algorithm.md:140-141 把本件列为 P1 的**输入**"
       "「滤光片透过率 T(λ)」「CCD QE 曲线 Q(λ)」（与 eng/packaging/config/filters.json 并列同表），"
       "非输出；且 eng/packaging/config/filters.json 的 transcription.source_path 指向本件 ⇒ 本件是**原件**；"
       "algorithm.md:140 记 provenance.status 当前 unverified（GAP-025）⇒ 未决科学缺口，正需人工审。"
       "G1 把该目录列作产出面，但 G1 自己的注释写的是「仪器响应实测数据表」，自相矛盾。")
_rule("lib/algorithms/photometry/data/response_curves/filters.json", "HUMAN", "GEN-2", _RC)
_rule("lib/algorithms/photometry/data/response_curves/qe_curves.json", "HUMAN", "GEN-2", _RC)

# 8) produced_by / not_produced_by 在这里是**策略声明**，不是生成自述
_rule("eng/contracts/data/config_separation_anchors.json", "HUMAN", "G1",
      "本件的 produced_by / not_produced_by(:18,:22,:72,:75,:133,:136) 声明的是「哪个配置 schema 拥有哪段字段"
      "命名空间」，not_produced_by 更是显式否认产出；本件另有 owner 与 authority(:6-9) 指向 docs 正本"
      "⇒ 人维护的锚点注册表。GEN-2 的 G2 是按键名匹配无判语义，此处为假阳性。")

# 9) 依赖锁里是人工法务/治理判断
_rule("lib/infrastructure/acr/docs/dependency-lock.json", "HUMAN", "G1",
      "每条依赖含 purpose 散文、license_spdx、compiled_into_binary / header_only / optional / platforms 等"
      "**人工法务与治理判断**；本件是 05_OPEN_SOURCE_REUSE_PLAN.md §11 的开源复用管控正本。"
      "generated_at 只是落款时间戳，不构成「内容可再产出」的证据。")

# 10) 人工决策台账不可再产出
_rule("实验/engineering-evidence/audit-2026-01/FIX_LEDGER.csv", "HUMAN", "G1",
      "本件列含 owner_decision / evidence / clause / fix_state / verified_by / verified_date / fix_note 等"
      "**人工裁决与条款引用**（例：FD-F-003、M1a-A-001 两行整行为裁决记录），无法由任何运行器再产出；"
      "与同面 tiles_index.json / MEASUREMENTS.csv / *.sha256 这类实测读数性质不同。"
      "GEN-2 的 O2 面把它按目录一刀切，属过宽。")

# 11) results/.gitignore 是手写版本控制件，不是读数载体
_rule("实验/additive-sky-seamless/code/audit_rework/results/.gitignore", "HUMAN", "G1",
      ".gitignore 是人手写的版本控制声明，不是运行读数载体；GEN-2 的 O1 面按目录命中把它判成生成物，"
      "属误报。")

# —— 丙组：G1 判 GENERATED，本权威版维持 GENERATED（采纳 G1，驳回 GEN-2）——

_rule("eng/contracts/ledgers/dead_config_keys.json", "GENERATED", "G1",
      "本件是「CHK-CONFIG-CONSUMED 显式台账」，每条记录生产代码零命中的死键并附推导位置，"
      "由源码扫描派生；eng/contracts/ledgers/ 本就是机读台账面。GEN-2 无 ledgers 面 ⇒ 漏判。")
for _p in ["dq001_codec_comparison.csv", "dq002_occupancy_mode.csv", "dq003_random_read_latency.csv",
           "dq004_drizzle_profile.csv", "dq005_writer_memory.csv", "dq006_auto_nside.csv",
           "dq007_signal_support_semantics.csv"]:
    _rule("lib/infrastructure/aio/tests/results/" + _p, "GENERATED", "G1",
          "tests/results/ 下为实测基准表（如 dq001_codec_comparison.csv 列 compress_us_median / "
          "decompress_us_median 等时延读数）⇒ 运行读数。GEN-2 的产出面只覆盖 实验/ 与 "
          "eng/tests/validation/，漏掉 lib/**/tests/results/ ⇒ 漏判。")

# ------------------------------------------------------------------ 容量
LANE_CAPACITY = 11000
TAIL_POOL = 2000


# ================================================================= 工具

def sh(cmd):
    return subprocess.run(cmd, cwd=REPO, shell=True,
                          capture_output=True, text=True, check=True).stdout


def tracked(scope):
    out = sh("git -c core.quotepath=false ls-files -- " + " ".join(scope))
    return [l for l in out.split("\n") if l]


def line_counts(paths):
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
    """→ (tier, reason). tier ∈ HUMAN / GENERATED / VENDOR"""
    ext = ext_of(path)
    parts = set(path.split("/")[:-1])
    if parts & VENDOR_DIR:
        return "VENDOR", "P0 vendor 目录: " + "/".join(sorted(parts & VENDOR_DIR))
    if path in RULINGS:
        verdict, side, basis = RULINGS[path]
        return verdict, "A1 裁定（采 %s）: %s" % (side, basis)
    for r in HAND_WRITTEN_RE:
        if r.search(path):
            return "HUMAN", "P3 手写面豁免: " + r.pattern
    if ext in DATA_EXT:
        for r in RUN_PLANE_RE:
            if r.search(path):
                return "GENERATED", "P1 产出面 %s ∧ P2 数据形态 .%s" % (r.pattern, ext)
    return "HUMAN", "默认保留（非产出面或非数据形态）"


def layer_of(path):
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
        if len(p) >= 3 and "." not in p[1]:
            return "ENG-" + p[1], "eng/" + p[1]
        return "ENG-ROOT", "eng/(根)"
    if p[0] == "实验":
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

    human = [r for r in rows if r["tier"] == "HUMAN"]
    gen = [r for r in rows if r["tier"] == "GENERATED"]
    vend = [r for r in rows if r["tier"] == "VENDOR"]
    H_F, H_L = len(human), sum(r["lines"] for r in human)

    # ---- SRS-1 层内 LPT 均衡装箱（不跨层）--------------------------------
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
            basis="SRS-1 层内 LPT 均衡装箱（n=ceil(层行数/%d)，严格不跨层）" % LANE_CAPACITY,
            members=[m["path"] for m in mem]))

    file2slice = {m: s["id"] for s in slice_objs for m in s["members"]}
    assert len(file2slice) == H_F, "片成员去重后数 %d ≠ 人工产物 %d" % (len(file2slice), H_F)

    # ---- 历史交付件 → 新片映射（沿用两车道一致还原的 HIST）----------------
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

    # ---- 交叉复核：119 / 925,309 -----------------------------------------
    json_files = [r for r in rows if r["ext"] == "json"]
    json_lines = sum(r["lines"] for r in json_files)
    top119 = sorted(json_files, key=lambda r: -r["lines"])[:119]
    claim = 925309
    falsified = {
        "claim_files": 119, "claim_lines": claim,
        "all_json_files": len(json_files), "all_json_lines": json_lines,
        "claim_exceeds_all_json_lines_by": claim - json_lines,
        "top119_json_lines": sum(r["lines"] for r in top119),
        "verdict": ("证伪：925,309 > 全部 %d 份 .json 的行数总和 %d，任何「取 119 份 JSON」的口径都不可能得到该行数"
                    % (len(json_files), json_lines)) if claim > json_lines else "未被证伪",
    }
    gen_ge2000 = [r for r in gen if r["lines"] >= 2000]

    summary = OrderedDict()
    summary["head"] = sh("git rev-parse HEAD").strip()
    summary["criteria"] = "A1 = 判定式 G1 主体 + 对 56 份 G1/GEN-2 分歧文件的逐条裁定"
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
    summary["rulings"] = [
        OrderedDict(path=p, verdict=v[0], side=v[1], basis=v[2],
                    lines=line_of.get(p, 0))
        for p, v in RULINGS.items()]
    summary["falsify_119"] = falsified
    summary["ledger_47"] = {
        "generated_ge_2000_lines_files": len(gen_ge2000),
        "generated_ge_2000_lines": sum(r["lines"] for r in gen_ge2000),
        "ledger_says": "轮次台账.md:13 记 47 份 / 554,209 行"}
    summary["historical"] = {
        "deliverables": len(DELIVERABLE_MAP),
        "distinct_manifests": len(HIST),
        "duplicate_manifests": len(DELIVERABLE_MAP) - len(HIST),
        "new_slices_covered": sorted(covered_slices),
        "new_slices_covered_count": len(covered_slices),
        "human_files_covered": len(covered_files),
        "human_lines_covered": covered_lines}
    summary["completion"] = {
        "rule": "既往产出不计入遍数（轮次台账.md:46）；判定规则 D1-1..D1-5",
        "completed_passes": 0, "passes_required": 10,
        "slices_covered": len(covered_slices), "slices_total": len(slice_objs),
        "lines_covered": covered_lines, "lines_total": H_L,
        "files_covered": len(covered_files), "files_total": H_F}

    # ---- 落盘 ----------------------------------------------------------
    with open(os.path.join(HERE, "分母实测-权威版.json"), "w", encoding="utf-8") as fh:
        json.dump(summary, fh, ensure_ascii=False, indent=2)

    with open(os.path.join(HERE, "逐份判定-权威版.csv"), "w", encoding="utf-8") as fh:
        fh.write("path,lines,ext,layer,tier,slice,reason\n")
        for r in rows:
            fh.write("%s,%d,%s,%s,%s,%s,%s\n" % (
                r["path"], r["lines"], r["ext"], r["layer"], r["tier"],
                file2slice.get(r["path"], ""), r["reason"]))

    with open(os.path.join(HERE, "片清单-权威版.yaml"), "w", encoding="utf-8") as fh:
        fh.write("# GOVERN-08 R2 权威版片划分 —— 机器可读\n")
        fh.write("# 本件为权威版，取代 片清单-G1.yaml（G1/92片口径复核后）与 片清单.yaml（GEN-2/74片）。\n")
        fh.write("# 两份旧件保留未删改，各自加注了取代说明。\n")
        fh.write("# 判定式: A1（= 判定式 G1 主体 + 56 份分歧文件逐条裁定）\n")
        fh.write("# 切片规则: SRS-1 层内 LPT 均衡装箱，严格不跨层；SRS-2 尾域合并\n")
        fh.write("# 生成命令: python3 \"run/GOVERN-08/审核包-R2/分片清单/authoritative_build.py\"\n")
        fh.write("# HEAD: %s\n" % summary["head"])
        fh.write("# 分母: 人工产物 %d 份 / %d 行（已排除生成物与第三方 vendor）\n" % (H_F, H_L))
        fh.write("元信息:\n")
        fh.write("  判定式: \"A1（= G1 主体 + 56 份 G1/GEN-2 分歧文件逐条裁定）\"\n")
        fh.write("  判定式正文: \"00-分母与片划分-权威版.md\"\n")
        fh.write("  切片规则: \"SRS-1 层内 LPT 均衡装箱，严格不跨层；SRS-2 尾域合并\"\n")
        fh.write("  切片规则正文: \"00-分母与片划分-权威版.md\"\n")
        fh.write("  取代: [\"分片清单/片清单-G1.yaml\", \"分片清单/片清单.yaml\"]\n")
        fh.write("  旧件处置: \"保留，未删改；各自文件头加注取代说明\"\n")
        fh.write("  车道: \"判定式 G1 车道的 92 片经 A1 裁定后为 %d 片（不跨层原则不变，片数因分母变化而变）\"\n"
                 % len(slice_objs))
        fh.write("  落盘位置: \"仓内 run/GOVERN-08/审核包-R2/分片清单/（非 /tmp）\"\n")
        fh.write("分母:\n  人工产物:\n    份数: %d\n    行数: %d\n" % (H_F, H_L))
        fh.write("  按根目录:\n")
        for root, v in summary["denominator_human"]["by_root"].items():
            fh.write("    %s: {份数: %d, 行数: %d}\n" % (root, v["files"], v["lines"]))
        fh.write("  生成物: {份数: %d, 行数: %d}\n" % (len(gen), sum(r["lines"] for r in gen)))
        fh.write("  第三方vendor: {份数: %d, 行数: %d}\n" % (len(vend), sum(r["lines"] for r in vend)))
        fh.write("  受审主体: {份数: %d, 行数: %d}\n" % (total_files, total_lines))
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
