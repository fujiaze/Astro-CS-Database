"""模块层 C++ 源码静态扫描器（纯标准库，不编译、不链接、不起子进程）。

用途
----
为 `eng/tests/module/test_module_state_hygiene.py` 提供**只读源码文本**的抽取能力，
把「AGENTS.md 第 7 节代码纪律」与「PIPELINE_BLOCK.md 命名块合同」这两组条款变成可执行的
静态判据。扫描器本身**不做裁决**：它只把扫描面读成结构化记录（静态可变对象、环境变量读取、
进程退出调用、线程派生点、provenance 键、降级记录键），裁决逻辑（登记面比对、逐条理由）
全部留在测试文件里。

这样切分的理由：`docs/engineering/testing/VALIDATION_EVIDENCE.md:193-196`（判别力核查 S6）
要求「注入点必须是该判定自身的判定逻辑，只换它，不换共用逻辑」。共用逻辑放在本模块，
判定逻辑放在测试文件，注入负例时只改扫描面（临时副本）与登记面，二者都属被核查判定自身。

已知局限（自报义务，对应 `VALIDATION_EVIDENCE.md:232-240` 第 10 节）
----------------------------------------------------------------
1. **无编译器前端**：本扫描器是词法 + 括号配平 + 正则的启发式扫描器，不构造 AST。
   `template<>` 偏特化的显式实例化定义、`extern "C" { ... }` 块内声明、
   宏展开后出现的静态对象都可能漏检（漏报面）。
2. **声明/定义边界不区分**：同一对象在头文件声明、在 `.cpp` 定义时按两个文件各记一次；
   本扫描器按「文件内出现即一条」计数，登记面必须与之逐条对齐。
3. **字符串字面量与注释先抹白**：所有注释与字符串常量被替换成等长空白（保留换行），
   因此注释里出现的 `std::vector<std::thread> pool;` 不会被误判为对象声明——
   这一条是本仓的**必要**步骤：`lib/infrastructure/scheduler/src/module_adapters.cpp:2091`
   的注释里就写着 `std::thread worker`。

已排除的采集面（同样属自报义务）
------------------------------
见 `EXCLUDED_SUBDIRS` 与 `EXCLUDED_PREFIXES` 两个常量，并在测试文件顶部重复声明。
"""

from __future__ import annotations

import os
import re
from dataclasses import dataclass, field
from typing import Iterable, Iterator, List, Sequence, Tuple

# ─────────────────────────────────────────────────────────────────────────────
# 采集面定义
# ─────────────────────────────────────────────────────────────────────────────

#: 模块算法树内**排除**的子目录（逐个写明理由，见测试文件顶部的「已排除的采集面」表）。
EXCLUDED_SUBDIRS: dict[str, str] = {
    "tools": "独立命令行小工具（自带 --cpu-workers 等参数面），不在 module_adapters.cpp 的 20 个 op 入口链上",
    "third_party": "vendored 第三方源码，按 CODE.md「第三方头的窄隔离」条款不纳入本项目纪律判据",
    "__pycache__": "非 C/C++ 源",
}

#: 采集面之外的仓库路径前缀。
EXCLUDED_PREFIXES: Tuple[str, ...] = (
    "build/",
    "out/",
    "run/",
    "实验/",
    "artifacts/",
    "testdata/",
    ".git/",
    "eng/",
    "docs/",
)

#: 按**文件名**排除的 vendored 第三方源。依据 `docs/engineering/standards/CODE.md:104-106`
#: 「vendored 第三方头（如 `nanoflann.hpp`）按其自身触发的诊断类别，在引入点做窄隔离」
#: ——它是第三方代码，不受本项目代码纪律判据约束。
EXCLUDED_FILES: dict[str, str] = {
    "nanoflann.hpp": "vendored 第三方头，按 CODE.md「第三方头的窄隔离」条款不纳入本项目纪律判据",
}

#: 模块算法树的源码根。
ALGORITHMS_ROOT = "lib/algorithms"

#: 模块适配层（20 个 op 入口都在这里，见任务书与 PIPELINE_BLOCK.md:86-87）。
ADAPTER_SOURCE = "lib/infrastructure/scheduler/src/module_adapters.cpp"

#: 模块算法树内纳入判据的源码后缀。
SOURCE_SUFFIXES: Tuple[str, ...] = (".cpp", ".c", ".h", ".hpp")

# ─────────────────────────────────────────────────────────────────────────────
# 词法层：注释与字符串抹白
# ─────────────────────────────────────────────────────────────────────────────


