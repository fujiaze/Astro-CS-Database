#!/usr/bin/env python3
"""cpu_features.h 位定义解析器 —— ISA 声明位的**唯一口径源**（R-60 / 清单侧）。

为什么要有这个模块: 能力"位号"在本仓至少被抄过三份（两个清单生成器各一份、门禁
各一份）。抄错一位的后果不是笔误而是**语义错位**: DYN-740 / R-28 的实际事故就是
生成器表里 avx512bw=1<<6 / dq=1<<7 / vl=1<<8 与 cpu_features.h 的
CD=1<<6 / BW=1<<7 / DQ=1<<8 / VL=1<<9 冲突 —— 同一位号两名两义, 生成出来的
required_features_bits 会被加载预检按**另一个能力**判读。本模块把
lib/infrastructure/benchmark/backend_host/cpu_features.h 当唯一事实源解析:
要位号就来这里取, 不再各抄一份; 取不到就抛错(fail-closed), 不静默退回硬编码表。

支持该头文件当前出现的两种定义形式:
  #define ACS_FEAT_SSE2     (1ull << 0)                      → 单位: 掩码 + 单成员
  #define ACS_FEAT_AVX512_PROVIDER_REQUIRED (A | B | C | D)   → 组位: 掩码 + 成员表

用法:
  from isa_feature_bits import FeatureBits
  fb = FeatureBits.load()                 # 默认找仓库内 cpu_features.h
  fb.mask("avx512cd")                     # → 64
  fb.mask("ACS_FEAT_AVX512_PROVIDER_REQUIRED")    # → 928（组位, 递归展开）
  fb.members("ACS_FEAT_AVX512_PROVIDER_REQUIRED") # → ('avx512f','avx512bw','avx512dq','avx512vl')
  fb.names                                # → 全部叶子位名（文件序）
"""
import os
import re

__all__ = ["FeatureBits", "UnknownFeature", "DEFAULT_HEADER_REL"]

DEFAULT_HEADER_REL = os.path.join("lib", "infrastructure", "benchmark", "backend_host",
                                  "cpu_features.h")

_DEF = re.compile(r"^\s*#\s*define\s+(ACS_FEAT_[A-Z0-9_]+)\s+(.+?)\s*$")
_UNIT = re.compile(r"^\(\s*1ull\s*<<\s*(\d+)\s*\)$")
_REF = re.compile(r"\bACS_FEAT_[A-Z0-9_]+\b")


class UnknownFeature(KeyError):
    """位名/宏名不在 cpu_features.h 里 —— 调用方必须当错误处理（fail-closed）。"""


def default_header_path(repo_root=None):
    """仓库内 cpu_features.h 的路径（repo_root 省略时按本文件位置往上找）。"""
    if repo_root:
        return os.path.join(repo_root, DEFAULT_HEADER_REL)
    here = os.path.dirname(os.path.abspath(__file__))          # eng/tools
    return os.path.join(os.path.dirname(os.path.dirname(here)), DEFAULT_HEADER_REL)


class FeatureBits:
    """位名 ↔ 掩码（含组宏递归展开）。不可变、进程内可缓存。"""

    def __init__(self, header_path):
        self.header_path = header_path
        self._defs = {}       # ACS_FEAT_X → 定义体文本
        self._order = []      # 定义出现序（叶子位按此行序输出）
        with open(header_path, encoding="utf-8") as f:
            text = f.read()
        # 反斜杠续行先并成一行: 组宏定义（如 ACS_FEAT_AVX512_PROVIDER_REQUIRED）在头文件里
        # 是跨行写的, 按行解析只会拿到一个 "\\" —— 实测踩过（组位掩码变 UnknownFeature）。
        text = re.sub(r"\\[ \t]*\n", " ", text)
        for ln in text.splitlines():
            m = _DEF.match(ln)
            if not m:
                continue
            name, body = m.group(1), m.group(2)
            if name not in self._defs:
                self._defs[name] = ""
                self._order.append(name)
            self._defs[name] += " " + body
        if not self._defs:
            raise UnknownFeature("cpu_features.h 里没有解析到任何 ACS_FEAT_ 定义: %s"
                                 % header_path)

    @classmethod
    def load(cls, header_path=None, repo_root=None):
        return cls(header_path or default_header_path(repo_root))

    # ── 名称归一 ───────────────────────────────────────────────────────────
    @staticmethod
    def _macro(name):
        n = str(name).strip()
        if n.startswith("ACS_FEAT_"):
            return n.upper()
        return "ACS_FEAT_" + n.upper()

    def _body(self, macro):
        if macro not in self._defs:
            raise UnknownFeature("%s 不在 %s" % (macro, self.header_path))
        return self._defs[macro]

    # ── 公开面 ─────────────────────────────────────────────────────────────
    def mask(self, name):
        """位掩码。组宏递归展开为成员的或。未知名 ⇒ UnknownFeature。"""
        macro = self._macro(name)
        body = self._body(macro).strip()
        m = _UNIT.match(body)
        if m:
            return 1 << int(m.group(1))
        refs = _REF.findall(body)
        if not refs:
            raise UnknownFeature("无法解析的定义: %s %s" % (macro, body[:80]))
        out = 0
        for r in refs:
            if r == macro:
                raise UnknownFeature("自引用定义: %s" % macro)
            out |= self.mask(r)
        return out

    def members(self, name):
        """叶子成员名（小写, 无前缀）, 文件定义序。单位 ⇒ 单元素元组。"""
        macro = self._macro(name)
        body = self._body(macro).strip()
        if _UNIT.match(body):
            return (macro[len("ACS_FEAT_"):].lower(),)
        out = []
        for r in _REF.findall(body):
            for leaf in self.members(r):
                if leaf not in out:
                    out.append(leaf)
        return tuple(out)

    @property
    def names(self):
        """全部**叶子**位名（小写无前缀）, 按头文件定义序。"""
        out = []
        for macro in self._order:
            if _UNIT.match(self._body(macro).strip()):
                leaf = macro[len("ACS_FEAT_"):].lower()
                if leaf not in out:
                    out.append(leaf)
        return tuple(out)

    @property
    def bit_by_name(self):
        return {n: self.mask(n) for n in self.names}

    def bits_of(self, feature_names):
        """名字集合/序列 → 掩码（未知名抛错, 不静默丢位）。"""
        out = 0
        for n in feature_names:
            out |= self.mask(n)
        return out
