#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""层1-E：把 49 个包身份按代际分成 14 组派工清单（组间含对照重叠），
输出 _evidence/packs/dispatch_groups.json（组 -> 身份 -> 摘要文件路径）"""
import os, re, json
P = "/workspace/Astro CS Database/设计大纲/_evidence/packs"
DIG = os.path.join(P, "digest")
ids = sorted(f[:-3] for f in os.listdir(DIG) if f.endswith(".md"))
def dpath(ident):
    cand = re.sub(r"[^\w.\-]+", "__", ident)[:90]
    for f in ids:
        if f == cand or f.startswith(cand[:40]):
            return os.path.join("_evidence/packs/digest", f + ".md")
    return None
GROUPS = [
 ("G01", "上游前史与初代开发包（CLI_Core v1.1 / engineering v1.0-v1.3）",
  ["AstroCS_CLI_Core_Development_Pack", "_new_pack_v1.1", "AstroCS_CLI_Core_Development_Pack_2026-07-24_v1",
   "AstroCS_Delivery_20260729", "engineering_archive_v1", "engineering_v1.2", "engineering_v1.3"]),
 ("G02", "权威开发包 v2.0 与 engineering_authoritative",
  ["AstroCS_Authoritative_Development_Pack_v2.0", "AstroCS_Authoritative_Development_Pack_2026-07",
   "engineering_authoritative", "AstroCS_Stage1_Wiki_Freeze_2026-07-30"]),
 ("G03", "Stage1 HISS 交付包与 Agent 包",
  ["AstroCS_Stage1_HISS_Delivery", "AstroCS_Stage1_HISS_Delivery_2026-07-31",
   "AstroCS_Stage1_HISS_Agent_Package_2026-07-31", "_agent_package"]),
 ("G04", "ACR 聚焦控制包 V1-V4（四代）",
  ["acr", "ACR_FOCUSED_CONTROL_PACKAGE", "ACR_FOCUSED_CONTROL_PACKAGE_V2",
   "ACR_FOCUSED_CONTROL_PACKAGE_V3", "ACR_FOCUSED_CONTROL_PACKAGE_V4"]),
 ("G05", "用户上传审计包 V2/V3 与 REAUDIT_V3 三件套",
  ["AstroCS_MAIN_AUDIT_SUPPLEMENT_V2_20260826", "AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827",
   "AstroCS_CP0", "REAUDIT_V3/v3_audit", "REAUDIT_V3/v3_cp0", "REAUDIT_V3/v3_reaudit",
   "REAUDIT_V3", "AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z"]),
 ("G06", "V4 CPU_ADAPTIVE 预发布控制包",
  ["AstroCS_MAIN_PRERELEASE_CONTROL_V4_CPU_ADAPTIVE_20260828", "CONTROL_V4"]),
 ("G07", "V5 单 CLI/AMD64 发布控制包与 V5 时代审核件",
  ["AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64", "REAUDIT_V4/v4_reaudit", "REAUDIT_V4",
   "prerelease_v5/AUDIT_REVIEW", "prerelease_v5/audit_src", "prerelease_v5/review",
   "AUDIT_PACKAGE_587fe0e341a7", "AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_ALPHA_20260828"]),
 ("G08", "V5 审核包与 V6 系统重构包",
  ["AstroCS_V5_AUDIT_REVIEW_20260830", "AstroCS_V6_SYSTEM_REFACTOR_ALPHA_CONTROL_20260830", "CONTROL_V6"]),
 ("G09", "V6.1 返工控制包",
  ["AstroCS_V6_1_REWORK_CONTROL_20260831"]),
 ("G10", "V7 模块化重筑基控制包（FINAL/FINAL3）",
  ["AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3",
   "AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3",
   "AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_20260902_FINAL"]),
 ("G11", "2026-09-02 归档容器（legacy 工程控制 v1.3-to-v6.1）与其收纳关系",
  ["archive/2026-09-02_legacy_v1.3-to-v6.1", "2026-09-02_legacy", "legacy"]),
 ("G12", "V8 与 V8.1 CI 控制包（含 superseded 副本与执行留痕）",
  ["AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905",
   "2026-09-09_superseded_V8.1_CI_CONTROL_20260905"]),
 ("G13", "宪章对齐控制包 V1 与控制包规范/模板",
  ["AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909", "cprun-v4-pack-template",
   "CONTROL_V1", "AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1"]),
 ("G14", "发布救援控制包 V3（最新一代）",
  ["AstroCS_RELEASE_RESCUE_CONTROL_V3_20260912"]),
]
known = set(ids)
out = []
for gid, title, wants in GROUPS:
    matched = []
    for wt in wants:
        for i in ids:
            ii = i.replace("__", "/")
            if wt.replace("__", "/") in ii or ii.startswith(wt[:30].replace("__", "/")) or wt[:24] in ii:
                if i not in [m[0] for m in matched]: matched.append((i, dpath(i)))
    out.append({"group": gid, "title": title, "identities": [m[0] for m in matched],
                "digests": [m[1] for m in matched if m[1]]})
assigned = {i for g in out for i in g["identities"]}
out.append({"group": "G99", "title": "未被上述任何组匹配到的身份（需人工判定是否属于控制包）",
            "identities": sorted(known - assigned),
            "digests": [dpath(i) for i in sorted(known - assigned)]})
json.dump(out, open(os.path.join(P, "dispatch_groups.json"), "w", encoding="utf-8"), ensure_ascii=False, indent=1)
for g in out:
    print("%s %-30s -> %d 身份: %s" % (g["group"], g["title"][:30], len(g["identities"]), ", ".join(x[:34] for x in g["identities"][:6])))
print("总身份 %d / 已分组 %d" % (len(known), len(assigned)))
