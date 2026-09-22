#!/usr/bin/env python3
"""ARCH-004 静态 checker: 未登记线程创建 / 硬编码线程数 / 私有线程池扫描 (§5 合同实现)。

RT-001 加固 (P0 M5a-G-005; 依据 ASTROCS_DESIGN.md §8 + ENGINEERING_SPEC.md §8/§10):
  1) 扫描面 = 生产源码面。原实现手抄 5 个根 [lib, providers, cli, runtime, include], 其中
     providers/ cli/ runtime/ include/ 4 个在 ARCH-001 根目录整合后已不存在（cli/ ->
     lib/infrastructure/cli, include/ -> lib/include），而 scan() 用 `if not os.path.isdir(root):
     continue` **静默**跳过 ⇒ 名义 5 根、实际只剩 1 根, PASS 文案照旧宣称"扫描面 = 5 根"。
     LINUXMAIN-PATH-01 B2 订正: 扫描面改由**权威来源派生 + fail-closed 锚存活**（见 SCAN_ROOTS
     与 assert_scan_face/graph_face_check）：① 生产根是单源常量, 任一不存在 ⇒ ANCHOR_STALE + exit 2;
     ② 根 CMakeLists.txt（BLD-002 唯一产品事实源）的 add_subdirectory 闭包目录必须落在扫描面内,
     否则 ANCHOR_STALE 点名 —— 这是"扫描面静默缩小"的机器判据;
     ③ REGISTERED 登记键必须落在扫描面内（否则登记形同虚设）。
  2) PATTERNS 增加 thread_pool (std::vector<std::thread>)：私有线程池声明必须显式登记
     （closing: 全仓 14 处池声明此前对正则不可见）。
  3) 取消行级 "watchdog" 豁免 —— 改为路径级登记（可审计、可计数）；
     （closing: 行内出现 watchdog 即整行放行的逃逸口已删除）。
  4) REGISTERED 的路径键必须命中当前树真实文件，悬空登记 = FAIL
     （closing: ARCH-001 迁移把 "lib/core" -> "lib/infrastructure/scheduler" 后，
     旧路径键静默失配、check 由绿转红却无人可见 —— 该失败模式由本项锁死）。
  5) 通过时打印扫描面与登记命中清单（非空转自证）。
"""
import os, re, subprocess, sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
ROOT_CMAKE = os.path.join(REPO, "CMakeLists.txt")   # BLD-002 唯一产品事实源

# ── 生产源码面（单源常量 + 锚存活）─────────────────────────────────────────────
# 权威来源: ENGINEERING_SPEC.md §7 目录规范「lib/ = 科学算法 + 公共头 + 第三方 + 基建」
#          + AGENTS.md §7 目录落位速查。
# ARCH-001 根目录整合的映射（LINUXMAIN-PATH-01 扫描清单 56/74/143/209 行）:
#   cli/     -> lib/infrastructure/cli
#   include/ -> lib/include
#   providers/ runtime/  从未在本树出现过（旧设计预留根, 已废弃）
# 每个根**必须存在**: 缺一个 ⇒ AnchorStale（exit 2），不再静默 continue。
SCAN_ROOTS = ["lib"]
# 构建图交叉校验的豁免前缀: eng/tests/** 是测试面, 本 checker 的职责只覆盖生产源码面
# （scan() 亦跳过 /tests/ 路径）。豁免是显式的并计入覆盖输出, 不是静默 continue。
GRAPH_FACE_EXEMPT_PREFIXES = ("eng/tests/",)
# 目录级跳过：测试/负例 fixture/归档/第三方均不属于生产源码面
SKIP_DIRS = {"archive", "third_party", "tests", "fixtures", "node_modules",
             "build", "out", ".git"}

ADD_SUBDIR_RE = re.compile(r"(?<![\w-])add_subdirectory\s*\(\s*([^\s)#]+)", re.I)


class AnchorStale(Exception):
    """扫描面锚失效 / 扫描面为空 —— fail-closed，exit 2（docs/ci/01_CHECKS.md §1）。"""


