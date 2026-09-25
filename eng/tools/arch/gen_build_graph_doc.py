#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 docs/architecture/BUILD_GRAPH.md 的机器块从**真实构建图**导出（一页纸 S1 第 25 条）。

存在理由
  该文档的「生产构建图」表原先手抄，三行的目标名与路径全部落空
  （astro_image_io.dll 所指 lib/infrastructure/aio/CMakeLists.txt 不在根图、
  hepix_drizzle 实名为 astrocs_drizzle、orchestrator.exe 实名为 orchestrator_legacy_cli，
  且未列 acsd 与 astrocs_phase2），而门只查「文档里是否出现某四个字符串」⇒ 表全错、门仍绿。
  表改为导出：本生成器写三个机器块，门 CON-BUILD-GRAPH 逐行比对。

三个机器块
  BUILD-GRAPH-TABLE   生产入口的传递闭包（target|kind|cmakelists|sources|src_fingerprint）；
  BUILD-GRAPH-NONPROD 真实存在但不在生产闭包内的目标（交付件/工具面，target|理由）；
  BUILD-GRAPH-NONROOT 不在根构建图内的目标（子项目自有 CMakeLists，target|cmakelists|理由）。

口径
  - 唯一事实源 = 根 CMakeLists.txt 沿未注释 add_subdirectory 递归（eng/ci/cmake_graph.py）；
  - 生成器自身 fail-closed：登记为非生产的目标若不在图中或已进入生产闭包，
    或登记为非根图的目标若已在根图中 ⇒ 非零退出，绝不把假事实写进文档；
  - 只写机器块之间的内容，块外文字（说明/复算命令）保持人工维护。

用法
  python3 eng/tools/arch/gen_build_graph_doc.py            # 就地更新机器块
  python3 eng/tools/arch/gen_build_graph_doc.py --out F    # 写别处（比对用）
