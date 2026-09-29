#!/usr/bin/env python3
"""check_doc_symbols.py — T402 doc symbols checker

Checks: 文档中反引号符号、文件和 config key 均可解析；排除 archive 清单
        扫描面完整性：权威域目录缺失 / 扫到 0 份文档 ⇒ 判红（静默退化为恒真门的防线）
        登记表行锚：evidence 形如 <file>:<行号> 时该行须逐字含该 token（FINAL-07/line-anchor）
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
  python3 eng/tools/quality/contracts/check_doc_symbols.py              # 判据本体
  python3 eng/tools/quality/contracts/check_doc_symbols.py --self-test  # 正例绿/负例红夹具面
"""
import argparse, itertools, json, pathlib, re, sys, csv
import shutil, subprocess, tempfile

# 权威文档域（判定面 = 这些目录下的 *.md）。改判定面必须显式改本常量：
# 目录缺失或一份文档都没扫到 ⇒ 本门没有判据对象，此时判 PASS 就是恒真门。
#
# FINAL-07（lead-01/auth-dirs）：判定面随文档迁移改指**新三集**。
# 旧值 ("science","algorithms","architecture","contracts","modules","design") 是**迁移前**
# 的目录名 —— 迁移后除 science 外**五个目录的 .md 数全为 0**（实测：algorithms 0 / architecture 0 /
# contracts 0 / modules 0 / design 0，跟踪件分别 3/6/4/2/0 且**无一是 .md**）。
# 本门扫的判据对象是 **.md**，故旧值实际只扫到 docs/science 的 52 篇（+2 篇 extras 共 54 份，
# 去重 53 份），而 docs/ 下 .md 共 253 篇 ⇒ **约 196 篇文档从未受本门判过**，
# 即「门绿」与「文档合规」之间存在约 78% 的盲区（恒真门形态）。
# 新值 = 迁移后的三个一级目录，判定面 249 篇（去重）/253 份（含 extras 重复）。
AUTH_DIRS = ("science", "engineering", "detail")

_HEADER_CORPUS = {}


def _header_corpus(repo: pathlib.Path):
    """lib/**、lib/include/** 的公开头文本（**一次**读入）。

    原实现对**每个** token 都重新 rglob 全树 + 逐个 read_text ⇒
    判定面 O(tokens × headers)，实测把 CON-DOC-SYMBOLS 拖到 120s 超时
    （CI 侧 120s timeout 判 TIMEOUT，判据再对也拿不到结论）。现在只读一次。
    """
    key = str(repo.resolve())
    if key in _HEADER_CORPUS:
        return _HEADER_CORPUS[key]
    texts = []
    for base in ("lib", "include"):
        for h in (repo / base).rglob("*.h"):
            if "third_party" in str(h) or "archive" in str(h):
                continue
            try:
                texts.append(h.read_text(encoding="utf-8", errors="ignore"))
            except OSError:
                continue
    _HEADER_CORPUS[key] = "\n".join(texts)
    return _HEADER_CORPUS[key]


def _defined_in_public_header(repo: pathlib.Path, token: str) -> bool:
    """token 是否在 lib/** 公开头中真实定义(枚举常量/宏/标识符级核实)。"""
    return re.search(r"\b" + re.escape(token) + r"\b", _header_corpus(repo)) is not None


# ── 非 API 命名空间解析面 ────────────────────────────────────────────
# 事由：原判据把文档反引号里的**所有**标识符形态 token 都拿去和
# `docs/architecture/api_inventory.csv`（只登记**函数签名**）比对 ⇒ 文档状态词
# (`ALG_PROPOSED`/`TARGET_NORMATIVE`/`NOT_IMPLEMENTED`/`PENDING_SO07`)、文档简写
# (`PROJECT_SPEC`/`FREEZE_LIST`/`CONFLICT_MATRIX`)、科学量符号
# (`snr_v`/`snr_v²`/`snr_identity`) 全部被误判为"API 符号不存在"（40 条）。
# 修法（**收窄判据、不放宽**）：token 必须能解析到**已登记命名空间之一**；
# 解析不到的仍然判红。新增两个 fail-closed 检查：
#   DOC-SYMBOL-REGISTRY-EVIDENCE —— 登记项 evidence 必须存在且逐字含该 token；
#   DOC-SYMBOL-REGISTRY-STALE    —— 登记项必须在扫描面真实出现（登记只减不增）。
NAMESPACES_REL = "docs/architecture/doc_symbol_namespaces.json"
GLOSSARY_REL = "docs/GLOSSARY.md"


def _doc_stem_index(repo):
    """docs/**/*.md 词干集合（文档简写引用的解析域）。"""
    return {p.stem for p in repo.glob("docs/**/*.md")}


_DOC_SUFFIXES = (".md", ".markdown")


def _stem_resolves(token, stems):
    """token 能否解析到 docs/ 的某个文档词干。

    FINAL-07（lead-01/auth-dirs）补齐：文档引用**带扩展名**时（`gaia_xpsd_client.md`）
    原实现只拿**无扩展名**词干集（`p.stem`）比对 ⇒ 永远匹配不上；该 token 又因「无 `/`」
    进不了上面的文件引用分支（该分支要求 `"/" in token and "." in token`）⇒ 一篇
    **真实存在**的兄弟文档被判 DOC-BAD-SYMBOL（实测 docs/detail/README.md:45 两例，
    两篇均 test -e EXISTS 且 git ls-files 各 1 行）。

    这**不是豁免**：剥扩展名后仍必须在词干集里精确命中，指向不存在文档的 `FOO.md`
    照旧判红。属把实现补齐到 doc_symbol_namespaces.json auto_domains[0] 的自述
    （「docs/**/*.md 的词干」），判据不放宽。
    """
    cands = [token]
    for suf in _DOC_SUFFIXES:
        if token.endswith(suf):
            cands.append(token[: -len(suf)])
            break
    return any(any(c == s or s.endswith("_" + c) for s in stems) for c in cands)


def _glossary_tokens(repo):
    """docs/GLOSSARY.md 的术语表 token 集合（唯一术语权威）。"""
    p = repo / GLOSSARY_REL
    if not p.is_file():
        return set()
    text = p.read_text(encoding="utf-8", errors="ignore")
    toks = set()
    for line in text.splitlines():
        if not line.strip().startswith("|"):
            continue
        for cell in line.strip().strip("|").split("|"):
            toks.update(re.findall(r"[A-Za-z_][A-Za-z0-9_\u00b2\u00b3]*", cell))
    return toks


def _line_of(text: str, offset: int) -> int:
    """字符偏移 → 1 基行号（判词要能定位到 文件:行，偏移对人不友好）。"""
    return text.count("\n", 0, offset) + 1


