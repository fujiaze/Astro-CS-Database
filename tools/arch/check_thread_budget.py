#!/usr/bin/env python3
"""ARCH-004 静态 checker: 未登记线程创建/硬编码线程数扫描 (§5 合同实现)。"""
import os, re, sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCAN_ROOTS = [os.path.join(REPO, "lib")]
EXEMPT = {
    "watchdog", "resource_monitor", "logger",        # 后台服务豁免(文件名级)
    "orchestrator.h",                                 # watchdog 成员声明
    "nanoflann.hpp",                                  # vendored 第三方头(非自研合同)
}
# 显式登记(必须带注记): 预算注入旧形态/迁移整改点, 否则 omp_set_num_threads 即 FAIL
REGISTERED = {
    "aio_pipeline_engine.cpp": "V5 迁移整改点: n_threads 改 host budget 注入(ABI-001 落地时收编)",
    "ac_api.cpp": "预算注入旧形态: n 来自 orchestrator set_num_threads(ARCH-003 host callback 取代)",
    "acr/examples": "ACR dormant 非生产路径(V5 不接入)",
    "baseline_backend.cpp": "ABI-003: per-call worker std::thread 由 host budget 租借驱动(ARCH-004 §1; 无持久池)",
    # HOSTFIX-23③ (run 34039050194, hosted Windows): weighted_integration
    # benchmark 两处 omp_set_num_threads —— 线程源 = benchmark 参数/lease,
    # 非硬编码字面量 (冻结约束 §5: 线程由逐内核 benchmark 选择, 这两处正是
    # 该机制的示范代码):
    #   weighted_integration_benchmark.cpp:228  omp_set_num_threads(e.openmp_threads)
    #     —— e.openmp_threads = std::thread::hardware_concurrency() 环境探测,
    #        benchmark 自描述 Env, 不是编译期字面量;
    #   weighted_integration_kernels.cpp:37    if (threads > 0) omp_set_num_threads(threads)
    #     —— threads 为调用方传入的逐内核 benchmark 线程租借, >0 才生效。
    # 登记键用文件名级(不含路径分隔符): hosted Windows 下 os.path.relpath
    # 产生反斜杠路径 lib\acr\examples\..., 目录级键 "acr/examples" 用
    # `k in rel` 子串匹配失配 → 托管报 "未登记"; 文件名键两种分隔符都命中。
    # 注: 文件级登记只豁免 omp_set_num_threads 的"未登记"项, hardcoded_num_threads
    # 字面量扫描独立生效, 本文件内再出现 omp_set_num_threads(<数字>) 仍 FAIL。
    "weighted_integration_benchmark.cpp": "ACR benchmark 示范代码: 线程源=benchmark Env(e.openmp_threads, hardware_concurrency 探测), 非硬编码; dormant 非生产路径(V5 不接入)",
    "weighted_integration_kernels.cpp": "ACR benchmark 示范代码: 线程源=逐内核 benchmark 线程租借参数(threads>0 才设置), 非硬编码; dormant 非生产路径(V5 不接入)",
    # ARCH-TB-001 (裁决 R-10 先实证后登记; 证据 run/arch_tb_001/logs/provenance_snippets.txt):
    # 三处均为 host budget 租约注入, 实参不是编译期字面量 ——
    #   calibration/src/module_entry.cpp:949  ac_set_num_threads((int)leased)
    #     leased <- ex->acquire(user_data, want); want = ex->max_workers ?:
    #     ex->available_cpus, 再被 config.max_workers 夹紧; 还原实参
    #     omp_prev = CAL_OMP_GET() = omp_get_max_threads() 运行时值(:1050);
    #     executor 缺失 / acquire 失败 → 硬 ACS_ERR_BUDGET, 不降级单线程。
    #   cosmetic/src/module_entry.cpp:767  同型 (还原 :785 / :826)。
    #   drizzle/src/module_entry.cpp:920   DRZ_OMP_SET((int)want), 还原 :932;
    #     acquire 失败 → 单线程降级 (drizzle 既有先例, BUDGET 缺额非致命)。
    # 三处均经 host executor 申请线程租约, 无私有长期线程池 (宪章 §10.4)。
    # 键为路径限定(非 basename): 不放行 hips/snr_estimator 等其它 module_entry.cpp。
    # 登记只豁免 omp_set_num_threads 的"未登记"项; hardcoded_num_threads 字面量
    # 扫描独立生效 (见 tests/arch/test_thread_budget.py::test_06), 本文件内出现
    # omp_set_num_threads(<数字>) 仍 FAIL。
    "lib/calibration/src/module_entry.cpp": "host budget 租约注入: 实参 leased <- executor->acquire(max_workers?:available_cpus, 被 config.max_workers 夹紧), 还原值 = omp_get_max_threads() 运行时值; 非编译期字面量; acquire 失败硬 BUDGET (ARCH-TB-001 实证)",
    "lib/drizzle/src/module_entry.cpp": "host budget 租约注入: 实参 want <- executor->acquire(max_workers?:available_cpus, 被 config.max_workers 夹紧)(:920), 还原值 = omp_get_max_threads() 运行时值(:932); 非编译期字面量 (ARCH-TB-001 实证)",
    "lib/cosmetic/src/module_entry.cpp": "host budget 租约注入: 实参 leased <- executor->acquire(max_workers?:available_cpus, 被 config.max_workers 夹紧), 还原值 = omp_get_max_threads() 运行时值; 非编译期字面量; acquire 失败硬 BUDGET (ARCH-TB-001 实证)",
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


PATTERNS = {
    "std::thread": re.compile(r"std::thread\s*\(|std::thread\s+\w+"),
    "std::async": re.compile(r"std::async\s*\("),
    "win_thread": re.compile(r"_beginthreadex|CreateThread\s*\("),
    "omp_set_num_threads": re.compile(r"omp_set_num_threads\s*\("),
    "hardcoded_num_threads": re.compile(r"num_threads\s*\(\s*\d+\s*\)"),
}

def scan():
    errors, registered = [], []
    for root in SCAN_ROOTS:
        for dirpath, dirs, files in os.walk(root):
            dirs[:] = [d for d in dirs if d not in ("archive", "third_party", "tests")]
            for fn in files:
                if not fn.endswith((".cpp", ".c", ".h", ".hpp")):
                    continue
                full = os.path.join(dirpath, fn)
                rel = _posix(os.path.relpath(full, REPO))
                is_exempt = any(_posix(k) in rel for k in EXEMPT)
                for ln, line in enumerate(open(full, encoding="utf-8", errors="replace"), 1):
                    if "/tests/" in rel:
                        continue
                    # 注释行不构成线程创建（executor.cpp:18 文档注释提及 std::thread 被误报）
                    if line.lstrip().startswith("//") or line.lstrip().startswith("*"):
                        continue
                    row_exempt = is_exempt or "watchdog" in line
                    for name, rx in PATTERNS.items():
                        if rx.search(line):
                            if name == "hardcoded_num_threads":
                                errors.append(f"{rel}:{ln}: {name} 字面量线程数禁止: {line.strip()[:70]}")
                            elif name == "omp_set_num_threads":
                                reg = registered_annotation(rel)
                                if reg is None:
                                    errors.append(f"{rel}:{ln}: omp_set_num_threads 未登记: {line.strip()[:70]}")
                                else:
                                    registered.append(f"{rel}:{ln}: {name} [{reg}]")
                            elif row_exempt:
                                registered.append(f"{rel}:{ln}: {name}")
                            else:
                                errors.append(f"{rel}:{ln}: 未登记线程创建 {name}: {line.strip()[:70]}")
    return errors, registered

def main():
    errors, registered = scan()
    if errors:
        print(f"THREAD_BUDGET_CHECK_FAIL ({len(errors)}):")
        for e in errors[:20]:
            print(" ", e)
        return 1
    print(f"THREAD_BUDGET_CHECK_PASS 未登记线程创建=0 硬编码线程数=0 豁免登记={len(registered)}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
