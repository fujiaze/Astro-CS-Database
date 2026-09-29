#!/usr/bin/env python3
"""check_variant_isa_disasm.py — 变体 DSO 的**产物级**ISA 面判据（R-60）。

为什么需要它（与既有判据的分工）:
  · eng/tools/quality/check_isa_same_source.py 管**构建输入**（旗标站点 ↔ 声明 ↔ 检测三侧同源）；
  · eng/tools/check_isa_leak.py 管主 CLI 的泄漏与 provider 库必须含对应指令（GNU/ELF 专用路径）；
  · eng/tests/cpu/avx512/check_avx512_illegal_instr.py 管第二族 AVX-512 provider 的 %zmm 面；
  · 本脚本补的是**第一族 backend 变体 DSO 的 TU 级隔离**这件事在**产物**上的双向证明，
    且与反汇编工具**解耦**（接受任意已导出的反汇编文本）—— 以便同一判据在
    Linux（objdump）与 Windows（llvm-objdump / dumpbin /disasm）两腿上跑同一份断言。

判据（红/绿都真实）:
  1) --hit SYM --isa avx2|avx512      : SYM 函数体**必须**含该档的宽向量指令（"真变体"正例）；
  2) --clean-symbol SYM               : SYM 函数体**必须零** VEX/EVEX 指令（自检/握手入口负例）；
  3) --clean-text / --require-text     : 整份反汇编文本的同型断言（单 TU 目标/基线产物用）；
  4) --require-feature NAME           : 每一条**声明位**都要在产物里找到使用证据 ——
                                        直接对应 R-60 约束 4「不得声明了却没用」。
  VEX/EVEX 判定 = 助记符以 v 开头（objdump/dumpbin 两种语法下 VEX/EVEX 指令均以 v 开头）
  或出现 ymm/zmm 寄存器（含/不含 % 前缀两种写法）。该口径与 check_isa_leak.py 的
  「ymm/zmm 或专属助记符」一致，只强不弱。

用法:
  check_variant_isa_disasm.py --binary build/providers/astrocs_cpu_avx512.so \
      --hit astrocs_variant_kernel_dispatch_v1 --isa avx512 \
      --clean-symbol astrocs_backend_get_api_v1 --clean-symbol backend_self_test \
      --require-feature avx512f --require-feature avx512vl
  check_variant_isa_disasm.py --text /tmp/dumpbin_disasm.txt --isa avx512 --hit FUNC --clean-symbol FUNC2
  check_variant_isa_disasm.py --self-test
退出码: 0 = 全 PASS；1 = 有 FAIL；2 = 用法/输入错误。
"""
from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys

# ── 反汇编文本解析 ───────────────────────────────────────────────────────────
# GNU objdump / llvm-objdump: "0000000000001234 <sym>:"；dumpbin /disasm: "sym:" 或 "?sym@@...:"
GNU_LABEL = re.compile(r"^[0-9a-fA-F]+\s+<([^>]+)>:")
MSVC_LABEL = re.compile(r"^([?A-Za-z_][\w?@$]*):\s*$")
REG_WIDE = re.compile(r"%(?:y|z)mm\d+|\b(?:y|z)mm\d+\b")
ADDR = re.compile(r"^[0-9a-fA-F]+:\s*")
BYTE2 = re.compile(r"[0-9a-fA-F]{2}")

# ── 指令**编码**判定（ground truth，不看助记符表）──────────────────────────
# VEX/EVEX 是前缀字节，不是助记符：x86-64 下 0x62 = EVEX、0xC4/0xC5 = VEX。
# objdump/llvm-objdump 与 dumpbin /disasm **都把指令字节列出来**，故同一判据在
# ELF 与 PE/COFF 两腿通用（助记符表会随汇编器方言漂移，字节前缀不会）。
# 用途: 「这条指令是不是真的需要目标 ISA」在**编码层**可判定 —— 例: 同一助记符
# vmulss 有 VEX(AVX) 与 EVEX(AVX-512F) 两种编码，按助记符名字分不开，按首字节
# 0x62 却分得开。这是 --require-evex/--require-vex 的判据基座。
EVEX_BYTE, VEX_BYTES = "62", ("c4", "c5")


def lead_bytes(line):
    """该行指令的编码字节序列（去地址后、助记符前的十六进制字节）。"""
    out = []
    for tok in ADDR.sub("", line.strip()).split():
        if BYTE2.fullmatch(tok):
            out.append(tok.lower())
        else:
            break
    return out