# ── 花括号形态的路径字面量（FINAL-07 / braces-crash）────────────────────
# 事由：:403 把抽取到的 token **原样**交给 Path.exists()。扫描面扩到 249 篇后首次
# 包含 docs/engineering/DUAL_LINE_CONTRACT.md，其 §2.1 点名清单用
# `docs/engineering/{A,B,C}.md` 记法，basename 281/406 **字节超 NAME_MAX(255)**
# ⇒ os.stat 抛 `OSError 36 File name too long` ⇒ **整门 traceback 退出**（不是判红）。
# 修法**不是** try/except 包住：那是把崩溃换成静默通过，等于造一个假绿。改为
# **先分类再判**：真正的「并集写法」展开后逐成员判存在，任一成员缺失即判红。
#
# 分类（返回 (kind, members)）：
#   none       —— 不含 { }，交给后续原有分支；
#   set        —— 真正的并集写法：**每一个**顶层 {…} 成员以 "," 分隔、成员内不含
#                { } 与空白 ⇒ 按**笛卡尔积**展开（支持**多个并列**花括号组，
#                如 widgets/{a,b,c}.{h,cpp} ⇒ 6 个成员；**不支持**花括号内再嵌套）。
#                展开规则有意覆盖本仓真实写法：**花括号可出现在路径中段**
#                （DUAL_LINE_CONTRACT.md 的 {abi/ABI_003_SECURE_LOADER,…}），
#                成员可自带 "/"；花括号外的前后缀原样保留
#                （PHASE{1,2,3}_API_V1.md ⇒ PHASE1_API_V1.md …）。
#   template   —— 占位符（{idx}/{ext}，无逗号）⇒ 不是集合，也不是"文件必须存在"
#                的判据对象（与 <out_hips>/x.json 同族）；
#   regex      —— 含正则字符类 [...]（[A-Za-z0-9/%._-]{0,32}）：{0,32} 是量词
#                不是集合，展开会造出假红；
#   prose      —— token 内含空白：路径字面量不含空白，含空白即散文里夹的枚举；
#   unbalanced —— 括号不配平 / 成员内含嵌套花括号 / 有空成员 ⇒ 同 template 处理；
#   overflow   —— 展开后成员数超 _BRACE_MAX_MEMBERS：防组合爆炸，同 template 处理。
# 后五类**既不判红也不判绿**，但一律计入 result["brace_forms"] 并打印
# CON-DOC-SYMBOLS_BRACE: 一行 —— 静默放过必须留痕，否则扫描面一变宽又成恒真门。
_BRACE_MAX_MEMBERS = 256


def _brace_form(token):
    """把含 { } 的 token 分类。返回 (kind, members)；非花括号 token 返回 ("none", [])。"""
    if "{" not in token and "}" not in token:
        return "none", []
    if "[" in token and "]" in token:
        return "regex", []          # 正则字符类 ⇒ 量词，不是集合
    if " " in token or chr(10) in token:
        return "prose", []          # 路径字面量不含空白
    groups, i, n = [], 0, len(token)
    while i < n:
        if token[i] == "{":
            j = token.find("}", i + 1)
            if j < 0:
                return "unbalanced", []
            groups.append((i, j, token[i + 1:j]))
            i = j + 1
        else:
            i += 1
    if not groups:
        return "template", []
    alts = []
    for _, _, inner in groups:
        parts = inner.split(",")
        if len(parts) < 2 or any((not q) or (" " in q) or ("{" in q) or ("}" in q)
                                 for q in parts):
            return "template", []   # 无逗号 ⇒ 占位符；空/嵌套成员 ⇒ 不展开
        alts.append(parts)
    total = 1
    for a in alts:
        total *= len(a)
    if total > _BRACE_MAX_MEMBERS:
        return "overflow", []
    members, seen = [], set()
    for combo in itertools.product(*alts):
        buf, prev = [], 0
        for (s, e, _), q in zip(groups, combo):
            buf.append(token[prev:s])
            buf.append(q)
            prev = e + 1
        buf.append(token[prev:])
        m = "".join(buf)
        if m not in seen:
            seen.add(m)
            members.append(m)
    return "set", members


def _exempt_form(token):
    """token **自身**可判定的「不是『文件必须存在』的判据对象」形态。返回理由串或 None。

    ① 历史引证：token 带 已删除/已归档/deleted/removed（文档已声明该文件不存在）；
    ② 通配：* 或 ?（花括号并集**不**在此豁免 —— 它已被 _brace_form 展开并逐成员判）；
    ③ 含空白的命令行 / 散文。
    三者都只看 token 自身，不需要外部豁免表。
    """
    if any(k in token for k in ("已删除", "已归档", "deleted", "removed")):
        return "历史引证标记"
    if "*" in token or "?" in token:
        return "通配"
    if " " in token:
        return "含空白(命令行/散文)"
    return None


class _Undecidable(Exception):
    """单个路径**判不出来**（OS 层拒绝该名字，如 ENAMETOOLONG）。

    与「不存在」是两件事：既不能放行（假绿），也不该冒到 main 外（整门崩）。
    调用方据此记 DOC-FILE-UNDECIDABLE（判红）。
    """


def _file_ref_exists(repo, rel, known_files):
    """文件引用的四步解析（known_files → 仓根相对 → lib 前缀 → 后缀匹配）。

    抽成函数是为了让**普通 token 与花括号展开出的成员走同一套**判据，
    免得两条路径日后各自漂移。返回 True/False，判不出来时抛 _Undecidable。
    """
    if rel in known_files:
        return True
    probes = (lambda: (repo / rel).exists(),
              lambda: (repo / "lib/algorithms/coverage" / rel).exists(),
              lambda: (repo / "lib" / rel).exists())
    for probe in probes:
        try:
            if probe():
                return True
        except OSError as exc:     # ENAMETOOLONG(36)/EINVAL/ENAMETOOLONG… 如实上抛
            raise _Undecidable("%s（%s）" % (rel, exc)) from exc
    # rglob 相对路径在 Windows 是 backslash —— 统一正斜杠再匹配
    return any(k.replace("\\", "/").endswith("/" + rel) for k in known_files)


# ── 行锚判据（FINAL-07 / line-anchor）────────────────────────────────────
# 事由：_load_symbol_namespaces 里 `rel, _, line_s = ev.rpartition(":")` 把行号切出来
# 后，**`line_s` 全程未被引用**（AST 死存储实测：赋值 1 次、读取 0 次）—— 本门对
# 登记项只判 ① evidence 文件存在 ② **整份文件**逐字含该 token，**从不判行号**。
# ⇒ 登记表里的行号是**死字段**：写成任何值门都不红（文件级仍含该 token）。
# 实测（上一单）：把某条 evidence 改回**同文件但错 7 行**的锚点，门**不红**；
# 改成「存在但不含 token 的文件」才红 ⇒ **文件级承重、行级不承重**。
# 独立审计早已记录同一形态：独立审计/证据/AUD-101-DB-05.md:56
# 「门只判文件级出现，行锚不受判据保护」（并实测 4 条登记里 3 条行锚漂移）。
# 修法（**收严，不放宽**）：evidence 形如 `<file>:<行号>` 时**逐字读该行**，
# 要求该行含该 token；不含即判红。
#
# 判词单列 DOC-SYMBOL-REGISTRY-LINEANCHOR，**不与文件级 EVIDENCE 混同**：
# 「文件没了」与「行漂了」是两种故障、两种修法，混同则回归对比无法逐项归因。
_LINE_EXPECT = "该行逐字含 token（<file>:<行号> 的行锚是判据，不是注记）"
_ANCHOR_NUM = re.compile(r"^[+-]?[0-9]+$")
# 注释行识别只认**该文件语言下确实是注释**的行首。
# ⚠ 绝不能「行首是 # 就当注释」一刀切 —— 本仓 46 条行锚里有 **4 条落在 C/C++
#    预处理指令上**（P2_SEMANTIC_ → rejection.h:66 `#define`；__AVX512CD__ →
#    avx512_backend.cpp:27 `#if defined(...)`；__AVX512F__ →
#    avx512_backend_kernels.cpp:17 `#if defined(_MSC_VER)`；RICE_1 → fitsio.h:296
#    `#define`）。`#define` 是**定义**不是注释，一刀切会当场造 4 条假红。
_CPP_DIRECTIVE = re.compile(
    r"^#\s*(?:define|include|include_next|undef|if|ifdef|ifndef|elif|else|endif|"
    r"pragma|error|warning|line)\b")
# 这些扩展名里 `#` 就是注释（Python/CMake）。Markdown 的 `#` 是标题、
# JSON/CSV/YAML/TXT 的 `#` 是数据 —— **都不是注释**，不得一刀切。
_HASH_IS_COMMENT_EXT = (".py", ".pyi", ".cmake")