def scan_roots():
    """返回扫描面绝对路径列表；任一生产根不存在 ⇒ AnchorStale（点名）。"""
    if not os.path.isfile(ROOT_CMAKE):
        raise AnchorStale("ROOT_CMAKE %s 不存在（BLD-002 唯一产品事实源）" % ROOT_CMAKE)
    roots = []
    for rel in SCAN_ROOTS:
        p = os.path.join(REPO, rel)
        if not os.path.isdir(p):
            raise AnchorStale(
                "SCAN_ROOTS 锚 %s 不存在 —— 生产源码扫描面会静默缩小, fail-closed 拒绝"
                "（目录迁移后必须同步本清单）" % rel)
        roots.append(p)
    if not roots:
        raise AnchorStale("SCAN_ROOTS 为空 —— 扫描面无定义")
    return roots


def _strip_cmake_comments(text):
    return "\n".join(re.sub(r"#.*$", "", ln) for ln in text.splitlines())


def build_graph_dirs():
    """根 CMakeLists.txt 的 add_subdirectory 闭包目录（字面路径边）。

    返回 (目录集, 变量驱动的未解析边, 悬空边)。未解析/悬空边不静默丢弃 ——
    由 graph_face_check 计入覆盖输出（扫描面本身取自单源常量, 不依赖本函数）。
    """
    seen, unresolved, dangling = set(), [], []
    stack = [ROOT_CMAKE]
    while stack:
        f = stack.pop()
        if f in seen:
            continue
        seen.add(f)
        try:
            with open(f, encoding="utf-8", errors="replace") as fh:
                text = _strip_cmake_comments(fh.read())
        except OSError as exc:
            raise AnchorStale("构建图 CMake 不可读: %s (%s)" % (f, exc))
        for m in ADD_SUBDIR_RE.finditer(text):
            tok = m.group(1).strip('"')
            rel_cmake = os.path.relpath(f, REPO).replace("\\", "/")
            if "${" in tok or "$<" in tok:
                unresolved.append("%s -> %s" % (rel_cmake, tok))
                continue
            d = os.path.normpath(os.path.join(os.path.dirname(f), tok))
            if not os.path.isdir(d):
                dangling.append("%s -> %s" % (rel_cmake, tok))
                continue
            sub = os.path.join(d, "CMakeLists.txt")
            if os.path.isfile(sub):
                stack.append(sub)
    return seen, unresolved, dangling


def graph_face_check(roots):
    """构建图目录 ⊆ 扫描面 的机器判据（扫描面静默缩小的直接检出）。

    图内每个非测试目录都必须落在某个生产根之下；否则 AnchorStale 点名该目录。
    """
    _, unresolved, dangling = build_graph_dirs()
    graph_dirs = set()
    stack = [ROOT_CMAKE]
    seen = set()
    while stack:
        f = stack.pop()
        if f in seen:
            continue
        seen.add(f)
        with open(f, encoding="utf-8", errors="replace") as fh:
            text = _strip_cmake_comments(fh.read())
        for m in ADD_SUBDIR_RE.finditer(text):
            tok = m.group(1).strip('"')
            if "${" in tok or "$<" in tok:
                continue
            d = os.path.normpath(os.path.join(os.path.dirname(f), tok))
            if not os.path.isdir(d):
                continue
            rel = os.path.relpath(d, REPO).replace("\\", "/")
            graph_dirs.add(rel)
            sub = os.path.join(d, "CMakeLists.txt")
            if os.path.isfile(sub):
                stack.append(sub)
    roots_norm = [os.path.normpath(r) for r in roots]
    outside, exempt = [], []
    for rel in sorted(graph_dirs):
        if rel.startswith(GRAPH_FACE_EXEMPT_PREFIXES):
            exempt.append(rel)
            continue
        full = os.path.normpath(os.path.join(REPO, rel))
        if not any(full == r or full.startswith(r + os.sep) for r in roots_norm):
            outside.append(rel)
    if outside:
        raise AnchorStale(
            "构建图目录不在扫描面内（扫描面已静默缩小）: " + ", ".join(outside))
    return len(graph_dirs), exempt, unresolved, dangling

EXEMPT = {
    "watchdog", "resource_monitor", "logger",        # 后台服务豁免(文件名级)
    "orchestrator.h",                                 # watchdog 成员声明
    "nanoflann.hpp",                                  # vendored 第三方头(非自研合同)
}

