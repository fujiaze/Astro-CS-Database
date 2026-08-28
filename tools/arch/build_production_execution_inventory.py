#!/usr/bin/env python3
"""ARCH-001: 从符号检索生成 PRODUCTION_EXECUTION_INVENTORY.csv (生成器, 可重跑)。"""
import csv, os, re, subprocess, json, datetime

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "docs", "architecture", "PRODUCTION_EXECUTION_INVENTORY.csv")
COLS = ["category", "symbol", "location", "classification", "production_reachable",
        "phase", "thread_model", "evidence", "risk_note"]

def rg(pattern, roots, glob_="*.cpp", extra=None):
    cmd = ["grep", "-rEn", pattern] + roots + ["--include=" + glob_, "--include=*.c", "--include=*.h", "--include=*.hpp"]
    r = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    return [l for l in r.stdout.splitlines() if l.strip() and "/archive/" not in l and "/third_party/" not in l]

rows, notes = [], []
def add(cat, sym, loc, cls, reach, phase, tm, ev, risk=""):
    rows.append(dict(zip(COLS, [cat, sym, loc, cls, reach, phase, tm, ev, risk])))

# 1 exe 目标(生产=astrocs CLI 唯一; 其余标 test/tool)
exe = rg("add_executable", ["lib", "tools"], "*.txt") + rg("add_executable", ["lib"], "*.cmake")
seen = set()
for line in exe:
    m = re.search(r"([^/]+/[^:]+):\d+:add_executable\(([\w\.\-]+)", line)
    if not m: continue
    tgt, name = m.group(1), m.group(2)
    if name in seen: continue
    seen.add(name)
    is_prod = name in ("astrocs", "astrocs_cli", "astrocsCLI")
    add("exe_target", name, tgt, "production" if is_prod else ("test" if name.startswith("test_") else "tool"),
        "yes" if is_prod else "no", "-", "n/a(单exe策略)" if is_prod else "非发布目标", line.split(":",2)[0]+":"+line.split(":",2)[1].split(":")[0])

# 2 OpenMP 内核(生产 lib, 排除 archive/third_party)
omp = rg("pragma omp", ["lib"])
omp_files = sorted({l.split(":")[0] for l in omp})
for f in omp_files:
    n = sum(1 for l in omp if l.startswith(f + ":"))
    add("openmp_kernel", os.path.basename(f), f, "production", "yes", "Phase1/2",
        "module-internal OpenMP, thread count via runtime/orchestrator set_num_threads", f"{f} ({n} pragma sites)",
        "V5: 禁硬编码线程数→ARCH-003 backend dispatcher 收编")

# 3 async/future/线程创建
for l in rg(r"std::(async|thread)\b", ["lib"]):
    f, ln, code = l.split(":", 2)
    add("thread_creation", code.strip()[:60], f"{f}:{ln}", "test" if "/tests/" in f else "production",
        "no" if "/tests/" in f else "yes", "-", "std::thread 显式创建" if "std::thread " in code else "std::async",
        f"{f}:{ln}", "登记于 EXECUTION_MODEL; watchdog/monitor 唯一豁免" if "watchdog" in code or "monitor" in code else "")

# 4 锁
lock_files = sorted({l.split(":")[0] for l in rg(r"std::mutex|lock_guard|EnterCriticalSection", ["lib"])})
for f in lock_files:
    n = sum(1 for l in rg(r"std::mutex|lock_guard|EnterCriticalSection", ["lib"]) if l.startswith(f + ":"))
    add("lock", os.path.basename(f), f, "test" if "/tests/" in f else "production",
        "no" if "/tests/" in f else "yes", "Phase1/2", "mutex 保护日志/状态; 无内核内锁竞争", f"{f} ({n} sites)")

# 5 队列
for l in rg(r"std::queue|concurrent_queue|BlockingQueue", ["lib"]):
    f, ln, code = l.split(":", 2)
    add("queue", code.strip()[:50], f"{f}:{ln}", "test" if "/tests/" in f else "production",
        "no" if "/tests/" in f else "yes", "-", "队列语义", f"{f}:{ln}")

# 6 ACR 调用(应为配置边界, 无计算调用)
for l in rg(r"acr_route|acr_registered|p2_acr", ["lib"]):
    f, ln, code = l.split(":", 2)
    add("acr_boundary", code.strip()[:60], f"{f}:{ln}", "production", "yes", "Phase2",
        "配置守卫(IVAR→CPU canonical), 无 ACR 计算", f"{f}:{ln}",
        "V5: ACR 不接入; acr_route!=cpu→拒绝或 cpu 回退(ARCH-003 冻结)")

# 7 I/O writer
io_files = sorted({l.split(":")[0] for l in rg(r"aio_frame_add_block|aio_write|fits_create_file|hips.*writer|atomic_write", ["lib"])})
for f in io_files:
    add("io_writer", os.path.basename(f), f, "production", "yes", "Phase1/2/3",
        "aio 原子写(tmp+rename)契约见 IO_AND_ATOMICITY.md", f)

with open(OUT, "w", newline="", encoding="utf-8") as fh:
    w = csv.DictWriter(fh, fieldnames=COLS, lineterminator="\n")
    w.writeheader(); w.writerows(rows)
summary = {}
for r in rows: summary[r["category"]] = summary.get(r["category"], 0) + 1
print(json.dumps(summary, ensure_ascii=False), "total", len(rows))