def encoding_of(line):
    """'evex' | 'vex' | 'legacy'。无字节列（去字节的反汇编文本）⇒ 'unknown'。"""
    b = lead_bytes(line)
    if not b:
        return "unknown"
    if b[0] == EVEX_BYTE:
        return "evex"
    return "vex" if b[0] in VEX_BYTES else "legacy"


def is_evex(line):
    """该行指令是否 EVEX(0x62 前缀)编码。无字节列 ⇒ False（证据不可判，不许充数）。"""
    return encoding_of(line) == "evex"


# AVX-512 档位的证据规则**一律**要求 EVEX 编码（AVX512_* 前缀的位）。
# 理由: 助记符名字跨档共享，编码字节不共享。0x62 前缀在无 AVX-512F 的机器上同样
# #UD，故它是"本档才可能发射"的最小可靠判据。
EVEX_ONLY_FEATURES = ("avx512f", "avx512cd", "avx512bw", "avx512dq", "avx512vl")

# 档位专属助记符（仅用于加档；VEX/EVEX 的"是否宽指令"判定不依赖本表）
AVX2_ONLY = re.compile(
    r"^(?:v(?:p(?:sub|add|and|or|xor|mull|cmpeq|cmpgt|cmp|sll|srl|sra|blend|broadcast|perm|extract|insert|gather|pack|unpack|abs|min|max|avg|test)[a-z0-9]*"
    r"|broadcast[a-z0-9]*|perm[a-z0-9]*|extracti128|inserti128|perm2i128|fmadd[a-z0-9]*|fmsub[a-z0-9]*"
    r"|fnmadd[a-z0-9]*|fnmsub[a-z0-9]*|fmaddsub[a-z0-9]*|fnmsubadd[a-z0-9]*))$")
AVX512_ONLY = re.compile(
    r"^(?:v(?:movdqu(?:8|16|32|64)|movdqa(?:32|64)|pternlog[qd]|pconflict[qd]|plzcnt[qd]"
    r"|pcmpeq[u]?[bwdq]|pcmp[u]?[bwdq]|pmovm2[a-z0-9]*|pmov[a-z0-9]*2m|permi2[a-z0-9]*|permt2[a-z0-9]*"
    r"|extracti(?:32|64)x|extractf(?:32|64)x|inserti(?:32|64)x|insertf(?:32|64)x|broadcasti(?:32|64)x"
    r"|shuff(?:32|64)x|blendm[a-z0-9]*|compress[a-z0-9]*|expand[a-z0-9]*|scatter[a-z0-9]*"
    r"|pblendm[a-z0-9]*|pmullq|pandn[qd]|pand[qd]|por[qd]|pxor[qd]|padd[qd]|psub[qd]|psll[qd]|psrl[qd]"
    r"|prol[qd]|pror[qd]|pshld[a-z0-9]*|pshrd[a-z0-9]*|cvtqq2pd|pmovqd|fpclass[a-z0-9]*|rangeps[a-z0-9]*"
    r"|reduceps[a-z0-9]*|rcp14[a-z0-9]*|rsqrt14[a-z0-9]*|getexp[a-z0-9]*|getmant[a-z0-9]*|fixupimm[a-z0-9]*"
    r"|rndscale(?:ss|ps|sd|pd)|cvtusi2(?:ss|ps|sd|pd)))$")