"""
import argparse
import importlib.util
import os
import pathlib
import sys

DOC_REL = "docs/architecture/BUILD_GRAPH.md"
MARKERS = ("BUILD-GRAPH-TABLE", "BUILD-GRAPH-NONPROD", "BUILD-GRAPH-NONROOT")

# 非生产闭包面：真实存在于根构建图、但不在生产入口链接闭包内的目标（交付件与工具面）。
# 每行 (target, 理由)。生成器会逐行核实「存在 ∧ 不在闭包内」。
NONPROD = [
    ("acsd_runtime", "安装树根的平台 SHARED（交付件，运行期加载）"),
    ("acsd_io", "安装树根的平台 SHARED（交付件，运行期加载）"),
    ("astrocs_noop", "安装到 modules/ 的一致性模块（交付件）"),
    ("astrocs_catalog_gaia", "安装到 modules/ 的星表服务模块（交付件）"),
    ("astrocs_p1_drizzle", "安装到 modules/ 的 Phase1 模块（交付件）"),
    ("astrocs_p1_calibration", "安装到 modules/ 的 Phase1 模块（交付件）"),
    ("astrocs_p1_cosmetic", "安装到 modules/ 的 Phase1 模块（交付件）"),
    ("astrocs_p1_hips_writer", "安装到 modules/ 的 Phase1 模块（交付件）"),
    ("astrocs_cpu_baseline", "安装到 providers/ 的 baseline provider（交付件）"),
    ("astrocs-stage2", "Phase2 工具面（ARCHITECTURE §1 迁移冻结：非入口）"),
    ("orchestrator_legacy_cli", "Phase1 编排工具面（ARCHITECTURE §1 迁移冻结：非入口）"),
    ("calibrated_pair_diag", "标定对诊断工具（非入口）"),
    ("rejection_cli", "排异诊断工具（非入口）"),
    ("m42_criterion_probe", "判据探针工具（非入口）"),
]

# 非根图目标：其 CMakeLists 未被根 CMakeLists 的 add_subdirectory 纳入。
NONROOT = [
    ("healpix_browser_qt", "lib/infrastructure/hips_browser/healpix_browser_qt/CMakeLists.txt",
     "浏览器工具（ARCHITECTURE §1 迁移冻结：工具面）"),
    ("browser_cli", "lib/infrastructure/hips_browser/healpix_browser_qt/CMakeLists.txt",
     "浏览器工具（ARCHITECTURE §1 迁移冻结：工具面）"),
    ("acr-benchmark", "lib/infrastructure/acr/tools/acr_benchmark/CMakeLists.txt",
     "ACR DORMANT（最高设计 §8：不进生产构建）"),
    ("acr_test_api", "lib/infrastructure/acr/tests/unit/CMakeLists.txt",
     "ACR DORMANT（最高设计 §8：不进生产构建）"),
]


def repo_default():
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


def load_graph_module(repo):
    path = os.path.join(repo, "eng", "ci", "cmake_graph.py")
    if not os.path.isfile(path):
        raise SystemExit("ANCHOR_MISSING: eng/ci/cmake_graph.py（真实构建图读取器）")
    spec = importlib.util.spec_from_file_location("acsd_cmake_graph", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def md_table(header, rows, per_chunk=9):
    """把行集渲染成 markdown 表；每 per_chunk 行换一个新表头。

    分块不是排版洁癖：CHK-DOC-HYGIENE 的 D4e 段落长度棘轮按「连续非空行」计数
    （> 12 行判违规），33 行的整表会触发它；分块后每块 ≤ 12 行，判据与内容一字不改。
    """
    sep = "|" + "|".join(["---"] * len(header)) + "|"
    out = []
    for i in range(0, len(rows), per_chunk):
        if i:
            out.append("")
        out.append("| " + " | ".join(header) + " |")
        out.append(sep)
        out += ["| " + " | ".join(r) + " |" for r in rows[i:i + per_chunk]]
    return out or ["| " + " | ".join(header) + " |", sep]


def render_blocks(repo, mod):
    repo = pathlib.Path(repo)
    graph = mod.parse_cmake_graph(repo)
    entry = mod.production_entry(repo)
    closure = mod.production_closure(graph, entry)
    targets = graph["targets"]
    if not targets:
        raise SystemExit("ANCHOR_EMPTY: 根构建图解析出 0 个 target")
    if not closure:
        raise SystemExit("ANCHOR_EMPTY: 生产闭包为空（禁止空转导出）")

    prod = []
    for name in sorted(closure, key=lambda n: (n != entry, n)):
        info = targets[name]
        prod.append([name, info["kind"], info["file"], str(len(info["sources"])),
                     mod.source_fingerprint(info["sources"])])

    nonprod = []
    for name, why in NONPROD:
        if name not in targets:
            raise SystemExit("NONPROD 登记项不在根构建图: %s（删掉该行或改对名字）" % name)
        if name in closure:
            raise SystemExit("NONPROD 登记项已在生产闭包内: %s（不得把生产目标标成非生产）" % name)
        nonprod.append([name, why])

    nonroot = []
    for name, cmake, why in NONROOT:
        if name in targets:
            raise SystemExit("NONROOT 登记项已在根构建图内: %s（登记失真）" % name)
        if not (repo / cmake).is_file():
            raise SystemExit("NONROOT 登记项的 CMakeLists 不存在: %s" % cmake)
        nonroot.append([name, cmake, why])

    blocks = {
        "BUILD-GRAPH-TABLE": md_table(["target", "kind", "cmakelists", "sources",
                                       "src_fingerprint"], prod),
        "BUILD-GRAPH-NONPROD": md_table(["target", "理由"], nonprod),
        "BUILD-GRAPH-NONROOT": md_table(["target", "cmakelists", "理由"], nonroot),
    }
    counts = {"prod": len(prod), "nonprod": len(nonprod), "nonroot": len(nonroot)}
    return blocks, counts


def apply_blocks(text, blocks):
    for marker, rows in blocks.items():
        begin = "<!-- " + marker + ":BEGIN -->"
        end = "<!-- " + marker + ":END -->"
        if begin not in text or end not in text:
            raise SystemExit("ANCHOR_MISSING: %s 缺机器块标记 %s" % (DOC_REL, marker))
        head, rest = text.split(begin, 1)
        _old, tail = rest.split(end, 1)
        text = head + begin + chr(10) + chr(10).join(rows) + chr(10) + end + tail
    return text


def main(argv=None):
    ap = argparse.ArgumentParser(description="从真实构建图导出 BUILD_GRAPH.md 机器块")
    ap.add_argument("--repo", default=repo_default())
    ap.add_argument("--out", default=None, help="输出路径（默认就地更新 %s）" % DOC_REL)
    args = ap.parse_args(argv)
    repo = os.path.abspath(args.repo)
    out = os.path.abspath(args.out) if args.out else os.path.join(repo, DOC_REL)
    mod = load_graph_module(repo)
    blocks, counts = render_blocks(repo, mod)
    src = pathlib.Path(repo) / DOC_REL
    target = pathlib.Path(out)
    # 就地更新时以目标文件为底（保留块外人工维护的说明文字）；写新文件时以仓内文档为底。
    base = target if target.is_file() else src
    text = base.read_text(encoding="utf-8")
    new = apply_blocks(text, blocks)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(new, encoding="utf-8")
    print("blocks: prod=%d nonprod=%d nonroot=%d -> %s"
          % (counts["prod"], counts["nonprod"], counts["nonroot"], out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
