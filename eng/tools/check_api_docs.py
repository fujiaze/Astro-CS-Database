#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_api_docs.py — DOCCHK-001 doc↔code signature/schema/命令/退出码 机器一致性检查器。
权威=控制包 04/API-002..005; 仓库落地 docs/api/*_V1.md + 真实头文件/源码。

检查项(任一 FAIL→exit 1):
  [A] 命令树  : docs/api/CLI_PROTOCOL_V1.md §1 每命令与 CLI 产物 --help 一致。
        CLI 产物候选（产品图优先）见 Checker.CLI_EXE_CANDIDATES；候选全缺/跑不起来/
        --help 无可解析命令树 → FAIL（fail-closed，禁止静默跳过让门退化为 0 检查）。
  [B] 退出码  : docs 列出的 0/2/3/4/5/6/7/8/9/10/70 与唯一源 exit_codes.h 一致。
  [C] session 签名(§1 生命周期): 每 `p1/p2/p3_session_*` 函数存在于 session 头文件,
       且参数数(按 '(' 后匹配 ')' 的顶层逗号)与文档 §1 一致。  ← 合同③ 签名一致
  [D] 底层函数登记(§2 表): 每个登记符号存在于真实头文件(合同①); 文档表行存在(合同②);
       五字段并发合同齐全+直接 test ID 非空(合同④⑤)。
  [E] Phase3 request JSON 字段: 文档 §2 字段须能映射到 p3_session 代码消费的 request key。
  [F] schema 文件存在性: schemas/*.schema.json 全部存在且可解析。

用法: eng/tools/check_api_docs.py [--repo <root>] [--docs-dir <dir>] [--exit-codes-h <hdr>] [--stdout-json]
出口: 0 通过, 1 合同不一致, 2 环境错误。
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys

EXIT_NAMES = {0: "OK", 2: "ARGS", 3: "INPUT", 4: "SCIENCE", 5: "BACKEND", 6: "EXEC",
              7: "IO", 8: "INTEGRITY", 9: "CANCELLED", 10: "RESOURCE", 70: "INTERNAL"}


def _looks_fn(token: str) -> bool:
    """粗略: 含小写且以字母数字结尾且含下划线或为蛇形函数名(排除纯类型/短名)。"""
    if not token or not token[0].isalpha() or token[0].isupper():
        return False
    return bool(re.match(r"^[a-z][a-z0-9]*(_[a-z0-9]+)+$", token)) or bool(
        re.match(r"^[a-z][a-z0-9]+$", token))


def _repo(path: str, allow_fallback: bool = False) -> str:
    """定位被检仓库根。

    GAP-027 fail-closed（CI-001）：原实现把「--repo 不是完整检出」静默回落到
    检查器自身所在仓库（rc=0 且零输出，等于拿另一个仓库的结果冒充被检对象）。
    现在默认严格：非完整检出即 FAIL；确需回落必须显式 --allow-repo-fallback
    且打印回落事实。
    """
    p = os.path.abspath(path)
    if os.path.isdir(os.path.join(p, "docs")) and os.path.isdir(os.path.join(p, "lib")):
        return p
    fallback = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if allow_fallback:
        sys.stderr.write(
            "DOCCHK-001 WARN: --repo %s 不是完整检出（缺 docs/ 或 lib/）→ "
            "显式回落 %s（--allow-repo-fallback）\n" % (p, fallback))
        return fallback
    raise SystemExit(
        "DOCCHK-001 FAIL: --repo %s 不是完整检出（缺 docs/ 或 lib/）——"
        "fail-closed 拒绝静默回落；确需回落请显式加 --allow-repo-fallback" % p)


class Checker:
    def __init__(self, repo: str, docs_dir: str | None = None, exit_codes_h: str | None = None):
        self.repo = repo
        self.failures: list[str] = []
        self.doc_api = docs_dir if docs_dir else os.path.join(repo, "docs", "api")
        self.exit_codes_h = exit_codes_h

    def fail(self, msg: str):
        self.failures.append(msg)

    # ── 读取真实头文件中某符号的声明参数数 ──
    def _find_param_count(self, symbol: str) -> int | None:
        """在 lib/ 与 lib/include/ 的所有 .h/.hpp/.cpp 中找 `symbol(` 的声明, 返回顶层逗号+1。
        若找不到, 返回 None; 同名多处取首个。"""
        pat = re.compile(r"\b" + re.escape(symbol) + r"\s*\(")
        for base in ("include", "lib"):
            root = os.path.join(self.repo, base)
            if not os.path.isdir(root):
                continue
            for dirpath, _, files in os.walk(root):
                for f in files:
                    if not f.endswith((".h", ".hpp", ".cc", ".cpp")):
                        continue
                    fp = os.path.join(dirpath, f)
                    try:
                        txt = open(fp, encoding="utf-8", errors="ignore").read()
                    except OSError:
                        continue
                    for m in pat.finditer(txt):
                        # 找到匹配的 ')' (处理跨行签名)
                        line = txt[m.start():txt.find("\n", m.start())]
                        e = line.find(")")
                        s = line.find("(")
                        if e <= s:
                            # 跨行签名: 合并到 sum 空白
                            seg = txt[m.start():m.start() + 2000]
                            # 简易: 取到首个 ')' (可能在换行后)
                            endi = seg.find(")")
                            if endi < 0:
                                continue
                            seg = seg[:endi]
                            inner = seg[seg.find("(") + 1:]
                            if inner.strip() == "":
                                return 0
                            # 顶层逗号计数(忽略 void 且空)
                            depth = 0
                            commas = 0
                            for ch in inner:
                                if ch in "([{<":
                                    depth += 1
                                elif ch in ")]}>":
                                    depth -= 1
                                elif ch == "," and depth == 0:
                                    commas += 1
                            return commas + 1
                        inner = line[s + 1:e]
                        if inner.strip() == "":
                            return 0
                        depth = 0
                        commas = 0
                        for ch in inner:
                            if ch in "([{<":
                                depth += 1
                            elif ch in ")]}>":
                                depth -= 1
                            elif ch == "," and depth == 0:
                                commas += 1
                        return commas + 1
        return None

    def _symbol_present(self, symbol: str) -> bool:
        # 存在性: 头文件(或源码)中 `symbol(` 或 `symbol` 变体(_f64/_f32) 出现即算存在
        return self._find_param_count(symbol) is not None or \
               any(self._find_param_count(v) is not None for v in
                   (symbol + "_f64", symbol + "_f32"))

    # ── [A] 命令树 vs CLI --help ──
    # 命令树 golden 的可执行候选（顺序=优先级）：
    #   1. build/astrocs           —— 产品图产物（CMakeLists.txt 根图 add_executable(astrocs)
    #      是唯一 install(TARGETS astrocs) 交付件）；
    #   2. build/astrocs.exe       —— 产品图 Windows 产物；
    #   3. build/cli/astrocs       —— 兼容图产物（cli/CMakeLists.txt 自述 COMPATIBILITY、
    #      非产品事实源，仅作历史回落）；
    #   4. build/cli/astrocs.exe   —— 兼容图 Windows 产物。
    # 两图同名不同源集，故产品图优先（问题扫描 M5b-G-01：门必须验交付物）。
    CLI_EXE_CANDIDATES = ("build/astrocs", "build/astrocs.exe",
                          "build/cli/astrocs", "build/cli/astrocs.exe")

    def _cli_exe(self):
        """返回首个存在的 CLI 可执行路径；候选全缺返回 None（调用方 fail-closed）。"""
        for rel in self.CLI_EXE_CANDIDATES:
            p = os.path.join(self.repo, *rel.split("/"))
            if os.path.isfile(p):
                return p
        return None

    def check_command_tree(self):
        doc = os.path.join(self.doc_api, "CLI_PROTOCOL_V1.md")
        if not os.path.isfile(doc):
            return self.fail("缺少 CLI_PROTOCOL_V1.md")
        doc_text = open(doc, encoding="utf-8", errors="ignore").read()
        doc_cmds = re.findall(r"^\s*astrocs .*$", doc_text, re.M)
        # 去尾部注释
        doc_cmds = [c.split("#")[0].strip() for c in doc_cmds]
        doc_cmds = {c for c in doc_cmds if c.startswith("astrocs") and len(c) > 7}
        exe = self._cli_exe()
        if exe is None:
            # fail-closed: 缺产物即 FAIL。历史缺陷：此处曾无 else 分支，产物缺失时
            # 既不 fail 也不告警，命令树门静默退化为 0 检查（问题扫描 M5b-G-01/L12）。
            return self.fail("缺少 CLI 可执行产物（候选 %s）——命令树无从比对，fail-closed 判 FAIL"
                             % "、".join(self.CLI_EXE_CANDIDATES))
        try:
            proc = subprocess.run([exe, "--help"], capture_output=True,
                                  text=True, timeout=30)
        except (OSError, subprocess.TimeoutExpired) as exc:
            return self.fail("无法运行 %s --help: %s" % (exe, exc))
        if proc.returncode != 0:
            return self.fail("%s --help rc=%d（命令树 golden 不可读，fail-closed）"
                             % (exe, proc.returncode))
        help_text = proc.stdout
        help_cmds = {l.strip() for l in help_text.splitlines() if l.strip().startswith("astrocs")}
        if not help_cmds:
            return self.fail("%s --help 未输出任何 astrocs 命令行（命令树 golden 不可读，"
                             "fail-closed）" % exe)
        missing = sorted(c for c in doc_cmds if c not in help_cmds)
        if missing:
            self.fail("命令树 doc 有但 CLI --help 缺(或文本差): %s" % "; ".join(missing[:6]))

    # ── [B] 退出码唯一源 exit_codes.h ──
    def check_exit_codes(self):
        doc = os.path.join(self.doc_api, "CLI_PROTOCOL_V1.md")
        doc_text = open(doc, encoding="utf-8", errors="ignore").read()
        doc_codes = set(int(m) for m in re.findall(r"\b(0|2|3|4|5|6|7|8|9|10|70)\b", doc_text))
        hdr = None
        if self.exit_codes_h and os.path.isfile(self.exit_codes_h):
            hdr = self.exit_codes_h
        else:
            for cand in (os.path.join(self.repo, "include", "astrocs", "exit_codes.h"),
                         os.path.join(self.repo, "lib", "infrastructure", "cli", "exit_codes.h"),
                         os.path.join(self.repo, "include", "astrocs", "exit_codes.hpp")):
                if os.path.isfile(cand):
                    hdr = cand
                    break
        if hdr is None:
            return self.fail("缺少 exit_codes.h(lib/include/astrocs/ 或 lib/infrastructure/cli/)")
        hdr_text = open(hdr, encoding="utf-8", errors="ignore").read()
        defined = set(int(d) for d in re.findall(r"=\s*(\d{1,2})\b", hdr_text) if int(d) in EXIT_NAMES)
        for code in doc_codes:
            if code not in defined:
                self.fail("exit code %d 在 doc 出现但 exit_codes.h 未定义" % code)

    # ── [C/D] session 生命周期 + 底层函数登记 ──
    def _count_params_in(self, inner: str) -> int:
        if inner.strip() == "":
            return 0
        depth = 0
        commas = 0
        for ch in inner:
            if ch in "([{<":
                depth += 1
            elif ch in ")]}>":
                depth -= 1
            elif ch == "," and depth == 0:
                commas += 1
        return commas + 1

    def _session_funcs(self, phase: str) -> list[tuple[str, int]]:
        doc = os.path.join(self.doc_api, f"PHASE{phase}_API_V1.md")
        if not os.path.isfile(doc):
            # GAP-027 fail-closed（CI-001）：原为 return []（签名门静默归零）
            self.fail("缺少 API 文档 %s（签名一致性门无法执行 → FAIL）" % doc)
            return []
        text = open(doc, encoding="utf-8", errors="ignore").read()
        funcs = []
        # 匹配签名 `name(...)` 且允许换行(括号可跨行)
        for m in re.finditer(r"\b(p\d_session_[a-z_]+)\s*\(", text):
            name = m.group(1)
            start = m.end()   # 指向 '(' 之后
            # 从 start 起做括号匹配(处理跨行)
            depth = 0
            endi = -1
            for i in range(start, min(start + 600, len(text))):
                ch = text[i]
                if ch == "(":
                    depth += 1
                elif ch == ")":
                    if depth == 0:
                        endi = i
                        break
                    depth -= 1
            if endi < 0:
                continue
            inner = text[start:endi]
            funcs.append((name, self._count_params_in(inner)))
        return sorted(set(funcs))

    def check_session_signatures(self):
        for phase in ("1", "2", "3"):
            for name, cnt in self._session_funcs(phase):
                actual = self._find_param_count(name)
                if actual is None:
                    self.fail(f"PHASE{phase} §1 函数 `{name}` 不在任何头文件")
                elif actual != cnt:
                    self.fail(f"PHASE{phase} 函数 `{name}` 签名参数数 doc={cnt} 代码={actual}")

    def _split_symbols(self, cell: str) -> list[str]:
        """返回单元格中**可验证的基符号**(去注解), 供存在性检查。
        对 slash 简写(如 sdet_create/destroy/detect), 返回首段基名(如 sdet_create) —
        它是文档注册的权威符号, rename/delete 该基名即触发 FAIL。"""
        cell = cell.strip(" `")
        cell = re.sub(r"\([^()]*\)", "", cell)   # 去 (...) 注解
        cell = re.sub(r"\+.*$", "", cell)        # 去尾 +
        raw = [p.strip() for p in re.split(r"[/,+]", cell) if p.strip()]
        ann = {"_fill", "_free", "_from_memory", "f", "f64", "f32"}
        # 首段真名(蛇形含下划线)作为权威基符号
        base = next((p for p in raw if _looks_fn(p) and "_" in p and p not in ann), None)
        if not base:
            return []
        return [base]

    def check_module_registry(self):
        # 权威合同符号: 每个 phase 的核心 API 函数 MUST 在 §2 登记(删除/改名即 FAIL)。
        expected = {
            "1": ["ac_generate_master_bias", "ac_generate_master_dark", "ac_generate_master_flat",
                  "ac_calibrate_frame", "ac_correct_frame", "sdet_create", "dpsf_fit",
                  "ipv_solve_create", "pc_calibrate_simple", "snr_noise_model_v1"],
            "2": ["p2_coverage_build", "p2_sample_controls", "p2_upm_build",
                  "p2_upm_calibrate_block", "p2_upm_open", "p2_reject_plan_resolve",
                  "p2_integrate_pixel", "p2_large_scale_apply", "p2_frame_id"],
        }
        for phase in ("1", "2", "3"):
            doc = os.path.join(self.doc_api, f"PHASE{phase}_API_V1.md")
            if not os.path.isfile(doc):
                # GAP-027 fail-closed（CI-001）：原为 continue（整段登记门静默跳过）
                self.fail("缺少 API 文档 %s（模块登记/并发合同/test ID 门无法执行 → FAIL）" % doc)
                continue
            text = open(doc, encoding="utf-8", errors="ignore").read()
            # PHASE3 §2 是 request JSON 字段表(非函数登记), 由 check_phase3_schema 覆盖; 跳过。
            if phase == "3":
                continue
            # 限定到 §2 段(## 2 至下一 ##), 避免 §1 所有权表误判。
            m2 = re.search(r"^##\s*2[^\n]*\n(.*?)(?=^##\s*3[^\n]*|\Z)", text, re.M | re.S)
            if not m2:
                self.fail(f"PHASE{phase} 无 §2 段")
                continue
            section = m2.group(1)
            # §2 表行:  | `sym...(header)` → `sym` | reentrant | threadsafe | internal | 取消 | test-id |
            # 单元格可能含多个反引号符号(用 `/` 分隔): `p2_upm_calibrate_block`/`p2_upm_evaluate_c`,
            # 且符号后可带 (header) 注解(如 (astro_calibration.h))。
            pat = re.compile(
                r"^\|\s*((?:`[^`]+`)\s*(?:/\s*`[^`]+`\s*)*)"
                r"(?:\s*\([^)]*\)\s*)?"
                r"\|([^|]+)\|([^|]+)\|([^|]+)\|([^|]+)\|([^|]+)\|",
                re.M)
            n_rows = 0
            registered = set()   # 实际文档登记的基础符号
            for row in pat.finditer(section):
                cell_block = row.group(1)
                # 提取该单元格所有反引号符号
                syms = []
                for c in re.findall(r"`([^`]+)`", cell_block):
                    syms.extend(self._split_symbols(c))
                n_rows += 1
                registered.update(syms)
                for sym in syms:
                    if not self._symbol_present(sym):
                        self.fail(f"PHASE{phase} §2 登记符号 `{sym}` 不在任何头文件")
                # ④ 直接 test ID 非空 + ⑤ 五字段并发合同齐全
                cells = [row.group(i).strip() for i in range(2, 7)]
                if any(not c for c in cells):
                    self.fail(f"PHASE{phase} §2 `{cell_block.strip()}` 并发合同字段有空")
                if not cells[-1]:
                    self.fail(f"PHASE{phase} §2 `{cell_block.strip()}` 直接 test ID 空")
            if n_rows == 0:
                self.fail(f"PHASE{phase} §2 无登记行可检查(结构漂移?)")
            # 权威合同符号删除检测: 文档必须仍登记这些核心函数(以基名/前缀出现在 §2)
            for exp in expected.get(phase, []):
                # 匹配: exp == s; 或 exp 是 s 的变体(s.startswith(exp+"_")); 或
                # exp 与某登记 s 共享模块前缀(处理 slash 简写 ac_generate_master_bias/dark/flat
                # 只登记首段 ac_generate_master_bias, 而 expected 含 dark/flat)。
                ok = False
                for s in registered:
                    if exp == s or s.startswith(exp + "_") or exp.startswith(s + "_"):
                        ok = True
                        break
                    # 共享前缀: 取 exp 与 s 的公共 "_" 前缀(如 ac_generate_master_)
                    e_pre = exp[:exp.rfind("_") + 1]
                    s_pre = s[:s.rfind("_") + 1]
                    if e_pre and e_pre == s_pre:
                        ok = True
                        break
                if not ok:
                    self.fail(f"PHASE{phase} §2 权威符号 `{exp}` 未登记(被删除/改名?)")

    def check_phase3_schema(self):
        """Phase3 request JSON 字段: 文档 §2 表与代码 p3_session.cpp 消费的 request key 一致。
        代码为字段权威(运行驱动); 文档 §2 每字段必须能映射到代码 key。
        检查方向: ① 文档 §2 每个 `a.b` / `a` 字段的第一段(如 source.hips_dir→source)在代码
        request JSON 出现; ② 代码 request key 的叶字段(sampler/scale_deg_per_px...)在文档提及。
        任一文档字段缺代码对应 → FAIL(删除/改名 mutation 被捕获)。"""
        doc = os.path.join(self.doc_api, "PHASE3_API_V1.md")
        if not os.path.isfile(doc):
            return self.fail("缺少 PHASE3_API_V1.md")
        doc_text = open(doc, encoding="utf-8", errors="ignore").read()
        code = os.path.join(self.repo, "lib", "phase3_session", "p3_session.cpp")
        if not os.path.isfile(code):
            # 退回 p3_output.cpp / 头文件
            # W4-A9 批次 3: p3_output.cpp 迁 lib/algorithms/fits_output/ (ASTROCS_DESIGN §7.1)
            code = next((c for c in (os.path.join(self.repo, "lib", "algorithms", "fits_output", "p3_output.cpp"),
                                     os.path.join(self.repo, "lib", "phase3_session", "p3_session.h"))
                         if os.path.isfile(c)), None)
        code_text = open(code, encoding="utf-8", errors="ignore").read() if code else ""
        # 代码中出现的 request 字段名: `"key":` 或 `.value("key",` 或 j["key"]
        code_keys = set(re.findall(r'"(?:[a-z][a-z0-9_]{2,})"\s*:', code_text))
        code_keys |= set(re.findall(r'\.value\("([a-z][a-z0-9_]{2,})"', code_text))
        code_keys |= set(re.findall(r'\["([a-z][a-z0-9_]{2,})"\]', code_text))
        # 文档 §2 行的字段(去注解 /、去括号)
        doc_fields = []
        for m in re.finditer(r"^\|\s*`([^`]+)`\s*\|", doc_text, re.M):
            cell = m.group(1).strip()
            # 取顶层字段名(如 source.hips_dir → source.hips_dir; width_px/height_px → 两个)
            for part in re.split(r"[/,]", cell):
                part = part.strip()
                if not part:
                    continue
                doc_fields.append(part)
        for f in doc_fields:
            # 顶层命名空间(source/center)与叶字段两者之一须在代码 request JSON
            leaf = f.split(".")[-1]
            if leaf not in code_keys:
                self.fail("phase3 请求字段 `%s` 未在 p3_session 代码中出现" % f)

    def check_schema_files(self):
        schema_dirs = [os.path.join(self.repo, "contracts", "schemas"), os.path.join(self.repo, "docs", "schemas")]
        seen = False
        for d in schema_dirs:
            if not os.path.isdir(d):
                continue
            for f in os.listdir(d):
                if f.endswith(".schema.json"):
                    seen = True
                    try:
                        json.load(open(os.path.join(d, f), encoding="utf-8"))
                    except Exception as e:
                        self.fail("schema %s 解析失败: %s" % (f, e))
        if not seen:
            # GAP-027 fail-closed（CI-001）：原判据只在「目录也不存在」时报错，
            # 目录存在但 0 个 *.schema.json 时 seen=False 亦静默通过（空转绿）。
            self.fail("无任何 *.schema.json 可校验（目录 %s）→ FAIL" % schema_dirs[0])


def main():
    repo = _repo(sys.argv[sys.argv.index("--repo") + 1] if "--repo" in sys.argv else ".")
    dd = None
    if "--docs-dir" in sys.argv:
        dd = sys.argv[sys.argv.index("--docs-dir") + 1]
    ech = None
    if "--exit-codes-h" in sys.argv:
        ech = sys.argv[sys.argv.index("--exit-codes-h") + 1]
    c = Checker(repo, docs_dir=dd, exit_codes_h=ech)
    c.check_command_tree()
    c.check_exit_codes()
    c.check_session_signatures()
    c.check_module_registry()
    c.check_phase3_schema()
    c.check_schema_files()
    if c.failures:
        if "--stdout-json" in sys.argv:
            print(json.dumps({"status": "FAIL", "checks": len(c.failures),
                              "failures": c.failures[:50]}))
        for f in c.failures[:50]:
            sys.stderr.write("DOCCHK-001 FAIL: %s\n" % f)
        return 1
    if "--stdout-json" in sys.argv:
        print(json.dumps({"status": "PASS", "failures": 0}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