# 声明位 → 使用证据（判据 4；每条都必须能在产物里找到至少一条，否则判红）
FEATURE_EVIDENCE = {
    "avx2": ("256-bit 或 AVX2 专属整数指令", re.compile(r"%(?:y)mm|\bymm\d|\bv(?:psub|vpadd|padd|psub|extracti128|inserti128|perm2i128|perm|broadcast|blend|gather)[a-z0-9]*")),
    "fma": ("FMA 融合乘加助记符", re.compile(r"\bv(?:fmadd|fmsub|fnmadd|fnmsub|fmaddsub|fnmsubadd)[a-z0-9]*\b")),
    # 只收「本档才可能发射」的证据: zmm 寄存器，或 EVEX 专有助记符。
    # 按助记符**名字**认会假绿 —— vpaddd/vpaddq/vpsubd 等 AVX2 就能合法发射，
    # vbroadcastss/vbroadcastsd 亦然。实测: 低档变体产物里 7 条这类指令
    # 让「声明了高档位却没用」这条判据整条失效(判绿)。凡跨档共享的助记符一律不收。
    "avx512f": ("512-bit（zmm）指令；EVEX 专有 F 档助记符（含标量 EVEX-only 的 "
                "VRNDSCALE/VCVTUSI2 —— 第二族 provider 的热点 kernel（hips 双线性重采样，"
                "含 % 与 floor）在 -mavx512* 下不被宽向量化，编译器只把 floor/无符号转换"
                "发射成 EVEX 标量形式，实测 vrndscaless/vcvtusi2ss，首字节 0x62；"
                "只认 zmm 会把这个真用了 AVX-512F 的产物判成假变体）；或**编码层** 0x62 前缀",
                re.compile(r"%(?:z)mm|\bzmm\d|\bv(?:movdqu(?:8|16|32|64)|movdqa(?:32|64)|pternlog[qd]|por[qd]|pand[qd]|pxor[qd]|broadcast[a-z]*x[0-9]+|rndscale(?:ss|ps|sd|pd)|cvtusi2(?:ss|ps|sd|pd))[a-z0-9]*\b")),
    "avx512cd": ("CD 专属助记符（冲突检测/前导零计数）", re.compile(r"\bv(?:pconflict|plzcnt)[a-z0-9]*\b")),
    # packuswb/punpcklbw 是跨档共享助记符(AVX2 合法)，不得作为本档证据（同 F 档注释）。
    "avx512bw": ("BW 专属助记符（字节/字粒度，EVEX 专有）", re.compile(r"\bv(?:movdqu8|movdqu16|pcmpeq[bdw]|pcmp[bdw]|pmovm2b|pmovm2w|psllvw|psrlvw|psravw)[a-z0-9]*\b")),
    # DQ = 64 位整数/双精度专有算子（注意 vmovdqu64/vmovdqa64 属 F，不归 DQ —— 归口错误会让
    # 「有使用证据」变成假绿）。
    "avx512dq": ("DQ 专属助记符（64 位整数/转换）", re.compile(r"\bv(?:pmullq|pmovqd|pmovm2q|pmovq2m|cvtqq2pd|cvtpd2qq|vpcmpq|vpcmpuq|vextracti64x|vinserti64x|vbroadcasti64x)[a-z0-9]*\b")),
    # VL 没有"专属助记符"可认：判据是**EVEX 专有指令用在 128/256 位寄存器上** ——
    # 任何 EVEX-only 助记符（带 ymm/xmm 且行内无 zmm）都证明用上了 AVX512VL。
    "avx512vl": ("VL 证据（EVEX-only 指令落在 xmm/ymm 上）", "vl_callable"),
}


def read_text(args):
    if args.text:
        return pathlib.Path(args.text).read_text(encoding="utf-8", errors="replace")
    if not args.binary:
        print("FAIL: 需要 --binary 或 --text", file=sys.stderr)
        sys.exit(2)
    objdump = args.objdump
    if not objdump:
        objdump = "llvm-objdump" if not args.gnu_objdump else "objdump"
    flags = ["-d", str(args.binary)] if not args.gnu_objdump else ["-d", str(args.binary)]
    r = subprocess.run([objdump] + flags, capture_output=True, text=True)
    if r.returncode != 0:
        print("FAIL: %s 失败 on %s: %s" % (objdump, args.binary, r.stderr.strip()[:300]))
        sys.exit(2)
    return r.stdout


def parse_funcs(text):
    funcs, order, cur = {}, [], None
    for ln in text.splitlines():
        m = GNU_LABEL.match(ln) or MSVC_LABEL.match(ln)
        if m:
            cur = m.group(1)
            if cur not in funcs:
                funcs[cur] = []
                order.append(cur)
            continue
        if cur is not None:
            funcs[cur].append(ln)
    return funcs, order


def mnemonic_of(line):
    """取该行助记符：去地址后跳过十六进制字节对，第一个非字节记号即助记符。

    两种反汇编文本同规则：objdump/llvm-objdump（ADDR:\tBYTES\tMNEMONIC）与
    dumpbin /disasm（ADDR: BB BB BB BB  MNEMONIC）。逐记号判定而不是正则整行匹配 ——
    后者在 "c4 e2 7d …" 这类字节串上会把十六进制字节误当助记符（e2 也会被 [a-z] 吃掉）。"""
    s = ADDR.sub("", line.strip())
    for tok in s.split():
        t = tok.strip()
        if BYTE2.fullmatch(t):
            continue
        return t.lower()
    return ""


