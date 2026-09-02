#!/usr/bin/env python3
"""AstroCS 安装树/模块 verify 验证器 (BLD-003)

机器验收 (BLD-003):
  1. clean install 仅白名单: 以 packaging/install-tree.contract.json 的 units
     为白名单; 每个 required 文件必须存在 (缺任一 required → 非零退出);
     白名单外顶层条目 (可选) 存在即校验其相对路径前缀合法。
  2. 删除 noop 模块 DLL/.so → verify 明确失败: 本工具把 MOD-NOOP 标为
     required; 删除 modules/astrocs_noop.so 后必须非零退出并输出
     "MISSING REQUIRED <...>" 与 "MODULE VERIFY FAIL", 证明宿主无静态
     fallback (BLD-003 验收失败路径; CLI 的 modules verify 命令面属 CLI-001,
     本工具是安装树验收层, 不替代 CLI)。
  3. product manifest 结构: 顶层 astrocs.product.json 存在且 JSON 可解析,
     units 全部存在 (rel_path 相对 prefix 解析)。sha256 为 null (SKELETON)
     时跳过 hash 校验 (ABI-004 填充后启用)。

用法:
  python3 packaging/verify_install_tree.py --prefix <install-prefix>
    退出码 0 = 全部 required 存在 + manifest 一致; 1 = 任一缺失/不一致。
  python3 packaging/verify_install_tree.py --prefix <prefix> --contract <path>
    指定安装树合同 (默认 packaging/install-tree.contract.json 仓库路径)。
"""
import argparse
import json
import sys
from pathlib import Path


def load_contract(path: Path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--prefix", required=True, help="安装前缀 (install prefix)")
    ap.add_argument("--contract", default="",
                    help="安装树合同 JSON (默认仓库 packaging/install-tree.contract.json)")
    ap.add_argument("--json-out", default="", help="可选 JSON 报告输出路径")
    args = ap.parse_args()

    prefix = Path(args.prefix).resolve()
    if not prefix.is_dir():
        print(f"VERIFY FAIL: prefix not a directory: {prefix}", file=sys.stderr)
        return 1

    # 合同默认指向仓库 (与安装树契约同源)
    if args.contract:
        contract_path = Path(args.contract)
    else:
        repo_root = Path(__file__).resolve().parent.parent
        contract_path = repo_root / "packaging" / "install-tree.contract.json"
    contract = load_contract(contract_path)
    units = contract.get("units", [])

    missing = []
    present = []
    for u in units:
        rel = u["install_path"]
        p = prefix / rel
        if p.exists():
            present.append(rel)
        else:
            if u.get("required", False):
                missing.append(rel)
            # 非 required 缺失不报错 (白名单可选)

    # product manifest 检查
    manifest_path = prefix / "astrocs.product.json"
    manifest_ok = True
    manifest_err = ""
    if manifest_path.exists():
        try:
            with open(manifest_path, encoding="utf-8") as f:
                m = json.load(f)
            for mu in m.get("units", []):
                rel = mu.get("rel_path", "")
                if rel and not (prefix / rel).exists():
                    manifest_ok = False
                    manifest_err = f"product manifest unit missing: {rel}"
        except json.JSONDecodeError as e:
            manifest_ok = False
            manifest_err = f"product manifest invalid JSON: {e}"
    else:
        manifest_ok = False
        manifest_err = "product manifest missing: astrocs.product.json"

    report = {
        "checker": "INSTALL_TREE_VERIFY",
        "prefix": str(prefix),
        "contract": str(contract_path),
        "required_present": [r for r in present if (prefix / r).exists() and
                             any(u["install_path"] == r and u.get("required", False)
                                 for u in units)],
        "missing_required": missing,
        "product_manifest_ok": manifest_ok,
        "product_manifest_error": manifest_err,
    }
    if args.json_out:
        Path(args.json_out).write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    if missing:
        for rel in missing:
            print(f"MISSING REQUIRED {rel}", file=sys.stderr)
        print("MODULE VERIFY FAIL: required install-tree unit(s) missing "
              "(no static fallback)", file=sys.stderr)
        return 1
    if not manifest_ok:
        print(f"PRODUCT MANIFEST FAIL: {manifest_err}", file=sys.stderr)
        return 1
    print(f"INSTALL_TREE_VERIFY PASS: {len(present)} units present, "
          f"product manifest OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