def blank_comments_and_strings(src: str) -> str:
    """把注释与字符串字面量替换成等长空白，保留换行，使行号与列宽不变。

    保留换行是硬要求：判据要给 `文件:行` 证据，行号必须仍指向原文对应行。
    """
    out: List[str] = []
    i = 0
    n = len(src)
    state = "code"
    while i < n:
        c = src[i]
        if state == "code":
            if c == "/" and i + 1 < n and src[i + 1] == "/":
                state = "line_comment"
                out.append("  ")
                i += 2
                continue
            if c == "/" and i + 1 < n and src[i + 1] == "*":
                state = "block_comment"
                out.append("  ")
                i += 2
                continue
            if c == '"':
                state = "string"
                out.append(" ")
                i += 1
                continue
            if c == "'":
                state = "char"
                out.append(" ")
                i += 1
                continue
            out.append(c)
            i += 1
            continue
        if state == "line_comment":
            if c == "\n":
                state = "code"
                out.append("\n")
            else:
                out.append(" ")
            i += 1
            continue
        if state == "block_comment":
            if c == "*" and i + 1 < n and src[i + 1] == "/":
                state = "code"
                out.append("  ")
                i += 2
                continue
            out.append("\n" if c == "\n" else " ")
            i += 1
            continue
        if state == "string":
            if c == "\\" and i + 1 < n:
                out.append("  ")
                i += 2
                continue
            if c == '"':
                state = "code"
            out.append("\n" if c == "\n" else " ")
            i += 1
            continue
        # state == "char"
        if c == "\\" and i + 1 < n:
            out.append("  ")
            i += 2
            continue
        if c == "'":
            state = "code"
        out.append(" " if c != "\n" else "\n")
        i += 1
    return "".join(out)


def blank_comments_only(src: str) -> str:
    """只抹注释，**保留**字符串字面量；保留行宽与换行。

    与 `blank_comments_and_strings` 配对使用：
    - 需要「这段是不是代码」时用全抹白版（字符串里的 `abort(` 也会造成假阳）；
    - 需要「这个字符串字面量的内容」时用本版（全抹白会把 `"k_photo"` 一并抹掉）。
    """
    out: List[str] = []
    i = 0
    n = len(src)
    state = "code"
    while i < n:
        c = src[i]
        if state == "code":
            if c == "/" and i + 1 < n and src[i + 1] == "/":
                state = "line_comment"
                out.append("  ")
                i += 2
                continue
            if c == "/" and i + 1 < n and src[i + 1] == "*":
                state = "block_comment"
                out.append("  ")
                i += 2
                continue
            out.append(c)
            i += 1
            continue
        if state == "line_comment":
            if c == "\n":
                state = "code"
                out.append("\n")
            else:
                out.append(" ")
            i += 1
            continue
        # block_comment
        if c == "*" and i + 1 < n and src[i + 1] == "/":
            state = "code"
            out.append("  ")
            i += 2
            continue
        out.append("\n" if c == "\n" else " ")
        i += 1
    return "".join(out)


def line_of(src: str, index: int) -> int:
    """给 0 基字符下标，返回 1 基行号。"""
    return src.count("\n", 0, index) + 1


def iter_code_lines(raw: str) -> Iterator[Tuple[int, str, str]]:
    """逐行产出 `(行号, 抹注释行, 全抹白行)`。

    - 抹注释行：注释已抹、字符串保留 ⇒ 用于取字符串字面量里的内容（变量名、JSON 键名）。
    - 全抹白行：注释与字符串都抹 ⇒ 用于判定「这是不是代码」以及扫描裸 token。

    只用其中一种都会出错（本模块前两版各踩一次）：只用全抹白版时
    `std::getenv("ACSD_X")` 与 `"k_photo"` 的读数全为 0；只用抹注释版时
    日志字符串里的 `abort(` 会被当成进程退出调用。
    """
    co = blank_comments_only(raw).split("\n")
    fb = blank_comments_and_strings(raw).split("\n")
    n = max(len(co), len(fb))
    for i in range(n):
        yield (
            i + 1,
            co[i] if i < len(co) else "",
            fb[i] if i < len(fb) else "",
        )


# ─────────────────────────────────────────────────────────────────────────────
# 采集面定位
# ─────────────────────────────────────────────────────────────────────────────


class ScanSurfaceError(RuntimeError):
    """采集面无法定位。调用方必须 fail-closed，不得按空集处理。"""


#: 判定仓库根所需的存在锚。任一缺失即判 `ANCHOR_STALE`。
REPO_ROOT_ANCHORS: Tuple[str, ...] = ("AGENTS.md", "VERSION", ADAPTER_SOURCE)


def repo_root(start: str | None = None) -> str:
    """向上定位仓库根。

    fail-closed：三个锚文件缺任一个都抛 `ScanSurfaceError`，调用方转 `pytest.fail`，
    绝不静默回退到当前目录（那会把「采集面为空」伪装成「一切合规」）。
    """
    cur = os.path.abspath(start or os.getcwd())
    while True:
        if all(os.path.isfile(os.path.join(cur, a)) for a in REPO_ROOT_ANCHORS):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            raise ScanSurfaceError(
                "ANCHOR_STALE: 仓库根锚缺失 " + " ".join(REPO_ROOT_ANCHORS)
            )
        cur = parent


def is_excluded(rel_path: str) -> str | None:
    """返回排除理由；未被排除则返回 None。"""
    norm = rel_path.replace(os.sep, "/")
    for prefix in EXCLUDED_PREFIXES:
        if norm.startswith(prefix):
            return f"EXCLUDED_PREFIX:{prefix}"
    base = norm.rsplit("/", 1)[-1]
    if base in EXCLUDED_FILES:
        return f"EXCLUDED_FILE:{base}"
    parts = norm.split("/")
    if ALGORITHMS_ROOT in parts:
        tail = parts[parts.index(ALGORITHMS_ROOT) + 1 :]
        for seg in tail[:-1]:
            if seg in EXCLUDED_SUBDIRS:
                return f"EXCLUDED_SUBDIR:{seg}"
    return None