# 显式登记(必须带注记): 预算注入形态 / 私有线程池 / 后台守护线程。
# 键 = 仓库相对 posix 路径（必须真实存在）；命中 = 子串匹配（兼容 Windows 反斜杠 relpath）。
REGISTERED = {
    # ── omp_set_num_threads：实参来自 host budget 租约（非编译期字面量）──
    "lib/algorithms/calibration/src/module_entry.cpp":
        "host budget 租约注入: 实参 leased <- executor->acquire(max_workers?:available_cpus, 被 config.max_workers 夹紧), 还原值 = omp_get_max_threads() 运行时值; 非编译期字面量; acquire 失败硬 BUDGET (ARCH-TB-001 实证)",
    "lib/algorithms/drizzle/src/module_entry.cpp":
        "host budget 租约注入: 实参 want <- executor->acquire(max_workers?:available_cpus, 被 config.max_workers 夹紧), 还原值 = omp_get_max_threads() 运行时值; 非编译期字面量 (ARCH-TB-001 实证)",
    "lib/algorithms/cosmetic/src/module_entry.cpp":
        "host budget 租约注入: 实参 leased <- executor->acquire(max_workers?:available_cpus, 被 config.max_workers 夹紧), 还原值 = omp_get_max_threads() 运行时值; 非编译期字面量; acquire 失败硬 BUDGET (ARCH-TB-001 实证)",
    "lib/algorithms/calibration/src/ac_api.cpp":
        "预算注入旧形态: n 来自调用方 set_num_threads(ARCH-003 host callback 取代); ac_api.cpp:130 进程级 omp_set_num_threads(n) 无保存/恢复(RT-001 W3-R2-004 未结)",
    "lib/infrastructure/aio/src/aio_pipeline_engine.cpp":
        "V5 迁移整改点: n_threads 改 host budget 注入(ABI-001 落地时收编)",
    "lib/infrastructure/scheduler/src/module_adapters.cpp":
        "ScopedOmpWorkerInjection(:240-:262) 保存/恢复型预算注入: 实参 workers/prev_ 均运行时值(prev_ = omp_get_max_threads()), 非编译期字面量; 本文件是全仓唯一线程预算注入实现"
        "; PERF-P1 p1_parallel_for 帧级并行 worker 内的 omp_set_num_threads(inner_omp): 内层并行度 = 剩余预算分配"
        "(n>=workers 时 1, 否则 workers/n), 由 Runtime lease 的 __workers 派生, 非编译期字面量; 目的是禁止"
        "帧级 x 帧内 N×N 超额订阅(drizzle_engine.cpp:1736 与 dpsf_psf.cpp 的并行区取 omp_get_max_threads,"
        "std::thread worker 的 ICV 是进程默认值)",
    # ── ACR：dormant 非生产路径 ──
    "lib/infrastructure/acr/examples/weighted_integration/weighted_integration_benchmark.cpp":
        "ACR benchmark 示范代码: 线程源=benchmark Env(e.openmp_threads, hardware_concurrency 探测), 非硬编码; dormant 非生产路径(V5 不接入)",
    "lib/infrastructure/acr/examples/weighted_integration/weighted_integration_kernels.cpp":
        "ACR benchmark 示范代码: 线程源=逐内核 benchmark 线程租借参数(threads>0 才设置), 非硬编码; dormant 非生产路径(V5 不接入)",
    "lib/infrastructure/acr/scheduler/dispatcher.cpp":
        "ACR dormant 调度器 per-call 池 x1 (:1174 workers): ACR 保留源码/不进生产构建与路由(ASTROCS_DESIGN §1.3/§8); 非 RT-001 修复对象",
    # ── 私有线程池声明（std::vector<std::thread>）──
    "lib/infrastructure/scheduler/src/executor.cpp":
        "唯一共享 executor 池 x2 (CPU heavy :39 + 有界 I/O :217), worker 数 = ThreadBudget.budget(); 设计允许的唯一长期池(ASTROCS_DESIGN §8 单一线程预算源)",
    "lib/infrastructure/scheduler/src/scheduler.cpp":
        "run() 期 per-run 池 x1 (budget_ 个线程): 线程数来自 ThreadBudget, 非硬编码; 但与 executor 池并存 -> 同 run 线程上界≈2×budget(RT-001 步骤2 单一 executor 未收编; W3-R2-008 佐证)",
    "lib/algorithms/coverage/src/sampler.cpp":
        "per-call 池 x1: workers = cfg.cpu_workers <- p2_session 传 budget.max_workers(租约), 调用内创建/join, 非长期池",
    "lib/algorithms/coverage/src/upm.cpp":
        "per-call 池 x5: cworkers = cfg.cpu_workers <- Runtime lease, 调用内创建/join, 非长期池",
    "lib/infrastructure/benchmark/cpu/baseline/src/baseline_provider.cpp":
        "provider per-call 池 x1: 线程数由调用方租借参数(n_threads)给出, 调用内创建/join",
    "lib/infrastructure/benchmark/cpu/avx2/src/avx2_provider.cpp":
        "provider per-call 池 x1: 同上(avx2 provider)",
    "lib/infrastructure/benchmark/cpu/avx512/src/avx512_provider.cpp":
        "provider per-call 池 x1: 同上(avx512 provider)",
    "lib/infrastructure/scheduler/src/normalize_workflow.cpp":
        "ARCH-502 normalize 异步工作流调度器 run() 作用域池 x2（RT-004-POOL-01）：帧 worker 池线程数 = cfg.workers、"
        "预取池线程数 = cfg.prefetch_threads（均由配置/预算注入，非编译期字面量）；两者均为 run() 内局部量，"
        "run 返回前全部 join 回收，无 detach/无常驻线程；池已由头文件成员移入 run() 局部量（头文件不再持有线程容器）",
    "lib/infrastructure/scheduler/src/mosaic_window.cpp":
        "ARCH-503 天区窗口调度器 run() 作用域池 x1（RT-004-POOL-01）：线程数 = cfg.workers（配置/预算注入），"
        "run() 内创建、run 返回前 join 回收，非长期池；头文件成员 pool_ 已删除",
    "lib/infrastructure/scheduler/src/export_stream.cpp":
        "ARCH-504 子块流式调度器 run() 作用域池 x1（RT-004-POOL-01）：线程数 = cfg.workers + 读/写各 1"
        "（配置/预算注入），run() 内创建、run 返回前 join 回收，非长期池；头文件成员 pool_ 已删除",
    "lib/include/astrocs/core/block_flow.h":
        "ARCH-505 命名块流执行器：不持有线程池（复用调用方注入的执行函数），登记为可见面",
    "lib/phase3_session/p3_session.cpp":
        "Session 期池 x1: ASTROCS_DESIGN §7.3 禁 Session 型模块 -> 该目录为 INT-001 删除对象(ARCH-001 DEFERRED)",
    # ── 独立 Oracle / 自查 harness（非生产路径）──
    "lib/algorithms/integration/v6/oracle/weight_chain_selfcheck.cpp":
        "权重链独立合成 Oracle 的 1/N worker 逐位一致自查(ENGINEERING_SPEC §9 自查自修): "
        "线程数 = std::thread::hardware_concurrency() 夹紧 [2,8](非编译期字面量); 池在作用域内创建、"
        "作用域内 join 回收, 无 detach/无常驻线程 ⇒ per-call 池; 该文件**不在根构建图内**"
        "(oracle/CMakeLists.txt 独立构建、不注册 ctest、非生产路径, ninja -t targets 无该目标) ⇒ "
        "不占生产线程预算; 本登记仅使既有声明可见, **不放宽**生产源码面任何线程创建判据",
    # ── 后台守护/监控线程（非并行池）──
    "lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp":
        "watchdog 超时守护线程 x2 (:5167 声明 + :5174 启动), 单线程超时通道(19_runtime.md §4 取消/超时), 非并行池; 原行级 watchdog 豁免已删除, 改为本路径级登记",
    "lib/infrastructure/cli/commands.cpp":
        "legacy CLI 监控线程 x1 (:667): lib/infrastructure/cli/** 不在构建图内(ARCH-001 §5), 归 CLI-001; 登记为可见面, 非 RT-001 修复对象",
    "lib/infrastructure/cli/runtime_client.cpp":
        "legacy CLI 取消监听线程 x1 (:349): 同上(CLI-001 域)",
}

