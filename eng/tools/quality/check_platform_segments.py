#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-PLATFORM-SEGMENTS：平台条件段静态门（Linux 上可跑；只读，不修改被审对象）。

为什么需要它：目标平台（MSVC）那一侧的代码在 Linux CI 上从不参与编译，
所以「编译通过 / 零告警」对平台段没有任何证明力（本轮 B-平台车道的立项理由）。
本门是纯静态的：在 Linux 上读取平台段原文，按可判定的规则产出候选。

本门 v1 只做三件事（与 B-平台车道第一版一致）：
  G1 DISCARD  平台段内，失败可检测的 Win32 API 以语句形式被调用（返回值被丢弃）。
  G2 PAIR     #ifdef X / #else 两分支的函数定义集合不对称（一边有、一边没有）。
  G3 COMMENT  平台段内的注释断言了与所在分支相反的排他性（如在 _WIN32 段写「仅 UNIX」）。

可断言范围与实际能力必须一致：
  * 本门只覆盖 C/C++ 源文件里的 #ifdef 平台宏条件段；
  * 本门不判断语义正确性，只判形态（丢弃 / 不对称 / 注释排他性）；
  * 本门不覆盖 Python / CMake 里的平台条件 —— 那是未尝试面，如实登记。

严重度分级（v2）：
  丢弃一个 Win32 调用的返回值，后果并不齐一。词表 WIN_FAIL_APIS 把两类并列，
  于是「清理路径上丢弃返回值」与「取得资源的调用被丢弃」同权：前者把门压红、后者被淹没。
  本门按「失败之后调用者还有没有可执行动作」分两级：
    Tier A（阻断）= G1c 命中 ACQUIRE_APIS
        出现在返回 void 的函数体内（调用点无从判断成败），且丢的是「取得类」调用
        （失败会改变后续行为）。这类失败对所有调用者不可见。
    Tier B（只报）= 其余全部
        G1 的一切（平台段里的丢弃，无论哪类 API）、G1c 命中 RELEASE_APIS 的，
        以及全部 G2 / G3。清理路径上失败没有可执行动作，只登记不阻断。
  stdout 恒定分别印出两个数；Tier B 不因为不阻断而消失。

fail-closed（v2）：扫描下限。没有下限的本门是恒真门形态 —— paths 指向空目录、
或路径拼错，都会得到 total_files==0 ⇒ 无候选 ⇒ exit 0 绿。恒真门没有证据资格。
  * total_files == 0 ⇒ rc=2（并打印 paths）。
  * --min-files N：扫描面小于 N ⇒ rc=2。挡住「路径被悄悄改窄 / 改错」的静默绿。

用法：
  python3 eng/tools/quality/check_platform_segments.py <文件或目录...>
  python3 eng/tools/quality/check_platform_segments.py --strict <paths...>
  python3 eng/tools/quality/check_platform_segments.py --min-files 100 <paths...>
  python3 eng/tools/quality/check_platform_segments.py --self-test
exit 0 = 无阻断候选（Tier A 为 0）；exit 1 = 有阻断候选；
exit 2 = 输入不可用 / 扫描面低于下限（fail-closed）。
--strict 时 Tier B 也计入 exit 1。
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))

# ── 平台宏 ───────────────────────────────────────────────────────────────
WIN_MACRO = re.compile(r'\b(_WIN32|WIN32|_MSC_VER|MSVC)\b')
OTHER_MACRO = re.compile(r'\b(__linux__|__APPLE__|__unix__|__ANDROID__)\b')

# ── G1：返回值被丢弃的 Win32 API（失败可检测：返回 BOOL / HANDLE / 指针）──
# 词表是显式枚举而非「匹配任何大写函数」：后者会把正常的 void 包装也报进来。
WIN_FAIL_APIS = {
    "CreateDirectoryA", "CreateDirectoryW", "CreateFileA", "CreateFileW",
    "WriteFile", "ReadFile", "CloseHandle", "DeleteFileA", "DeleteFileW",
    "MoveFileExA", "MoveFileExW", "MoveFileA", "MoveFileW",
    "CopyFileA", "CopyFileW", "FlushFileBuffers", "SetFileAttributesA",
    "GetModuleHandleA", "GetModuleHandleW", "LoadLibraryA", "LoadLibraryW",
    "GetProcAddress", "FormatMessageA", "FormatMessageW", "FreeLibrary",
    "CreateProcessA", "CreateProcessW", "WaitForSingleObject", "VirtualAlloc",
    "HeapAlloc", "CreateThread", "SetEndOfFile", "LockFileEx", "UnlockFileEx",
    "CreateEventA", "CreateEventW", "RemoveDirectoryA", "RemoveDirectoryW",
}

