#!/usr/bin/env python3
"""ARCH-004 静态 checker: 未登记线程创建 / 硬编码线程数 / 私有线程池扫描 (§5 合同实现)。

RT-001 加固 (P0 M5a-G-005; 依据 ASTROCS_DESIGN.md §8 + ENGINEERING_SPEC.md §8/§10):
  1) 扫描面 = 生产源码面 lib/ providers/ lib/infrastructure/cli/ runtime/ lib/include/（eng/tests/fixtures/archive/
     third_party 除外）—— 不再只扫 lib/（closing: "扫描面仍只 lib/"）。
  2) PATTERNS 增加 thread_pool (std::vector<std::thread>)：私有线程池声明必须显式登记
     （closing: 全仓 14 处池声明此前对正则不可见）。
  3) 取消行级 "watchdog" 豁免 —— 改为路径级登记（可审计、可计数）；
     （closing: 行内出现 watchdog 即整行放行的逃逸口已删除）。
  4) REGISTERED 的路径键必须命中当前树真实文件，悬空登记 = FAIL
     （closing: ARCH-001 迁移把 "lib/core" -> "lib/infrastructure/scheduler" 后，
     旧路径键静默失配、check 由绿转红却无人可见 —— 该失败模式由本项锁死）。
  5) 通过时打印扫描面与登记命中清单（非空转自证）。
"""
import os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SCAN_ROOTS = [
    os.path.join(REPO, "lib"),
    os.path.join(REPO, "providers"),
    os.path.join(REPO, "cli"),
    os.path.join(REPO, "runtime"),
    os.path.join(REPO, "include"),
]
# 目录级跳过：测试/负例 fixture/归档/第三方均不属于生产源码面
SKIP_DIRS = {"archive", "third_party", "tests", "fixtures", "node_modules",
             "build", "out", ".git"}

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
        "ScopedOmpWorkerInjection(:240-:262) 保存/恢复型预算注入: 实参 workers/prev_ 均运行时值(prev_ = omp_get_max_threads()), 非编译期字面量; 本文件是全仓唯一线程预算注入实现",
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
        "ARCH-502 normalize 异步工作流调度器：预取专用线程 x1（prefetch_pool），线程数 = cfg.prefetch_threads"
        "（配置注入，默认 1 表示关闭预取；非编译期字面量）；生命周期限于 run()，与 worker 池分离",
    "lib/include/astrocs/core/normalize_workflow.h":
        "ARCH-502 同上的 worker 池声明（std::vector<std::thread> pool_）：线程数 = cfg.workers（ThreadBudget 注入），"
        "run() 内创建/join，非长期池",
    "lib/include/astrocs/core/mosaic_window.h":
        "ARCH-503 天区窗口调度器 worker 池 x1：线程数 = cfg.workers（ThreadBudget 注入），run() 内创建/join，非长期池",
    "lib/include/astrocs/core/export_stream.h":
        "ARCH-504 子块流式调度器 worker 池 x1：线程数 = cfg.workers（ThreadBudget 注入），run() 内创建/join，非长期池",
    "lib/include/astrocs/core/block_flow.h":
        "ARCH-505 命名块流执行器：不持有线程池（复用调用方注入的执行函数），登记为可见面",
    "lib/phase3_session/p3_session.cpp":
        "Session 期池 x1: ASTROCS_DESIGN §7.3 禁 Session 型模块 -> 该目录为 INT-001 删除对象(ARCH-001 DEFERRED)",
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

def validate_registry():
    """登记悬空核验: 含路径分隔符的登记键必须命中当前树真实文件。"""
    errors = []
    for key in REGISTERED:
        if "/" not in key:
            continue
        if not os.path.isfile(os.path.join(REPO, key)):
            errors.append("登记悬空(路径不存在): %s —— 迁移/重命名后必须同步登记键" % key)
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
    """返回 (errors, registered, scanned_files)。"""
    errors, registered = [], []
    scanned = 0
    for root in SCAN_ROOTS:
        if not os.path.isdir(root):
            continue
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

def main():
    errors, registered, scanned = scan()
    errors = errors + validate_registry()
    if errors:
        print(f"THREAD_BUDGET_CHECK_FAIL ({len(errors)}):")
        for e in errors[:20]:
            print(" ", e)
        return 1
    print(f"THREAD_BUDGET_CHECK_PASS 扫描文件数={scanned} 未登记线程创建=0 硬编码线程数=0 "
          f"登记键={len(REGISTERED)} 已登记命中={len(registered)}")
    for r in registered:
        print("  ", r)
    return 0

if __name__ == "__main__":
    sys.exit(main())