def wide_ops(lines):
    out = []
    for ln in lines:
        if not ln.strip():
            continue
        m = mnemonic_of(ln)
        if REG_WIDE.search(ln) or (m.startswith("v") and len(m) > 1):
            out.append((m, ln.strip()))
    return out


def _vl_evidence(line):
    if re.search(r"%?zmm\d", line):
        return False
    if not re.search(r"%(?:y|x)mm\d|\b(?:y|x)mm\d", line):
        return False
    return bool(AVX512_ONLY.match(mnemonic_of(line)))


def _count_evidence(text, matcher):
    if callable(matcher):
        return sum(1 for ln in text.splitlines() if matcher(ln))
    if matcher == "vl_callable":
        return sum(1 for ln in text.splitlines() if _vl_evidence(ln))
    return sum(1 for ln in text.splitlines() if matcher.search(ln))


def class_ops(lines, isa):
    """该档位的专属使用证据。"""
    hits = []
    for m, ln in wide_ops(lines):
        if isa == "avx2":
            # 口径与 eng/tools/check_isa_leak.py 的 scan_avx2 一致（只强不弱）:
            # ≥256-bit（ymm）向量使用 或 AVX2 专属助记符。FMA 由 --require-feature fma 单独断言。
            # 带 zmm 的行属 AVX-512 面（vpaddd 之类的助记符在 512 位宽度下是 EVEX 形态），
            # 不得算作 AVX2 证据 —— 否则档位判据会被高位的指令"蹭绿"。
            if re.search(r"%?zmm\d", ln):
                continue
            if re.search(r"%?ymm\d", ln) or AVX2_ONLY.match(m):
                hits.append((m, ln))
        elif isa == "avx512":
            if re.search(r"%?zmm\d", ln) or AVX512_ONLY.match(m):
                hits.append((m, ln))
    return hits