def iter_scan_surface(root: str) -> Iterator[str]:
    """产出采集面内的仓库相对 POSIX 路径（有序、可复算）。

    采集面 = `lib/algorithms/**` 下后缀属 `SOURCE_SUFFIXES` 的文件 + 模块适配层单文件。
    排除面见 `EXCLUDED_SUBDIRS` / `EXCLUDED_FILES` / `EXCLUDED_PREFIXES`。
    """
    algo_root = os.path.join(root, ALGORITHMS_ROOT)
    if not os.path.isdir(algo_root):
        raise ScanSurfaceError(f"ANCHOR_STALE: 模块算法树缺失 {ALGORITHMS_ROOT}")
    found: List[str] = []
    for dirpath, dirnames, filenames in os.walk(algo_root):
        rel_dir = os.path.relpath(dirpath, root).replace(os.sep, "/")
        dirnames[:] = sorted(d for d in dirnames if d not in EXCLUDED_SUBDIRS)
        for fn in sorted(filenames):
            if os.path.splitext(fn)[1] not in SOURCE_SUFFIXES:
                continue
            rel = f"{rel_dir}/{fn}"
            if is_excluded(rel) is not None:
                continue
            found.append(rel)
    found.append(ADAPTER_SOURCE)
    return iter(sorted(found))


def read_source(root: str, rel_path: str) -> str:
    """读一个扫描面文件。读不出即抛 `ScanSurfaceError`（fail-closed）。"""
    abs_path = os.path.join(root, rel_path.replace("/", os.sep))
    if not os.path.isfile(abs_path):
        raise ScanSurfaceError(f"ANCHOR_STALE: 扫描面文件不存在 {rel_path}")
    with open(abs_path, "r", encoding="utf-8", errors="replace") as fh:
        return fh.read()


# ─────────────────────────────────────────────────────────────────────────────
# 声明级扫描：命名空间作用域 + static 存储期
# ─────────────────────────────────────────────────────────────────────────────

#: 打开**真实**块（函数体 / 类体 / 命名空间体 / extern "C" 块）的关键字。
_BLOCK_KEYWORDS = ("namespace", "struct", "class", "union", "enum", "extern")

#: 存储期 / 线程模型限定词。出现任一个即判为「static 存储期」。
_STATIC_KEYWORDS = ("static", "thread_local")

#: 命名空间作用域下算「可变容器」的模板类型。
_CONTAINER_TYPES = (
    "std::vector",
    "std::array",
    "std::deque",
    "std::list",
    "std::map",
    "std::multimap",
    "std::unordered_map",
    "std::set",
    "std::unordered_set",
    "std::string",
    "std::unique_ptr",
    "std::shared_ptr",
    "std::weak_ptr",
    "std::mutex",
    "std::recursive_mutex",
    "std::atomic",
    "std::condition_variable",
)

_CONTAINER_RE = re.compile(r"\b(" + "|".join(re.escape(t) for t in _CONTAINER_TYPES) + r")\s*<")
_CONTAINER_PLAIN_RE = re.compile(r"\b(" + "|".join(re.escape(t) for t in _CONTAINER_TYPES) + r")\b")
_STATIC_RE = re.compile(r"\b(static|thread_local)\b")
_IDENT_TAIL_RE = re.compile(r"[A-Za-z_0-9>\]]$")
_TYPEDEF_RE = re.compile(r"\btypedef\b")
_USING_RE = re.compile(r"^\s*using\b")
_TEMPLATE_INST_RE = re.compile(r"\btemplate\b\s*<[^;]*?>\s*(bool|void)\b")
_FUNC_RE = re.compile(r"^\s*(?:[A-Za-z_][\w:<>,\s*&]*\s+)?[A-Za-z_]\w*\s*\(")


@dataclass(frozen=True)
class StaticObject:
    """一条命名空间作用域或 static 存储期的可变对象记录。"""

    path: str
    line: int
    name: str
    kind: str  # "namespace-scope" | "static-scope" | "thread-local"
    type_text: str
    decl: str

    @property
    def locator(self) -> str:
        return f"{self.path}:{self.line}"

    def key(self) -> Tuple[str, str]:
        return (self.path, self.name)


def _statement_kind(buf: str) -> str:
    """判断缓冲区末尾的 `{` 是「初始化列表」还是「真实块」。"""
    tail = buf.rstrip()
    if not tail:
        return "block"
    if any(re.search(r"\b" + kw + r"\b", buf) for kw in _BLOCK_KEYWORDS):
        return "block"
    if tail.endswith(")") or tail.endswith(";"):
        return "block"
    if _IDENT_TAIL_RE.search(tail):
        return "init"
    return "init"


