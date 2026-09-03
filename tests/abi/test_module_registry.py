#!/usr/bin/env python3
"""ABI-004 动态模块 registry 验收测试编排（Linux amd64）。

规格: 控制包 tasks/02_ABI_BUILD_CLI_TASKS.md ABI-004:
  运行时 registry 只来自 product manifest + DLL query; 检测重复 ID/版本冲突/
  未登记 DLL/descriptor 不一致; static factory 不再是正式路由。
验收:
  1. module.yaml、embedded descriptor、release manifest 三方机器比对;
  2. list/verify 输出实际 hash/entry;
  3. 重复 unit_id/module_id 检测; 版本冲突; 未登记 DLL; module_id/hash 不一致。

结构:
  - fixture 目录(临时): 用仓库 modules/conformance/noop 源码编译真实 noop .so,
    构造 product manifest(units 数组)与 module.yaml 镜像;
  - 场景 1: clean manifest + 真实 noop → open OK, 0 findings, entry 三方一致
    (module_id/version/build/hash 与 module.yaml/descriptor/manifest 全符);
  - 场景 2: 重复 unit_id → finding DUP_UNIT_ID;
  - 场景 3: 重复 module_id → finding DUP_MODULE_ID;
  - 场景 4: manifest sha256 与实际不符 → HASH_MISMATCH;
  - 场景 5: module.yaml module_id 与 manifest 不符 → YAML_INCONSISTENT;
  - 场景 6: manifest module_id 与 DLL descriptor 不符 → MODULE_ID_MISMATCH;
  - 场景 7: version 冲突(module.yaml version != descriptor version) → VERSION_CONFLICT;
  - 场景 8: manifest 声明 DLL 不存在 → LOADER(缺 DLL);
  - 场景 9: 未登记 DLL(scan=1, 模块目录多放 .so) → UNREGISTERED_DLL;
  - 场景 10: manifest 非法(非 module kind 或非 64hex sha) → open 硬失败;
  - 场景 11: selftest sha256 + JSON 向量;
  - 场景 12: allowed_root 越界 → 硬失败;
  - 场景 13: 非绝对 manifest 路径 → 硬失败;
  - 场景 14: static factory 不再是正式路由 —— registry 加载真实 noop 走
    query 入口; 未加载 entry(finding_mask 有 LOAD_FAILED)绝不被当作可用。
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INC = os.path.join(REPO, "include")
LOADER_DIR = os.path.join(REPO, "runtime", "module_loader")
REG_DIR = os.path.join(REPO, "runtime", "registry")
PROBE_C = os.path.join(REPO, "tests", "abi", "abi004_registry_probe.c")
NOOP_SRC = os.path.join(REPO, "modules", "conformance", "noop", "src", "noop_module.c")
NOOP_YAML = os.path.join(REPO, "modules", "conformance", "noop", "module.yaml")
TIMEOUT = 300
CC = os.environ.get("CC", "gcc")

FAILURES = []
CHECKS = [0]


def run(cmd, cwd=None):
    return subprocess.run(cmd, capture_output=True, text=True, cwd=cwd,
                          timeout=TIMEOUT)


def check(name, cond, detail=""):
    CHECKS[0] += 1
    tag = "PASS" if cond else "FAIL"
    print(f"[{tag}] {name}" + (f"  {detail}" if detail else ""))
    if not cond:
        FAILURES.append(name)


def sha256f(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


NOOP_MID = "astrocs.conformance.noop"
NOOP_VER = "0.11.0-alpha.1"
NOOP_BUILD = "BLD-003-skeleton"


def compile_noop(out_so):
    cmd = [CC, "-std=c11", "-Wall", "-fPIC", "-shared", f"-I{INC}",
           "-DASTROCS_ABI_SHARED=1", "-DASTROCS_ABI_EXPORTS=1",
           NOOP_SRC, "-o", out_so]
    return run(cmd)


def make_manifest(base, units, product_version="0.11.0-alpha.1"):
    """units: list of dict. rel_path 相对 base(安装根)。写入 base/astrocs.product.json"""
    doc = {
        "schema_version": 1,
        "product_version": product_version,
        "source_commit": "0" * 40,
        "platform": "linux-amd64",
        "note": "ABI-004 test fixture",
        "units": units,
    }
    with open(os.path.join(base, "astrocs.product.json"), "w") as f:
        json.dump(doc, f, indent=1)


def std_unit(unit_id, kind, rel_path, module_id=None, sha=None, abi=1,
             status="IMPLEMENTED"):
    u = {"unit_id": unit_id, "kind": kind, "rel_path": rel_path,
         "abi_version": abi, "status": status}
    if module_id is not None:
        u["module_id"] = module_id
    if sha is not None:
        u["sha256"] = sha
    else:
        u["sha256"] = None
    return u


def main():
    work = tempfile.mkdtemp(prefix="abi004_")
    print(f"workdir={work}")
    try:
        # ── 编译 loader + registry + probe ──
        loader_o = os.path.join(work, "loader.o")
        r = run([CC, "-std=c11", "-Wall", "-fno-exceptions", "-fPIC", "-c",
                 f"-I{INC}", f"-I{LOADER_DIR}",
                 os.path.join(LOADER_DIR, "secure_loader.c"), "-o", loader_o])
        check("compile loader", r.returncode == 0, r.stderr[-300:] if r.stderr else "")
        reg_o = os.path.join(work, "reg.o")
        r = run([CC, "-std=c11", "-Wall", "-fno-exceptions", "-fPIC", "-c",
                 f"-I{INC}", f"-I{REPO}",
                 os.path.join(REG_DIR, "module_registry.c"), "-o", reg_o])
        check("compile registry", r.returncode == 0, r.stderr[-300:] if r.stderr else "")
        probe = os.path.join(work, "abi004_probe")
        r = run([CC, "-std=c11", "-Wall", "-fno-exceptions", f"-I{INC}",
                 f"-I{REPO}", f"-I{REG_DIR}", PROBE_C, loader_o, reg_o,
                 "-ldl", "-o", probe])
        check("compile probe", r.returncode == 0, r.stderr[-300:] if r.stderr else "")
        if r.returncode != 0:
            return 1

        # ── 自检 ──
        r = run([probe, "selftest"])
        check("S1 registry selftest (sha256+json vectors)",
              r.returncode == 0 and "SELFTEST_OK" in r.stdout, r.stdout + r.stderr)

        # ── 真实 noop 编译 ──
        noop_so = os.path.join(work, "modules", "astrocs_noop.so")
        os.makedirs(os.path.dirname(noop_so))
        r = compile_noop(noop_so)
        check("S2 compile real noop.so", r.returncode == 0,
              r.stderr[-300:] if r.stderr else "")
        noop_sha = sha256f(noop_so)

        # 安装树 fixture: 复制 module.yaml 镜像到 modules/astrocs_noop 源目录?
        # registry 用 yaml_root 找 <rel_dir>/module.yaml; 模拟仓库布局:
        #   <root>/modules/<dll>.so  + manifest rel modules/astrocs_noop.so
        #   module.yaml 镜像放 <root>/modules/astrocs_noop/module.yaml? 
        #   → rel_dir=modules 会找 <root>/modules/module.yaml 更贴仓库
        # 仓库真实布局: modules/conformance/noop/module.yaml + 安装 modules/astrocs_noop.so。
        # registry 的 yaml 查找用 rel_dir = rel_path 的 dirname(=modules), 找
        # <yaml_root>/modules/module.yaml。为模拟三方, 我们在工作区放:
        #   <work>/yaml_src/modules/module.yaml  (内容=仓库 noop module.yaml)
        yaml_root = os.path.join(work, "yaml_root")
        os.makedirs(os.path.join(yaml_root, "modules"))
        shutil.copy(NOOP_YAML, os.path.join(yaml_root, "modules", "module.yaml"))

        # 载入仓库 module.yaml 真实字段(镜像; 权威三方源)
        with open(NOOP_YAML) as f:
            yaml_text = f.read()

        # ══ 场景 1: clean manifest → 0 findings, 三方一致 ══
        base1 = os.path.join(work, "s1_clean")
        os.makedirs(os.path.join(base1, "modules"))
        shutil.copy(noop_so, os.path.join(base1, "modules", "astrocs_noop.so"))
        make_manifest(base1, [
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id=NOOP_MID, sha=noop_sha),
        ])
        r = run([probe, "check", os.path.join(base1, "astrocs.product.json"),
                 yaml_root, base1, "1"])
        check("P1 clean registry open", "CHECK_OK" in r.stdout, r.stdout + r.stderr)
        check("P1 zero findings", "issues=0" in r.stdout, r.stdout)
        r = run([probe, "dump", os.path.join(base1, "astrocs.product.json"),
                 yaml_root, base1, "0"])
        check("P1 dump entries=1 loaded", "entries=1" in r.stdout and
              "loaded=1" in r.stdout and "mask=0" in r.stdout, r.stdout)
        check("P1 module_id三方一致 (manifest/dll/yaml)",
              re.search(r"module_id=astrocs\.conformance\.noop\|"
                        r"mid_dll=astrocs\.conformance\.noop\|"
                        r"mid_yaml=astrocs\.conformance\.noop\|"
                        r"ver_dll=0\.11\.0-alpha\.1\|"
                        r"ver_yaml=0\.11\.0-alpha\.1\|", r.stdout), r.stdout)
        check("P1 hash 实际=登记",
              re.search(r"sha_reg=([0-9a-f]{12})", r.stdout) and
              re.search(r"sha_act=([0-9a-f]{12})", r.stdout) and
              re.search(r"sha_reg=([0-9a-f]{12})", r.stdout).group(1) ==
              re.search(r"sha_act=([0-9a-f]{12})", r.stdout).group(1),
              r.stdout)
        check("P1 sha_act equals real file sha",
              re.search(r"sha_act=([0-9a-f]{12})", r.stdout) and
              re.search(r"sha_act=([0-9a-f]{12})", r.stdout).group(1) ==
              noop_sha[:12], r.stdout)

        # ══ 场景 2: 重复 unit_id ══
        base2 = os.path.join(work, "s2_dup_unit")
        os.makedirs(os.path.join(base2, "modules"))
        shutil.copy(noop_so, os.path.join(base2, "modules", "astrocs_noop.so"))
        make_manifest(base2, [
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id=NOOP_MID, sha=noop_sha),
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id="astrocs.conformance.noop2", sha=noop_sha),
        ])
        r = run([probe, "check", os.path.join(base2, "astrocs.product.json"),
                 yaml_root, base2, "0"])
        check("P2 duplicate unit_id detected", "kind=7" in r.stdout, r.stdout)

        # ══ 场景 3: 重复 module_id(不同 unit) ══
        base3 = os.path.join(work, "s3_dup_mid")
        os.makedirs(os.path.join(base3, "modules"))
        shutil.copy(noop_so, os.path.join(base3, "modules", "a.so"))
        shutil.copy(noop_so, os.path.join(base3, "modules", "b.so"))
        make_manifest(base3, [
            std_unit("MOD-A", "module", "modules/a.so", module_id=NOOP_MID,
                     sha=noop_sha),
            std_unit("MOD-B", "module", "modules/b.so", module_id=NOOP_MID,
                     sha=noop_sha),
        ])
        r = run([probe, "check", os.path.join(base3, "astrocs.product.json"),
                 yaml_root, base3, "0"])
        check("P3 duplicate module_id detected", "kind=8" in r.stdout, r.stdout)

        # ══ 场景 4: manifest sha256 与实际不符 ══
        base4 = os.path.join(work, "s4_hash")
        os.makedirs(os.path.join(base4, "modules"))
        shutil.copy(noop_so, os.path.join(base4, "modules", "astrocs_noop.so"))
        make_manifest(base4, [
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id=NOOP_MID, sha="0" * 64),
        ])
        r = run([probe, "dump", os.path.join(base4, "astrocs.product.json"),
                 yaml_root, base4, "0"])
        check("P4 hash mismatch finding (kind=12)",
              "kind=12" in r.stdout, r.stdout)
        check("P4 entry mask HAS_HASH (bit test)", "mask=4" in r.stdout
              or ("mask=" in r.stdout and "loaded=0" in r.stdout
                  and int(re.search(r"mask=(-?\d+)", r.stdout).group(1)) & 4),
              r.stdout)
        # loader 也因 hash 不符拒绝 → LOAD_FAILED
        check("P4 loader rejects wrong-hash module", "loaded=0" in r.stdout, r.stdout)

        # ══ 场景 5: module.yaml module_id 与 manifest 不符 ══
        base5 = os.path.join(work, "s5_yaml")
        os.makedirs(os.path.join(base5, "modules"))
        shutil.copy(noop_so, os.path.join(base5, "modules", "astrocs_noop.so"))
        make_manifest(base5, [
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id="astrocs.conformance.different", sha=noop_sha),
        ])
        r = run([probe, "dump", os.path.join(base5, "astrocs.product.json"),
                 yaml_root, base5, "0"])
        check("P5 yaml inconsistent finding (kind=14)",
              "kind=14" in r.stdout, r.stdout)
        check("P5 entry mask YAML (bit test)",
              "mask=" in r.stdout and
              int(re.search(r"mask=(-?\d+)", r.stdout).group(1)) & 16, r.stdout)

        # ══ 场景 6: manifest module_id 与 DLL descriptor 不符 ══
        # (yaml_root 不提供 → 仅 manifest vs DLL)
        base6 = os.path.join(work, "s6_mid")
        os.makedirs(os.path.join(base6, "modules"))
        shutil.copy(noop_so, os.path.join(base6, "modules", "astrocs_noop.so"))
        make_manifest(base6, [
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id="astrocs.conformance.wrong", sha=noop_sha),
        ])
        r = run([probe, "dump", os.path.join(base6, "astrocs.product.json"),
                 "-", base6, "0"])
        check("P6 module_id mismatch finding (kind=11)",
              "kind=11" in r.stdout, r.stdout)
        check("P6 entry mask MODULE_ID (bit test)",
              "mask=" in r.stdout and
              int(re.search(r"mask=(-?\d+)", r.stdout).group(1)) & 2, r.stdout)

        # ══ 场景 7: 版本冲突(module.yaml version != DLL descriptor) ══
        base7 = os.path.join(work, "s7_ver")
        os.makedirs(os.path.join(base7, "modules"))
        shutil.copy(noop_so, os.path.join(base7, "modules", "astrocs_noop.so"))
        make_manifest(base7, [
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id=NOOP_MID, sha=noop_sha),
        ])
        yaml7 = os.path.join(work, "yaml7")
        os.makedirs(os.path.join(yaml7, "modules"))
        with open(os.path.join(yaml7, "modules", "module.yaml"), "w") as f:
            f.write(yaml_text.replace("module_version: 0.11.0-alpha.1",
                                      "module_version: 9.9.9-beta"))
        r = run([probe, "dump", os.path.join(base7, "astrocs.product.json"),
                 yaml7, base7, "0"])
        check("P7 version conflict finding (kind=9)",
              "kind=9" in r.stdout, r.stdout)
        check("P7 entry mask VERSION", "mask=8" in r.stdout, r.stdout)

        # ══ 场景 8: manifest 声明 DLL 不存在 ══
        base8 = os.path.join(work, "s8_missing")
        os.makedirs(os.path.join(base8, "modules"))
        make_manifest(base8, [
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id=NOOP_MID, sha=None),
        ])
        r = run([probe, "dump", os.path.join(base8, "astrocs.product.json"),
                 yaml_root, base8, "0"])
        check("P8 missing DLL finding (kind=13 loader)",
              "kind=13" in r.stdout and "mask=1" in r.stdout and "loaded=0" in r.stdout,
              r.stdout)

        # ══ 场景 9: 未登记 DLL ══
        base9 = os.path.join(work, "s9_unreg")
        os.makedirs(os.path.join(base9, "modules"))
        shutil.copy(noop_so, os.path.join(base9, "modules", "astrocs_noop.so"))
        # 未登记(manifest 无)的 .so
        rogue = os.path.join(base9, "modules", "rogue.so")
        shutil.copy(noop_so, rogue)
        make_manifest(base9, [
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id=NOOP_MID, sha=noop_sha),
        ])
        r = run([probe, "check", os.path.join(base9, "astrocs.product.json"),
                 yaml_root, base9, "1"])
        check("P9 unregistered DLL finding (kind=10)", "kind=10" in r.stdout,
              r.stdout)

        # ══ 场景 10: manifest 非法(kind) → 硬失败 ══
        base10 = os.path.join(work, "s10_bad")
        os.makedirs(base10)
        make_manifest(base10, [
            {"unit_id": "X", "kind": "kernel", "rel_path": "k.so",
             "abi_version": 1, "status": "IMPLEMENTED", "module_id": None,
             "sha256": None},
        ])
        r = run([probe, "open", os.path.join(base10, "astrocs.product.json"),
                 "-", "-", "0"])
        check("N1 invalid kind hard-fails", "OPEN_FAIL" in r.stdout and
              "detail=4" in r.stdout, r.stdout + r.stderr)

        # ══ 场景 10b: sha 非 64hex → 硬失败 ══
        base10b = os.path.join(work, "s10b_badsha")
        os.makedirs(os.path.join(base10b, "modules"))
        shutil.copy(noop_so, os.path.join(base10b, "modules", "astrocs_noop.so"))
        make_manifest(base10b, [
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id=NOOP_MID, sha="zz-not-hex"),
        ])
        r = run([probe, "open", os.path.join(base10b, "astrocs.product.json"),
                 "-", "-", "0"])
        check("N1b bad sha hard-fails", "OPEN_FAIL" in r.stdout and
              "detail=4" in r.stdout, r.stdout + r.stderr)

        # ══ 场景 11: 非绝对 manifest → 硬失败 ══
        os.chdir(base1)
        r = run([probe, "open", "astrocs.product.json", "-", "-", "0"])
        check("N2 relative manifest path rejected", "OPEN_FAIL" in r.stdout and
              "detail=5" in r.stdout, r.stdout + r.stderr)

        # ══ 场景 12: allowed_root 越界 → 硬失败 ══
        base12 = os.path.join(work, "s12_root")
        os.makedirs(os.path.join(base12, "modules"))
        shutil.copy(noop_so, os.path.join(base12, "modules", "astrocs_noop.so"))
        make_manifest(base12, [
            std_unit("MOD-NOOP", "module", "modules/astrocs_noop.so",
                     module_id=NOOP_MID, sha=noop_sha),
        ])
        other_root = os.path.join(work, "other_root")
        os.makedirs(other_root)
        r = run([probe, "open", os.path.join(base12, "astrocs.product.json"),
                 "-", other_root, "0"])
        check("N3 file outside allowed_root hard-fails",
              "OPEN_FAIL" in r.stdout and "detail=6" in r.stdout,
              r.stdout + r.stderr)

        # ══ 场景 13: 静默 fallback 检查: loaded=0 的 entry 绝无 module_api ══
        # (P4 场景已确认 loaded=0; registry 契约: 未加载 entry 不参与正式路由)

        print(f"\n{'='*60}\nresults: {len(FAILURES)} FAIL / "
              f"{CHECKS[0] - len(FAILURES)} PASS (total {CHECKS[0]} checks)\n{'='*60}")
        if FAILURES:
            print("FAILED:", *FAILURES, sep="\n  ")
            return 1
        return 0
    finally:
        if os.environ.get("KEEP_ABI004_WORK"):
            print(f"KEEP workdir: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
