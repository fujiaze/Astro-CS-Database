#!/usr/bin/env python3
"""LEG-002..004: 旧生产路径退出校验 (orchestrator/AIO PipelineEngine/old Stage2/ACR)。

规则 (LEG 规格: 逐个确认无 canonical caller、链接符号、文档入口、安装产物后退出):
1. 生产二进制 (build/root-cmake/astrocs) 不含目标目录符号。
2. 根 CMakeLists 不链入目标目录。
3. 文档入口标注退出或已清理。
4. 源码目录保留 (不破坏删除), 但无生产引用。

契约（ENGINEERING_SPEC.md §8，2026-09-16 增补）:
- **锚存活**: 本文件硬编码引用的仓库路径集中在 ANCHORS；任一失效 ⇒ 打印
  `ANCHOR_STALE: <常量名> <路径>` 并 `exit 2`，**不得 traceback、不得静默通过**。
- **fail-closed**: 生产二进制缺失 / nm 不可用 / 生产源码扫描面为空 / ACR 内核真源
  内容漂移 ⇒ 一律判红（exit 1），不得把「找不到」当「无违规」。
- **可执行负例面**: `--self-test` 内含 1 组正例 + 9 组注入负例。

exit 0 = PASS；exit 1 = 判据违规；exit 2 = 锚失效或依赖不可用（fail-closed）。
"""
import argparse
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]

# ── 锚（硬编码引用的仓库路径；ARCH-001/ROOT-007 迁移后由 --self-test 与 ANCHOR_STALE 守护）──
ANCHORS = {
    "AIO_ENGINE_HEADER": "lib/infrastructure/aio/include/aio_pipeline_engine.h",
    "ACR_KERNEL_SOURCE": "lib/algorithms/coverage/src/acr_kernels.cpp",
    "ROOT_CMAKE": "CMakeLists.txt",
    "PUBLIC_API_DOC": "docs/contracts/PUBLIC_API.md",
}
BINARY_CANDIDATES = ("build/root-cmake/astrocs", "build/acsd", "build/cli/astrocs")
PROD_SCAN_ROOTS = ("lib", "cli")
# ACR 内核真源文件名：本文件定义 register_phase2_acr_(kernels)，其余生产文件不得调用它
ACR_KERNEL_DEFINER = "acr_kernels.cpp"
NON_PROD_SEGMENTS = ("/tests/", "/tools/", "/testdata/", "/fixtures/", "/benchmarks/")

# ── LEG-002 链接闭包 / 安装面判据（ECP-ORCH-LEG002，W4-A3 实施）─────────────────
# 事由：原第 2 臂是**纯词面子串**（`if "orchestrator" in cmake`），连注释都命中；
# 而政策自述的四条实质规则是「无 canonical caller、链接符号、文档入口、**安装产物**」
# —— 即判「是否进产品交付面」，不是判「是否被编译」。ORCH-001 为让 SAT-001/MASK-002
# 的代码真被编译而接线 add_subdirectory（不进 acsd 链接闭包、不进安装树、不进
# 产品 manifest），词面臂把「编译」等同于「生产引用」⇒ 判红，但四条实质规则实测三条
# PASS、第 2 条实质亦 PASS（见 run/PROJECT-GOVERNANCE-01/ORCH-001/LEG-002-裁决请求.md §3）。
# 新判据（双向）：
#   红 = orchestrator 目录出现在 ① acsd 链接闭包 ② install 白名单/产品 manifest
#        ③ 生产符号面（原第 1 臂保留）
#   绿 = 仅存在**非安装、非产品**的编译型 target，且以上三面 0 命中
# 先补新臂、再删旧臂（同批过 --self-test）：只删不补会留下"可以偷偷链进生产二进制"的口子。
ORCH_DIR = "lib/infrastructure/pipeline/orchestrator"
ORCH_CPP_DIR = ORCH_DIR + "/cpp"
LINK_ROOT_TARGET = "acsd"
CMAKE_SCAN_EXCLUDE = ("/third_party/", "/build/", "/run/", "/.git/")
# 产品交付面 manifest（**只收真产品 manifest / 安装树合同**；eng/packaging/dependency-lock.json
# 这类依赖清单不是产品交付面，收进来会把"被列为依赖"误判成"进产品"）。
PRODUCT_MANIFEST_GLOBS = ("*.product.json", "**/*.product.json",
                          "eng/packaging/install-tree.contract.json")
