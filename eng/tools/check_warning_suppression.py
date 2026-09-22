#!/usr/bin/env python3
"""QA-001: 生产编译警告抑制清零校验。

规则:
1. V6 生产模块 (phase1/2/3/core/io/cli 非第三方) 无 -w//w/Wno 全域抑制。
2. -w 抑制仅限第三方/遗留源 (cfitsio/AIO/drizzle/hips V5 代码) — 豁免并登记。
3. 生产构建 (GCC Release) 警告计数 = 0 (增量基线, 第三方豁免源不计)。
exit 0 = PASS。

行为约定:
- 静态检查 (生产源扫描 + CMake 层) 发现违规时直接报错返回:
  不 touch 生产源、不触发构建 (消除无谓副作用)。
- 静态干净且构建树存在时, 才 touch 一个生产源文件 (强制重编) 并统计警告。
- 构建命令经 subprocess 位置参数传递路径 (cwd + $1/$2), 仓库路径含空格
  (如 "/workspace/Astro CS Database") 不再发生 cd 词法断裂。

扫描面派生与 fail-closed (LINUXMAIN-PATH-01 B1):
- 原实现把 CMake 层扫描面手抄成 (cli/CMakeLists.txt, CMakeLists.txt, tests/unit/CMakeLists.txt):
  前两者随 ARCH-001 根目录整合退役 (cli/ -> lib/infrastructure/cli, tests/ -> eng/tests),
  于是扫描面**静默**从 3 个文件缩到 1 个, 而 PASS 文案照旧宣称"CMake 层已查"。
  同类问题在源面同样存在: PROD_DIRS 里退役的 cli/ 由 `if not base.is_dir(): continue` 静默跳过。
- 本版扫描面全部由权威来源派生, 且**任何锚失效都判红点名** (ENGINEERING_SPEC §10「锚存活」/
  「路径不存在即判红」, docs/ci/01_CHECKS.md §1):
  * 源面 = PROD_DIRS 单源常量, 每个目录必须存在, 缺一个 ⇒ ANCHOR_STALE + exit 2;
  * CMake 面 = 根 CMakeLists.txt (BLD-002 唯一产品事实源) 的 add_subdirectory 闭包
    ∪ 仓库内全部 CMakeLists.txt (目录枚举 = 图面超集, 图面变化不可能静默缩小);
    根 CMakeLists.txt 缺失 / 枚举为空 / 图面文件不在枚举面内 ⇒ ANCHOR_STALE + exit 2。
- 判据本身也重建: 原 CMake 判据只匹配 `set_property(SOURCE ... "${ACS_WARN_SUPPRESS}")`,
  而 `ACS_WARN_SUPPRESS` 已从全树消失 ⇒ 该判据恒真 (空扫描假绿)。现判据 = 编译选项上下文
  (target_compile_options / add_compile_options / set_property(... COMPILE_OPTIONS|COMPILE_FLAGS))
  里的**整词 -w**, 命中必须显式登记 (CMAKE_W_REGISTERED, 只减不增); 未登记即判红。
  `ACS_WARN_SUPPRESS` 仍作为"历史形态复活"哨兵单独断言。

用法:
  python3 eng/tools/check_warning_suppression.py            # 真形态 (静态 + 构建警告计数)
  python3 eng/tools/check_warning_suppression.py --selftest  # 正/负例面 (临时树夹具, 不触碰真仓库)

退出码: 0 = PASS; 1 = FAIL; 2 = ANCHOR_STALE (锚失效/扫描面为空, fail-closed)。
"""
import pathlib, re, sys, subprocess

REPO = pathlib.Path(__file__).resolve().parents[2]
ROOT_CMAKE = REPO / "CMakeLists.txt"          # BLD-002 唯一产品事实源

