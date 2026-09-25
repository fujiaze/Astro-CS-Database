#!/usr/bin/env python3
"""ARCH-001: 生成 PRODUCTION_EXECUTION_INVENTORY.csv（生成器，可重跑）。

存在理由（独立审查一页纸 S1 第 22／29 条）
  * 第 22 条：本生成器曾被校验器跑到**被校对象**上（eng/tests/arch/test_inventory.py
    的 test_05 先读跟踪件字节、再在原地重跑生成器覆写该文件）⇒ 差异在第一跑即被抹掉、
    第二跑必然通过。故本生成器支持 --out：校验器写临时目录再比对，跟踪件保持只读。
  * 第 29 条：exe_target 面原先只扫 lib/ 与 eng/tools，生产入口 acsd（根 CMakeLists.txt）
    因此完全不在登记面，而「没登记」还被写成通过条件。故 exe 面补读**根构建图目标集**
    （eng/ci/cmake_graph.py，唯一实现）：登记集合必须包含构建产出的可执行目标集合。
    非根构建图的子项目目标按原 rg() 口径保留（登记面只增不减，分类与注记原样）。

口径
  - 只读源树；唯一写操作 = 写 --out（默认跟踪件）；
  - 确定性：同一棵树两次运行逐字节一致（幂等由 test_inventory.py::test_05 断言）；
  - 源文件读失败 fail-fast（宪章 §14.4），不得静默产出不完整清单；
  - 根构建图读取器缺失时**不静默**：打到 stderr（仅夹具树允许缺，真仓由测试判红）。
"""
import argparse, csv, fnmatch, importlib.util, json, os, pathlib, re, sys

REPO_DEFAULT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
_AP = argparse.ArgumentParser(description="ARCH-001 生产执行清单生成器（可重跑）")
_AP.add_argument("--repo", default=REPO_DEFAULT, help="仓库根（默认由脚本位置推导）")
_AP.add_argument("--out", default=None,
                 help="输出 CSV 路径（默认跟踪件 docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv）")
_ARGS = _AP.parse_args()
REPO = os.path.abspath(_ARGS.repo)
OUT = os.path.abspath(_ARGS.out) if _ARGS.out else os.path.join(
    REPO, "docs", "architecture", "PRODUCTION_EXECUTION_INVENTORY.csv")
COLS = ["category", "symbol", "location", "classification", "production_reachable",
        "phase", "thread_model", "evidence", "risk_note"]

def rg(pattern, roots, glob_="*.cpp", extra=None):
    """跨平台符号检索, 复刻原 grep -rEn <pattern> <roots> --include=<glob_> --include=*.c/.h/.hpp。
    输出与 grep 同为 '<相对路径>:<行号>:<行内容>'; 确定性排序(按 路径+行号), 与 OS walk 顺序无关,
    且每个模式只扫一遍树(调用方复用返回列表, 不得对每个匹配文件重扫)。
    排除 /archive/ /third_party/(原 grep 在结果中过滤, 此处直接在遍历时跳过等价)。"""
    inc = {glob_, "*.c", "*.h", "*.hpp"}
    rx = re.compile(pattern)
    out = []
    for root in roots:
        base = os.path.join(REPO, root)
        for dirpath, dirnames, filenames in os.walk(base):
            # 遍历顺序显式确定: 目录名排序 + 文件名排序, 不依赖 OS walk 顺序。
            dirnames[:] = sorted(d for d in dirnames
                                 if d not in ("archive", "third_party", ".git", "__pycache__"))
            for fn in sorted(filenames):
                if not any(fnmatch.fnmatch(fn, p) for p in inc):
                    continue
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, REPO).replace(os.sep, "/")
                if "/archive/" in rel or "/third_party/" in rel:
                    continue
                # 读失败必须 fail-fast (宪章 §14.4): 静默跳过会让清单缺行且无人知晓,
                # 而清单正是 §12.3 机器一致性检查的输入 —— 静默跳过等于静默放宽该门。
                with open(full, "r", encoding="utf-8", errors="replace") as f:
                    for ln, line in enumerate(f, 1):
                        content = line.rstrip("\n").rstrip("\r")
                        if rx.search(content):
                            out.append(f"{rel}:{ln}:{content}")
    out.sort(key=lambda l: (l.split(":", 1)[0], int(l.split(":", 1)[1].split(":", 1)[0])))
    return [l for l in out if l.strip()]

rows, notes = [], []
def add(cat, sym, loc, cls, reach, phase, tm, ev, risk=""):
    rows.append(dict(zip(COLS, [cat, sym, loc, cls, reach, phase, tm, ev, risk])))