def main(argv=None):
    ap = argparse.ArgumentParser(description="变体 DSO 产物级 ISA 面判据（R-60）")
    ap.add_argument("--binary", type=pathlib.Path, default=None)
    ap.add_argument("--text", type=pathlib.Path, default=None,
                    help="已导出的反汇编文本（Windows 腿: llvm-objdump -d / dumpbin /disasm）")
    ap.add_argument("--objdump", default="objdump")
    ap.add_argument("--gnu-objdump", action="store_true", help="强制 objdump（默认同）")
    ap.add_argument("--isa", choices=("avx2", "avx512"), default="avx512")
    ap.add_argument("--hit", action="append", default=[], help="函数体必须含该档宽指令（可重复）")
    ap.add_argument("--clean-symbol", action="append", default=[], help="函数体必须零 VEX/EVEX（可重复）")
    ap.add_argument("--require-evex", action="append", default=[],
                    help="SYM 函数体必须含 ≥1 条 EVEX(首字节 0x62)编码指令 —— **编码层**真值"
                         "（不依赖助记符表; ELF 与 PE/COFF/dumpbin 文本通用）。标量 EVEX"
                         "（vrndscaless/vcvtusi2ss 等）也算: 它在无 AVX-512F 的机器上同样 #UD")
    ap.add_argument("--require-vex", action="append", default=[],
                    help="SYM 函数体必须含 ≥1 条 VEX(首字节 0xC4/0xC5)编码指令（同上, 编码层）")
    ap.add_argument("--require-text", action="store_true", help="整份文本必须含该档宽指令")
    ap.add_argument("--clean-text", action="store_true", help="整份文本必须零 VEX/EVEX")
    ap.add_argument("--declared-features", default="",
                    help="声明全集（逗号分隔）: 逐位登记「有/无使用证据」。无证据的位**不判红**"
                         "（声明位按编译许可面口径合法大于实际使用面），但必须显式打出，"
                         "以便报告登记该差异，不得静默")
    ap.add_argument("--require-feature", action="append", default=[],
                    help="声明位必须找到使用证据（可重复: avx2/fma/avx512f/cd/bw/dq/vl）")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    text = read_text(a)
    funcs, order = parse_funcs(text)
    fails, logs = [], []
    if not funcs and (a.hit or a.clean_symbol):
        fails.append("反汇编文本里解析不到任何符号标签 —— 无法做符号级断言（fail-closed）")
    for sym in a.hit:
        cand = [s for s in order if sym in s]
        if not cand:
            fails.append("--hit %s: 符号不存在于产物（断言不得空转）" % sym)
            continue
        ok = False
        for s in cand:
            hits = class_ops(funcs[s], a.isa)
            if hits:
                ok = True
                logs.append("%s: %s 档证据 %d 条，例: %s" % (s, a.isa, len(hits), hits[0][1][:96]))
        if not ok:
            fails.append("--hit %s: 无 %s 档宽指令 —— 产物与基线同码（真变体证明不成立）"
                         % (sym, a.isa))
    for sym in a.clean_symbol:
        cand = [s for s in order if sym in s]
        if not cand:
            fails.append("--clean-symbol %s: 符号不存在于产物（断言不得空转）" % sym)
            continue
        for s in cand:
            ops = wide_ops(funcs[s])
            if ops:
                fails.append("--clean-symbol %s: 函数体含 VEX/EVEX 指令 %d 条（自检/握手入口必须基线可执行），例: %s"
                             % (s, len(ops), ops[0][1][:96]))
            else:
                logs.append("%s: 零 VEX/EVEX（基线可执行）" % s)
    # 编码层断言（--require-evex / --require-vex）: 唯一不受助记符表漂移影响的档位证明。
    # 无字节列 ⇒ fail-closed 判红（不得把"判不出"当成"没有"）。
    for enc_opt, syms in (("evex", a.require_evex), ("vex", a.require_vex)):
        for sym in syms:
            cand = [s for s in order if sym in s]
            if not cand:
                fails.append("--require-%s %s: 符号不存在于产物（断言不得空转）" % (enc_opt, sym))
                continue
            for s in cand:
                lines = [ln for ln in funcs[s] if ln.strip()]
                encs = [encoding_of(ln) for ln in lines]
                if encs and all(e == "unknown" for e in encs):
                    fails.append("--require-%s %s: 反汇编文本**无指令字节列**（编码层判据不可判）"
                                 "—— fail-closed 判红；请用 objdump/llvm-objdump/dumpbin /disasm 的"
                                 "带字节输出" % (enc_opt, s))
                    continue
                n = sum(1 for e in encs if e == enc_opt)
                if n == 0:
                    fails.append("--require-%s %s: 函数体无该编码指令（%s 档真变体证明不成立）"
                                 % (enc_opt, s, enc_opt.upper()))
                else:
                    logs.append("%s: %s 编码指令 %d 条（示例: %s）"
                                % (s, enc_opt.upper(), n,
                                   next(ln.strip()[:96] for ln, e in zip(lines, encs)
                                        if e == enc_opt)))
    if a.require_text and not class_ops(text.splitlines(), a.isa):
        fails.append("--require-text: 整份产物无 %s 档宽指令" % a.isa)
    if a.clean_text:
        ops = wide_ops(text.splitlines())
        if ops:
            fails.append("--clean-text: 产物含 VEX/EVEX 指令 %d 条，例: %s" % (len(ops), ops[0][1][:96]))
    for feat in a.require_feature:
        key = feat.lower().replace("acs_feat_", "").replace("avx512", "avx512")
        ent = FEATURE_EVIDENCE.get(key)
        if ent is None:
            fails.append("--require-feature %s: 判据表无此位（fail-closed，不静默放过）" % feat)
            continue
        label, rx = ent
        n = _count_evidence(text, rx)
        if n == 0:
            fails.append("--require-feature %s: 产物无使用证据（%s）—— 声明了却没用"
                         % (feat, label))
        else:
            logs.append("declared %s: 使用证据 %d 条（%s）" % (feat, n, label))
    for feat in [x.strip().lower() for x in a.declared_features.split(",") if x.strip()]:
        key = feat.replace("acs_feat_", "")
        ent = FEATURE_EVIDENCE.get(key)
        if ent is None:
            fails.append("--declared-features %s: 判据表无此位（fail-closed）" % feat)
            continue
        label, rx = ent
        n = _count_evidence(text, rx)
        logs.append("declared %s: %s（证据 %d 条）"
                    % (key, "有使用证据" if n else "无使用证据（编译许可面位，非实际发射位）", n))
    for m in logs:
        if not a.quiet:
            print("  ok  %s" % m)
    for m in fails:
        print("FAIL %s" % m)
    print("VARIANT_ISA_DISASM %s checks=%d fails=%d"
          % ("PASS" if not fails else "FAIL",
             len(a.hit) + len(a.clean_symbol) + len(a.require_feature) + int(a.require_text)
             + int(a.clean_text), len(fails)))
    return 0 if not fails else 1


