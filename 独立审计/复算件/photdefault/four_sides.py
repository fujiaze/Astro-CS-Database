"""AUDIT-06 第②层复算：测光空间增益开关 `photometry.fit.spatial_gain_order` 四侧取值钉表。

只读仓库文本（不构建、不跑任何 eng/** 检查器、不 import 仓库 Python）。
每侧输出一行：侧 | 路径:行 | 逐字取值 | 与文档判定（默认 0）是否一致。
"""
import io
import os
import re
import sys

REPO = r"F:\Astro dev\Astro CS Normalization Database"
sys.stdout.reconfigure(encoding="utf-8")

TARGETS = [
    ("①文档/正本(默认应为0)", "实验/photometric-magnitude/docs/p1-spatial-gain.md",
     r"默认保持"),
    ("①文档/正本(推荐阶)", "实验/photometric-magnitude/docs/p1-spatial-gain.md",
     r"默认 order 1"),
    ("①文档/正本(二阶不可用)", "实验/photometric-magnitude/docs/p1-spatial-gain.md",
     r"分半不可复现"),
    ("①文档/定案摘要", "实验/photometric-magnitude/results/REVERSE_VERIFY_CANON.md",
     r"前置条件未满足"),
    ("①科学层(只规定低阶)", "docs/science/PHOTOMETRY.md", r"④ \*\*拟合"),
    ("①验收层", "ACCEPTANCE_SPEC.md", r"apply photometry"),
    ("②算法层请求默认", "lib/algorithms/photometry/cpp/src/frame_photometry_fit.h",
     r"int spatial_gain_order"),
    ("②算法层参数默认", "lib/algorithms/photometry/cpp/src/spatial_gain.h",
     r"int order_requested"),
    ("③调度层兜底常量", "lib/infrastructure/scheduler/src/module_adapters.cpp",
     r"kDefaultSpatialGainOrder"),
    ("③调度层读取表达式", "lib/infrastructure/scheduler/src/module_adapters.cpp",
     r"fit_cfg\.contains\(\"spatial_gain_order\"\)"),
    ("③通道使能门", "lib/infrastructure/scheduler/src/module_adapters.cpp",
     r"const bool fit_enabled"),
    ("③像素施加分支", "lib/infrastructure/scheduler/src/module_adapters.cpp",
     r"sc->m_order > 0"),
    ("③帧级失败码", "lib/infrastructure/scheduler/src/module_adapters.cpp",
     r"PHOT_SPATIAL_GAIN_FIT_FAILED"),
]

SURFACES = [
    ("出厂配置模板", "eng/packaging/config/templates/normalize.phase_config.json"),
    ("defaults 正本", "eng/packaging/config/defaults.json"),
    ("配置登记表", "eng/packaging/config/config_registry.json"),
    ("phase_config 合同", "eng/contracts/schemas/phase_config_normalize.schema.json"),
    ("配置合同说明", "docs/contracts/CONFIG_CONTRACT.md"),
    ("插件工作细节", "docs/plugins/algorithms_phase1/06_photometry.md"),
    ("模块 README", "lib/algorithms/photometry/README.md"),
    ("默认值分歧台账", "eng/ci/ledgers/config_default_divergences.json"),
    ("死键台账", "eng/ci/ledgers/dead_config_keys.json"),
    ("设计条文台账", "eng/ci/ledgers/design_clauses.json"),
    ("CLI 协议", "docs/api/CLI_PROTOCOL_V1.md"),
]


def read(rel):
    p = os.path.join(REPO, *rel.split("/"))
    if not os.path.isfile(p):
        return None
    with io.open(p, encoding="utf-8") as f:
        return f.read().splitlines()


print("== 四侧逐字取值 ==")
for label, rel, pat in TARGETS:
    lines = read(rel)
    if lines is None:
        print("%-24s %s : 文件不存在" % (label, rel))
        continue
    rx = re.compile(pat)
    hit = [(i + 1, l.strip()) for i, l in enumerate(lines) if rx.search(l)]
    if not hit:
        print("%-24s %s : 无命中（模式 %s）" % (label, rel, pat))
    for ln, txt in hit[:3]:
        print("%-24s %s:%d | %s" % (label, rel, ln, txt[:170]))

print("\n== 第四侧：该键在各配置/登记面的存在性 ==")
for label, rel in SURFACES:
    lines = read(rel)
    if lines is None:
        print("%-14s %-56s 文件不存在" % (label, rel))
        continue
    hits = [(i + 1, l.strip()[:90]) for i, l in enumerate(lines)
            if "spatial_gain" in l]
    print("%-14s %-56s %s" % (label, rel,
                              ("零登记" if not hits else "命中 %s" % hits[:2])))

print("\n== 净效应推导（静态取值链，不执行代码）==")
md = read("lib/infrastructure/scheduler/src/module_adapters.cpp")
blk = "\n".join(md[5461:5490])
for kw in ("fit_cfg.empty()", "p1_flag(fit_cfg, \"enabled\"", "kDefaultSpatialGainOrder"):
    print("  含 %-32s : %s" % (kw, kw in blk))
print("  出厂模板含 photometry 段 :",
      any("photometry" in l for l in
          (read("eng/packaging/config/templates/normalize.phase_config.json") or [])))
snip = "\n".join(read("eng/tests/validation/release02/fix_p1_photometry_apply/"
                      "l4_photometry_fit_snippet.json"))
print("  L4 生产启用片段含该键 :", "spatial_gain_order" in snip)
vis = "\n".join(read("eng/tools/e2e/make_vis_configs.py"))
print("  e2e/vis 配置生成器含该键 :", "spatial_gain_order" in vis,
      "| 其 fit.enabled :", "enabled" in vis)

print("\n== 判据可见性：CHK-CONFIG-DEFAULTS 的键集合来源 ==")
g = "\n".join(read("eng/ci/check_config_defaults.py"))
for src in ("templates/*.json", "defaults.json", "phase_config_*.schema.json",
            "watch_keys"):
    print("  键集来源包含 %-28s : %s" % (src, src in g))
