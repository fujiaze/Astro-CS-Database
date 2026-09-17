#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""module_map_fixture.py — MOD-001 检查器合成 fixture（正例 + 五类负例 + facade/no-op）。

用途：为 tools/quality/check_module_map.py 提供「完整合法映射样例」与逐类负例，
使门「能绿能红」可复现，且**不依赖 lib/algorithms/ 是否已由 ARCH-001 建立**。

正例（mutation=None）：按 docs/modules/MODULE_MAP.yaml 的 23 行期望，在一个临时仓库里
物化全部 7 项必备面（README / module.yaml / 公开头 / 单一 entrypoint 实现 /
CMake target / 共址测试 / 有效合同引用）+ 产品清单 unit + 合同索引 + schema 文件
→ 检查器 rc=0（证明判据可达，且正例不是靠放宽判据换来的）。

负例（MUTATIONS，每类都必须 rc!=0）：
  missing_manifest      删除某模块 module.yaml（缺 manifest）
  duplicate_entrypoint  两个模块抢同一注册键 (module_id, entrypoint)（重复 entrypoint）
  missing_target        删除某模块 CMakeLists 里的 add_library（无 target）
  missing_tests         删除某模块共址测试目录（无测试）
  dangling_contract_ref 某模块声明不存在的 DATA 合同（悬空合同引用）
  facade_session        entrypoint 直接转发整阶段 Session（facade）
  noop_entrypoint       entrypoint 函数体零调用（no-op/无可执行路径）