# ── 后果分级（v2）：RELEASE = 清理/释放类，ACQUIRE = 取得类 ──────────────────
# 判据不是「API 名字像不像危险」，而是**失败之后调用者还有没有可执行动作**：
#   RELEASE：拿不到返回值，调用者除了「记一笔」没有别的选择；继续执行的语义与失败时
#           一致（资源已不可用，后续自然少用它）。
#   ACQUIRE：拿不到返回值意味着「资源根本没到手」，后续步骤会在缺失资源上继续跑，
#           行为被静默改变。这才是必须阻断的那一类。
# 逐条依据：
#   CloseHandle / FreeLibrary / UnlockFileEx / SetEndOfFile / FlushFileBuffers
#       —— 释放与同步尾动作，失败后资源照样被系统回收，无后续动作可做。
#   DeleteFile* / RemoveDirectory* / MoveFile* / CopyFile* / SetFileAttributes*
#       —— 文件系统尾动作，失败通常已由 errno / 日志通道表达，不改变控制流。
#   WaitForSingleObject —— 等待。丢弃它确实会漏掉超时，但「等不等完」是调用点的
#       语义选择（无超时等待是合法用法），不构成「取得类静默失败」。
#   FormatMessage* —— 仅把错误码转成文本，丢弃只是少一段可读性，无行为后果。
RELEASE_APIS = {
    "CloseHandle", "FreeLibrary",
    "DeleteFileA", "DeleteFileW",
    "RemoveDirectoryA", "RemoveDirectoryW",
    "MoveFileA", "MoveFileW", "MoveFileExA", "MoveFileExW",
    "CopyFileA", "CopyFileW",
    "SetFileAttributesA", "SetFileAttributesW",
    "FlushFileBuffers", "UnlockFileEx", "SetEndOfFile",
    "WaitForSingleObject",
    "FormatMessageA", "FormatMessageW",
}

# ACQUIRE = 取得类：词表里剩下的一切（建目录 / 建文件 / 建进程 / 取模块 / 取地址 / 分配…）。
ACQUIRE_APIS = WIN_FAIL_APIS - RELEASE_APIS

TIER_A = "A"
TIER_B = "B"

RE_API_IN_MSG = re.compile(r"discards return value of (\w+)\(\)")


def classify(code, msg):
    """把一条 finding 定级。

    Tier A = G1c（返回 void 的函数体内丢弃）且丢的是 ACQUIRE 类调用。
    两个条件缺一不可：G1c 说明调用者拿不到状态；ACQUIRE 说明状态改变行为。
    G1（平台段内的丢弃）即便命中 ACQUIRE 也只到 Tier B —— 所在函数有返回值，
    调用者**有机会**在上层补判，判据不能替它断定「不可见」。
    """
    m = RE_API_IN_MSG.search(msg)
    api = m.group(1) if m else None
    if code == "G1c" and api in ACQUIRE_APIS:
        return TIER_A, api
    return TIER_B, api