def strip_template_args(text: str) -> str:
    """把配平的 `<...>` 模板实参段抹成空格，保留行宽与换行。

    抹掉之后 `std::atomic<bool> g_enabled{false};` 变成 `std::atomic  g_enabled{false};`，
    声明符（真正被声明的那个标识符）落在类型名之后，不再被实参里的标识符干扰。

    已知局限（自报义务）：`a < b` 形式的比较表达式若出现在命名空间作用域的声明里会被误抹。
    本仓采集面内未出现该形态。
    """
    out = list(text)
    i = 0
    n = len(text)
    while i < n:
        if text[i] == "<":
            depth = 0
            j = i
            while j < n:
                if text[j] == "<":
                    depth += 1
                elif text[j] == ">":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            if j >= n:
                break  # 不配平：放弃，不抹
            for k in range(i, j + 1):
                if out[k] != "\n":
                    out[k] = " "
            i = j + 1
            continue
        i += 1
    return "".join(out)


def _declarator_name(decl: str) -> str:
    """从声明文本里取出被声明的标识符。

    步骤：抹掉模板实参 → 按 `=` / `{` / `;` 取声明符头 → 先试数组 `name[...]`，
    再取头部分词序列里的最后一个标识符。
    """
    head = re.split(r"[={;]", decl, maxsplit=1)[0]
    m = re.search(r"([A-Za-z_]\w*)\s*\[", head)
    if m:
        return m.group(1)
    head = re.sub(r"\b(const|volatile)\b", " ", head)
    for ch in "*&":
        head = head.replace(ch, " ")
    head = re.sub(r"[()]", " ", head)
    idents = re.findall(r"[A-Za-z_]\w*", head)
    if not idents:
        return "<匿名>"
    return idents[-1]


def _make_record(path: str, raw: str, buf: str, buf_line: int, kind: str) -> StaticObject | None:
    decl = " ".join(buf.split())
    if _TYPEDEF_RE.search(decl) or _USING_RE.match(decl):
        return None  # 类型别名不是对象
    if decl.startswith("template"):
        # 函数模板声明 / 显式实例化定义都不是对象声明（实测命中 18 处，如
        # lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp:1837）。
        return None
    if _FUNC_RE.match(decl):
        return None  # 函数声明/定义不是对象
    cm = _CONTAINER_RE.search(decl)
    if cm:
        type_text = cm.group(1) + "<…>"
    else:
        pm = _CONTAINER_PLAIN_RE.search(decl)
        if not pm:
            return None
        type_text = pm.group(1)
    if re.search(r"[A-Za-z0-9_]\s*::\s*[A-Za-z_]\w*\s*::", decl):
        return None  # 带命名空间限定的成员定义，不在本文件的作用域面内
    return StaticObject(
        path=path,
        line=buf_line,
        name=_declarator_name(strip_template_args(decl)),
        kind=kind,
        type_text=type_text,
        decl=decl[:200],
    )


def find_static_objects(path: str, raw: str) -> List[StaticObject]:
    """抽出 `path` 内命名空间作用域与 static 存储期的可变容器对象。"""
    txt = blank_comments_and_strings(raw)
    depth = 0
    ns_levels = {0}
    ns_stack: List[int] = []
    buf = ""
    buf_line = 1
    brace_in_stmt = 0
    skip_depth = 0
    out: List[StaticObject] = []
    i = 0
    n = len(txt)
    at_line_start = True
    while i < n:
        c = txt[i]
        if skip_depth:
            if c == "{":
                skip_depth += 1
            elif c == "}":
                skip_depth -= 1
            i += 1
            continue
        if brace_in_stmt:
            if c == "{":
                brace_in_stmt += 1
            elif c == "}":
                brace_in_stmt -= 1
            i += 1
            continue
        # 预处理指令行自带边界：行首 `#` 之后到行尾的缓冲一律丢弃。
        # 否则 `#include <mutex>` 这类不以 `;` 结尾的行会把后续真实声明并进同一条语句
        # （实测假阳：lib/algorithms/psf/src/dpsf_log.cpp:1）。
        if at_line_start and c in " \t":
            i += 1
            continue
        if at_line_start and c == "#":
            j = txt.find("\n", i)
            if j < 0:
                break
            buf = ""
            i = j + 1
            at_line_start = True
            continue
        if c == "\n":
            # 纯空白缓冲（上一行残留的行尾注释被抹白后的尾随空格）在换行处清空，
            # 否则下一条真实声明会沿用上一行的起始行号，产生 off-by-one 的 `文件:行` 证据。
            # 含非空白的缓冲（跨行声明，如 module_adapters.cpp:308-310）必须保留。
            if not buf.strip():
                buf = ""
            at_line_start = True
            i += 1
            continue
        at_line_start = False
        if c == "{":
            kind = _statement_kind(buf)
            if kind == "block":
                if re.search(r"\bnamespace\b", buf) and not re.search(r"\b(static|thread_local)\b", buf):
                    ns_levels.add(depth + 1)
                    ns_stack.append(depth + 1)
                elif depth in ns_levels and _is_type_block(buf):
                    skip_depth = 1  # 类 / 结构 / 枚举体：成员不在本判据面内
                    depth += 1
                    i += 1
                    buf = ""
                    continue
                depth += 1
                buf = ""
                i += 1
                continue
            brace_in_stmt = 1
            buf += c
            i += 1
            continue
        if c == "}":
            if ns_stack and depth == ns_stack[-1]:
                ns_levels.discard(depth)
                ns_stack.pop()
            depth -= 1
            buf = ""
            i += 1
            continue
        if not buf:
            buf_line = line_of(raw, i)
        buf += c
        if c == ";":
            rec = None
            if depth in ns_levels:
                rec = _make_record(path, raw, buf, buf_line, "namespace-scope")
            elif _STATIC_RE.search(buf):
                kind = "thread-local" if "thread_local" in buf else "static-scope"
                rec = _make_record(path, raw, buf, buf_line, kind)
            if rec is not None:
                out.append(rec)
            buf = ""
        i += 1
    return out