零副作用：只在调用方给的 root 下生成文件；不读也不改真实仓库（除只读取期望映射表）。
"""
from __future__ import annotations

import copy
import json
import pathlib
import shutil

import yaml

REPO = pathlib.Path(__file__).resolve().parents[3]
MAP_REL = "docs/modules/MODULE_MAP.yaml"
MUTATIONS = (
    "missing_manifest",
    "duplicate_entrypoint",
    "missing_target",
    "missing_tests",
    "dangling_contract_ref",
    "facade_session",
    "noop_entrypoint",
)


def canonical_map(repo=None):
    """只读取仓库内期望映射表（fixture 的期望面与真实映射表同源，杜绝两份真值）。"""
    repo = pathlib.Path(repo) if repo else REPO
    return yaml.safe_load((repo / MAP_REL).read_text(encoding="utf-8"))


def _write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _module_yaml(m):
    inp = m.get("_fixture_input_ports") or ["in." + str(m["id"])]
    outp = m.get("_fixture_output_ports") or ["out." + str(m["id"])]
    doc = {
        "id": "MOD-astrocs-" + str(m["id"]).replace("_", "-"),
        "module_id": m["module_id"],
        "module_version": m["module_version"],
        "abi_version": m["abi_version"],
        "phase_scope": m["scope"],
        "module_status": "IMPLEMENTED",
        "entrypoint": m["entrypoint"],
        "node_operations": [str(m["id"]) + "_op"],
        "input_ports": inp,
        "output_ports": outp,
        "data_contracts": list(m.get("data_contracts") or []),
        "schema_links": list(m.get("schema_links") or []),
    }
    return yaml.safe_dump(doc, allow_unicode=True, sort_keys=False)


def _unit_id_for(m):
    pm = m.get("product_manifest") or {}
    if pm.get("expected_unit_ids"):
        return str(pm["expected_unit_ids"][0])
    if pm.get("match_unit_id"):
        return str(pm["match_unit_id"])
    return "MOD-" + str(m["target"]).upper()


def _header_text(m):
    guard = "ASTROCS_" + str(m["id"]).upper() + "_H"
    return (
        "#pragma once\n"
        "/* " + str(m["id"]) + " public header — fixture (MOD-001) */\n"
        "#define " + guard + " 1\n"
        "#define ASTROCS_" + str(m["id"]).upper() + "_ABI_VERSION 1\n"
        "typedef struct { unsigned int struct_size; unsigned int abi_version; } astrocs_"
        + str(m["id"]) + "_desc_v1;\n"
        "int astrocs_" + str(m["id"]) + "_execute(const void *host, const char *req, void *out);\n"
    )


def _source_text(m, body=None):
    ep = m["entrypoint"]
    if m.get("entrypoint_kind") == "exe-main":
        default = (
            "int main(int argc, char **argv) {\n"
            "    (void)argc; (void)argv;\n"
            "    return astrocs_cli_dispatch(argc, argv);\n"
            "}\n"
        )
        impl = "int astrocs_cli_dispatch(int argc, char **argv) { (void)argc; (void)argv; return 0; }\n"
    else:
        default = (
            'extern "C" int ' + ep + '(const void *host, const char *req, void *out) {\n'
            "    return astrocs_" + str(m["id"]) + "_execute(host, req, out);\n"
            "}\n"
        )
        impl = ("int astrocs_" + str(m["id"]) + "_execute(const void *host, const char *req, void *out) {\n"
                "    return astrocs_" + str(m["id"]) + "_kernel(host, req, out);\n"
                "}\n"
                "int astrocs_" + str(m["id"]) + "_kernel(const void *host, const char *req, void *out) {\n"
                "    (void)host; (void)req; (void)out; return 0;\n"
                "}\n")
    if body is not None:
        return body + "\n" + impl
    return default + impl


def _cmake_text(m):
    verb = "add_executable" if m.get("entrypoint_kind") == "exe-main" else "add_library"
    return (
        "cmake_minimum_required(VERSION 3.20)\n"
        "# fixture (MOD-001): " + str(m["id"]) + " 独立 target\n"
        + verb + "(" + str(m["target"]) + " src/" + str(m["id"]) + ".cpp)\n"
    )


def build_repo(root, mutation=None, repo=None):
    """在 root 下物化合成仓库；返回 root。mutation 取 MUTATIONS 之一（None=正例）。"""
    root = pathlib.Path(root)
    root.mkdir(parents=True, exist_ok=True)
    base = copy.deepcopy(canonical_map(repo))
    doc = copy.deepcopy(base)
    mods = doc["modules"]
    assert len(mods) == 23, "fixture 期望 23 行映射表，实际 %d" % len(mods)
    # 合同索引/schema 面按**未变异**期望生成：负例注入的悬空引用必须保持悬空，
    # 否则 fixture 会自己把负例补绿（自我实现的真值 = 无效真值）。
    base_contract_ids = sorted({str(x) for m in base["modules"] for x in (m.get("data_contracts") or [])})
    base_schema_links = sorted({str(x) for m in base["modules"] for x in (m.get("schema_links") or [])})

    if mutation == "duplicate_entrypoint":
        mods[1]["module_id"] = mods[0]["module_id"]
        mods[1]["entrypoint"] = mods[0]["entrypoint"]
        if isinstance(mods[1].get("product_manifest"), dict):
            mods[1]["product_manifest"]["match_module_id"] = mods[0]["module_id"]
    if mutation == "dangling_contract_ref":
        mods[4]["data_contracts"] = ["DATA-DOES-NOT-EXIST-999"]
    if mutation == "legacy_contract_ok":
        # 合成正例（附录 H.5）：引用一个「已由 legacy 映射裁决」的旧 ID。
        # 它必须**不在** - id: 列表里（base_contract_ids 由未变异期望生成），
        # 只能经 legacy_contract_id_map 解析 ⇒ 门若漏读该权威就会红（活性自持）。
        mods[4]["data_contracts"] = ["DATA-LEGACY-FIXTURE-001"]

    _write(root / MAP_REL, yaml.safe_dump(doc, allow_unicode=True, sort_keys=False))
    # 权威模块总表原样带入 fixture（只读复制）：使「ID 集合 == 00_INDEX §2」交叉核验在
    # fixture 上同样是活的，而不是被合成索引自证。
    src_repo = pathlib.Path(repo) if repo else REPO
    _write(root / "docs/plugins/00_INDEX.md",
           (src_repo / "docs/plugins/00_INDEX.md").read_text(encoding="utf-8"))

    contract_ids, schema_links, units = [], [], []
    for m in mods:
        mdir = root / str(m["target_dir"])
        _write(root / str(m["readme"]),
               "# " + str(m["id"]) + "（MOD-001 fixture）\n\n模块 ID：" + str(m["id"])
               + "\n职责：合成正例模块，仅用于证明检查器判据可达。\n")
        _write(root / str(m["module_yaml"]), _module_yaml(m))
        _write(mdir / "include" / (str(m["id"]) + ".h"), _header_text(m))
        body = None
        if mutation == "facade_session" and m is mods[5]:
            body = ('extern "C" int ' + str(m["entrypoint"])
                    + '(const void *host, const char *req, void *out) {\n'
                    "    return astrocs_p1_session_run(host, req, out);\n"
                    "}")
        if mutation == "noop_entrypoint" and m is mods[6]:
            body = ('extern "C" int ' + str(m["entrypoint"])
                    + '(const void *host, const char *req, void *out) {\n'
                    "    (void)host; (void)req; (void)out;\n"
                    "    return 0;\n"
                    "}")
        _write(mdir / "src" / (str(m["id"]) + ".cpp"), _source_text(m, body))
        _write(root / str(m["target_file"]), _cmake_text(m))
        _write(root / str(m["co_located_tests"]) / (str(m["id"]) + "_test.cpp"),
               "// " + str(m["id"]) + " co-located test (fixture)\nint main() { return 0; }\n")
        contract_ids.extend(str(x) for x in (m.get("data_contracts") or []))
        schema_links.extend(str(x) for x in (m.get("schema_links") or []))
        pm = m.get("product_manifest") or {}
        if pm.get("required", True):
            units.append({
                "unit_id": _unit_id_for(m),
                "kind": pm.get("unit_kind") or "module",
                "rel_path": "modules/" + str(m["target"]) + ".so",
                "abi_version": 1,
                "module_id": pm.get("match_module_id"),
                "sha256": None,
                "status": "IMPLEMENTED",
            })

    index_lines = ["schema: astrocs.contract-index/v1", "version: 1.0.0", "contracts:"]
    for cid in base_contract_ids:
        index_lines += ["  - id: " + cid, "    type: DATA", "    status: ACTIVE",
                        "    path: docs/contracts/DATA_ARTIFACTS.md"]
    if mutation == "legacy_contract_ok":
        index_lines += ["legacy_contract_id_map:",
                        "  - legacy_id: DATA-LEGACY-FIXTURE-001",
                        "    decision: mapped",
                        "    canonical_objects: [signal]",
                        "    relation: single_object",
                        "    canonical_schema_files: [contracts/schemas/unified/signal.schema.json]"]
    _write(root / (doc.get("conventions") or {}).get("contract_index_file", "docs/contracts/INDEX.yaml"),
           "\n".join(index_lines) + "\n")
    for rel in base_schema_links:
        _write(root / rel, '{"$comment": "MOD-001 fixture schema", "type": "object"}\n')
    _write(root / (doc.get("conventions") or {}).get("product_manifest_file",
                                                     "packaging/astrocs.product.json"),
           json.dumps({"schema_version": 1, "product_version": "0.11.0-alpha.2",
                       "source_commit": "0" * 40, "platform": "linux-amd64",
                       "note": "MOD-001 fixture", "units": units},
                      ensure_ascii=False, indent=2) + "\n")

    if mutation == "missing_manifest":
        (root / str(mods[0]["module_yaml"])).unlink()
    if mutation == "missing_target":
        (root / str(mods[2]["target_file"])).unlink()
    if mutation == "missing_tests":
        shutil.rmtree(root / str(mods[3]["co_located_tests"]), ignore_errors=True)
    return root


if __name__ == "__main__":
    import sys
    import tempfile
    with tempfile.TemporaryDirectory(prefix="mod001-fixture-") as td:
        for name in (None,) + MUTATIONS:
            p = build_repo(pathlib.Path(td) / (name or "positive"), mutation=name)
            print("built", name or "positive", "->", p)
    sys.exit(0)
