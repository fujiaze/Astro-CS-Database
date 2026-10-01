#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""remote_pe_isa_manifest.json — PE/COFF 产物面判据的**远端执行清单**（Windows x64）。

为什么有这份清单:
  产物面 ISA 判据必须能在**发行平台的产物**上判红才成立。本机（Linux x86-64）能
  实跑 ELF 腿，也能用 clang 生成 COFF 容器实跑 llvm-objdump/GNU-objdump 路径，
  但 **MSVC 真实工具链的代码生成**与 **dumpbin /disasm 路径**只能在 Windows 上跑 ——
  本机既无 MSVC SDK，也无 dumpbin。这一段因此是「可回执的远端执行清单」，
  不是「已通过」。

  ***未运行 ≠ 通过。***  下方 status=NOT_RUN 的条目在本机一次都没有跑过；
  任何引用本清单的结论都必须在远端跑完并把 result 字段从 NOT_RUN 改成实际回执后，
  才允许出现在交付说明里。

回执要求（远端执行者按此填）:
  · 每条 run 记录 exit_code、产物 sha256、所用工具与版本；
  · 每条 expect=RED 的用例必须给出 FAIL 行原文（证明不是空转判红）;
  · 产物与反汇编文本作为回执附件留存（不重编译也能复算）。

用法（远端，仓库根目录下）:
  python3 eng/tools/quality/remote_pe_isa_run.py --list
  python3 eng/tools/quality/remote_pe_isa_run.py --phase build
  python3 eng/tools/quality/remote_pe_isa_run.py --phase gate
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MANIFEST = os.path.join(REPO, "eng", "tools", "quality", "remote_pe_isa_manifest.json")
HERE = os.path.dirname(os.path.abspath(__file__))

DOC = {
    "schema_version": 1,
    "id": "ISA-PE-REMOTE",
    "title": "PE/COFF 产物面 ISA 判据 · 远端执行清单（Windows x64 / MSVC + dumpbin）",
    "status": "NOT_RUN",
    "not_run_means": "NOT_RUN != PASS。本清单任何条目在本机一次都没有执行；未跑过的判据"
                     "不得在交付说明、门禁报表或台账中按「通过」引用。",
    "retired_steps_note": "下方 phase 条目中，cmd 引用 eng/tools/quality/check_variant_isa_disasm.py 与 "
                          "check_manifest_isa_artifact.py 的几步，其判据脚本已随 G08-01 门禁删除一并移除。"
                          "此处按远端清单的**历史记录原样保留**（改写即伪造当时的执行记录），它们不是本机"
                          "执行面：仅在显式 --phase gate 时才由 subprocess 发起，且 status 恒为 NOT_RUN。"
                          "门禁重建（G08-10）时以新判据重写本清单。",
    "why_remote_only": [
        "MSVC 真实代码生成需 Windows SDK/cl 环境，本机无（Linux 开发节点）。",
        "dumpbin 是 Windows 自带工具，本机无（已实测 which dumpbin = MISSING）。",
        "本机已实跑的是：ELF 腿（GNU objdump 2.44）+ COFF 容器腿（llvm-objdump 18.1.8 / "
        "GNU objdump 2.44，容器由 GNU objcopy -O pe-x86-64 从 clang 生成的真实变体 TU 转换）。"
        "MSVC 代码生成本身**未**被任何一次执行覆盖。",
    ],
    "prereq": [
        "Windows x64，Visual Studio 2022（cl 19.3x）与 Windows SDK",
        "仓库已 checkout 到本地路径（脚本按仓库相对路径工作）",
        "已在 Windows 上完成一次 Release 配置+构建：cmake -S . -B build -G Ninja "
        "-DCMAKE_BUILD_TYPE=Release",
    ],
    "phases": [
        {
            "id": "build",
            "note": "产出 Windows 侧变体产物（PE/COFF）与清单",
            "steps": [
                {
                    "id": "PE-BUILD",
                    "cmd": ["cmake", "--build", "build", "--config", "Release",
                            "--target", "acsd_cpu_avx2", "acsd_cpu_avx512"],
                    "expect_exit": 0,
                },
                {
                    "id": "PE-MANIFEST",
                    "cmd": ["python3", "eng/tools/gen_provider_manifests.py",
                            "--repo", ".", "--build-dir", "build",
                            "--providers-dir", "build/providers", "--isa-only",
                            "--out", "build/providers/backends.manifest.json",
                            "--compiler", "MSVC-19.38.33130.0"],
                    "expect_exit": 0,
                    "note": "MSVC 腿的声明位 = /arch: 档位许可面（avx512 含 CD = 992）。"
                            "rc=5 即声明面与旗标口径失配（该拒出）。",
                },
            ],
        },
        {
            "id": "gate",
            "note": "同一套产物面判据在 PE/COFF 产物上跑；dumpbin 腿与 objdump 腿各跑一遍",
            "steps": [
                {
                    "id": "PE-DUMPBIN-TEXT",
                    "cmd": ["dumpbin", "/nologo", "/disasm",
                            "build/providers/acsd_cpu_avx512.dll",
                            ">", "run/pe/avx512_dumpbin.txt"],
                    "expect_exit": 0,
                    "produces": "run/pe/avx512_dumpbin.txt",
                },
                {
                    "id": "PE-DUMPBIN-CHECK",
                    "cmd": ["python3", "eng/tools/quality/check_variant_isa_disasm.py",
                            "--text", "run/pe/avx512_dumpbin.txt",
                            "--isa", "avx512",
                            "--hit", "acsd_variant_kernel_dispatch_v1",
                            "--require-feature", "avx512f",
                            "--require-feature", "avx512vl",
                            "--declared-features", "avx512f,avx512cd,avx512bw,avx512dq,avx512vl"],
                    "expect_exit": 0,
                },
                {
                    "id": "PE-DUMPBIN-FACE-CLEAN",
                    "cmd": ["python3", "eng/tools/quality/check_variant_isa_disasm.py",
                            "--binary", "build/providers/acsd_cpu_avx512.dll",
                            "--dumpbin", "dumpbin",
                            "--clean-symbol", "acsd_backend_get_api_v1",
                            "--clean-symbol", "backend_self_test"],
                    "expect_exit": 0,
                },
                {
                    "id": "PE-LLVM-OBJDUMP-GATE",
                    "cmd": ["python3", "eng/tools/quality/check_manifest_isa_artifact.py",
                            "--manifest", "build/providers/backends.manifest.json",
                            "--artifacts-dir", "build/providers",
                            "--objdump", "llvm-objdump"],
                    "expect_exit": 0,
                    "note": "清单↔产物交叉（M1..M6）。若判红在 M5（用了却没声明），"
                            "那是真缺陷，不得改判据消红 —— 按 finding 流程处置。",
                },
                {
                    "id": "PE-LLVM-OBJDUMP-NEG-MISMATCH",
                    "cmd": ["python3", "eng/tools/quality/check_manifest_isa_artifact.py",
                            "--manifest", "build/providers/backends.manifest.json",
                            "--artifacts-dir", "build/providers",
                            "--objdump", "llvm-objdump",
                            "--inject", "declared-avx512-artifact-avx2"],
                    "expect_exit": 1,
                    "note": "负例自证：喂一个不匹配的产物必须判红（红=判据在工作）。",
                },
            ],
        },
    ],
    "receipt_template": {
        "executed_on": "<host / os / cpu>",
        "toolchain": {"cl": "<cl 完整版本行>", "dumpbin": "<dumpbin /? 首行版本>",
                      "llvm_objdump": "<llvm-objdump --version 首行，或 N/A>"},
        "steps": [
            {"id": "PE-BUILD", "exit_code": None, "artifact_sha256": None, "note": ""},
            {"id": "PE-MANIFEST", "exit_code": None, "note": ""},
            {"id": "PE-DUMPBIN-TEXT", "exit_code": None, "output_sha256": None, "note": ""},
            {"id": "PE-DUMPBIN-CHECK", "exit_code": None, "stdout_tail": "", "note": ""},
            {"id": "PE-DUMPBIN-FACE-CLEAN", "exit_code": None, "stdout_tail": "", "note": ""},
            {"id": "PE-LLVM-OBJDUMP-GATE", "exit_code": None, "stdout_tail": "", "note": ""},
            {"id": "PE-LLVM-OBJDUMP-NEG-MISMATCH", "exit_code": None, "stdout_tail": "",
             "note": "必须给 FAIL 行原文"},
        ],
        "verdict": "NOT_RUN",
    },
}