def _parse_line_anchor(line_s):
    """evidence 行号后缀 → 声称的 1 基行号；**不是**纯数字则 None（= 没有行锚）。

    区分两件事：
      · `line_s` 为空 / 非数字（路径自带冒号如 `docs/a:b.md`、区间写法 `12-15`）
        ⇒ 判为「这条 evidence 没有行锚」，不判红、计数留痕。
      · `line_s` 是数字（哪怕 0 或负数）⇒ 登记表**声称**了一个行锚，越界即硬错判红。
    绝不能把越界值当成「没有行锚」—— 那等于给 `:0` 留了一条逃逸后门。
    """
    if not _ANCHOR_NUM.match(line_s or ""):
        return None
    return int(line_s)


def _is_comment_line(rel, line):
    """该行是否是该文件语言下的注释行（被注释掉的出现不承重）。"""
    s = line.lstrip()
    if not s:
        return False                      # 空行不判注释（否则报一个无意义的红）
    if s.startswith("//") or s.startswith("/*"):
        return True
    if s.startswith("*") and not s.startswith("*/"):
        return True                       # 块注释续行
    if s.startswith("#"):
        low = rel.lower()
        if low.endswith(_HASH_IS_COMMENT_EXT) or low.rsplit("/", 1)[-1].startswith("cmake"):
            return not s.startswith("#!")  # Python/CMake：# 就是注释（shebang 除外）
        return _CPP_DIRECTIVE.match(s) is None   # C/C++：预处理指令不是注释
    return False


def _line_anchor_fault(repo, rel, line_no, tok, lines_of):
    """核对 `rel:line_no` 这一行是否**逐字承载** tok。

    返回判红理由串；承载则返回 None。lines_of 是**按 rel 缓存**的取行函数
    （同一份大文件被多条登记项引用时只读一次 —— 与文件级检查共用同一份语料）。
    """
    try:
        lines = lines_of(rel)
    except OSError as exc:
        return "evidence 文件读不出: %s（%s）" % (rel, exc)
    if not 1 <= line_no <= len(lines):
        return "行锚越界: %s:%d，文件共 %d 行" % (rel, line_no, len(lines))
    hit = lines[line_no - 1]
    if tok in hit:
        if _is_comment_line(rel, hit):
            return "行锚行是注释行（被注释掉的不承重）: %s:%d = %r" % (
                rel, line_no, hit.strip()[:80])
        return None
    hint = ""
    if line_no < len(lines) and tok in lines[line_no]:
        hint = "；该 token 实际出现在 :%d" % (line_no + 1)
    return "行锚行不含该 token: %s:%d = %r%s" % (rel, line_no, hit.strip()[:80], hint)