BUILD_NINJA_CANDIDATES = ("build/root-cmake/build.ninja", "build/build.ninja",
                          "run/ci/build-gcc-release/build.ninja")


def _strip_cmake_comments(text):
    """去掉 CMake 行注释 —— 新判据只看**实际接线**，注释提及不再命中（旧词面臂的缺陷面）。"""
    return "\n".join(ln.split("#", 1)[0] for ln in text.splitlines())


def cmake_files(repo):
    """判据输入集合：根 CMakeLists + eng/cmake/*.cmake + lib|tests|app 下的 CMakeLists。"""
    out = []
    root = repo / "CMakeLists.txt"
    if root.is_file():
        out.append(root)
    cm = repo / "eng" / "cmake"
    if cm.is_dir():
        out += sorted(cm.rglob("*.cmake"))
    for base in ("lib", "tests", "app"):
        d = repo / base
        if d.is_dir():
            out += sorted(d.rglob("CMakeLists.txt"))
    keep = []
    for f in out:
        posix = "/" + f.relative_to(repo).as_posix()
        if any(seg in posix for seg in CMAKE_SCAN_EXCLUDE):
            continue
        keep.append(f)
    return keep


# 三个 pattern 一律只捕 target 名（group 1）；参数体由 _cmd_calls 用配对括号取。
# 注意：pattern 结束于 target 名，取参数体必须显式找其后的 "("，不能用 m.end()-1
# （从名字中间开始扫会跨命令吞文本 ⇒ 假红/假绿）。
_TGT_DECL = re.compile(r"\b(?:add_library|add_executable)\s*\(\s*([A-Za-z_][\w.-]*)", re.I)
_TGT_LINK = re.compile(r"\btarget_link_libraries\s*\(\s*([A-Za-z_][\w.-]*)", re.I)
_TGT_SOURCES = re.compile(r"\btarget_sources\s*\(\s*([A-Za-z_][\w.-]*)", re.I)
_INSTALL = re.compile(r"\binstall\s*\(", re.I)
_LINK_KEYWORDS = {"PRIVATE", "PUBLIC", "INTERFACE", "LINK_PRIVATE", "LINK_PUBLIC", "debug",
                  "optimized", "general"}


def _cmake_units(repo):
    """返回 [(rel_posix, text_nc)]；text_nc 已去注释。一次读入，多处复用。"""
    units = []
    for f in cmake_files(repo):
        try:
            txt = _strip_cmake_comments(f.read_text(encoding="utf-8", errors="ignore"))
        except OSError:
            continue
        units.append((f.relative_to(repo).as_posix(), txt))
    return units