def dump():
    with open(MANIFEST, "w", encoding="utf-8") as f:
        json.dump(DOC, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(MANIFEST)


def phase(name, dry=False):
    if name not in ("build", "gate"):
        print("--phase 只接受 build|gate", file=sys.stderr)
        return 2
    rc = 0
    for ph in DOC["phases"]:
        if ph["id"] != name:
            continue
        for st in ph["steps"]:
            cmd = st["cmd"]
            printable = " ".join(cmd)
            if dry:
                print("[DRY] %-26s %s   (expect exit %d)" % (st["id"], printable,
                                                             st["expect_exit"]))
                continue
            print("[RUN ] %-26s %s" % (st["id"], printable), flush=True)
            r = subprocess.run(cmd, cwd=REPO)
            ok = (r.returncode == st["expect_exit"])
            print("       -> exit=%d expect=%d %s"
                  % (r.returncode, st["expect_exit"], "OK" if ok else "MISMATCH"), flush=True)
            if not ok:
                rc = 1
    if not dry:
        print("\n远端回执尚未填写 ⇒ verdict 仍为 NOT_RUN。**未运行 ≠ 通过。**")
    return rc


def main(argv=None):
    ap = argparse.ArgumentParser(description="PE/COFF 产物面判据 · 远端执行清单")
    ap.add_argument("--emit", action="store_true", help="写出 remote_pe_isa_manifest.json")
    ap.add_argument("--list", action="store_true", help="列出全部步骤（不执行）")
    ap.add_argument("--phase", default="", choices=("", "build", "gate"))
    ap.add_argument("--dry-run", action="store_true", help="只打印命令不执行")
    a = ap.parse_args(argv)
    if a.emit:
        dump()
        return 0
    if a.list or not a.phase:
        print("status = %s —— %s" % (DOC["status"], DOC["not_run_means"]))
        for ph in DOC["phases"]:
            print("\n[%s] %s" % (ph["id"], ph["note"]))
            for st in ph["steps"]:
                print("  %-26s expect exit %d :: %s"
                      % (st["id"], st["expect_exit"], " ".join(st["cmd"])))
        return 0
    return phase(a.phase, a.dry_run)


if __name__ == "__main__":
    sys.exit(main())