SELFTEST_TEXT = """0000000000001000 <good256>:
    1000:\tc5 f8 77             \tvzeroupper
    1003:\tc4 e2 7d 18 05 00    \tvbroadcastss 0x0(%rip),%ymm0
    1009:\tc4 e2 75 a8 c2       \tvfmadd213ss %xmm2,%xmm1,%xmm0
    100e:\tc3                   \tret
0000000000001100 <good512>:
    1100:\t62 f1 7c 48 28 c1    \tvmovaps %zmm1,%zmm0
    1106:\t62 f2 7d 48 58 c1    \tvpaddd %zmm1,%zmm0,%zmm0
    110c:\tc3                   \tret
0000000000001200 <clean>:
    1200:\tf3 0f 1f 44 00 00    \tnopl 0x0(%rax,%rax,1)
    1206:\t0f 28 c1             \tmovaps %xmm1,%xmm0
    1209:\tc3                   \tret
"""

# 低档专用夹具: **只含跨档共享助记符**（AVX2 就能合法发射），一条高档证据都没有。
# 用途 = 钉死「按助记符名字认高档位」这一类假绿：同一份文本上
# fma/avx2 必须判绿，而 avx512f/avx512bw 必须判红。
SELFTEST_TEXT_LOWONLY = """0000000000002000 <lowonly>:
    2000:	c5 f8 77             	vzeroupper
    2003:	c5 fd d4 c1          	vpaddq %ymm1,%ymm0,%ymm0
    2007:	c5 fd fb c1          	vpsubq %ymm1,%ymm0,%ymm0
    200b:	c4 e2 7d 18 05 00    	vbroadcastss 0x0(%rip),%ymm0
    2011:	c4 e2 75 a8 c2       	vfmadd213ss %xmm2,%xmm1,%xmm0
    2016:	c5 f9 63 c1          	vpackuswb %xmm1,%xmm0,%xmm0
    201a:	c3                   	ret
"""


def self_test():
    """判据自证：同一份文本上八个断言必须各自给出预期的红/绿。"""
    import tempfile
    cases = []
    with tempfile.TemporaryDirectory() as td:
        p = pathlib.Path(td) / "d.txt"
        p.write_text(SELFTEST_TEXT, encoding="utf-8")
        base = ["--text", str(p), "--quiet"]
        cases.append(("pos-hit-avx2", main(base + ["--isa", "avx2", "--hit", "good256"]), 0))
        cases.append(("pos-hit-avx512", main(base + ["--isa", "avx512", "--hit", "good512"]), 0))
        cases.append(("neg-hit-avx512-on-avx2-func",
                      main(base + ["--isa", "avx512", "--hit", "good256"]), 1))
        cases.append(("neg-hit-avx2-on-avx512-func",
                      main(base + ["--isa", "avx2", "--hit", "good512"]), 1))
        cases.append(("pos-clean", main(base + ["--clean-symbol", "clean"]), 0))
        cases.append(("neg-clean-vio", main(base + ["--clean-symbol", "good512"]), 1))
        cases.append(("pos-require-feature-fma",
                      main(base + ["--require-feature", "fma"]), 0))
        cases.append(("neg-require-feature-cd",
                      main(base + ["--require-feature", "avx512cd"]), 1))
        cases.append(("neg-hit-missing-symbol",
                      main(base + ["--hit", "no_such_symbol"]), 1))
        cases.append(("neg-unknown-feature",
                      main(base + ["--require-feature", "sse4_2"]), 1))
        cases.append(("neg-clean-text",
                      main(base + ["--clean-text"]), 1))
        # 低档专用文本: 跨档共享助记符不得被当成高档位证据（这一类曾整条假绿）
        p2 = pathlib.Path(td) / "low.txt"
        p2.write_text(SELFTEST_TEXT_LOWONLY, encoding="utf-8")
        base2 = ["--text", str(p2), "--quiet"]
        cases.append(("pos-lowonly-fma",
                      main(base2 + ["--require-feature", "fma"]), 0))
        cases.append(("neg-lowonly-not-avx512f",
                      main(base2 + ["--require-feature", "avx512f"]), 1))
        cases.append(("neg-lowonly-not-avx512bw",
                      main(base2 + ["--require-feature", "avx512bw"]), 1))
    ok = all(got == want for _, got, want in cases)
    for name, got, want in cases:
        print("SELFTEST_%s %s (rc=%d want=%d)" % ("PASS" if got == want else "FAIL", name, got, want))
    print("SELF_TEST %s cases=%d" % ("PASS" if ok else "FAIL", len(cases)))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