def _is_type_block(buf: str) -> bool:
    return bool(re.search(r"\b(struct|class|union|enum)\b", buf))


# ─────────────────────────────────────────────────────────────────────────────
# 像素维度 / 像素域词表
# ─────────────────────────────────────────────────────────────────────────────

#: 像素维度来源词（AGENTS §7「不私藏大块数据长期副本」的机器化抓手）。
#: `w` / `h` 两项按 `.w` / `.h` 成员访问形态匹配，覆盖本仓 `img.w * img.h` 的主 idiom。
PIXEL_DIM_TOKENS: Tuple[str, ...] = (
    "width",
    "height",
    "n_pix",
    "npix",
    "n_pixels",
    "npixels",
    "num_pixels",
    "naxis0",
    "naxis1",
    "img_w",
    "img_h",
    "pix_w",
    "pix_h",
    "n_rows",
    "n_cols",
    "nrow",
    "ncol",
    "plane_size",
    "total_px",
    "np_total",
    "nx",
    "ny",
    "nw_px",
    "nh_px",
)

#: 以成员访问形态出现的短维度名（`img.w`、`img.h`）。
PIXEL_DIM_MEMBER_TOKENS: Tuple[str, ...] = ("w", "h")

#: 像素域类型词。
PIXEL_DOMAIN_TYPES: Tuple[str, ...] = (
    "AioImage",
    "AioImageView",
    "AioPixel",
    "PixelBuffer",
    "PixelPlane",
    "ImageView",
    "SkyImage",
    "VariancePlane",
    "SignalPlane",
)

_PIXEL_DIM_RE = re.compile(
    r"(?<![A-Za-z0-9_])(" + "|".join(PIXEL_DIM_TOKENS) + r")(?![A-Za-z0-9_])"
)
_PIXEL_DIM_MEMBER_RE = re.compile(
    r"\.\s*(" + "|".join(PIXEL_DIM_MEMBER_TOKENS) + r")\b(?![A-Za-z0-9_])"
)
_PIXEL_DOMAIN_RE = re.compile(
    r"(?<![A-Za-z0-9_])(" + "|".join(PIXEL_DOMAIN_TYPES) + r")(?![A-Za-z0-9_])"
)

#: 对某静态对象的写操作形态。
_MUTATION_RE_TMPL = r"(?:\b{name}\s*=(?!=)|\b{name}\s*\.\s*(?:assign|resize|reserve|push_back|emplace_back|insert|clear|swap)\b)"


def pixel_dimension_hits(text: str) -> List[str]:
    """返回文本中命中的像素维度词（含 `.w` / `.h` 成员访问形态）。"""
    hits = {m.group(1) for m in _PIXEL_DIM_RE.finditer(text)}
    hits |= {"." + m.group(1) for m in _PIXEL_DIM_MEMBER_RE.finditer(text)}
    return sorted(hits)


def pixel_domain_hits(text: str) -> List[str]:
    """返回文本中命中的像素域类型词。"""
    return sorted({m.group(1) for m in _PIXEL_DOMAIN_RE.finditer(text)})


def pixel_sized_initialiser(decl: str) -> List[str]:
    """静态对象的声明/初始化子句是否以像素维度为尺寸来源。"""
    tail = decl
    for kw in ("=", "{", "("):
        idx = tail.find(kw)
        if idx >= 0:
            tail = tail[idx:]
            break
    return pixel_dimension_hits(tail)


def pixel_domain_mutations(raw: str, obj: StaticObject) -> List[Tuple[int, List[str], List[str]]]:
    """找出 `obj` 在本文件内被写入、且写入语句含像素维度或像素域类型的行。

    返回 `(行号, 像素维度词, 像素域类型词)` 列表。逐行判定，够用且不误伤。

    遮蔽消解（必须做，否则本仓实测假阳）：文件作用域静态对象与函数局部变量**同名**是
    常态。`lib/algorithms/psf/src/dpsf_psf.cpp:95` 声明 `static std::vector<DpsfDiagRec>* v`，
    而 `:506` 的 `double v = static_cast<double>(image[y * width + x]);` 是一个**局部**标量。
    若按名字匹配，后者会被算成对文件作用域静态对象的像素域写入。故凡本行同时把该名字
    **重新声明**为局部对象（`类型 名 = / ; / {` 形态）时，本行不计。
    """
    txt = blank_comments_and_strings(raw)
    mut = re.compile(_MUTATION_RE_TMPL.format(name=re.escape(obj.name)))
    shadow = re.compile(
        r"(?<![.\w])(?:[A-Za-z_][\w:]*(?:\s*<[^<>;()]*>)?\s*[*&]?\s+)"
        + re.escape(obj.name)
        + r"\s*(?:=|\{|;|,)"
    )
    hits: List[Tuple[int, List[str], List[str]]] = []
    for offset, line in enumerate(txt.split("\n")):
        if not mut.search(line):
            continue
        if shadow.search(line):
            continue  # 本行是同名局部对象的声明，不是对静态对象的写入
        dims = pixel_dimension_hits(line)
        doms = pixel_domain_hits(line)
        if dims or doms:
            hits.append((offset + 1, dims, doms))
    return hits


