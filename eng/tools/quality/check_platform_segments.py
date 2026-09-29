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

用法：
  python3 eng/tools/quality/check_platform_segments.py <文件或目录...>
  python3 eng/tools/quality/check_platform_segments.py --self-test
exit 0 = 无候选；exit 1 = 有候选；exit 2 = 输入不可用。
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
    ap.add_argument('paths', nargs='*')
    ap.add_argument('--self-test', action='store_true')
    ap.add_argument('--json-out')
    ap.add_argument('--quiet', action='store_true')
    a = ap.parse_args(argv)
    if a.self_test:
        return _self_test(a.json_out)
    if not a.paths:
        print('usage: check_platform_segments.py <files or dirs> | --self-test')
        return 2
    total_files = 0
    total_find = 0
    tot_lines = 0
    win_lines = 0
    nowin_lines = 0
    per = {}
    for p in _iter_sources(a.paths):
        try:
            with open(p, encoding='utf-8', errors='replace') as fh:
                text = fh.read()
        except OSError:
            print('[skip] unreadable: %s' % p)
            continue
        f, st = scan_text(p, text)
        total_files += 1
        tot_lines += st['lines']
        win_lines += st['win_lines']
        nowin_lines += st['nowin_lines']
        if f:
            per[p] = f
            total_find += len(f)
            if not a.quiet:
                for code, ln, msg in f:
                    print('%s %s:%d: %s' % (code, p, ln, msg))
    if not a.quiet:
        print('denominator: files %d | lines %d | win-branch lines %d | nowin-branch lines %d'
              % (total_files, tot_lines, win_lines, nowin_lines))
        print('candidates: %d in %d files' % (total_find, len(per)))
    if a.json_out:
        import json
        with open(a.json_out, 'w', encoding='utf-8') as fh:
            json.dump({'files': total_files, 'lines': tot_lines,
                       'win_lines': win_lines, 'nowin_lines': nowin_lines,
                       'findings': per}, fh, ensure_ascii=False, indent=2)
    return 1 if total_find else 0


if __name__ == '__main__':
    sys.exit(main())
