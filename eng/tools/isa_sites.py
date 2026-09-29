#!/usr/bin/env python3
"""ISA 旗标站点口径表读取/推导器 —— 「清单声明位 = 构建口径」的唯一取数口（R-60）。

表 = eng/tools/quality/isa_sites.json，由 eng/tools/quality/check_isa_same_source.py 强制
与根 CMakeLists.txt 的 target_compile_options(<tgt> ...) 逐字对齐（站点未登记 / 旗标集合不等
即红）。清单侧只要从**这里**取数，"清单声明的 ISA" 就不可能相对 "真正编进去的 ISA" 悄悄多
一位或少一位 —— 多一位是虚假能力声明，少一位是加载放行后首调撞非法指令。

本模块只做读与推导，不写表、不在别处缓存副本。
"""
import json
import os

DEFAULT_SITES = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "quality", "isa_sites.json")
MSVC_PLATFORM = "MSVC/clang-cl"
GNU_PLATFORM = "GNU/Clang"

__all__ = ["DEFAULT_SITES", "MSVC_PLATFORM", "GNU_PLATFORM", "UnknownSite",
           "platform_of", "load", "site_of", "flags_for", "permitted"]


class UnknownSite(KeyError):
    """站点 id 不在口径表里 / 旗标未登记映射 —— 调用方必须当错误处理（fail-closed）。"""


def platform_of(compiler):
    """编译器标识 → 平台键。CMake 传的是 ${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}
    （MSVC-19.38 / GNU-14.2.0 / Clang-18.1.8）；clang-cl 的 ID 仍是 Clang，故另认字样。"""
    c = (compiler or "").strip().lower()
    if c.startswith("msvc") or "msvc" in c or "clang-cl" in c or c in ("cl", "cl.exe"):
        return MSVC_PLATFORM
    return GNU_PLATFORM


def load(path=None):
    with open(path or DEFAULT_SITES, encoding="utf-8") as f:
        return json.load(f)


def site_of(reg, site_id):
    for s in reg.get("sites", []):
        if s.get("id") == site_id:
            return s
    return None


def flags_for(site, platform):
    """该平台**适用**的旗标（MSVC/clang-cl 取 /arch:...，其余取 -m...），保持登记序。"""
    want_slash = platform == MSVC_PLATFORM
    return [f for f in (site.get("flags") or []) if f.startswith("/") == want_slash]


def permitted(reg, site, platform, fb):
    """该平台被旗标**许可**的位面 → (适用旗标, 位名(头文件序), 掩码)。

    口径: ∪ flag_feature_map[适用旗标] ∪ platform_extra_bits[平台]。
    platform_extra_bits 记的是"平台旗标确实许可、但旗标—位映射表里没单独写出来的位"
    （R-60 实证: MSVC /arch:AVX512 的许可面比 GCC 四子集旗标多 CD，而 /arch: 档位没有
    子集档位旗标可写）。未登记映射的旗标 ⇒ UnknownSite（不猜它能开什么）。
    """
    fmap = reg.get("flag_feature_map") or {}
    names, unknown = [], []
    for f in flags_for(site, platform):
        entry = fmap.get(f)
        if entry is None:
            unknown.append(f)
            continue
        for v in (entry if isinstance(entry, list) else [entry]):
            if v not in names:
                names.append(v)
    if unknown:
        raise UnknownSite("站点 %s 的旗标未登记映射: %s" % (site.get("id"), unknown))
    for v in ((site.get("platform_extra_bits") or {}).get(platform) or []):
        if v not in names:
            names.append(v)
    leaf = []
    for n in names:
        for m in fb.members(n):
            if m not in leaf:
                leaf.append(m)
    leaf = [n for n in fb.names if n in leaf]
    return flags_for(site, platform), leaf, fb.bits_of(leaf)