# 0 真实构建图（唯一实现 eng/ci/cmake_graph.py；一页纸 S1 第 25/26/29 条共用）
def _load_graph_module():
    path = os.path.join(REPO, "eng", "ci", "cmake_graph.py")
    if not os.path.isfile(path):
        return None
    spec = importlib.util.spec_from_file_location("acsd_cmake_graph", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

GRAPH_MOD = _load_graph_module()
GRAPH = None
ENTRY = None
if GRAPH_MOD is not None:
    GRAPH = GRAPH_MOD.parse_cmake_graph(pathlib.Path(REPO))
    ENTRY = GRAPH_MOD.production_entry(pathlib.Path(REPO))
else:
    print("WARNING: 未找到 eng/ci/cmake_graph.py —— 根构建图 exe 面未登记（仅夹具树允许）",
          file=sys.stderr)

# 1 exe 目标(生产=acsd CLI 唯一; 其余标 test/tool)
exe = rg("add_executable", ["lib", "eng/tools"], "*.txt") + rg("add_executable", ["lib"], "*.cmake")
seen = set()
for line in exe:
    m = re.search(r"([^/]+/[^:]+):\d+:add_executable\(([\w\.\-]+)", line)
    if not m: continue
    tgt, name = m.group(1), m.group(2)
    if name in seen: continue
    seen.add(name)
    is_prod = name in ("acsd", "astrocs_cli", "astrocsCLI")
    add("exe_target", name, tgt, "production" if is_prod else ("test" if name.startswith("test_") else "tool"),
        "yes" if is_prod else "no", "-", "n/a(单exe策略)" if is_prod else "非发布目标", line.split(":",2)[0]+":"+line.split(":",2)[1].split(":")[0])

# 1b 根构建图目标集（一页纸 S1 第 29 条）：生产入口与全部构建产出的可执行目标必须
#    落在登记面里 —— 「登记集合 包含 构建产出的可执行目标集合」由
#    eng/tests/arch/test_inventory.py::test_04 断言（不是断言「production exe 数 == 0」）。
if GRAPH is not None:
    for _name in sorted(GRAPH_MOD.executable_targets(GRAPH)):
        if _name in seen:
            continue
        _info = GRAPH["targets"][_name]
        _is_prod = (_name == ENTRY)
        _loc = _info["file"]
        _is_test = (_name.startswith("test_") or _name.endswith("_test")
                    or "/tests/" in _loc or "/test/" in _loc)
        add("exe_target", _name, _loc,
            "production" if _is_prod else ("test" if _is_test else "tool"),
            "yes" if _is_prod else "no", "-",
            "n/a(单exe策略)" if _is_prod else "非发布目标",
            GRAPH_MOD.target_anchor(pathlib.Path(REPO), _name, _info),
            "发布面唯一入口（ARCHITECTURE 不变量 1 / 最高设计 §6.2）" if _is_prod else "非发布目标")
        seen.add(_name)

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
# 只扫一遍树再分组计数: 共享工作树下(p1hips/p1cal/... 等并行写者)重复全树重扫会使
# 计数与实际命中行取自不同时刻, 产生静默漂移。
lock_hits = rg(r"std::mutex|lock_guard|EnterCriticalSection", ["lib"])
lock_counts = {}
for l in lock_hits:
    k = l.split(":")[0]
    lock_counts[k] = lock_counts.get(k, 0) + 1
for f in sorted(lock_counts):
    add("lock", os.path.basename(f), f, "test" if "/tests/" in f else "production",
        "no" if "/tests/" in f else "yes", "Phase1/2", "mutex 保护日志/状态; 无内核内锁竞争",
        f"{f} ({lock_counts[f]} sites)")

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

# 7b 非生产可达面登记（CLEAN-401 B2 / CONFORM-SWEEP-4 P3X-06）
# 规则 7 是"符号命中 ⇒ production"的**模式级**声明；对只被测试目标编译的遗留实现
# 会过度声明（清单与事实不符）。登记项 = 已由机器判据核实"不在生产入口 acsd 的
# 传递闭包内"的文件：按既有列语义改标 classification=test / production_reachable=no
# （它仍被编译，但只在测试面），依据写进 risk_note。
# 反向存活（fail-closed）：该事实由 eng/ci/ledgers/spec_named_impl_gaps.json 的
# SNI-S4-P3X-06 台账承载 —— 一旦文件被接进生产闭包，该台账即 stale，
# CHK-SPEC-NAMED-IMPL-ON-PROD-PATH 判红，强制删掉本登记项（登记项不得成为免检区）。
PRODUCTION_UNREACHABLE = {
    "lib/phase3_session/p3_v6_export.cpp":
        "CLEAN-401 B2 / CONFORM-SWEEP-4 P3X-06：v6 导出实现未接入生产命名块管线；"
        "仅由 eng/tests/integration/v6_p3 的 v6_p3_export_test 编译；"
        "不在生产入口 acsd 的传递闭包内（判据 CHK-SPEC-NAMED-IMPL-ON-PROD-PATH E3）；"
        "待 CLEAN-401 退役处置",
}
for f in io_files:
    if f in PRODUCTION_UNREACHABLE:
        add("io_writer", os.path.basename(f), f, "test", "no", "Phase1/2/3",
            "aio 原子写(tmp+rename)契约见 IO_AND_ATOMICITY.md", f, PRODUCTION_UNREACHABLE[f])
    else:
        add("io_writer", os.path.basename(f), f, "production", "yes", "Phase1/2/3",
            "aio 原子写(tmp+rename)契约见 IO_AND_ATOMICITY.md", f)
# 锚存活（ENGINEERING_SPEC §8）：登记项不得静默失效。只在"登记文件所在目录存在"
# 时判（生成器也被 eng/tests/arch/test_inventory_generator.py 拷到临时树里跑，
# 临时树没有 lib/phase3_session/ —— 那是夹具不是真仓，不适用本锚）。
for _rel in PRODUCTION_UNREACHABLE:
    _p = os.path.join(REPO, _rel)
    if os.path.isdir(os.path.dirname(_p)):
        if not os.path.isfile(_p):
            raise SystemExit("PRODUCTION_UNREACHABLE 锚缺失(目录在/文件没了): %s" % _rel)
        if _rel not in io_files:
            raise SystemExit("PRODUCTION_UNREACHABLE 登记项已失效(不再命中 I/O 符号面): %s" % _rel)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", newline="", encoding="utf-8") as fh:
    w = csv.DictWriter(fh, fieldnames=COLS, lineterminator="\n")
    w.writeheader(); w.writerows(rows)
summary = {}
for r in rows: summary[r["category"]] = summary.get(r["category"], 0) + 1
print(json.dumps(summary, ensure_ascii=False), "total", len(rows))