# ─────────────────────────────────────────────────────────────────────────────
# 环境变量读取
# ─────────────────────────────────────────────────────────────────────────────

_GETENV_RE = re.compile(r"(?:std::)?getenv\s*\(\s*\"([A-Za-z_][A-Za-z0-9_]*)\"")


@dataclass(frozen=True)
class EnvRead:
    """一条 `getenv("NAME")` 字面量读取。"""

    path: str
    line: int
    var: str
    decl: str

    @property
    def locator(self) -> str:
        return f"{self.path}:{self.line}"


@dataclass(frozen=True)
class DynamicEnvRead:
    """一条经变量间接取名的 `getenv(name)` 读取（旋钮函数）。"""

    path: str
    line: int
    callee: str
    known_args: Tuple[str, ...]

    @property
    def locator(self) -> str:
        return f"{self.path}:{self.line}"


@dataclass
class EnvScan:
    literals: List[EnvRead] = field(default_factory=list)
    dynamic: List[DynamicEnvRead] = field(default_factory=list)


_ENV_CALL_RE = re.compile(r"getenv\s*\(\s*([A-Za-z_]\w*)\s*\)")


def find_env_reads(path: str, raw: str) -> EnvScan:
    """抽出本文件的环境变量读取。

    - 字面量形 `std::getenv("ACSD_X")` 记入 `literals`；
    - 间接形 `std::getenv(name)` 记入 `dynamic`，并把该旋钮函数的调用点实参解析出来。

    实参取自**原文行**（变量名在字符串字面量里，抹白行里没有），
    但「本行是不是真代码」用**抹白行**判定（注释里的 getenv 不算）。
    """
    scan = EnvScan()
    for lineno, co_line, fb_line in iter_code_lines(raw):
        if "getenv" not in co_line:
            continue
        for m in _GETENV_RE.finditer(co_line):
            scan.literals.append(
                EnvRead(path=path, line=lineno, var=m.group(1), decl=" ".join(fb_line.split())[:160])
            )
        if _GETENV_RE.search(co_line):
            continue
        m = _ENV_CALL_RE.search(co_line)
        if m:
            callee = m.group(1)
            owner = _enclosing_function(raw, lineno) or "<unknown>"
            scan.dynamic.append(
                DynamicEnvRead(
                    path=path,
                    line=lineno,
                    callee=f"{owner}({callee})",
                    known_args=tuple(_resolve_call_args(raw, owner)),
                )
            )
    return scan


_FUNC_HEAD_RE = re.compile(
    r"^[\w:<>,\s*&]*\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?\{?\s*$"
)


def _enclosing_function(raw: str, lineno: int) -> str | None:
    """给出行号，返回其所在函数的名字（向上找最近一个形如 `name(...) {` 的行）。"""
    co_lines = blank_comments_only(raw).split("\n")
    for i in range(min(lineno, len(co_lines)) - 2, -1, -1):
        m = _FUNC_HEAD_RE.match(co_lines[i])
        if m:
            return m.group(1)
    return None


def _resolve_call_args(raw: str, callee: str) -> List[str]:
    """解析 `callee("ENV_NAME")` 形式的调用实参（旋钮函数的真实取值集合）。"""
    out: List[str] = []
    pat = re.compile(r"\b" + re.escape(callee) + r"\s*\(\s*\"([A-Za-z_][A-Za-z0-9_]*)\"")
    for _lineno, co_line, _fb_line in iter_code_lines(raw):
        if callee not in co_line:
            continue
        for m in pat.finditer(co_line):
            out.append(m.group(1))
    return sorted(set(out))


# ─────────────────────────────────────────────────────────────────────────────
# 进程退出 / 终止调用
# ─────────────────────────────────────────────────────────────────────────────

#: 视为「直接退出进程」的调用名。
EXIT_CALLS: Tuple[str, ...] = ("exit", "_exit", "_Exit", "abort", "quick_exit", "std::terminate")

_EXIT_CALL_RE = re.compile(r"(?<![\w.>:])(?:std::)?(" + "|".join(EXIT_CALLS) + r")\s*\(")


@dataclass(frozen=True)
class ExitCall:
    path: str
    line: int
    name: str
    decl: str
    reason: str | None = None  # 非 None 时表示已判定为「成员函数同名，非 libc 调用」

    @property
    def locator(self) -> str:
        return f"{self.path}:{self.line}"


_MEMBER_OWN_RE = re.compile(r"\b(?:void|int|bool|auto)\s+(\w+)::(\w+)\s*\(")

#: 成员函数**声明 / 定义**形态（不是调用）。本仓实测命中两处假阳：
#: `lib/algorithms/fits_output/p3_output.h:149` 的 `void abort();`
#: 与 `lib/infrastructure/scheduler/src/module_adapters.cpp:15359` 的 `void abort() override {}`。
_CALL_SHAPE_RE = re.compile(
    r"^\s*[A-Za-z_][\w:<>,\s*&]*\b(?:" + "|".join(EXIT_CALLS) + r")\s*"
    r"(?:\([^()]*\))?\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:=\s*0\s*)?[;{]"
)


