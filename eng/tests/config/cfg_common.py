#!/usr/bin/env python3
"""CFG-001 测试公共工具：零依赖（只用仓库自带 eng/tests/common/jsonschema_min.py）。"""
import hashlib
import json
import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
VALIDATOR = os.path.join(REPO, "eng", "tests", "common", "jsonschema_min.py")


def load_validator():
    import importlib.util

    spec = importlib.util.spec_from_file_location("cfg_jsonschema_min", VALIDATOR)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def load_json(rel):
    with open(os.path.join(REPO, rel), encoding="utf-8") as fh:
        return json.load(fh)


def load_text(rel):
    with open(os.path.join(REPO, rel), encoding="utf-8") as fh:
        return fh.read()


def sha256_file(rel):
    with open(os.path.join(REPO, rel), "rb") as fh:
        return hashlib.sha256(fh.read()).hexdigest()


def file_size(rel):
    return os.path.getsize(os.path.join(REPO, rel))


def read_lines(rel):
    with open(os.path.join(REPO, rel), encoding="utf-8") as fh:
        return fh.read().split("\n")


def validate(schema, instance):
    """返回 (path, message) 列表；空列表 = 通过。"""
    return load_validator().validate(instance, schema)


def property_names(node, acc=None):
    """递归收集 schema 节点下所有 properties 的键名（含 items/$defs/oneOf/allOf/anyOf/if/then/else）。"""
    if acc is None:
        acc = set()
    if isinstance(node, dict):
        props = node.get("properties")
        if isinstance(props, dict):
            acc.update(props.keys())
        for key, val in node.items():
            if key == "properties":
                continue
            property_names(val, acc)
    elif isinstance(node, list):
        for item in node:
            property_names(item, acc)
    return acc


def leaf_strings(node, prefix=""):
    """收集 (path, value)：用于「禁止名在任何键位出现」的扫描。"""
    out = []
    if isinstance(node, dict):
        for key, val in node.items():
            out.append((prefix + "/" + str(key), key))
            out.extend(leaf_strings(val, prefix + "/" + str(key)))
    elif isinstance(node, list):
        for i, item in enumerate(node):
            out.extend(leaf_strings(item, prefix + "/%d" % i))
    return out


def json_key_paths(node, prefix=""):
    """所有 JSON 键的完整路径（含数组下标），用于「某键不得出现」的机器断言。"""
    out = []
    if isinstance(node, dict):
        for key, val in node.items():
            here = prefix + "/" + str(key)
            out.append(here)
            out.extend(json_key_paths(val, here))
    elif isinstance(node, list):
        for i, item in enumerate(node):
            out.extend(json_key_paths(item, prefix + "/%d" % i))
    return out


def anchor_lines(rel):
    return read_lines(rel)


def grep_line(rel, line_no, token):
    lines = read_lines(rel)
    if line_no < 1 or line_no > len(lines):
        return None
    return token in lines[line_no - 1]


ZERO_RE = re.compile("zero", re.IGNORECASE)