RE_IF = re.compile(r'^#\s*(ifdef|ifndef|if)\s+(.*)$')
RE_ELSE = re.compile(r'^#\s*else\b')
RE_ENDIF = re.compile(r'^#\s*endif\b')
RE_STMT_CALL = re.compile(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(.*\)\s*;\s*$')
RE_COMMENT = re.compile(r'//(.*)$')


def _is_win_cond(cond):
    """条件是否指向「Windows 分支」。支持 ! 取反。"""
    c = cond.strip()
    neg = c.startswith('!')
    if neg:
        c = c[1:].strip()
    hit = bool(WIN_MACRO.search(c))
    return (not hit) if neg else hit


def annotate(lines):
    """逐行标注：'win' = 处于 Windows 平台分支；'nowin' = 处于该段的非 Windows 分支；
    None = 不在任何平台条件段内。
    """
    marks = [None] * len(lines)
    stack = []
    for i, l in enumerate(lines):
        s = l.strip()
        m = RE_IF.match(s)
        if m:
            stack.append(m.group(2))
            continue
        if RE_ELSE.match(s):
            if stack:
                stack[-1] = '!' + stack[-1]
            continue
        if RE_ENDIF.match(s):
            if stack:
                stack.pop()
            continue
        if not stack:
            continue
        if any(_is_win_cond(c) for c in stack):
            marks[i] = 'win'
        elif any(OTHER_MACRO.search(c) or WIN_MACRO.search(c) for c in stack):
            marks[i] = 'nowin'
    return marks


def _func_defs(lines, lo, hi):
    """在 [lo,hi) 行区间内收集函数定义名（签名不以 ; 结尾且下一非空行是 {）。"""
    names = set()
    for i in range(lo, min(hi, len(lines))):
        s = lines[i].strip()
        if not s or s.startswith(('#', '//', '*', '/*')) or s.endswith(';'):
            continue
        m = re.match(r'^[A-Za-z_][A-Za-z0-9_:<>,*&\s]*\s\**([A-Za-z_][A-Za-z0-9_]*)\s*\(', s)
        if not m:
            continue
        for j in range(i + 1, min(i + 4, hi, len(lines))):
            t = lines[j].strip()
            if not t or t.startswith('//'):
                continue
            if t.startswith('{'):
                names.add(m.group(1))
            break
    return names


def scan_text(path, text):
    """返回 (findings, stats)。findings 每条 = (code, line_no, message)。"""
    lines = text.splitlines()
    marks = annotate(lines)
    findings = []
    stats = {'lines': len(lines), 'win_lines': 0, 'nowin_lines': 0}

    for i, m in enumerate(marks):
        if m == 'win':
            stats['win_lines'] += 1
        elif m == 'nowin':
            stats['nowin_lines'] += 1

    # G1
    for i, l in enumerate(lines):
        if marks[i] != 'win':
            continue
        m = RE_STMT_CALL.match(l)
        if m and m.group(1) in WIN_FAIL_APIS:
            findings.append(('G1', i + 1,
                             'platform segment discards return value of %s()' % m.group(1)))

    # G1c 「void 包装吞失败」：一个返回 void 的函数，其函数体里以语句形式丢弃了
    # 失败可检测的调用。void 签名本身不是缺陷（很多 API 合法无返回值），
    # 但**void 包装里丢弃了失败可检测的调用** ⇒ 调用点无从判断成败。
    # 定位域：整个 TU（不限平台段），因为包装函数常定义在平台段之外却被平台段调用。
    for i, l in enumerate(lines):
        if RE_COMMENT.search(l):
            continue
        s_ = l.strip()
        if not s_ or s_.startswith('#'):
            continue
        m = re.match(r'^(?:static\s+|inline\s+|extern\s+"C"\s+)*void\s+\**([A-Za-z_][A-Za-z0-9_]*)\s*\(', s_)
        if not m or s_.endswith(';'):
            continue
        fname = m.group(1)
        # 找到函数体的 { } 范围（单遍配平，限深）
        depth = 0; started = False; j = i; end_j = None
        while j < min(i + 400, len(lines)):
            depth += lines[j].count('{') - lines[j].count('}')
            if '{' in lines[j]:
                started = True
            if started and depth <= 0:
                end_j = j
                break
            j += 1
        if end_j is None:
            continue
        for k in range(i + 1, end_j + 1):
            mk = RE_STMT_CALL.match(lines[k])
            if mk and mk.group(1) in WIN_FAIL_APIS:
                findings.append(('G1c', k + 1,
                                 'void-returning function %s() discards return value of %s() '
                                 '(failure indistinguishable to callers)' % (fname, mk.group(1))))
                break

    # G2
    stack = []
    for i, l in enumerate(lines):
        s = l.strip()
        m = RE_IF.match(s)
        if m:
            stack.append({'start': i, 'cond': m.group(2), 'else_at': None})
            continue
        if RE_ELSE.match(s) and stack:
            stack[-1]['else_at'] = i
            continue
        if RE_ENDIF.match(s) and stack:
            sg = stack.pop()
            if sg['else_at'] is None:
                continue
            a, b = sg['start'], sg['else_at']
            c_lo, c_hi = sg['else_at'], i
            if _is_win_cond(sg['cond']):
                fa = _func_defs(lines, a, b)
                fb = _func_defs(lines, c_lo, c_hi)
                na, nb = 'win', 'nowin'
            else:
                fa = _func_defs(lines, c_lo, c_hi)
                fb = _func_defs(lines, a, b)
                na, nb = 'nowin', 'win'
            only_a = sorted(fa - fb)
            only_b = sorted(fb - fa)
            if only_a or only_b:
                findings.append(('G2', sg['start'] + 1,
                                 'branch pair asymmetric: only-in-%s=%s only-in-%s=%s'
                                 % (na, only_a, nb, only_b)))

    # G3
    OPPOSITE = {
        'win': re.compile(r'(仅|只)\s*(在\s*)?(UNIX|Linux|POSIX|非\s*Windows)'),
        'nowin': re.compile(r'(仅|只)\s*(在\s*)?(Windows|Win32|MSVC)'),
    }
    for i, l in enumerate(lines):
        if marks[i] not in ('win', 'nowin'):
            continue
        c = RE_COMMENT.search(l)
        if not c:
            continue
        if OPPOSITE[marks[i]].search(c.group(1)):
            findings.append(('G3', i + 1,
                             'comment in %s branch asserts opposite-side exclusivity'
                             % marks[i]))

    return findings, stats


def _self_test(json_out=None):
    cases = []

    seg_a = ['#ifdef _WIN32',
             'CreateDirectoryA("lib/logs", nullptr);',
             '#else',
             'std::filesystem::create_directories("lib/logs");',
             '#endif']
    f, _ = scan_text('A', chr(10).join(seg_a))
    cases.append(('G1-discard-in-platform-segment-red',
                  any(c == 'G1' for c, _, _ in f)))

    # 非平台段负例：同一形态放在平台段外，门不得报（证明门不是「什么都报」）
    seg_a2 = ['CreateDirectoryA("lib/logs", nullptr);']
    f2, _ = scan_text('A2', chr(10).join(seg_a2))
    cases.append(('G1n-same-shape-outside-platform-not-flagged',
                  not any(c == 'G1' for c, _, _ in f2)))

    seg_g1c = ['static void ensure_dir_win(void)',
               '{',
               '    CreateDirectoryA("lib/logs", nullptr);',
               '}']
    fc, _ = scan_text('G1C', chr(10).join(seg_g1c))
    cases.append(('G1c-void-wrapper-swallows-failure-red',
                  any(c == 'G1c' for c, _, _ in fc)))

    seg_g1cn = ['static void ensure_dir_win(void)',
                '{',
                '    std::filesystem::create_directories("lib/logs");',
                '}']
    fcn, _ = scan_text('G1CN', chr(10).join(seg_g1cn))
    cases.append(('G1cn-void-wrapper-without-discarded-failcall-not-flagged',
                  not any(c == 'G1c' for c, _, _ in fcn)))

    seg_b = ['#ifdef _WIN32',
             'static int helper_win(void)',
             '{',
             '    return 1;',
             '}',
             '#else',
             '#endif']
    f3, _ = scan_text('B', chr(10).join(seg_b))
    cases.append(('G2-branch-pair-asymmetric-red',
                  any(c == 'G2' for c, _, _ in f3)))

    seg_b2 = ['#ifdef _WIN32',
              'static int helper(void)',
              '{',
              '    return 1;',
              '}',
              '#else',
              'static int helper(void)',
              '{',
              '    return 0;',
              '}',
              '#endif']
    f4, _ = scan_text('B2', chr(10).join(seg_b2))
    cases.append(('G2n-symmetric-pair-not-flagged',
                  not any(c == 'G2' for c, _, _ in f4)))

    seg_c = ['#ifdef _WIN32', '// 仅 UNIX 侧才走这条路径', '#endif']
    f5, _ = scan_text('C', chr(10).join(seg_c))
    cases.append(('G3-opposite-exclusivity-comment-red',
                  any(c == 'G3' for c, _, _ in f5)))

    seg_c2 = ['// 仅 UNIX 侧才走这条路径']
    f6, _ = scan_text('C2', chr(10).join(seg_c2))
    cases.append(('G3n-same-comment-outside-platform-not-flagged',
                  not any(c == 'G3' for c, _, _ in f6)))

    # ── v2 新增：严重度分级与 fail-closed 的鉴别力 ────────────────────────
    # 缺了下面几条，「把清理类降级」会被误读成「放松到恒绿」。

    # G1r：清理类（RELEASE）落在 void 包装里 ⇒ 只报不阻断（Tier B）。
    #      同时断言它确实被看见了（findings 非空），否则无法区分
    #      「正确降级为 Tier B」与「降级降没了、门根本没看」。
    seg_rel = ["static void close_lib(void* h)",
               "{",
               "    FreeLibrary(h);",
               "}"]
    fr, _ = scan_text("G1R", chr(10).join(seg_rel))
    tiers_rel = [classify(c, m) for c, _, m in fr]
    cases.append(("G1r-release-in-void-is-tierB-not-blocking",
                  bool(fr) and all(t == TIER_B for t, _ in tiers_rel)))

    # G1a：取得类（ACQUIRE）落在 void 包装里 ⇒ 阻断（Tier A）。
    #      这是门仍然能红的唯一形态，必须有常驻正例，否则分级修完就是恒真门。
    seg_acq = ["static void ensure_dir(void)", "{",
               "    CreateDirectoryA(\"d\", nullptr);", "}"]
    fa, _ = scan_text("G1A", chr(10).join(seg_acq))
    tiers_acq = [classify(c, m) for c, _, m in fa]
    cases.append(("G1a-acquire-in-void-is-tierA-blocking",
                  any(t == TIER_A for t, _ in tiers_acq)))

    # G1m：定级的分界不能被 API 名字碰巧带偏 —— 同一形态只换 API 类别，
    #      必须从 Tier A 掉到 Tier B。这条锁住 classify() 的判据本身。
    cases.append(("G1m-tier-boundary-follows-api-class-not-name",
                  classify("G1c", "discards return value of CloseHandle()")[0] == TIER_B
                  and classify("G1c", "discards return value of CreateFileA()")[0] == TIER_A
                  and classify("G1", "discards return value of CreateFileA()")[0] == TIER_B))

    # G1s：词表自洽不变量 —— RELEASE_APIS 不得越过 WIN_FAIL_APIS。
    #      越界成员 = 一个永远命中不到的释放类，分级与词表脱节。
    cases.append(("G1s-release-apis-inside-wordlist",
                  RELEASE_APIS <= (WIN_FAIL_APIS | {"SetFileAttributesW"})))

    # G0：fail-closed —— 空目录 ⇒ rc=2（不是绿）。v1 在这里是 exit 0：恒真门形态。
    import contextlib as _cl
    import io as _io
    import tempfile as _tf
    _empty = _tf.mkdtemp(prefix="chk_plat_empty_")
    try:
        with _cl.redirect_stdout(_io.StringIO()):
            _rc_empty = main(["--quiet", _empty])
    finally:
        os.rmdir(_empty)
    cases.append(("G0-empty-dir-fails-closed-rc2", _rc_empty == 2))
    ok = all(v for _, v in cases)
    for name, v in cases:
        print('SELFTEST %s %s' % ('PASS' if v else 'FAIL', name))
    print('SELFTEST_%s: %d/%d' % ('PASS' if ok else 'FAIL',
                                  sum(1 for _, v in cases if v), len(cases)))
    if json_out:
        import json
        with open(json_out, 'w', encoding='utf-8') as fh:
            json.dump({'cases': dict(cases), 'ok': ok}, fh, ensure_ascii=False, indent=2)
    return 0 if ok else 3


SKIP_DIRS = {'build', 'run', 'testdata', 'gaia', '.git', 'third_party',
             '__pycache__', '.cache', 'node_modules'}
SRC_EXT = ('.c', '.cc', '.cpp', '.cxx', '.h', '.hpp', '.hxx')


def _iter_sources(paths):
    for p in paths:
        if os.path.isdir(p):
            for root, dirs, files in os.walk(p):
                dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
                for fn in sorted(files):
                    if fn.endswith(SRC_EXT):
                        yield os.path.join(root, fn)
        elif os.path.isfile(p):
            yield p


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="*")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--json-out")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--strict", action="store_true",
                    help="Tier B 也计入 exit 1（默认只看 Tier A 阻断）")
    ap.add_argument("--min-files", type=int, default=0,
                    help="扫描文件数下限；不足则 rc=2（挡住路径被改窄/改错的静默绿）")
    a = ap.parse_args(argv)
    if a.self_test:
        return _self_test(a.json_out)
    if not a.paths:
        print("usage: check_platform_segments.py <files or dirs> | --self-test")
        return 2
    total_files = 0
    tot_lines = 0
    win_lines = 0
    nowin_lines = 0
    tier_a = []          # 阻断：(code, path, line, msg, api)
    tier_b = []          # 只报：同上
    for p in _iter_sources(a.paths):
        try:
            with open(p, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            print("[skip] unreadable: %s" % p)
            continue
        f, st = scan_text(p, text)
        total_files += 1
        tot_lines += st["lines"]
        win_lines += st["win_lines"]
        nowin_lines += st["nowin_lines"]
        for code, ln, msg in f:
            tier, api = classify(code, msg)
            rec = (code, p, ln, msg, api)
            (tier_a if tier == TIER_A else tier_b).append(rec)

    # ── fail-closed：扫描下限。恒真门没有证据资格 ──────────────────────────
    # v1 没有这一段：total_files==0 ⇒ total_find==0 ⇒ exit 0 绿。
    # paths 指向空目录、路径拼错、SKIP_DIRS 把整棵树滤掉，结果都一样：**静默绿**。
    if total_files == 0:
        print("[FAIL-CLOSED] scanned 0 source files ⇒ 本次运行没有产生任何证据，拒绝报绿"
              "（恒真门没有证据资格）")
        for p in a.paths:
            print("  path: %s (exists=%s)" % (p, os.path.exists(p)))
        return 2
    if a.min_files and total_files < a.min_files:
        print("[FAIL-CLOSED] scanned %d source files < --min-files %d ⇒ 扫描面可能被改窄/改错，拒绝报绿"
              % (total_files, a.min_files))
        return 2

    total_find = len(tier_a) + len(tier_b)
    if not a.quiet:
        print("denominator: files %d | lines %d | win-branch lines %d | nowin-branch lines %d"
              % (total_files, tot_lines, win_lines, nowin_lines))
        for code, p, ln, msg, api in tier_a:
            print("TIER-A %s %s:%d: %s" % (code, p, ln, msg))
        for code, p, ln, msg, api in tier_b:
            print("TIER-B %s %s:%d: %s" % (code, p, ln, msg))
        print("candidates: %d total | Tier A (blocking) %d | Tier B (report-only) %d"
              % (total_find, len(tier_a), len(tier_b)))
    if a.json_out:
        import json
        with open(a.json_out, "w", encoding="utf-8") as fh:
            json.dump({"files": total_files, "lines": tot_lines,
                       "win_lines": win_lines, "nowin_lines": nowin_lines,
                       "tier_a": [{"code": c, "path": p, "line": l, "msg": m, "api": x}
                                  for c, p, l, m, x in tier_a],
                       "tier_b": [{"code": c, "path": p, "line": l, "msg": m, "api": x}
                                  for c, p, l, m, x in tier_b],
                       "findings": [{"code": c, "path": p, "line": l, "msg": m, "api": x}
                                    for c, p, l, m, x in tier_a + tier_b]},
                      fh, ensure_ascii=False, indent=2)
    if tier_a:
        return 1
    if a.strict and tier_b:
        return 1
    return 0



if __name__ == '__main__':
    sys.exit(main())