# 生产源目录 (V6 模块)。ARCH-001 迁移后路径；迁移前 =
# ["lib/phase1", "lib/phase2/src", "lib/phase3_session", "lib/core", "lib/io",
#  "lib/cpu", "cli"]。
# 退役根目录订正 (LINUXMAIN-PATH-01 B1): 旧 `cli` 的真身是 lib/infrastructure/cli
# (AGENTS.md §7「基建 cli/{normalize,mosaic,export}」, 根 CMakeLists.txt:856
#  add_subdirectory(lib/infrastructure/cli)); lib/cpu 在迁移清单外且树中不存在, 已删除。
# 本清单是**单源常量**: 每个目录都必须存在, 缺一个即 ANCHOR_STALE (禁止静默跳过)。
PROD_DIRS = [
    "lib/algorithms/star_detection/wrapper_phase1",
    "lib/algorithms/platesolve/wrapper_phase1",
    "lib/algorithms/photometry/wrapper_phase1",
    "lib/algorithms/noise_snr/wrapper_phase1",
    "lib/algorithms/coverage/src",
    "lib/algorithms/projection",
    "lib/algorithms/resample",
    "lib/algorithms/fits_output",
    "lib/phase3_session",
    "lib/infrastructure/scheduler",
    "lib/infrastructure/aio",
    "lib/infrastructure/aio/io",
    "lib/infrastructure/cli",
]

# CMake 层扫描面的权威来源: git 索引 (tracked 文件 = 仓库内容面)。
# 为什么不手抄路径: 手抄副本会随目录整合退役而静默失配 (本项即为此类缺陷的修复)。
# 为什么用 tracked 而不是 rglob: run/ 与 build/ 是 gitignore 的临时/构建区, 里面残留的
# 一次性夹具 (如 run/<task>/**/CMakeLists.txt) 不是仓库内容面; 而 tracked 面**严格包含**
# 根构建图 (BLD-002 根 CMakeLists.txt + add_subdirectory 闭包), 故不可能静默缩小。

# 编译选项上下文的命令名: 只有这些命令的实参算"编译选项", 别处出现的 -w
# (如 add_test 里的 shell `[ -w "$d" ]`) 不算抑制指令。
COMPILE_OPT_CMDS = ("target_compile_options", "add_compile_options")
SET_PROPERTY_MARKERS = ("COMPILE_OPTIONS", "COMPILE_FLAGS")
W_FLAG_RE = re.compile(r"(?<![-\w])-w(?![\w-])")
HISTORICAL_SUPPRESS_VAR = "ACS_WARN_SUPPRESS"

# CMake 层 -w 显式登记 (规则 2: 第三方/遗留源豁免, 只减不增)。
# 键 = "<仓库相对 CMakeLists.txt 路径>::<目标名(或 set_property 的源)>"。
CMAKE_W_REGISTERED = {
    "CMakeLists.txt::astrocs_cfitsio":
        "vendored cfitsio (第三方; 规则 2 豁免面, 根 CMakeLists.txt:438)",
    "lib/infrastructure/pipeline/orchestrator/cpp/CMakeLists.txt::astrocs_orchestrator_jsv":
        "vendored json-schema-validator (第三方; 该文件 :27 注记「与 astrocs_cfitsio 同款处置」)",
    "lib/infrastructure/pipeline/orchestrator/cpp/tests/CMakeLists.txt::orchestrator_gate_module_aio":
        "ORCH-001 批次 3 可执行门夹具 (该文件 :56-:60 声明源为生产 aio_pipeline.cpp / aio_log.cpp,"
        " 即规则 2 豁免的 AIO 遗留源); 登记为可见面, 不放宽任何判据",
}


class AnchorStale(Exception):
    """扫描面锚失效 / 扫描面为空 —— fail-closed, exit 2。"""


def _read(p: pathlib.Path) -> str:
    return p.read_text(encoding="utf-8", errors="ignore")


def _strip_cmake_comments(text: str) -> str:
    """去掉 CMake 注释行尾注释 (注释里的 -w 是说明, 不是指令)。"""
    return "\n".join(re.sub(r"#.*$", "", ln) for ln in text.splitlines())


def _cmake_commands(text: str):
    """产出 (命令名, 实参文本, 行号)。按括号配对切分实参。

    逐命令解析 (而非全文本正则) 是为了不把 add_test 里的 shell 文本
    (如 `[ -w "$d/astrocs_p1star" ]`) 误判成编译选项抑制。
    """
    out = []
    for m in re.finditer(r"(?<![\w-])([A-Za-z_][A-Za-z0-9_]*)\s*\(", text):
        name = m.group(1)
        i = m.end()
        depth, j = 1, i
        while j < len(text) and depth:
            if text[j] == "(":
                depth += 1
            elif text[j] == ")":
                depth -= 1
            j += 1
        if depth:
            continue
        line = text.count("\n", 0, m.start()) + 1
        out.append((name.lower(), text[i:j - 1], line))
    return out


