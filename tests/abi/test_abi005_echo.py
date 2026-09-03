#!/usr/bin/env python3
"""ABI-005 echo conformance module 验收测试编排（Linux amd64）。

规格: 控制包 tasks/02_ABI_BUILD_CLI_TASKS.md ABI-005:
  实现无科学含义的 echo module，覆盖 artifact read/write、host allocator、
  logger、metrics、cancel、executor、error; 作为所有平台 ABI 探针。
验收:
  1. Windows/Linux dynamic load（Linux 同源 .so 验证; Windows 语义同源由
     WIN-* 实机承担）;
  2. 每个 host callback 有正/负测试（unit: 经独立 fake host 驱动全部回调）;
  3. 删除/替换 DLL 证明无静态 fallback（registry 加载失败即报缺, 无备用实现）;
  4. 编译零告警; 唯一导出 astrocs_module_query_v1。

结构:
  - 编译真实 echo 模块源码 (modules/conformance/echo/src/echo_module.c) 为
    .so, 断言 stderr 无 warning（-Wall -Wextra -fno-exceptions）;
  - exports 检查: nm -D 仅 astrocs_module_query_v1;
  - 编译并运行共址单元测试 (tests/unit/echo_host_callback_test.c): 全部 host
    callback（artifact read/write、allocator、logger、cancel、executor、
    config_query、error、metrics inspect）正/负断言 → ALL PASS;
  - ABI-003 loader 正测: 绝对 canonical + sha256 + module_id + build_id 全校验
    加载真实 echo .so → LOAD_OK/DESCRIBE_OK/RELEASE_OK; 篡改文件 → detail=5;
  - ABI-004 registry 正测: 三方（module.yaml/descriptor/manifest）一致
    → 0 findings, loaded=1 mask=0;
  - 删除/替换 DLL 负测: registry 报缺（kind=13）/hash mismatch（kind=12）,
    loaded=0 —— 证明无静态 fallback（宿主无 echo 实现静态副本: loader/registry
    对象无未定义引用、宿主源码无直调）。
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
ECHO_DIR = os.path.join(REPO, "modules", "conformance", "echo")
ECHO_SRC = os.path.join(ECHO_DIR, "src", "echo_module.c")
ECHO_INC = os.path.join(ECHO_DIR, "include")
ECHO_YAML = os.path.join(ECHO_DIR, "module.yaml")
UNIT_C = os.path.join(ECHO_DIR, "tests", "unit", "echo_host_callback_test.c")
LOADER_DIR = os.path.join(REPO, "runtime", "module_loader")
REG_DIR = os.path.join(REPO, "runtime", "registry")
LOADER_PROBE_C = os.path.join(REPO, "tests", "abi", "abi003_loader_probe.c")
REG_PROBE_C = os.path.join(REPO, "tests", "abi", "abi004_registry_probe.c")
TIMEOUT = 300
CC = os.environ.get("CC", "gcc")

ECHO_MID = "astrocs.conformance.echo"
ECHO_VER = "0.11.0-alpha.1"
ECHO_BUILD = "ABI-005-echo"

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


def compile_echo(out_so):
    """编译真实 echo .so; 断言零告警（-Wall -Wextra）。"""
    cmd = [CC, "-std=c11", "-Wall", "-Wextra", "-fno-exceptions", "-fPIC",
           "-shared", "-fvisibility=hidden", f"-I{INC}", f"-I{ECHO_INC}",
           "-DASTROCS_ABI_SHARED=1", "-DASTROCS_ABI_EXPORTS=1",
           ECHO_SRC, "-o", out_so]
    r = run(cmd)
    if r.returncode != 0:
        return r
    # 零告警断言
    warnings = [ln for ln in r.stderr.splitlines() if "warning" in ln.lower()]
    check("C1 echo compile zero warnings (-Wall -Wextra -fno-exceptions)",
          not warnings, r.stderr[-400:] if warnings else "")
    check("C2 echo compile rc=0", r.returncode == 0,
          r.stderr[-300:] if r.stderr else "")
    return r


def make_manifest(base, units, product_version="0.11.0-alpha.1"):
    doc = {
        "schema_version": 1,
        "product_version": product_version,
        "source_commit": "0" * 40,
        "platform": "linux-amd64",
        "note": "ABI-005 test fixture",
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
    u["sha256"] = sha
    return u


def scan_no_static_echo():
    """宿主侧无 echo 静态副本: 产品宿主源码（runtime/lib/cli/providers）不得
    直调或定义 astrocs_module_query_v1（该入口唯一实现位于 module DLL; loader
    只经 dlsym 字符串解析）。include/astrocs/abi/module_api_v1.h 是共享声明头
    （module 与 host 都 include），非实现，排除; 其余宿主源中出现
    `astrocs_module_query_v1(`（前导非引号）即静态直调证据。"""
    banned = []
    root_dirs = ["runtime", "lib", "cli", "providers"]
    for d in root_dirs:
        base = os.path.join(REPO, d)
        if not os.path.isdir(base):
            continue
        for root, _dirs, files in os.walk(base):
            for fn in files:
                if not (fn.endswith(".c") or fn.endswith(".h") or
                        fn.endswith(".cpp")):
                    continue
                p = os.path.join(root, fn)
                try:
                    txt = open(p, encoding="utf-8", errors="replace").read()
                except OSError:
                    continue
                # 直调形态: 标识后紧跟 '(' 且前一个非引号字符（排除 dlsym 字符串）
                for m in re.finditer(r'astrocs_module_query_v1\s*\(', txt):
                    pre = txt[max(0, m.start() - 1):m.start()]
                    if pre != '"':
                        banned.append(p)
                        break
    return banned


def main():
    work = tempfile.mkdtemp(prefix="abi005_")
    print(f"workdir={work}")
    try:
        # ── 编译真实 echo .so ──
        so_dir = os.path.join(work, "modules")
        os.makedirs(so_dir)
        echo_so = os.path.join(so_dir, "astrocs_echo.so")
        r = compile_echo(echo_so)
        if not os.path.exists(echo_so):
            print(r.stdout + r.stderr)
            return 1
        echo_sha = sha256f(echo_so)

        # ── 1. 唯一导出验证 ──
        r = run(["nm", "-D", "--defined-only", echo_so])
        exports = [ln for ln in r.stdout.splitlines() if " T " in ln or " D " in ln]
        dyn = re.findall(r"\bastrocs_module_query_v1\b", r.stdout)
        check("E1 exports only astrocs_module_query_v1",
              r.returncode == 0 and len(dyn) == 1 and " T " in r.stdout
              and len([ln for ln in exports if "astrocs_module_query_v1" not in ln]) == 0,
              r.stdout)
        check("E2 no extra dynamic exports",
              r.returncode == 0 and not
              [ln for ln in exports if "astrocs_module_query_v1" not in ln],
              r.stdout)

        # ── 2. 单元测试: 全部 host callback 正/负 ──
        unit_bin = os.path.join(work, "echo_host_callback_test")
        r = run([CC, "-std=c11", "-Wall", "-Wextra", "-fno-exceptions",
                 f"-I{INC}", f"-I{ECHO_INC}", UNIT_C, echo_so,
                 f"-Wl,-rpath,{so_dir}", "-o", unit_bin])
        check("U1 compile unit host-callback test zero warnings",
              r.returncode == 0 and
              not [ln for ln in r.stderr.splitlines() if "warning" in ln.lower()],
              r.stderr[-400:] if r.stderr else "")
        if r.returncode != 0:
            print(r.stderr)
            return 1
        r = run([unit_bin])
        check("U2 unit host-callback tests ALL PASS",
              r.returncode == 0 and "ALL PASS" in r.stdout,
              r.stdout + r.stderr)
        check("U3 unit module id matches",
              f"module={ECHO_MID}" in r.stdout, r.stdout)

        # ── 3. ABI-003 loader 动态加载（Linux 同源）──
        loader_o = os.path.join(work, "secure_loader.o")
        r = run([CC, "-std=c11", "-Wall", "-fno-exceptions", "-fPIC", "-c",
                 f"-I{INC}", f"-I{LOADER_DIR}",
                 os.path.join(LOADER_DIR, "secure_loader.c"), "-o", loader_o])
        check("L1 compile secure_loader", r.returncode == 0,
              r.stderr[-300:] if r.stderr else "")
        probe = os.path.join(work, "abi003_probe")
        r = run([CC, "-std=c11", "-Wall", "-fno-exceptions",
                 f"-I{INC}", f"-I{LOADER_DIR}", LOADER_PROBE_C, loader_o,
                 "-ldl", "-o", probe])
        check("L2 compile loader probe", r.returncode == 0,
              r.stderr[-300:] if r.stderr else "")
        if r.returncode != 0:
            return 1
        r = run([probe, "load", "module", echo_so, ECHO_MID, echo_sha,
                 ECHO_BUILD, so_dir, "1"])
        check("L3 dynamic load echo.so with hash+mid+build",
              r.returncode == 0 and "LOAD_OK" in r.stdout and
              "DESCRIBE_OK" in r.stdout and "RELEASE_OK" in r.stdout,
              r.stdout + r.stderr)
        check("L3 module_id matches module.yaml",
              f"module_id={ECHO_MID}" in r.stdout, r.stdout)
        check("L3 build matches ABI-005-echo",
              f"build={ECHO_BUILD}" in r.stdout, r.stdout)
        check("L3 loaded sha matches file sha",
              re.search(r"sha=([0-9a-f]{64})", r.stdout) and
              re.search(r"sha=([0-9a-f]{64})", r.stdout).group(1) == echo_sha,
              r.stdout)

        # ── 3b. 替换 DLL（篡改）→ hash mismatch 拒 ──
        tampered = os.path.join(work, "tampered_echo.so")
        data = bytearray(open(echo_so, "rb").read())
        data[len(data) // 2] ^= 0x01
        open(tampered, "wb").write(bytes(data))
        r = run([probe, "load", "module", tampered, ECHO_MID, echo_sha,
                 ECHO_BUILD, os.path.dirname(tampered), "1"])
        check("L4 replaced DLL hash mismatch rejected",
              "LOAD_FAIL" in r.stdout and "detail=5" in r.stdout,
              r.stdout + r.stderr)

        # ── 4. ABI-004 registry 三方一致正测 ──
        reg_o = os.path.join(work, "reg.o")
        r = run([CC, "-std=c11", "-Wall", "-fno-exceptions", "-fPIC", "-c",
                 f"-I{INC}", f"-I{REPO}",
                 os.path.join(REG_DIR, "module_registry.c"), "-o", reg_o])
        check("R1 compile module_registry", r.returncode == 0,
              r.stderr[-300:] if r.stderr else "")
        rprobe = os.path.join(work, "abi004_probe")
        r = run([CC, "-std=c11", "-Wall", "-fno-exceptions",
                 f"-I{INC}", f"-I{REPO}", f"-I{REG_DIR}", REG_PROBE_C,
                 loader_o, reg_o, "-ldl", "-o", rprobe])
        check("R2 compile registry probe", r.returncode == 0,
              r.stderr[-300:] if r.stderr else "")
        if r.returncode != 0:
            return 1
        # yaml 镜像: registry 按 rel_dir=modules 找 <yaml_root>/modules/module.yaml
        yaml_root = os.path.join(work, "yaml_root")
        os.makedirs(os.path.join(yaml_root, "modules"))
        shutil.copy(ECHO_YAML, os.path.join(yaml_root, "modules", "module.yaml"))

        base1 = os.path.join(work, "s1_clean")
        os.makedirs(os.path.join(base1, "modules"))
        shutil.copy(echo_so, os.path.join(base1, "modules", "astrocs_echo.so"))
        make_manifest(base1, [
            std_unit("MOD-ECHO", "module", "modules/astrocs_echo.so",
                     module_id=ECHO_MID, sha=echo_sha),
        ])
        r = run([rprobe, "check", os.path.join(base1, "astrocs.product.json"),
                 yaml_root, base1, "1"])
        check("R3 registry clean open", "CHECK_OK" in r.stdout, r.stdout + r.stderr)
        check("R3 zero findings", "issues=0" in r.stdout, r.stdout)
        r = run([rprobe, "dump", os.path.join(base1, "astrocs.product.json"),
                 yaml_root, base1, "0"])
        check("R3 entry loaded=1 mask=0",
              "entries=1" in r.stdout and "loaded=1" in r.stdout and
              "mask=0" in r.stdout, r.stdout)
        check("R3 module_id 三方一致 (manifest/dll/yaml)",
              re.search(r"module_id=" + re.escape(ECHO_MID) + r"\|"
                        r"mid_dll=" + re.escape(ECHO_MID) + r"\|"
                        r"mid_yaml=" + re.escape(ECHO_MID) + r"\|"
                        r"ver_dll=" + re.escape(ECHO_VER) + r"\|"
                        r"ver_yaml=" + re.escape(ECHO_VER) + r"\|",
                        r.stdout), r.stdout)
        check("R3 build_dll matches ABI-005-echo",
              f"build_dll={ECHO_BUILD}" in r.stdout, r.stdout)
        check("R3 hash 登记=实际",
              re.search(r"sha_reg=([0-9a-f]{12})", r.stdout) and
              re.search(r"sha_act=([0-9a-f]{12})", r.stdout) and
              re.search(r"sha_reg=([0-9a-f]{12})", r.stdout).group(1) ==
              re.search(r"sha_act=([0-9a-f]{12})", r.stdout).group(1),
              r.stdout)

        # ── 5. 删除 DLL → registry 报缺; 无静态 fallback ──
        base2 = os.path.join(work, "s2_deleted")
        os.makedirs(os.path.join(base2, "modules"))
        # manifest 声明 DLL 但目录内不放置文件 → 缺 DLL
        make_manifest(base2, [
            std_unit("MOD-ECHO", "module", "modules/astrocs_echo.so",
                     module_id=ECHO_MID, sha=echo_sha),
        ])
        r = run([rprobe, "check", os.path.join(base2, "astrocs.product.json"),
                 yaml_root, base2, "1"])
        check("N1 deleted DLL detected (loader missing)",
              "CHECK_OK" in r.stdout and "issues=1" in r.stdout and
              "kind=13" in r.stdout, r.stdout)
        r = run([rprobe, "dump", os.path.join(base2, "astrocs.product.json"),
                 yaml_root, base2, "0"])
        check("N1 deleted DLL loaded=0 (no fallback)",
              "loaded=0" in r.stdout and "mask=1" in r.stdout, r.stdout)
        check("N1 no entry becomes usable (no static copy loaded)",
              "loaded=1" not in r.stdout, r.stdout)

        # ── 6. 替换 DLL（内容被改）→ registry hash mismatch, 无 fallback ──
        base3 = os.path.join(work, "s3_replaced")
        os.makedirs(os.path.join(base3, "modules"))
        # 放入篡改副本（内容 ≠ 登记 sha）
        shutil.copy(tampered, os.path.join(base3, "modules", "astrocs_echo.so"))
        make_manifest(base3, [
            std_unit("MOD-ECHO", "module", "modules/astrocs_echo.so",
                     module_id=ECHO_MID, sha=echo_sha),
        ])
        r = run([rprobe, "check", os.path.join(base3, "astrocs.product.json"),
                 yaml_root, base3, "0"])
        check("N2 replaced DLL hash mismatch finding",
              "CHECK_OK" in r.stdout and "kind=12" in r.stdout, r.stdout)
        r = run([rprobe, "dump", os.path.join(base3, "astrocs.product.json"),
                 yaml_root, base3, "0"])
        check("N2 replaced DLL loaded=0 mask HASH",
              "loaded=0" in r.stdout and "mask=4" in r.stdout, r.stdout)

        # ── 7. 无静态 fallback（源码层）──
        banned = scan_no_static_echo()
        check("N3 host source has no static echo implementation call",
              not banned, str(banned))
        # loader/registry 对象: 不得有对入口符号的编译期未定义引用（仅 dlsym）
        r = run(["nm", "-u", loader_o, reg_o])
        check("N4 loader/registry objects have no undefined query ref",
              "astrocs_module_query_v1" not in r.stdout, r.stdout)

        print(f"\n{'='*60}\nresults: {len(FAILURES)} FAIL / "
              f"{CHECKS[0] - len(FAILURES)} PASS "
              f"(total {CHECKS[0]} checks)\n{'='*60}")
        if FAILURES:
            print("FAILED:", *FAILURES, sep="\n  ")
            return 1
        return 0
    finally:
        if os.environ.get("KEEP_ABI005_WORK"):
            print(f"KEEP workdir: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