def _load_symbol_namespaces(repo, findings, lstat):
    """读命名空间登记表并逐条校验 evidence（fail-closed）。返回 (tokens, ok)。"""
    p = repo / NAMESPACES_REL
    if not p.is_file():
        findings.append({"id": "DOC-SYMBOL-REGISTRY-MISSING", "severity": "P1",
                         "file": NAMESPACES_REL, "symbol": NAMESPACES_REL,
                         "observed": "命名空间登记表不存在", "expected": "exists"})
        return set(), False
    try:
        doc = json.loads(p.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        findings.append({"id": "DOC-SYMBOL-REGISTRY-BAD-JSON", "severity": "P1",
                         "file": NAMESPACES_REL, "symbol": NAMESPACES_REL,
                         "observed": "不可解析: %s" % exc, "expected": "valid JSON"})
        return set(), False
    toks, ok = set(), True
    # 行锚语料按 rel 缓存：同一份大文件（module_adapters.cpp 有 5 条登记项指向它）
    # 只读一次，行级与文件级共用同一份 splitlines 结果。
    _lines_cache = {}

    def _lines_of(_r):
        if _r not in _lines_cache:
            _lines_cache[_r] = (repo / _r).read_text(
                encoding="utf-8", errors="ignore").splitlines()
        return _lines_cache[_r]

    for ent in doc.get("registered_symbols", []):
        tok = ent.get("token")
        ev = ent.get("evidence", "")
        if not tok or not ev:
            findings.append({"id": "DOC-SYMBOL-REGISTRY-EVIDENCE", "severity": "P1",
                             "file": NAMESPACES_REL, "symbol": str(tok),
                             "observed": "登记项缺 token/evidence", "expected": "两者齐备"})
            ok = False
            continue
        rel, _, line_s = ev.rpartition(":")
        rel = rel or ev
        # FINAL-07（braces-crash）同形态加固：evidence 也是一条路径，若登记表里
        # 出现 `docs/{A,B}.py:3` 这类并集写法，`.is_file()` 同样会 OSError 36 崩掉
        # 整门（doc_symbol_namespaces.json 正在被别人编辑 ⇒ 同一形态随时可复现）。
        # 这里按同一规则展开：并集写法要求**全部成员**都命中，否则判红（不放宽）。
        # 实测当前登记表 0 条 brace evidence ⇒ 本加固不改今日判定结果。
        _bkind, _bmembers = _brace_form(rel)
        _rel_list = _bmembers if _bkind == "set" else [rel]
        _ev_ok = True
        for _r in _rel_list:
            try:
                if not (repo / _r).is_file():
                    _ev_ok = False
                    break
            except OSError as exc:
                _ev_ok = False
                _ev_err = "%s（%s）" % (_r, exc)
                break
        if not _ev_ok:
            findings.append({"id": "DOC-SYMBOL-REGISTRY-EVIDENCE", "severity": "P1",
                             "file": NAMESPACES_REL, "symbol": tok,
                             "observed": "evidence 文件不存在 %s" % ev, "expected": "exists"})
            ok = False
            continue
        evp = repo / _rel_list[0]
        evtext = evp.read_text(encoding="utf-8", errors="ignore")
        if tok not in evtext:
            findings.append({"id": "DOC-SYMBOL-REGISTRY-EVIDENCE", "severity": "P1",
                             "file": NAMESPACES_REL, "symbol": tok,
                             "observed": "evidence 文件未逐字含该 token: %s" % ev,
                             "expected": "token 出现在 evidence"})
            ok = False
            continue
        # ⚠ **判据次序**：文件级在前、行级在后，这是**有意**的，不是随手写的。
        # 「该 token 整个文件里都没有」⇒ 报 EVIDENCE（词准：文件里根本没有）；
        # 「该 token 在文件里但不在这一行」⇒ 报 LINEANCHOR（词准：行漂了）。
        # 若把两级调换，前者会被误报成「行漂了」，而后者的真正原因被掩盖。
        # token 折行书写（`FOO_` / `BAR = 1`）落在前者：判红，但判词是 EVIDENCE。
        # ── 行锚承重（FINAL-07 / line-anchor）：line_s 从此**是**判据 ──────────
        # 上游 :352 把行号切出来后从未引用 ⇒ 行号是死字段（见本节上方判据说明）。
        # 现在逐字读该行核对。并集写法（brace set）与文件级判据同口径：
        # **全部成员**的行锚都要命中，任一不命中即判红。
        _anchor = _parse_line_anchor(line_s)
        if _anchor is None:
            # 纯文件级 evidence（无行号 / 后缀非纯数字，如路径自带冒号）
            # ⇒ **不判红**（保持兼容），但**计数留痕**并在 stdout 打出，
            #    否则「一条都没判过」与「判据坏了」在输出上不可区分（恒真门形态）。
            lstat["no_line"] += 1
        else:
            _anchor_ok = True
            for _r in _rel_list:
                _bad = _line_anchor_fault(repo, _r, _anchor, tok, _lines_of)
                if _bad is None:
                    lstat["pass"] += 1
                    continue
                findings.append({"id": "DOC-SYMBOL-REGISTRY-LINEANCHOR", "severity": "P1",
                                 "file": NAMESPACES_REL, "symbol": tok,
                                 "observed": _bad, "expected": _LINE_EXPECT})
                lstat["fault"] += 1
                _anchor_ok = False
            lstat["claimed"] += 1
            if not _anchor_ok:
                # fail-closed 与文件级 EVIDENCE 失败**同口径**：行锚未过审的登记项
                # 不得进解析面（否则它仍在替文档里的 token 挡 DOC-BAD-SYMBOL，
                # 那就是一条没审过的豁免）。连带效应：引用该 token 的文档会同时
                # 报 DOC-BAD-SYMBOL —— 这是同一次故障的级联，不是误报。
                ok = False
                continue
        toks.add(tok)
    return toks, ok


# ── 内置夹具面（--self-test）：正例判绿 + 负例判红 + 恢复判绿 ────────────────
# 夹具全部落临时目录（**不写仓库正文**）；子进程跑**真实 CLI** —— 判据怎么判，
# 夹具面就怎么判。路径一律拼接构造，不在源码里写死仓库正文里的 docs 路径字面量。
_FIXTURE_DOC = "FIXTURE_SAMPLE.md"


def _dp(*parts):
    """夹具仓库相对路径（拼接构造）。"""
    return str(pathlib.PurePosixPath("docs", *parts))


def _registry(entries):
    return {"schema": "astrocs/doc-symbol-namespaces/v1", "auto_domains": [],
            "registered_symbols": list(entries)}


def _fixture(root, name, *, doc_tokens=(), registry=None, skip_registry=False,
             domains=None, extra_files=()):
    """造一个最小夹具仓库，返回其根目录。"""
    base = pathlib.Path(root) / name
    (base / "docs" / "architecture").mkdir(parents=True, exist_ok=True)
    for d in (AUTH_DIRS if domains is None else domains):
        (base / "docs" / d).mkdir(parents=True, exist_ok=True)
    if not skip_registry:
        reg = _registry([]) if registry is None else registry
        (base / "docs" / "architecture" / "doc_symbol_namespaces.json").write_text(
            json.dumps(reg, ensure_ascii=False), encoding="utf-8")
    if doc_tokens:
        p = base / "docs" / "science" / _FIXTURE_DOC
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("# 夹具文档\n\n正文：" + "、".join("`%s`" % t for t in doc_tokens) + "\n",
                     encoding="utf-8")
    for rel, text in extra_files:
        p = base / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
    return base


def _fixture_run(root, name, **kw):
    base = _fixture(root, name, **kw)
    out = base / "selftest_out.json"
    proc = subprocess.run(
        [sys.executable, str(pathlib.Path(__file__).resolve()),
         "--repo", str(base), "--out-json", str(out)],
        cwd=str(root), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if not out.is_file():
        return proc.returncode, None, proc.stdout
    return proc.returncode, json.loads(out.read_text(encoding="utf-8")), proc.stdout


def self_test():
    """正例判绿 / 负例判红 / 恢复判绿；任一例不符预期 ⇒ rc=1。"""
    # ⚠ 夹具 evidence **故意写成 3 行**且 token 只落在**第 2 行**。
    # 理由：行锚判据的核心断言是「偏 1 行必须判红」。若夹具文件只有 1 行，
    # 「偏 1 行」就等于「越界」，测的就不是行锚漂移而是越界，两种故障混在一起
    # 就分不清哪条判据在起作用。3 行 + token 在中间 ⇒ :1 与 :3 是**纯偏移**，
    # :9999 才是**越界**，两者可分别证伪。
    common_files = (
        ("fixture_evidence.py",
         "FIXTURE_UNRELATED_A = 1\nFIXTURE_REGISTERED_TOKEN = 2\nFIXTURE_UNRELATED_B = 3\n"),
        ("lib/include/fixture_case.h", "#define FIXTURE_HEADER_CONST 1\n"),
        (_dp("architecture", "api_inventory.csv"),
         "symbol,signature\nFIXTURE_API_SYMBOL,int fixture_api_symbol(void)\n"),
        (_dp("GLOSSARY.md"),
         "| 术语 | 定义 |\n|---|---|\n| FIXTURE_GLOSSARY_TERM | 夹具术语 |\n"),
    )
    live_tokens = ("FIXTURE_REGISTERED_TOKEN", "FIXTURE_HEADER_CONST",
                   "FIXTURE_API_SYMBOL", "FIXTURE_GLOSSARY_TERM")
    reg_ok = _registry([{"token": "FIXTURE_REGISTERED_TOKEN", "namespace": "fixture",
                         "evidence": "fixture_evidence.py:2",
                         "reason": "夹具登记项：evidence 该行逐字含该 token"}])
    reg_stale = _registry([
        {"token": "FIXTURE_REGISTERED_TOKEN", "namespace": "fixture",
         "evidence": "fixture_evidence.py:2", "reason": "夹具登记项"},
        {"token": "FIXTURE_STALE_TOKEN", "namespace": "fixture",
         "evidence": "fixture_stale_evidence.py:1", "reason": "夹具登记项"}])
    # 行锚判据的夹具面（FINAL-07 / line-anchor）。每一例的 evidence 都指向
    # **同一个 3 行文件**，token 恒在第 2 行 ⇒ 只有行号在变，故障类型单一可归因。
    def _reg_at(line_s, tok="FIXTURE_REGISTERED_TOKEN"):
        return _registry([{"token": tok, "namespace": "fixture",
                           "evidence": "fixture_evidence.py:%s" % line_s,
                           "reason": "夹具登记项：行锚用例"}])
    # 注释 / 预处理指令的载体：必须**真的**有第二行非注释定义，
    # 否则「注释掉了」与「文件里根本没有」两种故障分不开。
    commented_files = (
        ("fixture_commented.py",
         "# FIXTURE_COMMENTED_TOKEN = 1\nFIXTURE_COMMENTED_TOKEN = 2\n"),)
    defined_files = (
        ("fixture_defined.h", "#define FIXTURE_DEFINED_TOKEN 1\n"),)
    wrapped_files = (
        ("fixture_wrapped.py", "FIXTURE_WRAPPED_\nTOKEN = 1\n"),)
    cases = (
        ("正例-四类命名空间全可解析", 0, None,
         dict(doc_tokens=live_tokens, registry=reg_ok, extra_files=common_files)),
        ("负例-悬空符号", 1, "DOC-BAD-SYMBOL",
         dict(doc_tokens=("FIXTURE_DANGLING_SYMBOL",), registry=reg_ok,
              extra_files=common_files)),
        ("负例-登记项不再命中(STALE)", 1, "DOC-SYMBOL-REGISTRY-STALE",
         dict(doc_tokens=("FIXTURE_REGISTERED_TOKEN",), registry=reg_stale,
              extra_files=common_files + (("fixture_stale_evidence.py",
                                           "FIXTURE_STALE_TOKEN = 1\n"),))),
        ("负例-evidence 未逐字含 token", 1, "DOC-SYMBOL-REGISTRY-EVIDENCE",
         dict(doc_tokens=("FIXTURE_EV_TOKEN",),
              registry=_registry([{"token": "FIXTURE_EV_TOKEN", "namespace": "fixture",
                                   "evidence": "fixture_evidence.py:1",
                                   "reason": "夹具登记项"}]),
              extra_files=common_files)),
        # ── 行锚判据（FINAL-07 / line-anchor）可证伪面 ──────────────────────
        # 负例①**本次的核心断言**：行号偏 1 行（向前）必须判红。改动前 line_s 从
        # 不被引用 ⇒ 这条必绿 ⇒ 改后必须由 DOC-SYMBOL-REGISTRY-LINEANCHOR 变红。
        ("负例-行锚偏 1 行(向前 :1)", 1, "DOC-SYMBOL-REGISTRY-LINEANCHOR",
         dict(doc_tokens=("FIXTURE_REGISTERED_TOKEN",), registry=_reg_at(1),
              extra_files=common_files)),
        ("负例-行锚偏 1 行(向后 :3)", 1, "DOC-SYMBOL-REGISTRY-LINEANCHOR",
         dict(doc_tokens=("FIXTURE_REGISTERED_TOKEN",), registry=_reg_at(3),
              extra_files=common_files)),
        ("负例-行锚越界(超出文件行数)", 1, "DOC-SYMBOL-REGISTRY-LINEANCHOR",
         dict(doc_tokens=("FIXTURE_REGISTERED_TOKEN",), registry=_reg_at(9999),
              extra_files=common_files)),
        ("负例-行锚越界(:0 不是合法 1 基行号)", 1, "DOC-SYMBOL-REGISTRY-LINEANCHOR",
         dict(doc_tokens=("FIXTURE_REGISTERED_TOKEN",), registry=_reg_at(0),
              extra_files=common_files)),
        # token 被**折行**书写（`FIXTURE_WRAPPED_` / `TOKEN = 1`）⇒ 两行都不逐字
        # 含它，**整份文件**也不逐字含它 ⇒ 命中的是**文件级** EVIDENCE，不是行锚判词。
        # 这条记录的是**判据次序**（见 _load_symbol_namespaces 里的次序说明）：
        # 文件级在前 ⇒ 「token 整个文件里都没有」报 EVIDENCE（词更准），
        # 「token 在文件里但不在该行」报 LINEANCHOR（词也准）。两者都是红。
        ("负例-token 折行(文件级先命中)", 1, "DOC-SYMBOL-REGISTRY-EVIDENCE",
         dict(doc_tokens=("FIXTURE_WRAPPED_TOKEN",),
              registry=_registry([{"token": "FIXTURE_WRAPPED_TOKEN", "namespace": "fixture",
                                   "evidence": "fixture_wrapped.py:1",
                                   "reason": "夹具登记项：token 折行"}]),
              extra_files=common_files + wrapped_files)),
        # 注释掉的承载不承重。
        ("负例-行锚行被注释掉", 1, "DOC-SYMBOL-REGISTRY-LINEANCHOR",
         dict(doc_tokens=("FIXTURE_COMMENTED_TOKEN",),
              registry=_registry([{"token": "FIXTURE_COMMENTED_TOKEN", "namespace": "fixture",
                                   "evidence": "fixture_commented.py:1",
                                   "reason": "夹具登记项：锚在被注释掉的行"}]),
              extra_files=common_files + commented_files)),
        # 防误红（**正控**）：`#define` 是定义不是注释，锚在它上面必须判绿。
        # 没有这一例，一刀切的「行首 # 即注释」判据会静默地把真锚判红。
        ("正例-行锚在 #define 上(预处理指令非注释)", 0, None,
         dict(doc_tokens=("FIXTURE_DEFINED_TOKEN",),
              registry=_registry([{"token": "FIXTURE_DEFINED_TOKEN", "namespace": "fixture",
                                   "evidence": "fixture_defined.h:1",
                                   "reason": "夹具登记项：锚在 #define 上"}]),
              extra_files=common_files + defined_files)),
        # 兼容面（**正控**）：不带行号的纯文件级 evidence **不判红**，只计数留痕。
        ("正例-evidence 不带行号(保持兼容)", 0, None,
         dict(doc_tokens=("FIXTURE_REGISTERED_TOKEN",),
              registry=_registry([{"token": "FIXTURE_REGISTERED_TOKEN", "namespace": "fixture",
                                   "evidence": "fixture_evidence.py",
                                   "reason": "夹具登记项：纯文件级 evidence"}]),
              extra_files=common_files)),
        ("负例-evidence 文件不存在", 1, "DOC-SYMBOL-REGISTRY-EVIDENCE",
         dict(doc_tokens=("FIXTURE_EV_FILE_TOKEN",),
              registry=_registry([{"token": "FIXTURE_EV_FILE_TOKEN", "namespace": "fixture",
                                   "evidence": "fixture_absent.py:1",
                                   "reason": "夹具登记项"}]),
              extra_files=common_files)),
        ("负例-登记表缺失", 1, "DOC-SYMBOL-REGISTRY-MISSING",
         dict(doc_tokens=("FIXTURE_REGISTERED_TOKEN",), skip_registry=True,
              extra_files=common_files)),
        ("负例-权威域目录缺失", 1, "DOC-SYMBOL-SCAN-SURFACE",
         dict(doc_tokens=(), registry=reg_ok, domains=(), extra_files=common_files)),
        ("负例-扫描面 0 份文档", 1, "DOC-SYMBOL-SCAN-SURFACE",
         dict(doc_tokens=(), registry=_registry([]), extra_files=())),
        ("恢复-正例重跑仍绿", 0, None,
         dict(doc_tokens=live_tokens, registry=reg_ok, extra_files=common_files)),
    )
    root = tempfile.mkdtemp(prefix="doc-symbols-selftest-")
    bad = []
    try:
        for i, (name, want_rc, want_id, kw) in enumerate(cases):
            rc, result, out = _fixture_run(root, "case%02d" % i, **kw)
            if result is None:
                bad.append("%s：未产出 JSON（rc=%s）\n%s" % (name, rc, (out or "")[-400:]))
                print("BAD  %s：无 JSON 输出" % name)
                continue
            ids = [f.get("id") for f in result.get("findings", [])]
            ok = rc == want_rc and result.get("status") == ("PASS" if want_rc == 0 else "FAIL")
            if ok and want_id is None:
                ok = not ids          # 正例必须一条 finding 都没有（防误红）
            if ok and want_id is not None:
                ok = want_id in ids   # 负例必须命中**指定**判词（防"红了但不是这条"）
            print("%s %s rc=%s findings=%s" % ("OK  " if ok else "BAD ", name, rc, ids))
            if not ok:
                bad.append("%s：期望 rc=%s finding=%s，实得 rc=%s findings=%s"
                           % (name, want_rc, want_id, rc, ids))
    finally:
        shutil.rmtree(root, ignore_errors=True)
    if bad:
        print("DOC-SYMBOL-SELFTEST_FAIL: %d/%d 例不符预期" % (len(bad), len(cases)))
        for item in bad:
            print("  - " + item)
        return 1
    # 正/负例数**从 cases 导出**，不写死 —— 写死「2 例」在本单加入行锚正控
    # （#define 锚、无行号 evidence）之后就会说出一句与夹具面不符的话。
    _n_pos = sum(1 for c in cases if c[1] == 0)
    print("DOC-SYMBOL-SELFTEST_PASS: %d/%d 例（正例 %d 例须绿且零 finding，负例 %d 例须红且命中指定判词）"
          % (len(cases), len(cases), _n_pos, len(cases) - _n_pos))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    ap.add_argument("--self-test", dest="self_test", action="store_true",
                    help="跑内置正例/负例夹具面（全部符合预期 = 0，任一例不符预期 = 1）")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    repo = pathlib.Path(args.repo)
    findings = []
    status = "PASS"
    doc_stems = _doc_stem_index(repo)
    glossary_toks = _glossary_tokens(repo)
    # 行锚判定账（FINAL-07 / line-anchor）。刻意**与 brace_forms 平级**单列：
    # 「46 条登记项里 0 条带行锚」这种状态必须看得见，否则「判据没跑」和
    # 「登记表本来就没写行号」在输出上完全一样 —— 这正是恒真门的形态。
    _lstat = {"claimed": 0, "pass": 0, "fault": 0, "no_line": 0}
    ns_tokens, ns_ok = _load_symbol_namespaces(repo, findings, _lstat)
    if not ns_ok:
        status = "FAIL"
    seen_tokens = set()
    # Scan docs/**/*.md excluding archive/history
    # Only authoritative docs per 05 L1 classification
    docs = []
    for d in AUTH_DIRS:
        docs.extend([p for p in (repo / "docs" / d).rglob("*.md") if "archive" not in str(p)])
    # 扫描面完整性守卫（fail-closed；静默退化防线）：权威域目录缺失、或一份文档都没扫到
    # ⇒ 本门没有判据对象。此时给 PASS 等于恒真门（真值无效应时度量不归零，见 AGENTS.md
    # §5/§9）：docs/ 一次重构、域清单改个名，全部悬空符号都会**静默**停止受判。
    # 判定面收缩必须显式改 AUTH_DIRS（可见的改口径），不得靠"扫不到"变绿。
    missing_domains = [d for d in AUTH_DIRS if not (repo / "docs" / d).is_dir()]
    if missing_domains or not docs:
        findings.append({"id": "DOC-SYMBOL-SCAN-SURFACE", "severity": "P1",
                         "file": "docs",
                         "symbol": ",".join(missing_domains) or "docs_scanned=0",
                         "observed": ("权威域目录缺失: " + ",".join(missing_domains))
                                     if missing_domains else "扫描面为 0 份文档",
                         # 域个数从 AUTH_DIRS **导出**，不写死 —— 旧实现写死「六个」，
                         # 迁移后 AUTH_DIRS 已是三集，写死会把判词说成与判定面不符的话。
                         "expected": "AUTH_DIRS 的 %d 个权威域目录齐备，且至少扫到 1 份 md"
                                     % len(AUTH_DIRS)})
        status = "FAIL"
    # Also include top-level docs that are authoritative: TRACEABILITY, PUBLIC_API etc handled via contracts
    # Only add if exists (fixtures may not have)
    #
    # FINAL-07（lead-01/auth-dirs）去重：迁移后 science 与 engineering **都在** AUTH_DIRS 内，
    # 这两篇早被上面的 rglob 收进扫描面，再 append 等于**同一份文档被扫两遍** ——
    # 后果是 docs_scanned 虚高、同一 (file,line,symbol) 的 finding **成对重复**。
    # 去重**不缩小判定面**（去重前后是同一集合的不同元素），只消除重复计数。
    # 按解析后真实路径去重，避免同一文件经不同拼写混入两次。
    for extra in [repo / "docs/engineering/PUBLIC_API.md", repo / "docs/science/DATA_SEMANTICS.md"]:
        if extra.exists() and extra.resolve() not in {p.resolve() for p in docs}:
            docs.append(extra)
    # Extract backtick symbols like `p2_integrate_pixel` or `docs/...` or `lib/...`
    backtick_re = re.compile(r'`([^`]+)`')
    # Load known symbols from API inventory
    #
    # FINAL-07（braces-crash）**同批修**：原实现这一面恒为空集，是恒真门。
    # ① docs/architecture/api_inventory.csv 第 1 行是 `#` 注释行（行内自述
    #    「保留作符号解析白名单（check_doc_symbols api_syms 面）」，即**设计意图**
    #    就是喂给本门），而 `csv.DictReader` 直接吃文件 ⇒ 该行被当**表头**；
    # ② 表头因此没有 `symbol` 列 ⇒ `r["symbol"]` 抛 KeyError；
    # ③ 那个**裸 `except: pass`** 把 KeyError 静默吞掉 ⇒ api_syms = set()。
    # 净效果：本门自述的 5 个命名空间里 `api_inventory` 那一面**恒为 0 个符号**
    # （实测 CSV 有 448 条已登记符号，一条都没进解析面）——判据看着在跑，
    # 实际这一面根本没判，与 AGENTS.md §5/§9 的「恒真门」形态同型。
    # 修法（**不放宽**）：跳过前导 `#` 注释行；文件缺失 / 不可解析 / 表头无
    # `symbol` 列 / 过滤后 0 条数据行 —— 一律**显式判红**，绝不退化为空集。
    api_syms = set()
    _INV_REL = "docs/architecture/api_inventory.csv"
    try:
        _raw = [ln for ln in (repo / _INV_REL).read_text(encoding="utf-8").splitlines()
                if ln.strip() and not ln.lstrip().startswith("#")]
        _rdr = csv.DictReader(_raw)
        if not _rdr.fieldnames or "symbol" not in _rdr.fieldnames:
            raise ValueError("表头无 symbol 列（实际表头=%r，注释行是否已过滤？）"
                             % (_rdr.fieldnames,))
        api_syms = {(r.get("symbol") or "").strip() for r in _rdr}
        api_syms.discard("")
        if not api_syms:
            raise ValueError("去掉 # 注释行后 0 条数据行")
    except Exception as exc:      # noqa: BLE001 —— 任何解析失败都必须显式判红
        api_syms = set()
        findings.append({"id": "DOC-SYMBOL-API-INVENTORY", "severity": "P1",
                         "file": _INV_REL, "symbol": _INV_REL,
                         "observed": "API 符号面不可用: %s" % exc,
                         "expected": "带 symbol 列的 CSV，且至少 1 个符号"})
        status = "FAIL"
    # Load known files
    # 原实现对**整仓** rglob("*")（含 build/ run/ .git/ 问题扫描/），
    # 187 份文档 × 全树遍历 ⇒ 单跑 4 分钟，CI 120s timeout 直接判 TIMEOUT。
    # 判定面只需要"文档可能引用的仓库文件"，排除重型产物目录后同判据更快。
    # 注意：`run/` 与 `reports/` **必须保留**在遍历面内 —— 文档常把运行产物写成
    # 任务目录相对路径（如 `spec/x.json` → `run/quality/<task>/spec/x.json`），
    # 下方"按 / 后缀唯一匹配"的兜底解析需要它们。只剪真正体量巨大的目录。
    _skip = (".git", "build", "问题扫描", "__pycache__", "AstroCS.wiki",
             "GaiaDR3", "GaiaDR3SP")
    # 2026-09-21 ROOT-CONSOLIDATION：原根目录 "BASS DR3"（58 MB / 367 文件）迁入
    # testdata/BASS_DR3/，按前缀继续排除以**保持原排除面不增不减**
    # （testdata/ 其余内容仍在遍历面内；HST_M16 仅 3 文件，不排除）。
    _skip_prefix = ("testdata/BASS_DR3/",)
    known_files = set()
    for p in repo.rglob("*"):
        if not p.is_file():
            continue
        rel = p.relative_to(repo).as_posix()
        if rel.split("/", 1)[0] in _skip or rel.startswith(_skip_prefix):
            continue
        known_files.add(rel)
    # 花括号形态的判定账（FINAL-07 / braces-crash；分类定义见 _brace_form）。
    # 全部进 result["brace_forms"] 并打印成独立一行 —— 「静默放过必须留痕」，
    # 否则扫描面一变宽（正是本次崩溃的成因）就会重演恒真门。
    _bstat = {"sets_expanded": 0, "members_expanded": 0, "members_missing": 0,
              "sets_exempted": 0, "template": 0, "regex": 0, "prose": 0,
              "unbalanced": 0, "overflow": 0}
    for doc in docs:
        text = doc.read_text(encoding="utf-8", errors="ignore")
        for m in backtick_re.finditer(text):
            token = m.group(1).strip()
            # Skip obvious non-symbol tokens (plain english, too short, contains spaces)
            if " " in token and "/" not in token: continue
            if token.startswith("http"): continue
            # 多行反引号是代码/正文块, 不是文件路径引用 — 跳过。
            # 不跳过时整段文本进入 os.stat(Errno 36 file name too long)
            # 或误判 DOC-BAD-FILE(Windows CI R8 实测 4 例)。
            if "\n" in token: continue
            # Skip composite file lists like rejection.cpp/integrate.cpp
            if "/" in token and ".cpp" in token:
                parts = token.split("/")
                if any(".cpp" in p for p in parts) and len(parts) == 2 and token.count(".") == 2:
                    continue
            # Skip glob patterns
            if "*" in token:
                continue
            # Check if token looks like file path
            if "/" in token and "." in token:
                # File reference: check exists or is doc-relative
                #
                # ── 花括号形态（FINAL-07 / braces-crash）────────────────────
                # 放在"是否文件引用"这条判定**之内**、扩展名白名单**之前**：
                #   ① 崩溃点（下方四步解析第一步）把 token 原样喂 Path.exists()，
                #      basename 281/406 字节超 NAME_MAX(255) ⇒ OSError 36 ⇒
                #      整门 traceback 退出（那不是判红，是崩）；
                #   ② 原兜底把"含 { }"当通配一律放过（旧的 _glob 分支），是**恒真门**：
                #      并集里写错一个成员，文档照样判绿。
                # 现在：set ⇒ 展开后**逐成员**走 _file_ref_exists（与普通 token 同一套），
                # **任一成员解析不到即判红**；非 set ⇒ 不判红也不判绿，但计数留痕。
                # ⚠ 位置是判据的一部分，不能挪到"是不是文件路径"判定**之外**：
                # 挪出去会把 acr_route∈{auto,cpu}、MAD({fhat})、{−0.274,0.774} 这类
                # **数学集合记法**也当路径并集展开 —— 实测那样会造出 97 个根本不存在
                # 的"成员" ⇒ 97 条假红（判据没坏，是分类面放错了）。
                _bkind, _bmembers = _brace_form(token)
                if _bkind != "none":
                    _bexempt = _exempt_form(token)
                    if _bkind == "set" and not _bexempt:
                        _miss, _undet = [], []
                        for _m in _bmembers:
                            try:
                                if not _file_ref_exists(repo, _m, known_files):
                                    _miss.append(_m)
                            except _Undecidable as _u:
                                _undet.append(str(_u))
                        _bstat["sets_expanded"] += 1
                        _bstat["members_expanded"] += len(_bmembers)
                        _bstat["members_missing"] += len(_miss)
                        if _miss:
                            findings.append({"id": "DOC-BAD-FILE", "severity": "P1",
                                             "file": str(doc.relative_to(repo)),
                                             "line": _line_of(text, m.start()), "symbol": token,
                                             "observed": "花括号并集 %d 个成员中 %d 个不存在: %s"
                                                         % (len(_bmembers), len(_miss),
                                                            ", ".join(_miss)),
                                             "expected": "exists（并集写法要求全部成员在位）"})
                            status = "FAIL"
                        for _u in _undet:
                            findings.append({"id": "DOC-FILE-UNDECIDABLE", "severity": "P1",
                                             "file": str(doc.relative_to(repo)),
                                             "line": _line_of(text, m.start()), "symbol": token,
                                             "observed": "路径判定不可完成: %s" % _u,
                                             "expected": "stat 成功且存在"})
                            status = "FAIL"
                    else:
                        _bstat["sets_exempted" if _bkind == "set" else _bkind] += 1
                    continue
                if token.endswith(".md") or token.endswith(".h") or token.endswith(".cpp") or token.endswith(".json"):
                    # 四步解析抽成 _file_ref_exists（known_files → 仓根相对 →
                    # lib/algorithms/coverage 前缀 → lib 前缀 → known_files 后缀匹配）。
                    # 文档常写模块内相对路径(如 healpix_drizzle/xxx.cpp,
                    # 真实位于 lib/algorithms/drizzle/healpix_drizzle/)，后缀匹配
                    # 兜底消 Windows CI R8 实测误报；rglob 相对路径在 Windows 是
                    # backslash — 统一正斜杠再匹配(R9 34178712916 实证)。
                    # 抽函数的唯一理由：让上面花括号展开出的成员与普通 token 走
                    # **同一套**判据，避免两条路径日后各自漂移。
                    try:
                        found = _file_ref_exists(repo, token, known_files)
                    except _Undecidable as _u:
                        # OS 层判不出来（ENAMETOOLONG 等）：既不静默放过（假绿），
                        # 也不让异常冒到 main 外（整门崩）——如实记判红。
                        findings.append({"id": "DOC-FILE-UNDECIDABLE", "severity": "P1",
                                         "file": str(doc.relative_to(repo)),
                                         "line": _line_of(text, m.start()), "symbol": token,
                                         "observed": "路径判定不可完成: %s" % _u,
                                         "expected": "stat 成功且存在"})
                        status = "FAIL"
                        found = False
                    if not found:
                        # 占位符路径(如 <out_hips>/diagnostics.json)非真实引用
                        if "<" in token or ">" in token:
                            found = True
                    if not found:
                        # 形态**明确不是**"文件必须存在"的判据对象（判据见 _exempt_form）：
                        #   ① 历史引证：token 自身带"已删除/已归档/deleted"标记；
                        #   ② 通配 * ?（**花括号已不在此** —— 它在上一段被展开并逐成员判红，
                        #      保留原豁免就是把并集写法重新变成恒真门）；
                        #   ③ 含空格的命令行（`grep -c gate2 eng/ci/checks.json`）。
                        # 三者都在 token 自身可判定，无需外部豁免表。
                        if _exempt_form(token):
                            found = True
                    if not found:
                        # Allow if is a non-retention namespace doc.
                        # `run/**` 加入非保留命名空间 —— run/ 是 ENGINEERING_SPEC
                        # §7 / AGENTS.md 明定的「运行产物、不入库」空间（gitignore），
                        # 对 run/ 里的产物断言"文件存在"本身是错口径（与 archive/
                        # third_party 同类）。判据只对**在版本库保留面内**的路径生效。
                        _nonret = ("archive", "third_party", "run/")
                        if not any(k in token for k in _nonret):
                            findings.append({"id":"DOC-BAD-FILE","severity":"P1","file":str(doc.relative_to(repo)),"line":_line_of(text, m.start()),"symbol":token,"observed":"file not found","expected":"exists"})
                            status="FAIL"
                continue
            # Skip pure header filenames (contain .h)
            if token.endswith(".h") or token.endswith(".cpp") or token.endswith(".hpp"):
                continue
            # Skip module short names (no dot, not API prefix)
            if token in {"snr_estimator","calibration","phase2","healpix_drizzle","astro_image_io","acr","photometric_calib","plate_solve","dynamic_psf","star_detector","orchestrator","healpix_db","common","gaia_client"}:
                continue
            # Skip C file refs like gaia_client.c
            if token.endswith(".c"):
                continue
            # Skip file:line refs like dpsf_psf.cpp:368
            if ":" in token and (token.endswith(tuple(str(i) for i in range(10))) or token.split(":")[-1].isdigit()):
                continue
            # Skip composite file lists with /
            if "/" in token and token.count("/") == 1 and "." in token and "," not in token:
                # Single file path - already handled as file reference
                pass
            # Composite file list like rejection.cpp/integrate.cpp -> skip
            if "/" in token and ".cpp" in token:
                parts = token.split("/")
                if any(".cpp" in p for p in parts):
                    continue
            # Skip dll/so names
            if token.endswith((".dll",".so",".json")):
                continue
            # Skip composite file lists like a/b/c.h
            if token.count("/") >= 2 and token.count(",") == 0 and "." not in token.split("/")[-1].split(",")[0]:
                # Heuristic: if token looks like path but contains multiple slashes without clear file, skip detailed check
                pass
            # Check if token looks like symbol (contains _ and no spaces)
            # 文档词干 / 术语词典 / 登记命名空间 命中即视为文档引用或科学量,
            # 不进 API-inventory 判据（api_inventory 只登记函数签名）。收窄判据面。
            seen_tokens.add(token)
            if (_stem_resolves(token, doc_stems) or token in glossary_toks
                    or token in ns_tokens):
                continue
            if re.match(r'^[A-Za-z_][\w:]*$', token.replace('.', '')):
                # Heuristic: if token contains _ and is plausible API symbol, check against inventory
                if "_" in token and len(token) >= 3:
                    # Allow known symbols or check if substring matches
                    if token not in api_syms and not any(token in s for s in api_syms) and not any(s in token for s in api_syms):
                        # Check if token is actually a known file stem or config key - skip if not API-like prefix
                        # Skip known status/error codes (not API symbols)
                        if token in {"AC_ERR_PARAM","AC_OK","AC_ERR_MEMORY","AC_ERR_INTERNAL","NO_DATA","NO_CANDIDATES","INVALID_INPUT","INVALID_CONFIGURATION","INVALID_METHOD","AC_ERR","TIMEOUT","CANCELLED","OK","ALL_REJECTED","ZERO_VALID_WEIGHT","UNDERDETERMINED","P2_INTEGRATE_OK","P2_INTEGRATE_NO_CANDIDATES","P2_STATUS_OK","MOFFAT4_FWHM_FACTOR","DPSF_ERR_PARAM","NO_SOLUTION","PC_API","P2_API","AC_API","SNR_API","CC_EXPORT","DPSF_EXPORT","SDET_EXPORT","IPV_API","AIO_EXPORT","HIO_EXPORT","THREAD_BUDGET_EXEMPT","BASELINE_OPCODE_PASS","LD_LIBRARY_PATH","backend_math_contract.h","PLAN_ONLY"}:
                            continue
                        # Only flag UPPER_CASE or known API prefix; lowercase vars like t_light/hp_res are sci params not API symbols
                        #
                        # FINAL-07（lead-01/auth-dirs）补齐：token 带**小写扩展名**时
                        # `token.isupper()` 恒为 False（`FOO_BAR.md` 的 `md` 是小写）⇒
                        # 哪怕词干是纯 UPPER_CASE、且**仓内根本不存在**，该 token 也永不进
                        # is_api_like ⇒ **静默免判**。实测 `GHOST_DOC_NO_SUCH_FILE.md` 判绿
                        # （可证伪性用例④）。判据本意是「UPPER_CASE 形态的符号」⇒ 判形态前
                        # 先剥扩展名。此处只**收严**（增红方向），不放宽任何一格。
                        _shape = token.rsplit(".", 1)[0] if "." in token else token
                        is_api_like = ((token.isupper() or _shape.isupper()) and "_" in _shape
                                       and len(_shape) >= 6) or token.startswith(("p2_","aio_","ac_","cc_","dpsf_","sdet_","ipv_","pc_","snr_","gaia_"))
                        if is_api_like:
                            # 公开头内真实定义的枚举常量/宏是合法公开符号
                            # (api_inventory 只登记函数签名) — 库头全文核实放行
                            # (Windows CI R8 实测误报 AIO_HIPS_RD_IVAR/SNR)。
                            if _defined_in_public_header(repo, token):
                                continue
                            findings.append({"id":"DOC-BAD-SYMBOL","severity":"P1","file":str(doc.relative_to(repo)),"line":_line_of(text, m.start()),"symbol":token,"observed":"symbol not in API inventory","expected":"exists"})
                            status="FAIL"
    # Check archive symbols should not be in active docs (exclude archive docs themselves)
    # 登记项只减不增守卫 — 登记了却在扫描面从未出现的 token 判红
    # (防止把登记表当只增不减的豁免表; 条目随文档改写失效即须同步清除)。
    stale = sorted(t for t in ns_tokens if t not in seen_tokens)
    if stale:
        findings.append({"id": "DOC-SYMBOL-REGISTRY-STALE", "severity": "P1",
                         "file": NAMESPACES_REL, "symbol": ",".join(stale),
                         "observed": "登记项未在扫描面出现 (登记表只减不增)",
                         "expected": "删除失效登记或修正 token"})
    if findings:
        status = "FAIL"
    result = {"tool":"check_doc_symbols","status":status,"docs_scanned":len(docs),
              "namespaces_domains":{"document_stems":len(doc_stems),
                                    "glossary_tokens":len(glossary_toks),
                                    "registered_symbols":len(ns_tokens),
                                    # FINAL-07（braces-crash）：这一格以前恒为 0 且
                                    # **没人看得见**（裸 except 吞掉）。显式报出来，
                                    # 「api_inventory 面恒空」这类恒真门当场可见。
                                    "api_inventory_symbols":len(api_syms)},
              "brace_forms":_bstat,
              "line_anchors":_lstat,
              "findings":findings,"passed": status=="PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    # FINAL-07（前台交办）：判词必须**任何模式下**都上流。原实现有 --out-json 就只写
    # 文件、stdout 一条判词都没有 ⇒ CI 里 CHK-DANGLING / CON-DOC-SYMBOLS 判红却无法
    # 定位（汇总层只能记"检查器未打印可识别判词"）。JSON 面完全不变，这里只补逐条
    # 「文件:行 符号 判定」；PASS 打一行摘要。
    # 花括号形态的判定账（**排在 PASS/FAIL 判词之前**，见下方注意）：
    # sets=展开判过的并集数 members=展开出的成员数 missing=其中解析不到的成员数；
    # not-a-set: template/regex/prose/unbalanced/overflow=识别为「不是并集写法」
    # 而未展开的数量。这些数字若全 0，说明扫描面里一条花括号路径都没有 ——
    # 该状态必须看得见，才能区分「确实没有」与「解析器坏了静默放过」。
    #
    # ⚠ 本行**必须是 stdout 末行之前的最后一条非判词行** —— 判词留在末行：
    # eng/tools/quality/contracts/generate_contract_report.py:47 取
    # out.stdout.strip().split(chr(10))[-1] 当 JSON 解析。把末行换成这一行
    # 同样会解析失败落到 except 兜底，但那是撞运气；保持判词在末行才是稳的。
    # 行锚判定账（FINAL-07 / line-anchor）。同样**必须排在 PASS/FAIL 判词之前**，
    # 且**排在 BRACE 行之前** —— BRACE 行是「stdout 末行之前的最后一条非判词行」
    # （见下方 ⚠ 与 generate_contract_report.py:47），本行插在它前面不破坏该契约。
    # claimed=登记项声称带行锚的条数 pass=其中行逐字含 token 的成员数
    # fault=判红的成员数 no_line=没有行锚（不判红、仅留痕）的条数。
    # claimed 与 pass 同时为 0 ⇒ 登记表一条行锚都没有，本门这一面**没在判**
    # （恒真门形态），必须一眼看得见。
    print("CON-DOC-SYMBOL-LINEANCHOR: claimed=%d pass=%d fault=%d no-line=%d"
          % (_lstat["claimed"], _lstat["pass"], _lstat["fault"], _lstat["no_line"]))
    print("CON-DOC-SYMBOLS_BRACE: sets=%d members=%d missing=%d | not-a-set: template=%d regex=%d prose=%d unbalanced=%d overflow=%d exempt=%d"
          % (_bstat["sets_expanded"], _bstat["members_expanded"], _bstat["members_missing"],
             _bstat["template"], _bstat["regex"], _bstat["prose"],
             _bstat["unbalanced"], _bstat["overflow"], _bstat["sets_exempted"]))
    if status == "FAIL":
        print("CON-DOC-SYMBOLS_FAIL: %d finding(s) / %d doc(s) scanned"
              % (len(findings), len(docs)))
        for f in findings:
            print("  - %s:%s [%s/%s] symbol=%s observed=%s expected=%s"
                  % (f.get("file", "?"), f.get("line", 1), f.get("id", "?"),
                     f.get("severity", "?"), f.get("symbol", "?"),
                     f.get("observed", ""), f.get("expected", "")))
    else:
        print("CON-DOC-SYMBOLS_PASS: docs_scanned=%d namespaces=%s"
              % (len(docs), result["namespaces_domains"]))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_doc_symbols" tests="{len(docs)}" failures="{failures}"><testcase classname="docs" name="symbols"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