ADD_SUBDIR_RE = re.compile(r"(?<![\w-])add_subdirectory\s*\(\s*([^\s)#]+)", re.I)


def build_graph_cmake_files(repo: pathlib.Path):
    """根 CMakeLists.txt 的 add_subdirectory 闭包 (字面路径边)。

    返回 (图面文件集, 变量驱动的未解析边, 悬空边)。未解析边不缩小覆盖面 ——
    调用方的目录枚举面是图面的超集; 悬空边随覆盖输出一并打印 (供前台定位)。
    """
    root = repo / "CMakeLists.txt"
    if not root.is_file():
        raise AnchorStale(f"ROOT_CMAKE {root} 不存在 (BLD-002 唯一产品事实源)")
    seen, unresolved, dangling = set(), [], []
    stack = [root]
    while stack:
        f = stack.pop()
        if f in seen:
            continue
        seen.add(f)
        for m in ADD_SUBDIR_RE.finditer(_strip_cmake_comments(_read(f))):
            tok = m.group(1).strip('"')
            if "${" in tok or "$<" in tok:
                unresolved.append(f"{f.relative_to(repo)} -> {tok}")
                continue
            sub = (f.parent / tok).resolve() / "CMakeLists.txt"
            if sub.is_file():
                stack.append(sub)
            else:
                dangling.append(f"{f.relative_to(repo)} -> {tok}")
    return seen, unresolved, dangling


def tracked_cmake_files(repo: pathlib.Path):
    """CMake 扫描面 = git 索引里的全部 CMakeLists.txt (tracked = 仓库内容面)。

    fail-closed: git 不可用 / 未列出任何 CMakeLists.txt / tracked 文件在工作树缺失
    ⇒ AnchorStale (点名), 不返回空集。
    """
    try:
        out = subprocess.run(
            ["git", "-c", "core.quotepath=false", "ls-files", "-z"],
            cwd=str(repo), capture_output=True, text=True, timeout=180)
    except Exception as exc:                      # git 不可用 ⇒ 保守判红
        raise AnchorStale(f"GIT_LS_FILES 不可用 ({exc}) —— 无法确定 CMake 扫描面")
    if out.returncode != 0:
        raise AnchorStale(f"GIT_LS_FILES rc={out.returncode}: {out.stderr.strip()[:200]}")
    files = [repo / p for p in out.stdout.split("\0")
             if p and pathlib.PurePosixPath(p).name == "CMakeLists.txt"]
    if not files:
        raise AnchorStale(f"CMAKE_SCAN_FACE: git ls-files 未列出任何 CMakeLists.txt (repo={repo})")
    missing = sorted(str(p.relative_to(repo)) for p in files if not p.is_file())
    if missing:
        raise AnchorStale("CMAKE_SCAN_FACE_TRACKED_MISSING: " + ", ".join(missing))
    return sorted(files)


def cmake_scan_face(repo: pathlib.Path):
    """返回 (扫描面, 构建图面, 未解析边, 悬空边)。

    扫描面 = tracked CMakeLists.txt (严格包含构建图); 构建图面仅用于交叉校验与覆盖输出:
    图面文件必须全在扫描面内, 否则 AnchorStale。
    """
    root = repo / "CMakeLists.txt"
    if not root.is_file():
        raise AnchorStale(f"ROOT_CMAKE {root} 不存在 (BLD-002 唯一产品事实源)")
    face = tracked_cmake_files(repo)
    graph, unresolved, dangling = build_graph_cmake_files(repo)
    not_in_face = sorted(str(p.relative_to(repo)) for p in graph if p not in set(face))
    if not_in_face:
        raise AnchorStale("CMAKE_GRAPH_NOT_IN_SCAN_FACE: " + ", ".join(not_in_face))
    return face, graph, unresolved, dangling


