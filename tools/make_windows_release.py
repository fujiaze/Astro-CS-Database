#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_windows_release.py — WIN-009: 生成 Windows amd64 alpha 发布包(09 §5)。
正式包结构(单一 user exe + 私有运行时 DLL + manifest + SBOM/licenses + hash):
   AstroCS-Windows-amd64-<X.Y.Z-alpha.N>.zip
   └─ astrocs/                    (根目录, 便于解包)
      ├─ astrocs.exe              (唯一用户可执行; 无旧 phase/benchmark/tool exe)
      ├─ msvcp140.dll              (私有运行时; vcomp/vcruntime 同理)
      ├─ vcomp140.dll
      ├─ vcruntime140.dll
      ├─ vcruntime140_1.dll
      ├─ VERSION                   (0.10.0-alpha.2+g<commit12>)
      ├─ backends.manifest.json    (05 §7; builtin baseline 无 shipped DSO → 空 backend 表)
      ├─ SBOM.spdx.json            (SPDX 2.3, 包名 alpha)
      ├─ LICENSES/NOTICE.txt       (本包静态自带 libs 来源标注)
      ├─ MANIFEST.json             (每文件 path,sha256,size)
      └─ SHA256SUMS                (除自身外全文件 hash)
用法: python3 tools/make_windows_release.py --exe <astrocs.exe> --out <outdir>
      [--dlls "msvcp140.dll vcomp140.dll vcruntime140.dll vcruntime140_1.dll"]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION_FILE = os.path.join(REPO, "VERSION")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def git(*args):
    return subprocess.run(["git", "-C", REPO, *args], capture_output=True,
                          text=True).stdout.strip()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True, help="Release astrocs.exe 路径")
    ap.add_argument("--out", required=True, help="输出目录(写入 zip + .sha256)")
    ap.add_argument("--dlls", default="", help="空格分隔的私有 DLL 路径列表")
    args = ap.parse_args()

    if not os.path.isfile(args.exe):
        print(f"ERR: exe not found {args.exe}", file=sys.stderr)
        return 2
    dll_paths = [d for d in args.dlls.split() if d]
    for d in dll_paths:
        if not os.path.isfile(d):
            print(f"ERR: dll not found {d}", file=sys.stderr)
            return 2

    base = open(VERSION_FILE, encoding="utf-8").read().strip() if os.path.isfile(VERSION_FILE) else "0.10.0-alpha.2"
    commit = git("rev-parse", "HEAD")
    c12 = commit[:12]
    pkg_version = f"{base}+g{c12}"
    pkg_name_base = f"AstroCS-Windows-amd64-{base}"

    work = tempfile.mkdtemp(prefix="winrel_")
    root = os.path.join(work, "astrocs")
    os.makedirs(root)
    os.makedirs(os.path.join(root, "LICENSES"))

    # 1) 唯一用户 exe(Windows 需 .exe 扩展名)
    shutil.copy2(args.exe, os.path.join(root, "astrocs.exe"))
    # 2) 私有运行时 DLL
    for d in dll_paths:
        shutil.copy2(d, os.path.join(root, os.path.basename(d)))

    # 3) VERSION(clean main 的 X.Y.Z-alpha.N+g<commit12>)
    with open(os.path.join(root, "VERSION"), "w", encoding="utf-8") as f:
        f.write(pkg_version + "\n")

    # 4) backends.manifest.json (05 §7): builtin baseline 无 shipped DSO
    bm = {
        "schema_version": 1,
        "generated_at_utc": "2026-08-30T16:00:00Z",
        "platform": "win-amd64",
        "commit": commit,
        "builtin_baseline": True,
        "backends": [],
        "note": "V5 CLI ships builtin baseline (no backend DLL); no shipped backend files",
    }
    with open(os.path.join(root, "backends.manifest.json"), "w", encoding="utf-8") as f:
        json.dump(bm, f, indent=1, ensure_ascii=False)

    # 5) SBOM (SPDX 2.3): 单一 CONTAINER(AstroCS CLI), 无外部可交付二进制
    license_text = (
        "AstroCS CLI 发布包自带许可证。仓库内第三方许可见控制包 PACKAGE_MANIFEST.md 与各 third_party LICENSE。"
        "本包对交付对象(AstroCS Windows amd64 单一 CLI + 私有 VC 运行时)建档。\n"
        "静态自带的第三方: cfitsio(BSD), zlib(zlib license), nlohmann/json(MIT), "
        "HEALPix(见 lib/plate_solve/LICENSE 等), OpenMP/VC 运行时(MSVC 红分发)。\n"
    )
    with open(os.path.join(root, "LICENSES", "NOTICE.txt"), "w", encoding="utf-8") as f:
        f.write(license_text)
    sbom = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"AstroCS-Windows-amd64-{base}",
        "documentNamespace": f"https://astrocs.local/spdx/{c12}",
        "creationInfo": {"created": "2026-08-30T16:00:00Z",
                         "creators": ["Tool:make_windows_release.py"]},
        "packages": [{
            "SPDXID": "SPDXRef-Package-AstroCS",
            "name": "AstroCS",
            "versionInfo": pkg_version,
            "downloadLocation": "NOASSERTION",
            "filesAnalyzed": True,
            "licenseConcluded": "NOASSERTION",
            "supplier": "Organization:AstroCS",
            "primaryPackagePurpose": "APPLICATION",
        }],
    }
    with open(os.path.join(root, "SBOM.spdx.json"), "w", encoding="utf-8") as f:
        json.dump(sbom, f, indent=1)

    # 6) MANIFEST.json(逐文件 path/sha256/size)
    entries = []
    for dirpath, _, files in os.walk(root):
        for fn in sorted(files):
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            entries.append({
                "path": rel,
                "sha256": sha256_file(full),
                "size": os.path.getsize(full),
            })
    manifest = {
        "schema_version": 1,
        "package": pkg_name_base,
        "version": pkg_version,
        "commit": commit,
        "platform": "win-amd64",
        "files": entries,
    }
    with open(os.path.join(root, "MANIFEST.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1, ensure_ascii=False)

    # 7) SHA256SUMS(manifest + 各文件, 含 VERSION/SBOM/licenses)
    sums = []
    for dirpath, _, files in os.walk(root):
        for fn in sorted(files):
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            sums.append(f"{sha256_file(full)}  astrocs/{rel}")
    with open(os.path.join(root, "SHA256SUMS"), "w", encoding="utf-8") as f:
        f.write("\n".join(sums) + "\n")

    # 8) 打 zip + .sha256
    os.makedirs(args.out, exist_ok=True)
    arch = os.path.join(args.out, f"{pkg_name_base}.zip")
    with zipfile.ZipFile(arch, "w", zipfile.ZIP_DEFLATED) as z:
        for dirpath, _, files in os.walk(root):
            for fn in sorted(files):
                full = os.path.join(dirpath, fn)
                arc = os.path.relpath(full, work).replace(os.sep, "/")
                z.write(full, arc)
    sums = sha256_file(arch)
    with open(arch + ".sha256", "w", encoding="utf-8") as f:
        f.write(f"{sums}  {os.path.basename(arch)}")

    print(f"PKG {arch}")
    print(f"SHA `${sums}`")
    print(f"FILES {len(entries)} version={pkg_version}")
    shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
