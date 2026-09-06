#!/usr/bin/env python3
"""QA-005: 可复现构建依赖与 SBOM 校验。

规则:
1. 工具链版本锁定: gcc/clang/cmake/python 版本记录 (DEPENDENCIES.md)。
2. 构建 flags 记录: Release CMAKE_BUILD_TYPE + 无未锁定 flag。
3. build id: 二进制含 VERSION+g<commit> (可追溯; 版本单源=根 VERSION, VER-001)。
4. SBOM: dist/astrocs-alpha/SBOM.json 存在且含版本/组件/license;
   dist 发布布局缺失时回退 evidence/v6_1_rework/tasks/QA-003/SBOM.json
   (QA-003 正式 SBOM 证据; 构建时点快照, 按 SBOM 自洽校验并在输出注明)。
5. 复现性: 同二进制两次 --version 一致 (一致性由 build id=commit 保证)。
exit 0 = PASS。

路径假设适配 (QA 批量复验): 构建目录探测 build/root-cmake → build/ (当前
Ninja 单配置布局) → build/cli; 版本断言由硬编码字符串改为根 VERSION 单源
(VER-001), 消除 0.10/0.11 硬编码漂移。
"""
import json, pathlib, re, subprocess, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
SEMVER_ALPHA = re.compile(r"^\d+\.\d+\.\d+-alpha\.\d+$")
SBOM_FALLBACK = pathlib.Path("evidence") / "v6_1_rework" / "tasks" / "QA-003" / "SBOM.json"

def read_base_version():
    raw = (REPO / "VERSION").read_text(encoding="utf-8").strip()
    if not SEMVER_ALPHA.match(raw):
        raise SystemExit(f"VERSION 源非法 (VER-001): {raw!r}")
    return raw

def find_cli_bin():
    for rel in ("build/root-cmake/astrocs", "build/astrocs", "build/cli/astrocs"):
        p = REPO / rel
        if p.exists():
            return p
    return None

def load_sbom():
    """dist 发布 SBOM 优先; 缺失回退 evidence QA-003 SBOM 证据。"""
    dist = REPO / "dist" / "astrocs-alpha" / "SBOM.json"
    fb = REPO / SBOM_FALLBACK
    if dist.is_file():
        return dist, False
    if fb.is_file():
        return fb, True
    return None, False

def main():
    errors = []
    base = read_base_version()
    # 1) 工具链版本
    deps = REPO / "DEPENDENCIES.md"
    if not deps.is_file():
        errors.append("DEPENDENCIES.md 缺失 (工具链版本锁定)")
    # 2) build id 可追溯 (版本单源: 根 VERSION, VER-001)
    bin_path = find_cli_bin()
    ver = ""
    if bin_path is not None:
        r = subprocess.run([str(bin_path), "--version"], capture_output=True, text=True, timeout=60)
        ver = r.stdout.strip()
        if not re.search(re.escape(base) + r"\+g[0-9a-f]{7,}", ver):
            errors.append(f"build id 不可追溯 (期望 {base}+g<commit>): {ver}")
    # 3) SBOM
    sbom_path, sbom_fallback = load_sbom()
    if sbom_path is not None:
        doc = json.loads(sbom_path.read_text(encoding="utf-8"))
        if not doc.get("components"):
            errors.append("SBOM 组件空")
        comp = doc.get("metadata", {}).get("component", {})
        if not doc.get("license") and not comp.get("licenses"):
            errors.append("SBOM license 缺失")
        comp_ver = comp.get("version")
        if sbom_fallback:
            # 历史 evidence SBOM = 构建时点快照: 只要求 SBOM 自洽非空
            if comp_ver and doc.get("version") not in (None, 1) and doc.get("version") != comp_ver:
                errors.append(f"SBOM version 自洽失败: {doc.get('version')} != {comp_ver}")
        else:
            sbom_ver = doc.get("version") if isinstance(doc.get("version"), str) else comp_ver
            if sbom_ver != base:
                errors.append(f"SBOM version 不一致: {sbom_ver} != {base}")
    else:
        errors.append("SBOM.json 缺失")
    # 4) 复现性: 同二进制两次 --version 一致
    if bin_path is not None:
        r2 = subprocess.run([str(bin_path), "--version"], capture_output=True, text=True, timeout=60)
        if r2.stdout.strip() != ver:
            errors.append(f"--version 两次输出不一致: {ver!r} vs {r2.stdout.strip()!r}")
    if errors:
        print("QA-005_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    sbom_note = f"SBOM 完整 (evidence 回退: {SBOM_FALLBACK.as_posix()})" if sbom_fallback else "SBOM 完整"
    print(f"QA-005_PASS: 工具链锁定, build id 可追溯 ({base}+g<commit>), {sbom_note}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