def _posix(rel):
    """相对路径归一为 posix 分隔符。

    hosted Windows 下 os.path.relpath 产生 lib\\calibration\\src\\... , 使含 "/" 的
    路径级键因子串匹配失配而误报"未登记"(HOSTFIX-23③ 同型根因)。归一只统一分隔符,
    不放宽任何判定。
    """
    return rel.replace("\\", "/")

def registered_annotation(rel):
    """返回 rel 命中的登记注记; 未登记 → None。两种路径分隔符等价。"""
    p = _posix(rel)
    return next((v for k, v in REGISTERED.items() if _posix(k) in p), None)

def validate_registry(roots=None):
    """登记核验: 含路径分隔符的登记键 ① 必须命中当前树真实文件 ② 必须落在扫描面内。

    ② 是 LINUXMAIN-PATH-01 B2 新增判据: 登记键若掉出扫描面, 该登记就形同虚设
    （文件已不被扫描, 登记却还在 ⇒ 覆盖假象）, 必须点名判红。
    """
    errors = []
    roots_norm = [os.path.normpath(r) for r in (roots if roots is not None else scan_roots())]
    for key in REGISTERED:
        if "/" not in key:
            continue
        full = os.path.normpath(os.path.join(REPO, key))
        if not os.path.isfile(full):
            errors.append("登记悬空(路径不存在): %s —— 迁移/重命名后必须同步登记键" % key)
            continue
        if not any(full == r or full.startswith(r + os.sep) for r in roots_norm):
            errors.append("登记键不在扫描面内: %s —— 扫描面已静默缩小或登记键写错" % key)
    return errors