def _balanced(text, start):
    """从 text[start] == "(" 起取到配对右括号的内容（含嵌套括号）。"""
    depth, i = 0, start
    while i < len(text):
        ch = text[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
        i += 1
    return text[start + 1:]


def _cmd_calls(text, pattern):
    """产出 (pattern 的 group(1) 捕获名, 参数 token 列表)。"""
    for m in pattern.finditer(text):
        # pattern 自身已吃掉命令的 "("（含 target 名），故其配对内容必须从**匹配区间内**
        # 的那个 "(" 起算；用 find("(", m.end()) 会跳到下一条命令的括号（实测把
        # add_subdirectory(...) 的参数当成 target_link_libraries 的依赖）。
        lp = text.rfind("(", m.start(), m.end())
        if lp < 0:
            continue
        body = _balanced(text, lp)
        yield m.group(1), [a for a in re.split(r"[\s\n]+", body.strip()) if a]


def orchestrator_targets(units):
    """在 orchestrator/cpp 子树里被声明的 target 名（含其源列表直接点名该目录的 target）。"""
    names = set()
    for rel, txt in units:
        in_subtree = rel == ORCH_CPP_DIR + "/CMakeLists.txt" or rel.startswith(ORCH_CPP_DIR + "/")
        for name, rest in _cmd_calls(txt, _TGT_DECL):
            if in_subtree or any(ORCH_CPP_DIR in a for a in rest):
                names.add(name)
        for name, rest in _cmd_calls(txt, _TGT_SOURCES):
            if any(ORCH_CPP_DIR in a for a in rest):
                names.add(name)
    return names


def _find_target_decl(units, target):
    pat = re.compile(r"\b(add_library|add_executable)\s*\(\s*" + re.escape(target) + r"\b", re.I)
    for rel, txt in units:
        if pat.search(txt):
            return rel
    return None


def link_edges(units):
    """target -> 依赖 target 名集合（点名的库名） + target -> 源 token 列表。"""
    links, sources = {}, {}
    for _rel, txt in units:
        for tgt, deps in _cmd_calls(txt, _TGT_LINK):
            bucket = links.setdefault(tgt, set())
            for d in deps:
                if d in _LINK_KEYWORDS or d.startswith("$") or d.startswith("-"):
                    continue
                bucket.add(d)
        for tgt, rest in _cmd_calls(txt, _TGT_SOURCES):
            sources.setdefault(tgt, set()).update(rest)
        for tgt, args in _cmd_calls(txt, _TGT_DECL):
            sources.setdefault(tgt, set()).update(args)
    return links, sources


def _closure(links, root):
    seen, stack = set(), [root]
    while stack:
        cur = stack.pop()
        for dep in links.get(cur, ()):
            if dep not in seen:
                seen.add(dep)
                stack.append(dep)
    return seen


def link_closure_hits(repo, units=None):
    """① acsd 链接闭包命中项（含"以源文件直接点名该目录"的写法）。"""
    units = _cmake_units(repo) if units is None else units
    orch = orchestrator_targets(units)
    links, sources = link_edges(units)
    hits = set()
    closure = _closure(links, LINK_ROOT_TARGET)
    hits |= (closure & orch)
    for t in closure | {LINK_ROOT_TARGET}:
        if any(ORCH_CPP_DIR in s for s in sources.get(t, ())):
            hits.add(t + " (源列表直接点名 " + ORCH_CPP_DIR + ")")
    return sorted(hits), orch, _find_target_decl(units, LINK_ROOT_TARGET)


def install_face_hits(repo, units=None):
    """② 安装白名单 / 产品 manifest 命中项。"""
    units = _cmake_units(repo) if units is None else units
    orch = orchestrator_targets(units)
    hits = []
    for rel, txt in units:
        for m in _INSTALL.finditer(txt):
            body = _balanced(txt, m.end() - 1)
            named = any(re.search(r"\b" + re.escape(t) + r"\b", body) for t in orch)
            if ORCH_CPP_DIR in body or named:
                hits.append("install 规则 (" + rel + ")")
    for pat in PRODUCT_MANIFEST_GLOBS:
        for f in sorted(repo.glob(pat)):
            posix = "/" + f.relative_to(repo).as_posix()
            if any(seg in posix for seg in CMAKE_SCAN_EXCLUDE):
                continue
            try:
                txt = f.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            named = any(re.search(r"\b" + re.escape(t) + r"\b", txt) for t in orch)
            if ORCH_CPP_DIR in txt or named:
                hits.append("产品 manifest " + f.relative_to(repo).as_posix())
    return sorted(set(hits))


def build_ninja_hits(repo):
    """③ 构建图佐证：acsd 链接边输入含 orchestrator（只加红，不消红）。"""
    for rel in BUILD_NINJA_CANDIDATES:
        p = repo / rel
        if not p.is_file():
            continue
        try:
            txt = p.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        joined = re.sub(r"\$\n\s*", " ", txt)
        for line in joined.splitlines():
            if not line.startswith("build "):
                continue
            head, _, tail = line[6:].partition(":")
            outs = [o.strip() for o in head.split() if o.strip()]
            if not any(o == LINK_ROOT_TARGET or o.endswith("/" + LINK_ROOT_TARGET) for o in outs):
                continue
            if ORCH_DIR in tail or "orchestrator" in tail.lower():
                return [rel + " 的 acsd 链接边输入含 " + ORCH_DIR]
    return []

def anchor_errors(repo):
    """锚存活前置断言（§8）。返回失效清单；非空 ⇒ 调用方必须 exit 2。"""
    return [f"ANCHOR_STALE: {name} {rel}" for name, rel in ANCHORS.items()
            if not (repo / rel).exists()]


def find_binary(repo):
    return next((repo / p for p in BINARY_CANDIDATES if (repo / p).exists()), None)


def binary_symbols(bin_path):
    """生产二进制符号面。nm 不可用 ⇒ 返回 None（调用方 fail-closed 判红）。"""
    if shutil.which("nm") is None:
        return None
    proc = subprocess.run(["nm", str(bin_path)], capture_output=True, text=True)
    return proc.stdout.lower()


def prod_sources(repo):
    """生产源码集合（lib/ 与 lib/infrastructure/cli/ 下 *.cpp，排除 eng/tests/tools/fixtures/benchmarks）。"""
    out = []
    for rel in PROD_SCAN_ROOTS:
        root = repo / rel
        if not root.exists():
            continue
        for f in sorted(root.rglob("*.cpp")):
            posix = "/" + f.relative_to(repo).as_posix()
            if any(seg in posix for seg in NON_PROD_SEGMENTS):
                continue
            out.append(f)
    return out


def check(repo, symbols_fn=binary_symbols):
    """跑全部 LEG-002..004 判据。返回 (errors, stats)。"""
    errors, stats = [], {}

    # ── 锚存活前置断言 ──
    stale = anchor_errors(repo)
    if stale:
        return stale, stats

    # ── 生产二进制 ──
    bin_path = find_binary(repo)
    stats["binary"] = str(bin_path.relative_to(repo)) if bin_path else None
    if bin_path is None:
        # GAP-027 fail-closed（CI-001）：原实现路径漂移后整段 nm 符号扫描静默跳过仍 PASS
        errors.append("未找到生产二进制（候选 build/root-cmake/astrocs、build/acsd、"
                      "build/cli/astrocs）→ 符号面无法验证，fail-closed 判 FAIL")
        syms = None
    else:
        syms = symbols_fn(bin_path)
        if syms is None:
            errors.append("nm 不可用 → 生产符号面无法验证，fail-closed 判 FAIL")
    stats["symbols_checked"] = syms is not None

    # ── LEG-002: 旧 Orchestrator（第 1 臂=生产符号面；第 2 臂=链接闭包/安装面，
    #    ECP-ORCH-LEG002 取代原词面子串实现）──
    if syms is not None and "orchestrat" in syms:
        errors.append("production binary contains orchestrator symbol: orchestrat")
    units = _cmake_units(repo)
    orch_targets = orchestrator_targets(units)
    stats["leg002_orchestrator_targets"] = sorted(orch_targets)
    orch_present = (repo / ORCH_CPP_DIR).exists()
    stats["leg002_orchestrator_dir"] = orch_present
    if orch_present and not orch_targets:
        # fail-closed：目录在、却解析不出任何 target ⇒ 判据面漂移（别把"解析失败"当"没接线"）
        errors.append("orchestrator 目录存在但未解析出任何 target（" + ORCH_CPP_DIR +
                      "）→ 判据面漂移，fail-closed 判 FAIL")
    root_decl = _find_target_decl(units, LINK_ROOT_TARGET)
    stats["leg002_link_root_decl"] = root_decl
    if orch_present and root_decl is None:
        errors.append("未在 CMake 面找到生产可执行 target " + LINK_ROOT_TARGET +
                      " → 链接闭包判据面漂移，fail-closed 判 FAIL")
    closure_hits, _orch, _decl = link_closure_hits(repo, units)
    stats["leg002_link_closure_hits"] = closure_hits
    for h in closure_hits:
        errors.append("orchestrator 进入 " + LINK_ROOT_TARGET + " 链接闭包: " + h +
                      " (LEG-002 链接闭包臂)")
    install_hits = install_face_hits(repo, units)
    stats["leg002_install_face_hits"] = install_hits
    for h in install_hits:
        errors.append("orchestrator 进入安装/产品交付面: " + h + " (LEG-002 安装面臂)")
    ninja_hits = build_ninja_hits(repo)
    stats["leg002_build_ninja_hits"] = ninja_hits
    for h in ninja_hits:
        errors.append("构建图佐证命中: " + h + " (LEG-002 链接闭包臂)")
    cmake = (repo / ANCHORS["ROOT_CMAKE"]).read_text(encoding="utf-8", errors="ignore")
    public_api = (repo / ANCHORS["PUBLIC_API_DOC"]).read_text(encoding="utf-8", errors="ignore")
    if "orchestrator.exe" in public_api and "LEG-002" not in public_api:
        errors.append("PUBLIC_API orchestrator.exe 未标 LEG-002 退出")

    # ── LEG-003: AIO PipelineEngine 调度职责 ──
    # (engine run API 无生产 caller; frame 数据结构 API 保留供 CLI drizzle 测试 wrapper 用)
    eng_h = (repo / ANCHORS["AIO_ENGINE_HEADER"]).read_text(encoding="utf-8", errors="ignore")
    engine_run_decl = ("aio_pipeline_engine_run_single" in eng_h or
                       "aio_pipeline_engine_run_batch" in eng_h)
    sources = prod_sources(repo)
    acr_src = repo / ANCHORS["ACR_KERNEL_SOURCE"]
    scan_face = [f for f in sources if f != acr_src]
    stats["prod_sources_scanned"] = len(sources)
    if not engine_run_decl:
        errors.append("AIO PipelineEngine run API 声明缺失（"
                      f"{ANCHORS['AIO_ENGINE_HEADER']}）→ 判据面漂移，fail-closed 判 FAIL")
    if not scan_face:
        errors.append("生产源码扫描面为空（lib/、lib/infrastructure/cli/ 下除 ACR 内核真源外无 *.cpp）"
                      "→ fail-closed 判 FAIL")
    caller_files = []
    for f in sources:
        if f.name == "aio_pipeline_engine.cpp":
            continue
        if "aio_pipeline_engine_run" in f.read_text(encoding="utf-8", errors="ignore"):
            caller_files.append(f.relative_to(repo).as_posix())
    if engine_run_decl and caller_files:
        errors.append(f"PipelineEngine run API has callers: {caller_files} (LEG-003)")
    stats["leg003_callers"] = caller_files

    # ── LEG-004: 旧 Stage2 工具 + ACR 隔离 ──
    # (a) 旧 stage2.cpp 工具不随根构建产出 (无安装产物)
    if (repo / "build" / "root-cmake" / "astrocs-stage2").exists():
        errors.append("astrocs-stage2 旧工具被生产构建产出 (LEG-004)")

    # (b) ACR dormant: ASTROCS_ENABLE_ACR=OFF; 生产二进制符号面无 acr
    if "ASTROCS_ENABLE_ACR" not in cmake:
        errors.append("ACR option 缺失 (LEG-004)")
    if syms is not None and "acr" in syms:
        errors.append("生产二进制含 ACR 符号 (LEG-004)")

    # (c) ACR 内核注册真源内容漂移 + 生产路径无 ACR 注册调用
    acr_txt = acr_src.read_text(encoding="utf-8", errors="ignore")
    if "register_phase2_acr" not in acr_txt:
        errors.append(f"ACR 内核注册真源内容漂移：{ANCHORS['ACR_KERNEL_SOURCE']} "
                      "不含 register_phase2_acr (LEG-004) → fail-closed 判 FAIL")
    for f in sources:
        if f.name == ACR_KERNEL_DEFINER:
            continue
        if "register_phase2_acr" in f.read_text(encoding="utf-8", errors="ignore"):
            errors.append(f"ACR kernel 注册存在: {f.relative_to(repo).as_posix()} "
                          "(LEG-004: 应 dormant 不注册)")
    return errors, stats


# ─────────────────────────── 可执行负例面（--self-test） ───────────────────────────
_CMAKE_OK = ("option(ASTROCS_ENABLE_ACR \"Build dormant ACR tree\" OFF)\n"
            "add_executable(acsd lib/infrastructure/cli/main.cpp)\n"
            "target_link_libraries(acsd PRIVATE astrocs_core)\n")
_ORCH_CMAKE_OK = ("add_library(astrocs_infra_orchestrator STATIC src/orchestrator.cpp)\n"
                  "add_executable(orchestrator_legacy_cli src/main.cpp)\n"
                  "target_link_libraries(orchestrator_legacy_cli PRIVATE astrocs_infra_orchestrator)\n")
_ORCH_SUBDIR_LINE = "add_subdirectory(lib/infrastructure/pipeline/orchestrator/cpp)\n"
_AIO_OK = "AIO_EXPORT int aio_pipeline_engine_run_single(PipelineEngine* e);\n"
_ACR_OK = "void register_phase2_acr_kernels() {}\n"


def _build_mini_repo(root, *, with_caller=False, with_acr_reg=False, drop_cmake_token=False,
                     drop_anchor=None, drop_prod_sources=False, cmake_orchestrator=False,
                     public_api_orchestrator=False, binary_symbol="benign_symbol",
                     with_orchestrator=True, orch_linked_into_astrocs=False,
                     orch_install_rule=False, orch_in_product_manifest=False,
                     cmake_comment_mentions_orchestrator=False, no_astrocs_target=False):
    (root / "lib" / "infrastructure" / "aio" / "include").mkdir(parents=True, exist_ok=True)
    (root / "lib" / "algorithms" / "coverage" / "src").mkdir(parents=True, exist_ok=True)
    (root / "lib" / "other" / "src").mkdir(parents=True, exist_ok=True)
    (root / "docs" / "contracts").mkdir(parents=True, exist_ok=True)
    (root / "build").mkdir(parents=True, exist_ok=True)
    # orchestrator 子树（ECP-ORCH-LEG002 判据面：允许"仅编译"、禁止"进产品"）
    if with_orchestrator:
        od = root / ORCH_CPP_DIR
        od.mkdir(parents=True, exist_ok=True)
        (od / "src").mkdir(parents=True, exist_ok=True)
        (od / "src" / "orchestrator.cpp").write_text("int orch_fn(){return 0;}\n", encoding="utf-8")
        (od / "CMakeLists.txt").write_text(_ORCH_CMAKE_OK, encoding="utf-8")
    cmake_txt = ("" if drop_cmake_token else _CMAKE_OK)
    if no_astrocs_target:
        cmake_txt = cmake_txt.replace("add_executable(acsd lib/infrastructure/cli/main.cpp)\n", "")
    if with_orchestrator:
        cmake_txt += _ORCH_SUBDIR_LINE
    if cmake_orchestrator or orch_linked_into_astrocs:
        cmake_txt += "target_link_libraries(acsd PRIVATE astrocs_infra_orchestrator)\n"
    if orch_install_rule:
        cmake_txt += ("install(TARGETS astrocs_infra_orchestrator DESTINATION lib)\n"
                      if with_orchestrator else
                      "install(DIRECTORY lib/infrastructure/pipeline/orchestrator DESTINATION lib)\n")
    if cmake_comment_mentions_orchestrator:
        cmake_txt += "# 说明：lib/infrastructure/pipeline/orchestrator/cpp 仅编译、不进产品\n"
    (root / ANCHORS["ROOT_CMAKE"]).write_text(cmake_txt, encoding="utf-8")
    if orch_in_product_manifest:
        (root / "packaging").mkdir(parents=True, exist_ok=True)
        (root / "packaging" / "astrocs.product.json").write_text(
            '{"units": ["lib/infrastructure/pipeline/orchestrator/cpp"]}\n', encoding="utf-8")
    (root / ANCHORS["PUBLIC_API_DOC"]).write_text(
        "orchestrator.exe\n" + ("" if public_api_orchestrator else "LEG-002 retired\n"),
        encoding="utf-8")
    (root / ANCHORS["AIO_ENGINE_HEADER"]).write_text(_AIO_OK, encoding="utf-8")
    (root / ANCHORS["ACR_KERNEL_SOURCE"]).write_text(_ACR_OK, encoding="utf-8")
    if not drop_prod_sources:
        (root / "lib" / "other" / "src" / "a.cpp").write_text("int f(){return 0;}\n", encoding="utf-8")
    if with_caller:
        (root / "lib" / "other" / "src" / "caller.cpp").write_text(
            "int g(){return aio_pipeline_engine_run_single(0);}\n", encoding="utf-8")
    if with_acr_reg:
        (root / "lib" / "other" / "src" / "acr_user.cpp").write_text(
            "void h(){register_phase2_acr_kernels();}\n", encoding="utf-8")
    if drop_anchor:
        (root / drop_anchor).unlink()
    src = root / "_probe.c"
    src.write_text(f"int {binary_symbol}(void){{return 0;}}\n", encoding="utf-8")
    subprocess.run(["gcc", "-shared", "-fPIC", "-o", str(root / "build" / "acsd"), str(src)],
                   capture_output=True)
    src.unlink()


def _self_test():
    """正例 1 组 + 负例 9 组。返回 rc（0 = 全部符合预期）。"""
    if shutil.which("gcc") is None or shutil.which("nm") is None:
        print("SELFTEST_FAIL: 需要 gcc 与 nm 才能构造二进制符号面负例（fail-closed）")
        return 2
    failures = []
    with tempfile.TemporaryDirectory(prefix="legacy-exit-selftest-") as tmp:
        base = pathlib.Path(tmp)

        pos = base / "positive"
        _build_mini_repo(pos)
        errs, _ = check(pos)
        if errs:
            failures.append(f"正例应 rc=0，实得 errors={errs}")

        # 正例 2 组：①仅编译（add_subdirectory，不进链接闭包/安装树/产品 manifest）⇒ 绿；
        #            ②根 CMake **注释**提及该目录 ⇒ 仍绿（ECP-ORCH-LEG002 的核心改动面）。
        pos_compile_only = base / "positive-compile-only"
        _build_mini_repo(pos_compile_only)
        errs, st = check(pos_compile_only)
        if errs:
            failures.append(f"正例(仅编译)应 rc=0，实得 errors={errs}")
        elif st.get("leg002_link_closure_hits") or st.get("leg002_install_face_hits"):
            failures.append(f"正例(仅编译)三面应为空，实得 {st}")

        pos_comment = base / "positive-comment-mention"
        _build_mini_repo(pos_comment, cmake_comment_mentions_orchestrator=True)
        errs, _ = check(pos_comment)
        if errs:
            failures.append(f"正例(注释提及)应 rc=0（词面臂已退役），实得 errors={errs}")

        cases = [
            ("leg003-caller-injected", dict(with_caller=True), "PipelineEngine run API has callers"),
            ("leg004-acr-symbol", dict(binary_symbol="acr_probe_symbol"), "生产二进制含 ACR 符号"),
            ("leg004-acr-option-missing", dict(drop_cmake_token=True), "ACR option 缺失"),
            ("leg004-acr-registration", dict(with_acr_reg=True), "ACR kernel 注册存在"),
            ("leg002-link-closure-injected", dict(orch_linked_into_astrocs=True),
             "进入 acsd 链接闭包"),
            ("leg002-install-rule-injected", dict(orch_install_rule=True),
             "进入安装/产品交付面"),
            ("leg002-product-manifest-injected", dict(orch_in_product_manifest=True),
             "进入安装/产品交付面"),
            ("leg002-public-api", dict(public_api_orchestrator=True), "PUBLIC_API orchestrator.exe"),
            ("failclosed-scan-empty",
             dict(drop_prod_sources=True, with_orchestrator=False),
             "生产源码扫描面为空"),
            ("failclosed-no-astrocs-target", dict(no_astrocs_target=True),
             "链接闭包判据面漂移"),
        ]
        for name, kwargs, expect in cases:
            d = base / name
            _build_mini_repo(d, **kwargs)
            errs, _ = check(d)
            if not errs:
                failures.append(f"负例 {name} 未变红（预期含『{expect}』）")
            elif not any(expect in e for e in errs):
                failures.append(f"负例 {name} 变红但未命中『{expect}』：{errs}")

        for name, drop in (("anchor-aio-header", ANCHORS["AIO_ENGINE_HEADER"]),
                           ("anchor-acr-source", ANCHORS["ACR_KERNEL_SOURCE"]),
                           ("anchor-root-cmake", ANCHORS["ROOT_CMAKE"]),
                           ("anchor-public-api", ANCHORS["PUBLIC_API_DOC"])):
            d = base / name
            _build_mini_repo(d, drop_anchor=drop)
            errs, _ = check(d)
            if not errs or not all(e.startswith("ANCHOR_STALE:") for e in errs):
                failures.append(f"锚失效负例 {name} 未产出 ANCHOR_STALE：{errs}")

    if failures:
        print("SELFTEST_FAIL:")
        for f in failures:
            print("  " + f)
        return 1
    print("SELFTEST_PASS: 正例 2 组（仅编译 / 注释提及 ⇒ 绿）；负例 10 组（caller / ACR 符号 / "
          "ACR option / ACR 注册 / 链接闭包注入 / install 规则注入 / 产品 manifest 注入 / "
          "PUBLIC_API / 空扫描面 / 缺 acsd target 判据面漂移）+ 4 个锚失效，均按预期变红")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="LEG-002..004 旧生产路径退出校验")
    ap.add_argument("--repo", default=None, help="仓库根（默认：本文件上一级）")
    ap.add_argument("--self-test", action="store_true", dest="self_test",
                    help="跑内置正例 + 注入负例，验证本检查器能红能绿")
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test()

    repo = pathlib.Path(args.repo).resolve() if args.repo else REPO
    stale = anchor_errors(repo)
    if stale:
        print("ANCHOR_STALE:")
        for e in stale:
            print("  " + e)
        return 2

    errors, stats = check(repo)
    if errors:
        print("LEGACY_EXIT_VIOLATION:")
        for e in errors:
            print("  " + e)
        return 1
    print("LEGACY_EXIT_PASS: 旧路径无生产符号/CMake/文档入口, 源码保留; ACR dormant 隔离"
          f" (binary={stats.get('binary')}, prod_sources={stats.get('prod_sources_scanned')})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