def _compile_option_hits(text: str):
    """编译选项上下文里出现整词 -w 的 (命令名, 目标/源标识, 行号) 列表。"""
    hits = []
    for name, args, line in _cmake_commands(_strip_cmake_comments(text)):
        if name not in COMPILE_OPT_CMDS and name != "set_property":
            continue
        if name == "set_property":
            if not any(mk in args for mk in SET_PROPERTY_MARKERS):
                continue
            head = args.split("PROPERTY")[0].replace("SOURCE", " ", 1).strip()
        else:
            head = args.strip()
        if not W_FLAG_RE.search(args):
            continue
        target = head.split()[0] if head.split() else "(?)"
        hits.append((name, target.strip('"'), line))
    return hits


def static_scan(repo: pathlib.Path = None):
    """生产源 + CMake 层抑制指令扫描 (纯读, 无副作用)。

    锚失效 / 扫描面为空 ⇒ 抛 AnchorStale (调用方转 exit 2), 不返回"无违规"。
    """
    repo = pathlib.Path(repo) if repo is not None else REPO
    errors = []
    scanned_src, scanned_cmake = 0, 0

    # ── 源面: 单源常量 + 锚存活 ──
    for d in PROD_DIRS:
        base = repo / d
        if not base.is_dir():
            raise AnchorStale(
                f"PROD_DIRS 锚 {d} 不存在 (repo={repo}) —— 生产源扫描面会静默缩小, "
                f"fail-closed 拒绝；目录迁移后必须同步本清单")
        for f in sorted(base.rglob("*.cpp")):
            scanned_src += 1
            txt = _read(f)
            # 仅匹配独立编译选项形态 (-w 前后非字母/连字符; not-wired 等注释词不匹配)
            if re.search(r"(?<!\w)-w(?!\w)", txt) or re.search(r'"?-Wno-', txt):
                errors.append(f"生产源含抑制指令: {f.relative_to(repo)}")

    # ── CMake 面: 构建图/目录枚举派生 + 锚存活 ──
    cmake_files, graph, unresolved, dangling = cmake_scan_face(repo)
    registered_hits, unregistered = [], []
    for cm in cmake_files:
        scanned_cmake += 1
        txt = _strip_cmake_comments(_read(cm))
        rel = str(cm.relative_to(repo))
        if HISTORICAL_SUPPRESS_VAR in txt:
            errors.append(f"{rel}: 出现历史抑制变量 {HISTORICAL_SUPPRESS_VAR} "
                          f"(全域抑制形态复活)")
        for name, target, line in _compile_option_hits(txt):
            key = f"{rel}::{target}"
            if key in CMAKE_W_REGISTERED:
                registered_hits.append(f"{rel}:{line}: {name}({target}) -w [已登记]")
            else:
                unregistered.append(f"{rel}:{line}: {name}({target}) 使用整词 -w "
                                    f"但未登记 (规则 2 只豁免第三方/遗留源)")
    errors.extend(unregistered)

    coverage = {
        "prod_dirs": len(PROD_DIRS),
        "src_files": scanned_src,
        "cmake_files": scanned_cmake,
        "cmake_graph_files": len(graph),
        "cmake_graph_unresolved_edges": len(unresolved),
        "cmake_graph_dangling_edges": len(dangling),
        "cmake_w_registered_hits": len(registered_hits),
        "cmake_w_registered_total": len(CMAKE_W_REGISTERED),
    }
    return errors, coverage, registered_hits, unresolved + dangling
# 构建统计脚本: $1 = 仓库根, $2 = 构建目录(相对仓库根)。
# 位置参数 + 引号内引用, 路径含空格安全; WSL 横幅过滤与 grep -c 'warning:'
# 统计语义与旧实现保持一致。
BUILD_COUNT_SCRIPT = (
    'cd "$1" || exit 9\n'
    'cmake --build "$2" --target astrocs 2>&1 '
    "| grep -v 'WSL\\|适用于 Linux' | grep -c 'warning:' || true\n"
)


def detect_build_tree():
    # 构建目录缺失时结构性跳过（windows runner 无构建树；探测顺序
    # build/root-cmake → build/，适配当前 Ninja 单配置布局 build/）。
    for cand in (REPO / "build" / "root-cmake", REPO / "build"):
        if (cand / "Makefile").exists() or (cand / "build.ninja").exists():
            return cand
    return None


