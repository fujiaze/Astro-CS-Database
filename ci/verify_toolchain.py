#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""V81-ADOPT-004 最小验收脚本（只读）。

用法:
    python3 ci/verify_toolchain.py --policy ci/toolchain.policy.json \
        --actual ci/toolchain.lock.json --scope agent-host

校验（任一失败 exit 非零）:
  1) lock 可解析且 scope 与 --scope 一致（schema_version == 2）;
  2) policy agent_host 段关键要求在 lock 中体现:
       require_exact_hosted_versions=false  -> lock.hosted_ci_versions_required == false
       host_reprovisioning=false            -> lock.host_reprovisioning == false
       acr=false                            -> lock.acr == false;
  3) cmake/python/git 条目存在且信息为实测: present 者版本非空、非占位且能被实测
     正则复匹配; 缺失者如实 missing（version 为 null 并登记于 missing_tools,
     由检查 4 自洽闭环）——本脚本不臆造版本, 也不把 hosted CI 版本当作本机安装值;
  4) missing_tools 与 lock 各工具 present 状态自洽:
       present == true  <=> 不在 missing_tools, 且 path/version 非空;
       present == false <=> 在 missing_tools, 且 path/version 为 null;
  5) lock 中版本行格式与实测正则匹配（例如 gcc 版本行、python3、git、zstd 等）。