def find_exit_calls(path: str, raw: str) -> List[ExitCall]:
    """抽出「直接退出进程」候选调用。

    假阳形态一（本仓实测命中 13 处）：成员函数与 libc 同名。
    `lib/algorithms/fits_output/p3_output.cpp:1061` 定义 `void P3FitsStream::abort()`，
    其余 12 处 `abort()` 都是该成员函数的**成员内无限定调用**。
    消解规则：若本文件存在 `X::abort()` / `X::exit()` 形式的成员定义，则该文件内
    同名无限定调用按成员调用消解（`reason` 记录消解依据），不算进程退出。
    `std::terminate` 不做此消解——它没有合法的成员同名形态。

    假阳形态二：成员函数**声明与定义行**本身，形如 `void abort();`。
    消解规则：行首存在返回类型词（即声明/定义形态）时不算调用。
    """
    txt = blank_comments_and_strings(raw)
    member_defined = {m.group(2) for m in _MEMBER_OWN_RE.finditer(txt)}
    out: List[ExitCall] = []
    for offset, line in enumerate(txt.split("\n")):
        if _CALL_SHAPE_RE.match(line):
            continue  # 声明 / 定义行，不是调用
        for m in _EXIT_CALL_RE.finditer(line):
            name = m.group(1)
            if name == "abort" and "abort" in member_defined:
                out.append(
                    ExitCall(
                        path=path,
                        line=offset + 1,
                        name=name,
                        decl=" ".join(line.split())[:160],
                        reason="MEMBER_SHADOWED: 本文件存在同名成员定义，非 libc abort",
                    )
                )
                continue
            out.append(
                ExitCall(path=path, line=offset + 1, name=name, decl=" ".join(line.split())[:160])
            )
    return out


# ─────────────────────────────────────────────────────────────────────────────
# 线程派生
# ─────────────────────────────────────────────────────────────────────────────

#: 线程派生形态。
THREAD_SPAWN_RE = re.compile(
    r"(?:std::thread\b|std::async\s*\(|\bstd::future\s*<|\bstd::promise\s*<"
    r"|omp_set_num_threads\s*\(|pthread_create\s*\()"
)

#: 自决线程数的形态（CONCURRENCY.md:24「线程数：外部可配置…取值来源 = 配置」的反面）。
SELF_DECIDED_WORKERS_RE = re.compile(
    r"(?:std::thread\s*::\s*hardware_concurrency\s*\(\)|sysconf\s*\(\s*_SC_NPROCESSORS)"
)

#: 显式并行 pragma / OpenMP 并行区（CONCURRENCY.md:35 允许的形态）。
EXPLICIT_PARALLEL_RE = re.compile(r"(?:#\s*pragma\s+omp\s+parallel|omp\s+for\s*\()")


@dataclass(frozen=True)
class ThreadFact:
    path: str
    line: int
    kind: str  # "spawn" | "self-decided-workers" | "explicit-parallel"
    token: str
    decl: str

    @property
    def locator(self) -> str:
        return f"{self.path}:{self.line}"


def find_thread_facts(path: str, raw: str) -> List[ThreadFact]:
    """抽出线程派生事实（逐行，三类互相独立）。"""
    txt = blank_comments_and_strings(raw)
    out: List[ThreadFact] = []
    for offset, line in enumerate(txt.split("\n")):
        s = " ".join(line.split())
        m_self = SELF_DECIDED_WORKERS_RE.search(line)
        if m_self:
            # 自决线程数单独成类，**不**同时计入 spawn：`std::thread::hardware_concurrency()`
            # 会被 `std::thread\b` 顺手命中，若重复计入会让 A2 对同一条报两次
            # （实测假阳：execution_options.h:24 既是 spawn 又是 self-decided）。
            out.append(ThreadFact(path, offset + 1, "self-decided-workers", m_self.group(0), s[:160]))
            continue
        for m in THREAD_SPAWN_RE.finditer(line):
            out.append(ThreadFact(path, offset + 1, "spawn", m.group(0), s[:160]))
        for m in EXPLICIT_PARALLEL_RE.finditer(line):
            out.append(ThreadFact(path, offset + 1, "explicit-parallel", m.group(0), s[:160]))
    return out


# ─────────────────────────────────────────────────────────────────────────────
# provenance KV / 降级记录
# ─────────────────────────────────────────────────────────────────────────────


#: 元素类型词表：裸指针 / 裸数组的判据只看元素类型，不看容器名。
_RAW_ELEMENT_TYPES: Tuple[str, ...] = (
    "char",
    "signed char",
    "unsigned char",
    "short",
    "unsigned short",
    "int",
    "unsigned int",
    "long",
    "unsigned long",
    "long long",
    "float",
    "double",
    "bool",
    "void",
    "size_t",
    "ptrdiff_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "std::size_t",
    "std::ptrdiff_t",
    "std::int8_t",
    "std::int16_t",
    "std::int32_t",
    "std::int64_t",
    "std::uint8_t",
    "std::uint16_t",
    "std::uint32_t",
    "std::uint64_t",
)