def count_build_warnings(build_dir, repo=None, build_cmd="cmake"):
    """构建并统计生产警告数, 返回 stdout 统计串 ("" / "0" / "N")。

    build_cmd 参数仅供测试注入假命令 (echo 等), 生产恒为 cmake。
    位置参数传递路径: $1=仓库根 $2=构建目录相对路径, cwd=仓库根。
    """
    repo = str(repo) if repo is not None else str(REPO)
    rel = str(build_dir) if not str(build_dir).startswith("/") \
        else pathlib.Path(build_dir).relative_to(repo)
    script = BUILD_COUNT_SCRIPT.replace("cmake", build_cmd, 1) \
        if build_cmd != "cmake" else BUILD_COUNT_SCRIPT
    r = subprocess.run(["bash", "-c", script, "bash", repo, rel],
                       capture_output=True, text=True, timeout=600, cwd=repo)
    return r.stdout.strip()


def measure_build(build_dir):
    """touch 强制重编一个生产文件后统计警告 (构建树存在时的功能需要)。

    返回警告统计串; 锚失效时返回 None (调用方 fail-closed 判 FAIL)。

    锚点订正 (DOC-205 回执 #1): 原锚 wrapper_phase1/noise_model.cpp 已按
    NOISE-MODEL-CANON-001 / EXP-206 定案从 HEAD 删除 (B 为 A 的退化子集, 退役),
    继续 touch() 只会在工作树里凭空造出一个 0 字节幽灵文件, 且因为该路径
    不在构建图内, cmake 什么都不重编 ⇒ "生产警告=0" 是空扫描假绿。
    改锚为仍在 astrocs_phase1_noise 源列表内的
    wrapper_phase1/snr_frame_science.cpp (CMakeLists.txt:646)。
    """
    rel_src = REPO / "lib" / "algorithms" / "noise_snr" / "wrapper_phase1" / "snr_frame_science.cpp"
    if not rel_src.is_file():
        # 锚存活 (ENGINEERING_SPEC.md §8): 硬编码锚失效 ⇒ 显式判红, 不静默跳过、
        # 不 touch 出幽灵文件。
        return None
    rel_src.touch()
    return count_build_warnings(build_dir)