PATTERNS = {
    "std::thread": re.compile(r"std::thread\s*\(|std::thread\s+\w+"),
    "std::async": re.compile(r"std::async\s*\("),
    "win_thread": re.compile(r"_beginthreadex|CreateThread\s*\("),
    "omp_set_num_threads": re.compile(r"omp_set_num_threads\s*\("),
    "hardcoded_num_threads": re.compile(r"num_threads\s*\(\s*\d+\s*\)"),
    "thread_pool": re.compile(r"std::vector\s*<\s*std::thread\s*>"),
}

def scan():
    """返回 (errors, registered, scanned_files)。扫描面前置锚存活断言（fail-closed）。"""
    errors, registered = [], []
    scanned = 0
    for root in scan_roots():
        for dirpath, dirs, files in os.walk(root):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
            for fn in files:
                if not fn.endswith((".cpp", ".c", ".h", ".hpp")):
                    continue
                full = os.path.join(dirpath, fn)
                rel = _posix(os.path.relpath(full, REPO))
                if "/tests/" in rel or rel.startswith("eng/tests/"):
                    continue
                scanned += 1
                is_exempt = any(_posix(k) in rel for k in EXEMPT)
                with open(full, encoding="utf-8", errors="replace") as fh:
                    lines = fh.readlines()
                for ln, line in enumerate(lines, 1):
                    # 注释行不构成线程创建（executor.cpp:12 文档注释提及 std::thread 被误报）
                    if line.lstrip().startswith("//") or line.lstrip().startswith("*"):
                        continue
                    for name, rx in PATTERNS.items():
                        if not rx.search(line):
                            continue
                        if name == "hardcoded_num_threads":
                            errors.append(f"{rel}:{ln}: {name} 字面量线程数禁止: {line.strip()[:70]}")
                        elif is_exempt:
                            registered.append(f"{rel}:{ln}: {name} [文件级豁免:{_exempt_key(rel)}]")
                        else:
                            reg = registered_annotation(rel)
                            if reg is None:
                                kind = "未登记私有线程池" if name == "thread_pool" else f"未登记线程创建 {name}"
                                errors.append(f"{rel}:{ln}: {kind}: {line.strip()[:70]}")
                            else:
                                registered.append(f"{rel}:{ln}: {name} [{reg}]")
    return errors, registered, scanned

def _exempt_key(rel):
    return next((k for k in EXEMPT if _posix(k) in rel), "?")