_RAW_PTR_RE = re.compile(
    r"(?:static\s+|thread_local\s+)?(?:const\s+|volatile\s+)*"
    r"(?:[A-Za-z_]\w*\s+)*\b(?:" + "|".join(re.escape(t) for t in _RAW_ELEMENT_TYPES) + r")"
    r"\s*\*\s*([A-Za-z_]\w*)\s*(?=[;,)=])"
)

_RAW_ARR_RE = re.compile(
    r"(?:static\s+|thread_local\s+)?(?:const\s+|volatile\s+)*"
    r"(?:[A-Za-z_]\w*\s+)*\b(?:" + "|".join(re.escape(t) for t in _RAW_ELEMENT_TYPES) + r")"
    r"\s+([A-Za-z_]\w*)\s*\[\s*([^\]]*)\s*\]\s*(?=[;,){])"
)


_SCOPE_LEAD_RE = re.compile(r"^\s*(?:static|thread_local)\b")
_NS_COL0_RE = re.compile(r"^(?!\s)[A-Za-z_]")


def find_raw_buffers(path: str, raw: str) -> List[Tuple[int, str, str]]:
    """抽出静态存储期（或命名空间作用域）的裸指针与裸数组声明。

    单独成一类，是因为它们是「模块私藏整帧副本」最常见的藏法：
    `static float* g_frame;` 与 `static double g_cache[W * H];` 都不含任何容器类型名，
    容器词表完全扫不到。返回 `(行号, 名字, 类别)`，类别取 `"raw-pointer"` / `"raw-array"`。

    作用域收窄（不做全量 AST，两条可辩护的行级规则）：
    1. 声明行以 `static` / `thread_local` 开头（存储期限定词领 decl），**或**
    2. 声明顶格写在第 0 列（命名空间作用域的形态）。
    并要求该行以 `;` 或 `=` 收尾（排除控制流与函数体首行）。
    不收窄时的实测假阳为 114 条，绝大多数是**函数形参与局部指针**（例如
    `lib/algorithms/calibration/cpp/cosmetic_corrector.cpp:23` 的
    `static void set_error(const char* msg) {` —— `static` 修饰的是函数，`msg` 是形参）。
    """
    out: List[Tuple[int, str, str]] = []
    for lineno, co_line, _fb_line in iter_code_lines(raw):
        stripped = co_line.rstrip()
        if not (stripped.endswith(";") or stripped.endswith("=")):
            continue
        if not (_SCOPE_LEAD_RE.match(co_line) or _NS_COL0_RE.match(co_line)):
            continue
        if "(" in co_line and stripped.endswith(");"):
            continue  # 函数声明行（形参里的裸指针），实测假阳：
            #            lib/infrastructure/scheduler/src/module_adapters.cpp:272
        m = _RAW_PTR_RE.search(co_line)
        if m:
            out.append((lineno, m.group(1), "raw-pointer"))
            continue
        m = _RAW_ARR_RE.search(co_line)
        if m:
            out.append((lineno, m.group(1), "raw-array"))
    return out


def find_raw_buffers_tu(path: str, raw: str) -> List[Tuple[int, str, str]]:
    """`find_raw_buffers` 的翻译单元版：只扫 `.cpp` / `.c`。

    头文件里的顶格裸指针几乎全是**函数形参**（实测
    `lib/algorithms/photometry/cpp/src/spatial_gain.h:161` 的 `double* H` 是形参），
    行级规则无法把它们与命名空间作用域对象分开，故头文件不进本项采集面。
    该收窄是本判据的**已知漏报面**，逐条登记在测试文件顶部的自报义务里。
    """
    if not path.endswith((".cpp", ".c")):
        return []
    return find_raw_buffers(path, raw)


def find_literal_keys(path: str, raw: str, keys: Sequence[str]) -> Dict[str, List[int]]:
    """返回每个 key 在本文件内作为字符串字面量出现的行号（去重升序）。

    取值走**原文行**（键名在字面量里），是否真代码走**抹白行**判定（注释里的键不算）。
    """
    hits: Dict[str, List[int]] = {}
    pending: Dict[str, List[int]] = {k: [] for k in keys}
    for lineno, co_line, _fb_line in iter_code_lines(raw):
        for k in keys:
            if ('"' + k + '"') in co_line:
                pending[k].append(lineno)
    for k, lines in pending.items():
        if lines:
            hits[k] = sorted(set(lines))
    return hits


#: 降级记录键的候选词（代码里出现的全部形态；合同只点名 `degraded_reason`）。
DEGRADATION_KEY_CANDIDATES: Tuple[str, ...] = (
    "degraded_reason",
    "degraded",
    "degrade",
    "degraded_scalar",
    "snr_degraded_reason",
    "detection_degraded_reason",
    "noise_mask_degraded",
    "sky_plane_degraded",
    "mask_degraded",
)

#: 合同点名的唯一降级记录键（PIPELINE_BLOCK.md:61）。
CONTRACT_DEGRADATION_KEY = "degraded_reason"

#: 合同点名的 provenance 头部 KV（PIPELINE_BLOCK.md:60）。
CONTRACT_PROVENANCE_KV: Tuple[str, ...] = (
    "frame_id",
    "photometry_applied",
    "k_photo",
    "snr_path_effective",
    "saturation_filter",
)