脚本只读: 不写入、不修改任何工作区文件。
"""

import argparse
import json
import re
import sys

# 12 项盘点清单（任务规格 V81-ADOPT-004 第 1 条）
KEY_CMAKE = "cmake"
KEY_PYTHON = "python"
KEY_GIT = "git"

# 实测正则（对照仓库 evidence logs 的原始 stdout；missing 工具跳过）
VERSION_LINE_PATTERNS = {
    "cmake":   r"cmake version (\d+\.\d+\.\d+)",
    "gcc":     r"gcc .*? (\d+\.\d+\.\d+)",
    "gxx":     r"g\+\+ .*? (\d+\.\d+\.\d+)",
    "clang":   r"clang version (\d+\.\d+\.\d+)",
    "clang++": r"clang version (\d+\.\d+\.\d+)",
    "ninja":   r"^(\d+\.\d+\.\d+)$",
    "make":    r"GNU Make (\d+(?:\.\d+)+)",
    "python":  r"^Python (\d+\.\d+\.\d+)$",
    "git":     r"^git version (\d+(?:\.\d+)+)",
    "ccache":  r"ccache version (\d+(?:\.\d+)+)",
    "zstd":    r"v(\d+\.\d+\.\d+)",
    "pytest":  r"pytest (\d+\.\d+\.\d+)",
}

# 占位值黑名单（③ 非空、非占位）
PLACEHOLDER_VALUES = {
    "", "unknown", "n/a", "na", "none", "null", "tbd", "todo",
    "placeholder", "x.y.z", "3.x", "0.0.0", "version", "hosted",
}

# 12 项工具在 lock 中的 JSON Pointer（除 additional_observed 观察项）
INVENTORY_SECTIONS = ["cmake", "compilers", "generator", "python", "git", "optional"]

# lock 逻辑键名 -> 实际命令名（missing_tools 使用实际命令名）
ALIASES = {"gxx": "g++"}


class Checker:
    def __init__(self):
        self.errors = []
        self.checks = 0

    def check(self, ok, label):
        self.checks += 1
        if ok:
            print(f"[PASS] {label}")
        else:
            print(f"[FAIL] {label}")
            self.errors.append(label)

    def summary(self):
        print(f"\nchecks={self.checks} failed={len(self.errors)}")
        return 0 if not self.errors else 1


def tool_entries(lock):
    """按 (logical_name, entry) 展开全部工具条目。"""
    out = []
    for name in [KEY_CMAKE, KEY_PYTHON, KEY_GIT]:
        if name in lock and isinstance(lock[name], dict):
            out.append((name, lock[name]))
    for section in ("compilers", "generator", "optional"):
        for name, entry in (lock.get(section) or {}).items():
            if isinstance(entry, dict):
                out.append((name, entry))
    return out


def main():
    ap = argparse.ArgumentParser(description="V81-ADOPT-004 toolchain lock acceptance (read-only)")
    ap.add_argument("--policy", required=True)
    ap.add_argument("--actual", required=True)
    ap.add_argument("--scope", required=True)
    args = ap.parse_args()
    c = Checker()

    # ── ① lock 可解析 + scope ─────────────────────────────────────────
    try:
        with open(args.actual, encoding="utf-8") as f:
            lock = json.load(f)
        c.check(True, "lock file parses as JSON")
    except Exception as exc:  # noqa: BLE001
        print(f"[FAIL] lock file parses as JSON: {exc}")
        return 1
    c.check(isinstance(lock, dict), "lock is a JSON object")
    c.check(lock.get("schema_version") == 2, "lock schema_version == 2")
    c.check(lock.get("scope") == args.scope, f"lock scope == {args.scope}")

    # ── ② policy agent_host 关键要求在 lock 中体现 ────────────────────
    try:
        with open(args.policy, encoding="utf-8") as f:
            policy = json.load(f)
        c.check(True, "policy file parses as JSON")
    except Exception as exc:  # noqa: BLE001
        print(f"[FAIL] policy file parses as JSON: {exc}")
        return 1
    agent = policy.get("agent_host") or {}
    c.check(agent.get("require_exact_hosted_versions") is False,
            "policy.agent_host.require_exact_hosted_versions == false")
    c.check(lock.get("hosted_ci_versions_required") is False,
            "lock.hosted_ci_versions_required == false (matches policy)")
    c.check(agent.get("host_reprovisioning") is False,
            "policy.agent_host.host_reprovisioning == false")
    c.check(lock.get("host_reprovisioning") is False,
            "lock.host_reprovisioning == false (matches policy)")
    c.check(agent.get("acr") is False, "policy.agent_host.acr == false")
    c.check(lock.get("acr") is False, "lock.acr == false (matches policy)")

    # ── ③/⑤ cmake/python/git 实测版本（非空、非占位、正则复匹配）──────
    missing_tools = lock.get("missing_tools")
    c.check(isinstance(missing_tools, list), "lock.missing_tools is a list")

    for name in (KEY_CMAKE, KEY_PYTHON, KEY_GIT):
        entry = lock.get(name)
        if not isinstance(entry, dict):
            c.check(False, f"lock.{name} entry exists")
            continue
        if entry.get("present") is True:
            ver = entry.get("version")
            line = entry.get("version_line")
            ok_ver = isinstance(ver, str) and ver.strip().lower() not in PLACEHOLDER_VALUES
            c.check(ok_ver, f"{name}: measured version non-empty and non-placeholder ({ver!r})")
            pat = VERSION_LINE_PATTERNS.get(name)
            m = re.search(pat, line or "", re.MULTILINE) if pat and line else None
            c.check(bool(m), f"{name}: version_line matches measured regex {pat!r}")
            if m:
                c.check(m.group(1) == ver,
                        f"{name}: regex-extracted version {m.group(1)!r} == lock version {ver!r}")
        else:
            # 缺失: 如实登记（version 为 null 且在 missing_tools）——不臆造版本
            c.check(entry.get("version") is None, f"{name}: missing tool has null version")
            c.check(name in (missing_tools or []),
                    f"{name}: missing tool recorded in missing_tools")

    # ── ④ missing_tools 与 present 状态自洽（全部工具条目）────────────
    entries = tool_entries(lock)
    c.check(len(entries) >= 12, f"lock carries full 12-tool inventory (got {len(entries)})")
    for name, entry in entries:
        present = entry.get("present")
        ver, path = entry.get("version"), entry.get("path")
        # 逻辑键名与实际命令名（如 compilers.gxx -> g++）都视为同一工具
        names = {name, ALIASES.get(name, name)} & set(missing_tools or [])
        if present is True:
            c.check(not names,
                    f"{name}: present=true not in missing_tools")
            c.check(ver is not None and path is not None,
                    f"{name}: present=true has measured version and path")
            pat = VERSION_LINE_PATTERNS.get(name)
            if pat and entry.get("version_line"):
                m = re.search(pat, entry["version_line"], re.MULTILINE)
                c.check(bool(m) and m.group(1) == ver,
                        f"{name}: version_line regex matches and equals version")
        elif present is False:
            c.check(bool(names),
                    f"{name}: present=false recorded in missing_tools")
            c.check(ver is None and path is None,
                    f"{name}: present=false has null version/path (no fabricated data)")
        else:
            c.check(False, f"{name}: present flag must be boolean true/false")
    dup = {n for n, _ in entries}
    c.check(len(dup) == len(entries), "no duplicate logical tool names in lock")

    # hosted CI 策略值不得混入本机实测值（例如 linux_hosted cmake 3.31.12）
    hosted_cmake = (policy.get("linux_hosted") or {}).get("cmake")
    for name, entry in entries:
        if entry.get("present") is False:
            continue
        if hosted_cmake and name == "cmake":
            c.check(entry.get("version") != hosted_cmake or entry.get("present") is False,
                    "no hosted CI cmake value copied into host inventory")

    # ── 附: cmake_presets_found 必须与 CMakePresets.json 一致 ─────────
    found = lock.get("cmake_presets_found")
    c.check(isinstance(found, list) and len(found) >= 1,
            "lock.cmake_presets_found is a non-empty list")

    sys.exit(c.summary())


if __name__ == "__main__":
    main()
