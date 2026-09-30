#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""源码扫描工具：注释剥离与源文件遍历。

判据读**被检对象**而不是文档文字：把 C/C++ 注释剥掉之后再统计标识符出现，
注释里提到的名字不构成声明。遍历源文件时按目录名跳过第三方代码、测试、归档件与
构建树，避免它们进入统计分母。

本模块只依赖标准库。
"""
from __future__ import annotations

import pathlib
import re

COMMENT_BLOCK = re.compile(r"/\*.*?\*/", re.S)
COMMENT_LINE = re.compile(r"//[^\n]*")

DEFAULT_SUFFIXES = (".cpp", ".h", ".hpp", ".cc")
DEFAULT_SKIP_PARTS = ("third_party", "tests", "archive", "build")


class SourceScanError(Exception):
    """源根不存在或不可遍历。调用方据此 fail-closed。"""


def strip_comments(text: str) -> str:
    """去掉 C/C++ 块注释与行注释；块注释以等量换行占位，保持行结构不变。"""
    text = COMMENT_BLOCK.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    return COMMENT_LINE.sub("", text)


def iter_source_files(root, suffixes=DEFAULT_SUFFIXES, skip_parts=DEFAULT_SKIP_PARTS):
    """按排序遍历源文件，产出 (绝对路径, 相对 root 的 posix 路径)。"""
    root = pathlib.Path(root)
    if not root.is_dir():
        raise SourceScanError("源根不存在: %s" % root)
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix not in suffixes:
            continue
        if any(part in path.parts for part in skip_parts):
            continue
        yield path, path.relative_to(root).as_posix()