def selftest():
    """机器可执行正/负例面（ENGINEERING_SPEC §10「能红能绿」）。

    负例 ① 生产根缺失 ⇒ ANCHOR_STALE；② 登记键掉出扫描面 ⇒ 判红；
    ③ 未登记线程创建 ⇒ 判红。正例：干净夹具 ⇒ 绿。
    """
    import shutil, tempfile
    fails = []
    with tempfile.TemporaryDirectory() as tmp:
        root = os.path.join(tmp, "repo")
        os.makedirs(os.path.join(root, "lib", "mod", "src"))
        with open(os.path.join(root, "CMakeLists.txt"), "w", encoding="utf-8") as fh:
            fh.write("add_subdirectory(lib/mod)\n")
        with open(os.path.join(root, "lib", "mod", "CMakeLists.txt"), "w",
                  encoding="utf-8") as fh:
            fh.write("# mod\n")
        with open(os.path.join(root, "lib", "mod", "src", "clean.cpp"), "w",
                  encoding="utf-8") as fh:
            fh.write("int f() { return 0; }\n")
        saved = (REPO, SCAN_ROOTS, REGISTERED)
        try:
            globals()["REPO"] = root
            globals()["SCAN_ROOTS"] = ["lib"]
            globals()["REGISTERED"] = {}
            errs, _, scanned = scan()
            if errs or scanned != 1:
                fails.append("正例 干净夹具异常: errs=%s scanned=%s" % (errs, scanned))
            # 负例 ①: 生产根缺失
            globals()["SCAN_ROOTS"] = ["lib", "providers"]
            try:
                scan()
                fails.append("负例1 生产根缺失未抛 AnchorStale")
            except AnchorStale:
                pass
            globals()["SCAN_ROOTS"] = ["lib"]
            # 负例 ②: 登记键掉出扫描面（面内文件正常, 键指向面外真实文件）
            os.makedirs(os.path.join(root, "other"))
            with open(os.path.join(root, "other", "x.cpp"), "w", encoding="utf-8") as fh:
                fh.write("int g() { return 0; }\n")
            globals()["REGISTERED"] = {"other/x.cpp": "面外登记"}
            errs = validate_registry()
            if not any("不在扫描面内" in e for e in errs):
                fails.append("负例2 面外登记键未被检出: %s" % errs)
            # 负例 ③: 未登记线程创建
            globals()["REGISTERED"] = {}
            with open(os.path.join(root, "lib", "mod", "src", "bad.cpp"), "w",
                      encoding="utf-8") as fh:
                fh.write("void h() { std::thread t; }\n")
            errs, _, _ = scan()
            if not any("未登记" in e for e in errs):
                fails.append("负例3 未登记线程创建未被检出: %s" % errs)
        finally:
            globals()["REPO"], globals()["SCAN_ROOTS"], globals()["REGISTERED"] = saved
            globals()["ROOT_CMAKE"] = os.path.join(saved[0], "CMakeLists.txt")
    if fails:
        print("THREAD_BUDGET_SELFTEST_FAIL:")
        for x in fails:
            print("  " + x)
        return 1
    print("THREAD_BUDGET_SELFTEST_PASS: 干净夹具绿; 生产根缺失 / 面外登记键 / "
          "未登记线程创建 各自判红")
    return 0


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if "--self-test" in argv:
        return selftest()
    try:
        roots = scan_roots()
        n_graph, exempt, unresolved, dangling = graph_face_check(roots)
        errors, registered, scanned = scan()
        errors = errors + validate_registry(roots)
    except AnchorStale as exc:
        print("THREAD_BUDGET_CHECK_FAIL: ANCHOR_STALE %s" % exc)
        return 2
    if errors:
        print(f"THREAD_BUDGET_CHECK_FAIL ({len(errors)}):")
        for e in errors[:20]:
            print(" ", e)
        return 1
    print(f"THREAD_BUDGET_CHECK_PASS 扫描根={SCAN_ROOTS} 扫描文件数={scanned} "
          f"未登记线程创建=0 硬编码线程数=0 "
          f"登记键={len(REGISTERED)} 已登记命中={len(registered)}")
    print(f"  覆盖: 构建图目录={n_graph}（测试面豁免={len(exempt)}）"
          f" 构建图未解析边={len(unresolved)} 悬空边={len(dangling)}")
    for x in unresolved + dangling:
        print("  构建图注记 ", x)
    for r in registered:
        print("  ", r)
    return 0

if __name__ == "__main__":
    sys.exit(main())