def selftest() -> int:
    """正/负例面: 全部在临时树夹具内跑, 不触碰真仓库、不触发构建。"""
    import tempfile

    fails = []
    with tempfile.TemporaryDirectory() as tmp:
        root = pathlib.Path(tmp)
        # 夹具必须是 git 仓库: CMake 扫描面取自 git 索引 (tracked 面)
        subprocess.run(["git", "init", "-q"], cwd=str(root), check=True,
                       capture_output=True, timeout=120)
        (root / "CMakeLists.txt").write_text(
            "add_subdirectory(lib/mod)\n"
            "target_compile_options(astrocs_cfitsio PRIVATE -w)\n",
            encoding="utf-8")
        (root / "lib" / "mod").mkdir(parents=True)
        (root / "lib" / "mod" / "CMakeLists.txt").write_text(
            "target_compile_options(mod_ok PRIVATE -Wall -Wextra -Wpedantic)\n"
            "add_test(NAME shell_w COMMAND sh -c \"[ -w /tmp ]\")\n",
            encoding="utf-8")
        for d in PROD_DIRS:
            (root / d).mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "add", "-A"], cwd=str(root), check=True,
                       capture_output=True, timeout=120)

        # 正例: 干净夹具 ⇒ 无 error; 登记的 cfitsio -w 可见但不判红
        errs, cov, reg, notes = static_scan(root)
        if errs:
            fails.append("正例 干净夹具误报: %s" % errs[:3])
        if cov["src_files"] != 0 or cov["cmake_files"] != 2:
            fails.append("正例 覆盖计数不符: %s" % cov)
        if not any("astrocs_cfitsio" in r for r in reg):
            fails.append("正例 登记命中未可见: %s" % reg)

        # 负例 ①: 未登记的 -w ⇒ 必红
        (root / "lib" / "mod" / "CMakeLists.txt").write_text(
            "target_compile_options(mod_bad PRIVATE -w)\n", encoding="utf-8")
        errs, _, _, _ = static_scan(root)
        if not any("未登记" in e for e in errs):
            fails.append("负例1 未登记 -w 未被检出: %s" % errs)

        # 负例 ②: add_test 里的 shell [ -w ] 不得误报
        (root / "lib" / "mod" / "CMakeLists.txt").write_text(
            "add_test(NAME shell_w COMMAND sh -c \"[ -w /tmp ]\")\n", encoding="utf-8")
        errs, _, _, _ = static_scan(root)
        if errs:
            fails.append("负例2 shell -w 误报: %s" % errs)

        # 负例 ③: 生产源面锚失效 ⇒ ANCHOR_STALE
        import shutil as _sh
        _sh.rmtree(root / PROD_DIRS[0])
        try:
            static_scan(root)
            fails.append("负例3 PROD_DIRS 锚失效未抛 AnchorStale")
        except AnchorStale:
            pass
        (root / PROD_DIRS[0]).mkdir(parents=True, exist_ok=True)

        # 负例 ④: 根 CMakeLists.txt 缺失 ⇒ ANCHOR_STALE
        (root / "CMakeLists.txt").unlink()
        try:
            static_scan(root)
            fails.append("负例4 根 CMakeLists.txt 缺失未抛 AnchorStale")
        except AnchorStale:
            pass
        (root / "CMakeLists.txt").write_text("add_subdirectory(lib/mod)\n", encoding="utf-8")

        # 负例 ⑤: 历史抑制变量复活 ⇒ 必红
        (root / "lib" / "mod" / "CMakeLists.txt").write_text(
            'set_property(SOURCE x.cpp PROPERTY COMPILE_OPTIONS "${ACS_WARN_SUPPRESS}")\n',
            encoding="utf-8")
        errs, _, _, _ = static_scan(root)
        if not any(HISTORICAL_SUPPRESS_VAR in e for e in errs):
            fails.append("负例5 历史抑制变量未被检出: %s" % errs)

    if fails:
        print("QA-001_SELFTEST_FAIL:")
        for f in fails:
            print("  " + f)
        return 1
    print("QA-001_SELFTEST_PASS: 干净夹具绿; 未登记 -w / PROD_DIRS 锚失效 / "
          "根 CMake 缺失 / 历史抑制变量 各自判红; add_test 内 shell -w 不误报")
    return 0


def main(argv=None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if "--selftest" in argv:
        return selftest()

    # 静态检查先行: 违规直接报错返回, 不 touch 源文件、不触发构建
    try:
        errors, coverage, registered_hits, graph_notes = static_scan()
    except AnchorStale as exc:
        print("QA-001_FAIL: ANCHOR_STALE %s" % exc)
        return 2
    if errors:
        print("QA-001_WARN_VIOLATION:")
        for e in errors:
            print("  " + e)
        print("QA-001 coverage: %s" % coverage)
        return 1
    build_dir = detect_build_tree()
    if build_dir is None:
        # GAP-027 fail-closed（CI-001）：原为 QA-001_SKIP + rc=0（缺构建产物即静默绿）
        print("QA-001_FAIL: 无可用构建树 (build/root-cmake 与 build/ 均缺失)，"
              "编译警告抑制门无法执行 → fail-closed 判 FAIL")
        return 1
    warn = measure_build(build_dir)
    if warn is None:
        # 锚存活 fail-closed (ENGINEERING_SPEC §8): 硬编码生产源不存在 ⇒ 判红,
        # 不得把「锚失效」当「零警告」。
        print("QA-001_FAIL: ANCHOR_STALE 强制重编锚 "
              "lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp 不存在 "
              "⇒ 编译警告统计无法执行, fail-closed 判 FAIL")
        return 1
    if warn not in ("", "0"):
        print("QA-001_WARN_VIOLATION:")
        print("  生产构建警告 %s 个 (非 0)" % warn)
        return 1
    print("QA-001_PASS: V6 生产模块零抑制指令; -w 仅第三方豁免(已登记); 生产警告=0")
    print("QA-001 coverage: %s" % coverage)
    for r in registered_hits:
        print("  登记命中 " + r)
    for g in graph_notes:
        print("  构建图注记 " + g)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
