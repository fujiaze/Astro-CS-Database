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
}
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
                rel = os.path.relpath(full, REPO)
                is_exempt = any(k in rel for k in EXEMPT)
                for ln, line in enumerate(open(full, encoding="utf-8", errors="replace"), 1):
                    if "/tests/" in rel.replace("\\", "/"):
                        continue
                    row_exempt = is_exempt or "watchdog" in line
                    for name, rx in PATTERNS.items():
                        if rx.search(line):
                            if name == "hardcoded_num_threads":
                                errors.append(f"{rel}:{ln}: {name} 字面量线程数禁止: {line.strip()[:70]}")
                            elif name == "omp_set_num_threads":
                                reg = next((v for k, v in REGISTERED.items() if k in rel), None)
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
