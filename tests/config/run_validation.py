#!/usr/bin/env python3
"""CFG-001 校验跑手：用仓库既有 jsonschema_min 校验一个实例。

用法: python3 tests/config/run_validation.py <schema.json> <instance.json> [--defs <name>]
退出码: 0 = 通过; 1 = 校验失败; 2 = 用法/文件错误
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cfg_common  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("schema")
    ap.add_argument("instance")
    ap.add_argument("--defs", default=None, help="校验 schema 的 $defs.<name> 子模式（默认整份 schema）")
    args = ap.parse_args()
    try:
        schema = cfg_common.load_json(args.schema)
        instance = cfg_common.load_json(args.instance)
    except (OSError, ValueError) as exc:
        print("USAGE_ERROR %s" % exc)
        return 2
    target = schema
    label = args.schema
    if args.defs:
        target = schema.get("$defs", {}).get(args.defs)
        if target is None:
            print("USAGE_ERROR $defs.%s not found in %s" % (args.defs, args.schema))
            return 2
        label = "%s#/$defs/%s" % (args.schema, args.defs)
    errors = cfg_common.validate(target, instance)
    if errors:
        print("VALIDATION_FAIL %s <- %s (%d errors)" % (label, args.instance, len(errors)))
        for path, msg in errors[:20]:
            print("  %s: %s" % ("/".join(str(p) for p in path) or "<root>", msg))
        return 1
    print("VALIDATION_PASS %s <- %s" % (label, args.instance))
    return 0


if __name__ == "__main__":
    sys.exit(main())
