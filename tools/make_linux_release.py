#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_linux_release.py — LNX-005: 生成 Linux amd64 alpha 发布包(09 §5 / 13 § 版本 + 05 §7 manifest)。
正式包结构(单一 user exe + manifest + SBOM/licenses + hash):
   AstroCS-Linux-amd64-<X.Y.Z-alpha.N>.tar.zst
   └─ astrocs/                     (根目录, 便于解包)
      ├─ bin/astrocs               (唯一用户可执行; 无旧 phase/benchmark/tool exe)
      ├─ MANIFEST.json             (每文件 path,sha256,size,mode; 单源: gen_version.py)
      ├─ backends.manifest.json    (05 §7; builtin baseline 无 shipped DSO → 空 backend 表)
      ├─ SBOM.spdx.json            (SPDX 2.3, 包名 alpha)
      ├─ LICENSES/                 (第三方/自有许可证; 本包 CLI 静态自带 libs → 标注来源)
      ├─ VERSION                    (0.9.0-alpha.1+g<commit12>)
      └─ SHA256SUMS                (除自身外全文件 hash; tar.zst 外层另附 .sha256)
用法: python3 tools/make_linux_release.py --bin <astrocs> --out <outdir> [--tar-gz]
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
                          text=True, check=True).stdout.strip()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True, help="Release astrocs 可执行路径")
    ap.add_argument("--out", required=True, help="输出目录(将写入 tar 包 + .sha256)")
    ap.add_argument("--tar-gz", action="store_true", help="zstd 不可用时以 .tar.gz 替代(09 §5 需记录)")
    args = ap.parse_args()

    if not os.path.isfile(args.bin):
        print(f"ERR: binary not found {args.bin}", file=sys.stderr)
        return 2
    base = open(VERSION_FILE, encoding="utf-8").read().strip() if os.path.isfile(VERSION_FILE) else "0.9.0-alpha.1"
    commit = git("rev-parse", "HEAD")
    c12 = commit[:12]
    pkg_version = f"{base}+g{c12}"
    pkg_name_base = f"AstroCS-Linux-amd64-{base}"

    work = tempfile.mkdtemp(prefix="lnxrel_")
    root = os.path.join(work, "astrocs")
    os.makedirs(os.path.join(root, "bin"))
    os.makedirs(os.path.join(root, "LICENSES"))

    # 1) 唯一用户 exe
    bin_dst = os.path.join(root, "bin", "astrocs")
    shutil.copy2(args.bin, bin_dst)

    # 2) VERSION(clean main 的 X.Y.Z-alpha.N+g<commit12>, 不带 dirty)
    with open(os.path.join(root, "VERSION"), "w", encoding="utf-8") as f:
        f.write(pkg_version + "\n")

    # 3) backends.manifest.json (05 §7): builtin baseline 无 shipped DSO
    bm = {
        "schema_version": 1,
        "generated_at_utc": "2026-08-30T10:45:00Z",
        "platform": "linux-amd64",
        "commit": commit,
        "builtin_baseline": True,
        "backends": [],
        "note": "V5 CLI ships builtin baseline (no backend DSO); no shipped backend files",
    }
    with open(os.path.join(root, "backends.manifest.json"), "w", encoding="utf-8") as f:
        json.dump(bm, f, indent=1, ensure_ascii=False)

    # 4) SBOM (SPDX 2.3): 本包由单一 CONTAINER(AstroCS CLI)承载, 无外部可交付二进制
    license_text = (
        "AstroCS CLI 发布包自带许可证。仓库内第三方许可见工程控制/RELEASE_V5/.../"
        "PACKAGE_MANIFEST.md 与各 third_party 目录 LICENSE。"
        "本 SBOM 对发布包交付对象(AstroCS Linux amd64 单一 CLI)建档。"
    )
    with open(os.path.join(root, "LICENSES", "NOTICE.txt"), "w", encoding="utf-8") as f:
        f.write(license_text + "\n")
    sbom = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"AstroCS-Linux-amd64-{base}",
        "documentNamespace": f"https://astrocs.local/spdx/{c12}",
        "creationInfo": {"created": "2026-08-30T10:45:00Z",
                         "creators": ["Tool:make_linux_release.py"]},
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

    # 5) MANIFEST.json(逐文件 path/sha256/size/mode)
    entries = []
    for dirpath, _, files in os.walk(root):
        for fn in sorted(files):
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            entries.append({
                "path": rel,
                "sha256": sha256_file(full),
                "size": os.path.getsize(full),
                "mode": oct(os.stat(full).st_mode & 0o777),
            })
    manifest = {
        "schema_version": 1,
        "package": pkg_name_base,
        "version": pkg_version,
        "commit": commit,
        "platform": "linux-amd64",
        "files": entries,
    }
    with open(os.path.join(root, "MANIFEST.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=1, ensure_ascii=False)

    # 6) SHA256SUMS(manifest + 各文件, 含 VERSION/SBOM/licenses)
    sums = []
    for dirpath, _, files in os.walk(root):
        for fn in sorted(files):
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            sums.append(f"{sha256_file(full)}  astrocs/{rel}")
    with open(os.path.join(root, "SHA256SUMS"), "w", encoding="utf-8") as f:
        f.write("\n".join(sums) + "\n")

    # 7) 打 tar(zstd 优先; 不可用则 .tar.gz 记录)
    os.makedirs(args.out, exist_ok=True)
    if args.tar_gz or shutil.which("zstd") is None:
        arch = os.path.join(args.out, f"{pkg_name_base}.tar.gz")
        subprocess.run(["tar", "-C", work, "-czf", arch, "astrocs"], check=True)
        arch_type = "tar.gz"
    else:
        arch = os.path.join(args.out, f"{pkg_name_base}.tar.zst")
        subprocess.run(["tar", "-C", work, "--zstd", "-cf", arch, "astrocs"], check=True)
        arch_type = "tar.zst"
    sums = sha256_file(arch)
    with open(arch + ".sha256", "w", encoding="utf-8") as f:
        f.write(f"{sums}  {os.path.basename(arch)}")

    print(f"PKG {arch}")
    print(f"SHA `${sums}`")
    print(f"FILES {len(entries)} version={pkg_version} arch={arch_type}")
    shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
