"""模块层状态卫生判据（静态源码扫描；不编译、不链接、不起子进程）。

用途
----
把三条**明文代码纪律**与一组**命名块合同条款**变成可复跑的机器判据：

| 判据 | 正本条款 |
|---|---|
| 模块不私藏大块数据长期副本 | `AGENTS.md:88`、`docs/engineering/contracts/PIPELINE_BLOCK.md:13` |
| 模块不私建线程池 | `AGENTS.md:89`、`docs/engineering/standards/CONCURRENCY.md:37` |
| 模块不直接退出进程 | `AGENTS.md:89`、`docs/engineering/standards/CODE.md:20` |
| 模块不读全局配置 | `AGENTS.md:89`、`AGENTS.md:91` |
| 降级必须显式 | `docs/engineering/contracts/PIPELINE_BLOCK.md:61` |
| provenance KV 随块流转 | `docs/engineering/contracts/PIPELINE_BLOCK.md:60` |
| 缓存必须有容量/身份/失效/线程模型 | `docs/engineering/standards/CODE.md:27` |
| thread-local 可复用 scratch | `docs/engineering/standards/CODE.md:35` |
| 句柄由调用方 close/free | `docs/engineering/contracts/OWNERSHIP_LIFETIME.md:9-11` |

手段只有一种：**读仓库源码文本**（`eng/tests/module/_source_scan.py`）。
本文件不启动产品可执行程序、不链接 `libacsd`、不跑端到端（AGENTS.md:8「测试集纪律」：
测试是工具不是门；本文件不产出流水线判决，与 `TEST.md:11-13` 一致）。

`Suite.Feature` 标识映射（命名口径见 `docs/engineering/testing/TEST.md:88`）
------------------------------------------------------------------
| 测试函数 | Suite.Feature |
|---|---|
| `test_ModuleStateHygiene_non_degenerate_scan_surface` | `ModuleStateHygiene.NonDegenerateScanSurface` |
| `test_ModuleStateHygiene_no_file_scope_frame_copy` | `ModuleStateHygiene.NoFileScopeFrameCopy` |
| `test_ModuleStateHygiene_no_private_thread_pool` | `ModuleStateHygiene.NoPrivateThreadPool` |
| `test_ModuleStateHygiene_no_direct_process_exit` | `ModuleStateHygiene.NoDirectProcessExit` |
| `test_ModuleStateHygiene_no_global_config_read` | `ModuleStateHygiene.NoGlobalConfigRead` |
| `test_ModuleStateHygiene_degradation_record_registry` | `ModuleStateHygiene.DegradationRecordRegistry` |
| `test_ModuleStateHygiene_degradable_block_source_resolvable` | `ModuleStateHygiene.DegradableBlockSourceResolvable` |
| `test_ModuleStateHygiene_provenance_kv_passthrough` | `ModuleStateHygiene.ProvenanceKvPassthrough` |
| `test_ModuleStateHygiene_neg_file_scope_frame_copy` | `ModuleStateHygiene.NegFileScopeFrameCopy` |
| `test_ModuleStateHygiene_neg_private_thread_pool` | `ModuleStateHygiene.NegPrivateThreadPool` |
| `test_ModuleStateHygiene_neg_direct_exit` | `ModuleStateHygiene.NegDirectExit` |
| `test_ModuleStateHygiene_neg_global_config_read` | `ModuleStateHygiene.NegGlobalConfigRead` |
| `test_ModuleStateHygiene_neg_empty_scan_surface` | `ModuleStateHygiene.NegEmptyScanSurface` |
| `test_ModuleStateHygiene_neg_provenance_drop` | `ModuleStateHygiene.NegProvenanceDrop` |

采集面
------
`lib/algorithms/**` 下 `.cpp/.c/.h/.hpp` ＋ `lib/infrastructure/scheduler/src/module_adapters.cpp`。
**已排除的采集面**（逐项理由，不堆叠）：

| 排除面 | 理由 | 现值（实测） |
|---|---|---|
| `lib/algorithms/**/tools/**` | 独立命令行小工具，自带 `--cpu-workers` 参数面，不在 20 个 op 入口链上 | 3 文件 |
| `lib/algorithms/**/third_party/**` | vendored 第三方源码 | 0 文件 |
| `nanoflann.hpp`（文件级） | `CODE.md:105`「vendored 第三方头…按其自身触发的诊断类别窄隔离」 | 1 文件 |
| `eng/`、`docs/`、`build/`、`out/`、`run/`、`实验/`、`artifacts/`、`testdata/` | 非生产源码面 | 不在 `lib/algorithms` 树内 |
| 头文件中的**裸指针/裸数组** | 头文件顶格裸指针几乎全是函数形参（实测 119 → 收窄后 13） | 见下「已知漏报面」 |

实测采集面规模：192 个源文件、29 条命名空间/static 可变容器对象、13 条静态裸缓冲、
21 处字面量 env 读取 + 1 处旋钮式 env 读取（解析出 2 个变量名）。

已知假阳形态（已逐条消解或如实登记）
------------------------------------
1. **成员函数与 libc 同名**：`lib/algorithms/fits_output/p3_output.cpp:1061` 定义
   `void P3FitsStream::abort()`，同文件另有 12 处无限定 `abort()` 调用。
   消解：文件内存在 `X::abort()` 成员定义时，同名无限定调用按成员调用消解。
   **未消解前该判据会误报 13 条**（实测）。
2. **成员函数声明/定义行**：`lib/algorithms/fits_output/p3_output.h:149` 的 `void abort();`
   与 `module_adapters.cpp:15359` 的 `void abort() override {}` 形如调用但不是调用。
   消解：行首存在返回类型词时不算调用。
3. **同名局部变量遮蔽静态对象**：`lib/algorithms/psf/src/dpsf_psf.cpp:95` 有
   `static std::vector<DpsfDiagRec>* v`，而 `:506` 的 `double v = …image[y * width + x]`
   是**局部标量**。消解：本行重新声明同名局部对象时不计为对静态对象的写入。
4. **注释里的代码**：本仓注释密度极高（`module_adapters.cpp:2091` 的注释里就写着
   `std::thread worker`）。消解：扫描前把注释与字符串抹白成等长空白。
5. **字符串字面量里的代码**：日志/错误串里出现 `abort(` 会被当成进程退出调用。
   消解：判「是否代码」用全抹白版，取「字面量内容」用只抹注释版（两版配对使用）。
6. **预处理指令不带分号**：`#include <mutex>` 会把后续真实声明并进同一条语句。
   消解：行首 `#` 起整行丢弃。
7. **模板实参里的标识符**：`std::atomic<bool> g_enabled{false}` 若不抹实参，
   声明符会被误取成 `false`。消解：配平抹除 `<...>` 实参段后再取声明符。

已知漏报面（如实登记，不靠判据掩盖）
------------------------------------
- **无编译器前端**：不构造 AST。宏展开后出现的静态对象、模板偏特化定义体内部的静态对象扫不到。
- **头文件裸指针**：行级规则无法把头文件顶格的函数形参与命名域对象分开，头文件裸缓冲不进判据。
- **跨行声明起始行**：跨多行的静态声明，行号取声明首行（`module_adapters.cpp:308` 为例）。
- **`const` 判定是词法的**：`static const std::vector<std::string> keys = {...}` 不会被
  可变容器词表按「不可变」自动豁免，仍需进登记面并逐条写理由（避免把豁免做成静默放行）。

恒真判据自查（对应派单硬性纪律第 5 条）
----------------------------------------
逐条自问「把整个扫描面删空它还报绿吗」：

- A1/A2/A3/A4/A5a/A6 全部是**登记面双向比对**或**词表命中**判据，采集面为空时实算对象数为 0，
  触发 `VALIDATION_EVIDENCE.md:168-176` 的零对象守卫第 1 条（实算数 > 0）判红 ⇒ 非恒真。
- A5b（`optional` 块可解析）在本仓**预期判红**，因为两面机器源都没有 `optional` 字段，
  可降级块集合为空 ⇒ 依 `VALIDATION_EVIDENCE.md:412` fail-closed 判红，而不是恒真通过。
- A7 自身即零对象守卫，恒红不可能、恒绿不可能（空扫描面判红）。
- 唯一接近恒真的候选是 A6「KV 字面量出现即透传」：它只验**出现**不验**透传语义**。
  本文件在 A6 的 docstring 与交付报告里都点名为已知弱判据，不拿它单独支撑任何结论。

本仓**预期红项**（实现或机器源与正本冲突，如实登记，不放宽判据）
--------------------------------------------------------------
| 判据 | 红项 | 正本原文 | 代码证据 |
|---|---|---|---|
| A2 | **auto 缺省路径未施加配置上限**（显式配置路径存在且有效，不写成「模块不读配置」） | `CONCURRENCY.md:24`「线程数：外部可配置，默认 min(可用核, **配置上限**)；取值来源 = 配置」、`:37`「模块不硬编码 worker 数」 | `lib/algorithms/coverage/include/astro/phase2/execution_options.h:23-26`（`default_cpu_workers()` 返回裸 `hardware_concurrency()`，无上限；只在 `cpu_workers == 0` 分支 `:28-30` 生效）；经 `stage2_common.h:34` → `stage2_common.cpp:666-676`（JSON 缺键时保留缺省）→ `:737` → `upm.cpp:650/785/827/949`、`sampler.cpp:965` 进入生产 |
| A4 | env 值**直接决定写盘路径**且该路径不在端口注册表声明里（并列记 `AGENTS.md:91` 的反向读法） | `AGENTS.md:89`「模块不读全局配置、**不写未声明文件**」；反向读法 `AGENTS.md:91`「运行参数优先由 config 读取，其次从运行环境自动获取」 | `lib/algorithms/psf/src/dpsf_psf.cpp:101`（`DPSF_DIAG_PATH` → `:106-130` 落 CSV）、`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:113`（`ACSD_DRIZZLE_TRACE` → `drizzle_lineage.jsonl` / `leaf_internal.jsonl`） |
| A5b | `optional=true` 的块在两面机器源上不可解析（纯 fail-closed，无第二种读法） | `PIPELINE_BLOCK.md:34`「`optional` … true 时缺失**必须**走显式降级声明」、`:61` | `eng/contracts/block_flow/stage_block_flow.json` 的 block 字段只有 `stage/block/lifecycle/produced_by/consumed_by`，`lifecycle` 取值域为 `STAGE/EXTERNAL_IN/EXTERNAL_OUT/SHORT`，与合同冻结的 `short/frame/run` 不同域；`lib/infrastructure/pipeline/module_ports.registry.json` 全树无 `optional` 键 |

另有两项**登记项**（不判红，登记备查）：`ipv_entry.cpp:48-49` 的进程级句柄单例
`g_gaia_handle` / `g_detector_handle`（互斥量保护）与
`module_adapters.cpp:1829-1830`「持有句柄的节点…必须按 worker 分实例，不得跨 worker 共享」
存在张力，见 `RAW_STATIC_BUFFERS` 登记理由。
"""

from __future__ import annotations

import json
import os
import re
import shutil
import sys
import tempfile

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import _source_scan as scan  # noqa: E402  （同目录扫描器，需先补 sys.path）

# ─────────────────────────────────────────────────────────────────────────────
# 锚点（`VALIDATION_EVIDENCE.md:413`：锚失效以 `ANCHOR_STALE` 具名判红）
# ─────────────────────────────────────────────────────────────────────────────

ANCHORS = {
    "AGENTS": "AGENTS.md",
    "CONCURRENCY": "docs/engineering/standards/CONCURRENCY.md",
    "CODE": "docs/engineering/standards/CODE.md",
    "OWNERSHIP": "docs/engineering/contracts/OWNERSHIP_LIFETIME.md",
    "PIPELINE_BLOCK": "docs/engineering/contracts/PIPELINE_BLOCK.md",
    "TEST_STD": "docs/engineering/testing/TEST.md",
    "VALIDATION_EVIDENCE": "docs/engineering/testing/VALIDATION_EVIDENCE.md",
    "BLOCK_FLOW": "eng/contracts/block_flow/stage_block_flow.json",
    "PORTS_REGISTRY": "lib/infrastructure/pipeline/module_ports.registry.json",
    "ADAPTER": scan.ADAPTER_SOURCE,
}

#: 各锚里被本文件引用的条款原文片段。片段失配即判红：
#: 条款可能被上游反转（`VALIDATION_EVIDENCE.md:222`「引用任何条款前核对该文档的现行版本」）。
ANCHOR_CLAUSES = {
    ("AGENTS", 88): "不私藏大块数据长期副本",
    ("AGENTS", 89): "模块不读全局配置",
    ("AGENTS", 91): "运行参数优先由 config 读取",
    ("PIPELINE_BLOCK", 60): "随块流转，下游逐项透传",
    ("PIPELINE_BLOCK", 61): "降级必须",
    ("CONCURRENCY", 24): "取值来源 = 配置",
    ("CONCURRENCY", 37): "不私建长期线程池",
    ("CODE", 35): "thread-local 可复用 scratch",
    ("CODE", 27): "cache 必须有 capacity/identity/invalidation/thread model",
    ("OWNERSHIP", 9): "由调用方负责 close/free",
}


def require_root() -> str:
    """定位仓库根；锚缺失即具名判红，绝不静默回退（fail-closed）。"""
    try:
        root = scan.repo_root(os.path.dirname(os.path.abspath(__file__)))
    except scan.ScanSurfaceError as exc:
        pytest.fail(f"ANCHOR_STALE: {exc}")
    for name, rel in ANCHORS.items():
        if not os.path.isfile(os.path.join(root, rel.replace("/", os.sep))):
            pytest.fail(f"ANCHOR_STALE: 锚 {name} 缺失 {rel}")
    return root


def check_anchor_clauses(root: str) -> None:
    """核对被引用的条款原文仍在本仓现行版本的对应行上。"""
    cache: dict[str, list[str]] = {}
    bad: list[str] = []
    for (name, lineno), fragment in ANCHOR_CLAUSES.items():
        rel = ANCHORS[name]
        if rel not in cache:
            with open(os.path.join(root, rel.replace("/", os.sep)), encoding="utf-8") as fh:
                cache[rel] = fh.read().split("\n")
        lines = cache[rel]
        if lineno > len(lines) or fragment not in lines[lineno - 1]:
            actual = lines[lineno - 1].strip()[:70] if lineno <= len(lines) else "<越界>"
            bad.append(f"{rel}:{lineno} 期望含 {fragment!r}，实际 {actual!r}")
    if bad:
        pytest.fail("ANCHOR_STALE: 条款原文与引用不符\n  " + "\n  ".join(bad))


def read_surface(root: str) -> list[tuple[str, str]]:
    """产出 `(相对路径, 源码文本)` 列表；采集面解析失败即 fail-closed。"""
    try:
        return [(rel, scan.read_source(root, rel)) for rel in scan.iter_scan_surface(root)]
    except scan.ScanSurfaceError as exc:
        pytest.fail(f"SCAN_SURFACE_UNRESOLVABLE: {exc}")


# ─────────────────────────────────────────────────────────────────────────────
# 登记面 1：命名空间作用域 / static 存储期的可变容器（AGENTS.md:88）
# ─────────────────────────────────────────────────────────────────────────────

#: `(相对路径, 对象名, 允许存在的理由)`。**逐条写理由，无一条略过**。
MODULE_STATIC_OBJECTS = [
    (
        "lib/algorithms/calibration/cpp/cosmetic_corrector.cpp",
        "g_last_error",
        "21",
        """std::string 错误消息槽；承载的是错误文本不是科学量，容量与帧像素数无关（无尺寸表达式）。""",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "g_enabled",
        "82",
        """std::atomic<bool> 诊断闸门，标量；由 ACSD_DRIZZLE_TRACE 置位，run 末 reset() 复位。""",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "g_dir",
        "83",
        """std::string 诊断输出目录路径字符串；不是像素数据，run 末 clear()。""",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "g_mtx",
        "85",
        """std::mutex，保护上述诊断态；无数据。""",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "g_leaves",
        "87",
        """std::vector<LeafRec> 逐叶诊断记录；push_leaf 有 kMaxTraceLeaves=20000 硬上限，run 末清空。""",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "g_fallback_needed",
        "88",
        """std::atomic<bool> 回退标志，标量。""",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "s_target_cache_gen",
        "1692",
        """std::atomic<uint64_t> run 代际计数（B4-22）；每次 run 递增的标量，不存数据。""",
    ),
    (
        "lib/algorithms/noise_snr/cpp/src/noise_model.cpp",
        "g_model_floor_clamp",
        "36",
        """同上，值是钳位计数标量 int64_t，_free 时 erase。""",
    ),
    (
        "lib/algorithms/noise_snr/cpp/src/noise_model.cpp",
        "g_model_registry_mutex",
        "45",
        """std::mutex，保护上述两张注册表（PERF-P1 帧级并行下的必要同步）。""",
    ),
    (
        "lib/algorithms/photometry/cpp/src/psfsw.cpp",
        "aliases",
        "656",
        """`static const std::vector<std::string>` 退役权重来源别名拒绝表；同上，不可变字面量表。""",
    ),
    (
        "lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp",
        "m",
        "52",
        """函数局部 `static std::mutex`，保护同文件的句柄全局；无数据。""",
    ),
    (
        "lib/algorithms/psf/src/dpsf_log.cpp",
        "g_dpsf_log_mutex",
        "13",
        """std::mutex，日志串行化；无数据。""",
    ),
    (
        "lib/algorithms/psf/src/dpsf_psf.cpp",
        "seq",
        "702",
        """std::atomic<long long> 诊断记录序号，标量。""",
    ),
    (
        "lib/algorithms/resample/p3_rsmp_failclosed.cpp",
        "v",
        "17",
        """`static const std::vector<std::string>` 退役权重产品键拒绝表（不可变字面量表）；同上。""",
    ),
    (
        "lib/algorithms/star_detection/src/sdet_log.cpp",
        "g_sdet_log_mutex",
        "13",
        """std::mutex，日志串行化；无数据。""",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "mu",
        "305",
        """`static std::mutex`，保护 RT-001 唯一 executor 池注册表（:305）。无数据。""",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "seq",
        "1486",
        """`static std::atomic<uint64_t>` 暂存文件名序号（:1486，p1_staging_path）；标量。""",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "seq",
        "12858",
        """`static std::atomic<uint64_t>` 暂存文件名序号（:12858，p2_mosaic_staging_path）；标量。""",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "g_selected",
        "84",
        """std::unordered_set<uint64_t> 选择集；键是 y*1000000+x 的定点打包整数，**不是** w*h 下标；
容量上界 = 选择集行数，load_selection() 开头 clear()。""",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "g_sources",
        "86",
        """std::vector<SourceRec> 逐源诊断记录；run 末 clear_buffers() 清空并 shrink_to_fit()，
上界 = 选择集大小，**不随帧像素数增长**。""",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp",
        "box_cells",
        "1768",
        """`static thread_local` 候选盒 scratch —— CODE.md:35 明文允许「thread-local 可复用 scratch」；
写入 `resize(nx*ny)` 的 nx/ny 是 HEALPix 候选盒的行列跨度（:1769-1770 由 x1-x0+1 导出），
不是帧像素宽高；逐次调用 resize，不跨帧累积。见 PIXEL_DIM_MUTATION_ALLOWLIST。""",
    ),
    (
        "lib/algorithms/fits_output/p3_output.cpp",
        "g_last_err",
        "48",
        """std::string 错误文本槽（匿名命名域内）；非科学量、无像素尺寸来源。
登记时另记一条并发观察项：它是进程级可写 std::string。""",
    ),
    (
        "lib/algorithms/noise_snr/cpp/src/noise_model.cpp",
        "g_model_floor",
        "33",
        """std::unordered_map<const NoiseWeightModelV1*, double> 以**模型句柄指针**为键的方差下限注册表；
值是标量，容量 = 在飞模型数，`_free` 时 erase（:88-89）；与 OWNERSHIP_LIFETIME.md:9-11 的
句柄生命周期一致，不存像素。""",
    ),
    (
        "lib/algorithms/photometry/cpp/src/psfsw.cpp",
        "keys",
        "647",
        """`static const std::vector<std::string>` 退役权重产品键**拒绝表**（不可变字面量表）；
元素是键名不是像素，构造后只读。""",
    ),
    (
        "lib/algorithms/psf/src/dpsf_psf.cpp",
        "m",
        "93",
        """`static std::mutex* = new std::mutex()`；源码 :87-92 自述**故意不 delete**，
目的是让诊断 flush 活到 atexit。泄漏是显式登记的既定设计，非隐藏副本。""",
    ),
    (
        "lib/algorithms/psf/src/dpsf_psf.cpp",
        "v",
        "95",
        """`static std::vector<DpsfDiagRec>* = new …`；逐源诊断记录，flush 后 clear()+shrink_to_fit()；
元素是 PSF 拟合统计量不是像素。同上为显式登记的故意泄漏。""",
    ),
    (
        "lib/algorithms/psf/src/dpsf_psf.cpp",
        "p",
        "100",
        """`static const std::string*` 缓存 `DPSF_DIAG_PATH`；路径字符串不是像素数据。
该变量取值来自环境变量，冲突另在 ENV_READ_REGISTRY 登记。""",
    ),
    (
        "lib/algorithms/resample/p3_rsmp_failclosed.cpp",
        "v",
        "44",
        """`static const std::vector<std::string>` 退役权重来源 token 拒绝表（不可变字面量表）；
与 psfsw.cpp:keys 同族不同表，各函数内独立命名。""",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "registry",
        "308",
        """`static std::vector<std::pair<weak_ptr<ThreadBudget>, shared_ptr<CpuHeavyExecutor>>>`；
元素是**预算源句柄对**，容量 = 进程内不同的 ThreadBudget 数（:311-323 同一预算源复用同池，
预算消亡即 erase）；存的是池句柄不是像素数据。""",
    ),
]

#: 静态裸指针 / 裸数组（翻译单元内）。**这是「模块私藏整帧副本」最常见的藏法**：
#: `static float* g_frame;` 与 `static double g_cache[W*H];` 都不含容器类型名。
RAW_STATIC_BUFFERS = [
    (
        "lib/algorithms/drizzle/hips/src/module_entry.cpp",
        "hips_err_msg",
        "93",
        """`static thread_local char[1024]` 固定 1024 字节错误缓冲；编译期常量尺寸，无像素维度来源。""",
    ),
    (
        "lib/algorithms/drizzle/src/module_entry.cpp",
        "rev",
        "261",
        """`static int8_t[256]` 固定 256 字节查表缓冲；编译期常量尺寸。""",
    ),
    (
        "lib/algorithms/integration/phase1_product/src/phase1_product.cpp",
        "kRetiredCanonicalWeightObject",
        "78",
        """`const char*` 指向字符串**字面量**，不可变常量表项。""",
    ),
    (
        "lib/algorithms/integration/phase1_product/src/phase1_product.cpp",
        "kRetiredWeightObjectRejectReason",
        "80",
        """`const char*` 指向字符串**字面量**，不可变常量表项。""",
    ),
    (
        "lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp",
        "kRetiredWeightModeRejectReason",
        "204",
        """`const char*` 指向字符串**字面量**，不可变常量表项。""",
    ),
    (
        "lib/algorithms/noise_snr/src/module_entry.cpp",
        "noise_err_msg",
        "82",
        """`static thread_local char[1024]` 固定 1024 字节错误缓冲；编译期常量尺寸。""",
    ),
    (
        "lib/algorithms/photometry/cpp/src/psfsw.cpp",
        "kRetiredWeightModeToken",
        "675",
        """`const char*` 指向字符串**字面量**，不可变常量表项。""",
    ),
    (
        "lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp",
        "g_detector_handle",
        "49",
        """同 g_gaia_handle：进程级检测器句柄单例，互斥量保护（:67/:83），同一组条款张力。""",
    ),
    (
        "lib/algorithms/shared/crypto/sha256.cpp",
        "hex",
        "99",
        """`static const char*` 指向十六进制数字符串**字面量**，不可变常量。""",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "kP3BunitSurfaceBrightness",
        "8678",
        """`constexpr const char*` 指向字符串**字面量**，物理单位常量。""",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "kP3BunitSbVariance",
        "8679",
        """`constexpr const char*` 指向字符串**字面量**，物理单位常量。""",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "kP3BunitSbIvar",
        "8680",
        """`constexpr const char*` 指向字符串**字面量**，物理单位常量。""",
    ),
    (
        "lib/algorithms/platesolve/cpp/ipv/src/ipv_entry.cpp",
        "g_gaia_handle",
        "48",
        """**登记项（非像素副本，但有条款张力）**：进程级 Gaia 客户端句柄单例，互斥量保护（:62/:78）。
与 OWNERSHIP_LIFETIME.md:9-11「handle 由调用方 close/free」及 
module_adapters.cpp:1829-1830「持有句柄的节点必须按 worker 分实例，不得跨 worker 共享」
存在张力：句柄被存进模块文件作用域而不是由调用方持有。登记备查，不在本单判红。""",
    ),
]

#: 允许存在「像素维度尺寸写入」的静态对象，逐条写明维度来源**不是帧像素宽高**。
PIXEL_DIM_MUTATION_ALLOWLIST = {
    ("lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp", "box_cells"): (
        "spherical_overlap.cpp:1769-1770 声明 nx = x1-x0+1、ny = y1-y0+1，"
        "是 HEALPix 候选盒的行列跨度；:1773 的 resize(nx*ny) 是 thread-local scratch 的"
        "逐次 resize（CODE.md:35）。不随帧像素数增长。"
    ),
}

# ─────────────────────────────────────────────────────────────────────────────
# 登记面 2：环境变量读取（AGENTS.md:89 + AGENTS.md:91）
# ─────────────────────────────────────────────────────────────────────────────

#: 裁决词表。
ENV_OK_OBSERVABILITY = "OK_OBSERVABILITY"  # 观测开关，缺省关，不改调度与科学数值
ENV_OK_TUNABLE = "OK_RUN_PARAM"  # 运行参数，按 AGENTS.md:91「其次从运行环境自动获取」
ENV_TEST_HOOK = "OK_TEST_HOOK"  # 故障注入钩子，仅在环境变量显式设置时触发
ENV_CONFLICT = "CONFLICT"  # 与 AGENTS.md:89 冲突，判红

#: `(相对路径, 变量名, 期望出现次数, 裁决, 理由)`。
ENV_READ_REGISTRY = [
    (
        "lib/algorithms/calibration/src/calibration_covariance.cpp",
        "ACSD_V6_CAL_FAULT",
        1,
        ENV_TEST_HOOK,
        "故障注入钩子；未设置时正常路径零开销、零行为改变。",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp",
        "ACSD_DRZ_SB_FAULT",
        1,
        ENV_TEST_HOOK,
        "面亮度 sink 的故障注入钩子。",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp",
        "ACSD_IVAR_FAULT",
        1,
        ENV_TEST_HOOK,
        "逆方差 sink 的故障注入钩子。",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "ACSD_DRIZZLE_FINE_PROFILE",
        1,
        ENV_OK_OBSERVABILITY,
        "逐像素 profiler 闸门；源码 :386-391 自述「默认关闭…返回 false 时热循环完全不调用 clock」，"
        "只影响计时开销不改科学数值。",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "ACSD_DRIZZLE_TRACE",
        1,
        ENV_CONFLICT,
        "**冲突（两种读法并列，判红只落在后半句）**。"
        "读法甲（反向读法，可能成立）：`AGENTS.md:91`「运行参数优先由 config 读取，"
        "**其次从运行环境自动获取**」承认 env 是合法的第二取值通道 ⇒ 单看「读 env」不违规。"
        "读法乙（本判据采用，咬得住条款）：`AGENTS.md:89`「模块不读全局配置、"
        "**不写未声明文件**」——该 env 的值**直接决定诊断输出的目录**（:113 读取后经 g_dir 落 "
        "drizzle_lineage.jsonl / leaf_internal.jsonl），而该路径**不在 "
        "module_ports.registry.json 的任何端口声明里**（已逐条核对该注册表全树无此路径）。"
        "⇒ 冲突点是「env 值决定未声明写盘路径」，不是「用了 env」。判红。",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
        "ACSD_P1_AXIS_SCRATCH_CAP",
        1,
        ENV_OK_TUNABLE,
        "scratch 池容量旋钮；:1655-1663 自述「只决定同时在飞的 stripe 数，**不进数值路径**」，"
        "缺省 0 = 用策略值。属 AGENTS.md:91 允许的「从运行环境自动获取」。",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp",
        "ACSD_DRIZZLE_FINE_PROFILE",
        1,
        ENV_OK_OBSERVABILITY,
        "同上文件的同一个 profiler 闸门，缺省关闭。",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp",
        "ACSD_DRZ_P3_FAULT",
        1,
        ENV_TEST_HOOK,
        "P3 路径故障注入钩子。",
    ),
    (
        "lib/algorithms/drizzle/healpix_drizzle/spherical_overlap_science.cpp",
        "ACSD_DRZ_FAULT",
        1,
        ENV_TEST_HOOK,
        "重叠计算路径故障注入钩子。",
    ),
    (
        "lib/algorithms/drizzle/hips/src/aio_publish.cpp",
        "ACSD_HIPS_PUBLISH_FAULT",
        1,
        ENV_TEST_HOOK,
        "HiPS 发布路径故障注入钩子。",
    ),
    (
        "lib/algorithms/fits_output/p3_output.cpp",
        "ACSD_HASH_FAIL_INJECT",
        1,
        ENV_TEST_HOOK,
        "哈希失败注入；:228 命中即清空输出并返回失败，是 fail-closed 的测试入口。",
    ),
    (
        "lib/algorithms/psf/src/dpsf_log.cpp",
        "DYNAMIC_PSF_LOG_LEVEL",
        1,
        ENV_OK_OBSERVABILITY,
        "日志级别；只改日志详略，不改控制流与数值。",
    ),
    (
        "lib/algorithms/psf/src/dpsf_psf.cpp",
        "DPSF_DIAG_PATH",
        1,
        ENV_CONFLICT,
        "**冲突（两种读法并列，判红只落在后半句）**。"
        "读法甲（反向读法，可能成立）：`AGENTS.md:91` 允许运行参数「其次从运行环境自动获取」。"
        "读法乙（本判据采用）：`AGENTS.md:89` 的「不读全局配置、**不写未声明文件**」——"
        "该 env 的值**直接决定诊断 CSV 的写出路径**（:100-102 缓存 `DPSF_DIAG_PATH`，"
        ":106-130 经 `dpsf_diag_flush()` 落盘），该路径**不在 module_ports.registry.json "
        "的任何端口声明里**。⇒ 冲突点是「env 值决定未声明写盘路径」，不是「用了 env」。判红。",
    ),
    (
        "lib/algorithms/star_detection/src/sdet_log.cpp",
        "STAR_DETECTOR_LOG_LEVEL",
        1,
        ENV_OK_OBSERVABILITY,
        "日志级别；只改日志详略。",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "ACSD_LEASE_TRACE",
        1,
        ENV_OK_OBSERVABILITY,
        "租约观测开关，缺省关。",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "ACSD_NODE_TRACE",
        2,
        ENV_OK_OBSERVABILITY,
        "节点归属观测开关（:428、:13870），缺省关。",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "ACSD_P1CAP_TRACE",
        1,
        ENV_OK_OBSERVABILITY,
        "帧轴并行分配快照；:1831-1835 自述「仅观测，不改变调度、并行度与科学结果」。",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "ACSD_P3_EXPORT_FAULT",
        1,
        ENV_TEST_HOOK,
        "导出路径故障注入钩子。",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "ACSD_RT001_FAULT",
        1,
        ENV_TEST_HOOK,
        "RT-001 executor 故障注入钩子。",
    ),
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "ACSD_VARPLANE_FAULT",
        1,
        ENV_TEST_HOOK,
        "天光面故障注入钩子。",
    ),
]

#: 旋钮式 env 读取（`getenv(name)`，实参不在调用点字面量给出）。
ENV_DYNAMIC_REGISTRY = [
    (
        "lib/infrastructure/scheduler/src/module_adapters.cpp",
        "p1_axis_env_u32(name)",
        ("ACSD_P1_AXIS_FRAME_WORKERS", "ACSD_P1_AXIS_INNER_OMP"),
        ENV_OK_TUNABLE,
        "并行轴 A/B 旋钮；:1850-1851 自述「仅 P1-PARALLEL-AXIS-REDESIGN-01 受控 A/B 用；"
        "缺省 0 = 用策略值」，非法输入返回 0 不覆盖。并行度由 lease 决定，旋钮只收窄不放大。",
    ),
]

# ─────────────────────────────────────────────────────────────────────────────
# 登记面 3：线程派生（CONCURRENCY.md:35 / :37）
# ─────────────────────────────────────────────────────────────────────────────

#: 任务书要求逐条核实的 `p1_parallel_for` / `p2_parallel_for` 结论：
#: 它们是 Runtime lease 注入的并行轴，**不是**私建线程池。证据如下。

#: `(相对路径, 行号, 允许存在的理由)`，覆盖扫描面内全部 `std::thread` / `omp_set_num_threads`。
THREAD_SPAWN_ALLOWLIST = {
    ("lib/infrastructure/scheduler/src/module_adapters.cpp", 2099): (
        "p1_parallel_for 的帧轴 worker 池。线程数 = 参数 frame_w，其值来自 "
        "p1_frame_workers(doc)（:2347-2355）= min(p1_workers(doc) 的 Runtime lease, "
        "p1_memory_cap(doc) 的内存闸门)；**无硬编码**。作用域内 join，非长期池。"
        "合同依据 CONCURRENCY.md:35「只用显式并行区（OpenMP / 线程池）」。"
    ),
    ("lib/infrastructure/scheduler/src/module_adapters.cpp", 9273): (
        "p2_parallel_for 的 worker 池。线程数 = 参数 workers，由 P2 op 从 Runtime lease 传入；"
        "**无硬编码**。作用域内 join，非长期池。"
    ),
    ("lib/infrastructure/scheduler/src/module_adapters.cpp", 2104): (
        "p1 worker 入口处按预算设置本线程的 OpenMP ICV；omp_set_num_threads 只改本线程 ICV"
        "（:2031 注释明写），析构时由 ScopedOmpWorkerInjection 还原（:385-389）。非线程池。"
    ),
    ("lib/infrastructure/scheduler/src/module_adapters.cpp", 380): (
        "ScopedOmpWorkerInjection 构造：workers 由 host budget 传入，析构（:387）还原。"
        ":374 注释明写「线程数仍唯一来自 host budget (宪章 §10.4), 无任何硬编码」。"
    ),
    ("lib/infrastructure/scheduler/src/module_adapters.cpp", 387): (
        "ScopedOmpWorkerInjection 析构：还原进入前的 ICV，不新建线程。"
    ),
    ("lib/algorithms/coverage/src/upm.cpp", 656): (
        "逐控制点求和并行。线程数 cworkers 来自 cfg.cpu_workers，:643-644 注释明写"
        "「并行 worker 数来自 Runtime lease(cfg.cpu_workers, p2_session 传 budget.max_workers)。"
        "无 hardware_concurrency」；:650 缺省回落 1（串行 reference）。作用域内 join。"
    ),
    ("lib/algorithms/coverage/src/upm.cpp", 787): (
        "逐 obs 权重计算并行；:783 注释「std::thread + lease worker」，worker 数同 :785 的 cfg.cpu_workers。"
    ),
    ("lib/algorithms/coverage/src/upm.cpp", 830): (
        "同上族的第三处并行段，worker 数同源于 cfg.cpu_workers（:827）。"
    ),
    ("lib/algorithms/coverage/src/upm.cpp", 952): (
        "同上族的第四处并行段，worker 数同源于 cfg.cpu_workers（:949）。"
    ),
    ("lib/algorithms/coverage/src/upm.cpp", 2201): (
        "dense tile 求值并行；:2194 注释明写「workers 由调用方传 lease, 无 OpenMP」，"
        ":2195 `const int nw = (workers > 0) ? workers : 1`。作用域内 join。"
    ),
    ("lib/algorithms/coverage/src/sampler.cpp", 972): (
        "生产默认 N-worker 并行；:961-963 注释明写「worker 数：来自 Runtime lease"
        "(p2_session 传 cfg.cpu_workers=budget.max_workers)。无 hardware_concurrency"
        "(模块不得自行开线程)」；:965 缺省回落 1。"
    ),
    ("lib/algorithms/calibration/src/ac_api.cpp", 175): (
        "公共 C API `ac_set_num_threads(int n)`：n 由**调用方**传入，无硬编码；"
        ":177 注释「线程数由 backend 管理」。"
    ),
    ("lib/algorithms/calibration/src/module_entry.cpp", 55): (
        "`CAL_OMP_SET` 宏定义；唯一调用点 :1082 传的是进入前的 omp_prev（还原用），非设并行度。"
    ),
    ("lib/algorithms/drizzle/src/module_entry.cpp", 49): (
        "`DRZ_OMP_SET` 宏定义；:926 传入 `ex->max_workers`（Runtime lease 租借值），"
        "且 :922 先 `acquire`、:936-937 释放、:938 还原 ICV。**lease 注入，非私建池**。"
    ),
    ("lib/algorithms/cosmetic/src/module_entry.cpp", 65): (
        "`COS_OMP_SET` 宏定义；唯一调用点 :849 传的是 omp_prev（还原用）。"
    ),
}

#: 模块**自决**线程数的位置。契约：CONCURRENCY.md:24「线程数…取值来源 = 配置」、
#: :37「模块不硬编码 worker 数」。本仓存在 1 处，判红。
SELF_DECIDED_WORKERS_VERDICT = {
    "lib/algorithms/coverage/include/astro/phase2/execution_options.h": (
        "CONCURRENCY_CONFLICT: 冲突**只在 auto 缺省路径**（execution_options.h:28-30 的 "
        "cpu_workers == 0 分支），不在显式配置路径。default_cpu_workers()（:23-26）返回裸 "
        "std::thread::hardware_concurrency()，**不施加配置上限**；合同 CONCURRENCY.md:24 要求"
        "「默认 min(可用核, 配置上限)」。显式配置路径存在且有效（stage2_common.cpp:673-676 从 "
        "JSON execution.cpu_workers 取值），故本项不是「模块不读配置」，"
        "而是「auto 缺省路径未施加配置上限」，叠加 CONCURRENCY.md:37「模块不硬编码 worker 数」。"
        "生产可达链：stage2_common.h:34 把它设为 P2Stage2Config::exec 的缺省 → "
        "stage2_common.cpp:666-676（JSON 缺键时保留该缺省）→ :737 mcfg.cpu_workers → "
        "upm.cpp:650/785/827/949 与 sampler.cpp:965 作为 cworkers/workers 的取值。"
    ),
}

# ─────────────────────────────────────────────────────────────────────────────
# 登记面 4：降级记录键（PIPELINE_BLOCK.md:61）
# ─────────────────────────────────────────────────────────────────────────────

#: 合同只点名一个键 `degraded_reason`。代码里的形态与合同**不一致**，逐条登记差异。
DEGRADATION_KEY_REGISTRY = {
    "degraded_reason": "CONTRACT_FORM",
    "detection_degraded_reason": "VARIANT_PREFIXED",
    "snr_degraded_reason": "VARIANT_PREFIXED",
    "degraded_scalar": "VARIANT_NOT_A_REASON_KEY",
    "degraded": "VARIANT_NOT_A_REASON_KEY",
    "degrade": "VARIANT_NOT_A_REASON_KEY",
    "noise_mask_degraded": "VARIANT_NOT_A_REASON_KEY",
    "sky_plane_degraded": "VARIANT_NOT_A_REASON_KEY",
    "mask_degraded": "VARIANT_NOT_A_REASON_KEY",
}


# ─────────────────────────────────────────────────────────────────────────────
# 判据实现：以下函数就是「该判定自身的判定逻辑」。
# 负例注入只改扫描面（临时副本），不改这里；也不改 _source_scan.py（共用逻辑）。
# ─────────────────────────────────────────────────────────────────────────────


def _scan(root: str) -> tuple[list[tuple[str, str]], list]:
    surface = read_surface(root)
    objs = []
    for rel, raw in surface:
        objs.extend(scan.find_static_objects(rel, raw))
    return surface, objs


def eval_a1(root: str) -> list[str]:
    """A1 判定逻辑：模块不得私藏大块数据长期副本（AGENTS.md:88）。

    三条同时成立才算通过：
      (i)   采集面内的静态可变对象集合与登记面**双向**逐条相等（键 = `文件:符号:行`）；
      (ii)  任一登记对象的声明/初始化子句不得含像素维度来源；
      (iii) 任一登记对象的像素维度写入必须逐条在 PIXEL_DIM_MUTATION_ALLOWLIST 里。
    返回违规描述列表（空 = 通过）。

    键为什么用 `文件:符号:行` 而不是 `文件:符号`：本仓存在**同名但不同对象**的合法静态量
    ——`module_adapters.cpp` 的 `seq`（:1486 在 `p1_staging_path()`、:12858 在
    `p2_mosaic_staging_path()`，两个互不相干的函数局部 static）与
    `p3_rsmp_failclosed.cpp` 的 `v`（:17 与 :44，分属两个访问器函数的两张拒绝表）。
    把它们合并成一条登记会是对代码的错误陈述，违反 `AGENTS.md:6`「不盲从任何文档」；
    用 `(文件, 符号, 行)` 作键既如实登记两个对象，又比按符号去重**更严**——
    重复条目、对象移位、声明被删，三种漂移都会红。
    """
    viol: list[str] = []
    surface, objs = _scan(root)
    if not objs:
        return ["ZERO_OBJECTS: 实算静态对象数为 0（VALIDATION_EVIDENCE.md:171 第 1 条）"]

    found = {(o.path, o.name, str(o.line)) for o in objs}
    reg = {(p, n, ln) for p, n, ln, _r in MODULE_STATIC_OBJECTS}
    for extra in sorted(found - reg):
        viol.append(
            f"UNDECLARED_STATIC_OBJECT: {extra[0]}:{extra[2]} 的 {extra[1]} 未在 MODULE_STATIC_OBJECTS 登记"
        )
    for missing in sorted(reg - found):
        viol.append(
            f"STALE_REGISTRY_ENTRY: MODULE_STATIC_OBJECTS 登记了 {missing[0]}:{missing[2]} 的 {missing[1]}，"
            f"代码中已不存在"
        )

    raw_by_path = dict(surface)
    for obj in objs:
        dims = scan.pixel_sized_initialiser(obj.decl)
        if dims:
            viol.append(
                f"PIXEL_SIZED_STATIC_INITIALISER: {obj.locator} {obj.name} 的初始化含像素维度 {dims}"
            )
        for lineno, hit_dims, doms in scan.pixel_domain_mutations(raw_by_path[obj.path], obj):
            key = (obj.path, obj.name)
            if key in PIXEL_DIM_MUTATION_ALLOWLIST:
                continue
            viol.append(
                f"PIXEL_SIZED_STATIC_MUTATION: {obj.path}:{lineno} 对 {obj.name} 的写入含"
                f" 像素维度 {hit_dims} 像素域类型 {doms}，且不在 PIXEL_DIM_MUTATION_ALLOWLIST"
            )

    # 裸指针 / 裸数组
    raw_found: set[tuple[str, str, str]] = set()
    for rel, raw in surface:
        for lineno, name, _kind in scan.find_raw_buffers_tu(rel, raw):
            raw_found.add((rel, name, str(lineno)))
    raw_reg = {(p, n, ln) for p, n, ln, _r in RAW_STATIC_BUFFERS}
    for extra in sorted(raw_found - raw_reg):
        viol.append(
            f"UNDECLARED_RAW_STATIC_BUFFER: {extra[0]}:{extra[2]} 的 {extra[1]} 未在 RAW_STATIC_BUFFERS 登记"
        )
    for missing in sorted(raw_reg - raw_found):
        viol.append(
            f"STALE_RAW_REGISTRY_ENTRY: RAW_STATIC_BUFFERS 登记了 {missing[0]}:{missing[2]} 的 {missing[1]}，"
            f"代码中已不存在"
        )
    return viol


def eval_a2(root: str) -> list[str]:
    """A2 判定逻辑：模块不私建线程池、不自决 worker 数（CONCURRENCY.md:35/37）。

    返回违规描述列表（空 = 通过）。
    """
    viol: list[str] = []
    surface = read_surface(root)
    spawn: set[tuple[str, int]] = set()
    self_decided: dict[str, int] = {}
    for rel, raw in surface:
        for fact in scan.find_thread_facts(rel, raw):
            if fact.kind == "spawn":
                spawn.add((rel, fact.line))
            elif fact.kind == "self-decided-workers":
                self_decided[rel] = fact.line

    for extra in sorted(spawn - set(THREAD_SPAWN_ALLOWLIST)):
        viol.append(f"UNREGISTERED_THREAD_SPAWN: {extra[0]}:{extra[1]} 未在 THREAD_SPAWN_ALLOWLIST 登记理由")
    for missing in sorted(set(THREAD_SPAWN_ALLOWLIST) - spawn):
        viol.append(f"STALE_THREAD_ALLOWLIST: THREAD_SPAWN_ALLOWLIST 登记了 {missing[0]}:{missing[1]}，代码中已不存在")

    for rel, lineno in sorted(self_decided.items()):
        verdict = SELF_DECIDED_WORKERS_VERDICT.get(rel)
        if verdict is None:
            viol.append(f"UNREGISTERED_SELF_DECIDED_WORKERS: {rel}:{lineno} 模块自决线程数且未登记")
        else:
            viol.append(f"SELF_DECIDED_WORKERS: {rel}:{lineno} {verdict}")
    return viol


def eval_a3(root: str) -> list[str]:
    """A3 判定逻辑：模块不直接退出进程（AGENTS.md:89、CODE.md:20）。

    扫描器已消解「成员函数与 libc 同名」「声明/定义行」两类假阳；本判定只收剩下的。
    返回违规描述列表（空 = 通过）。
    """
    viol: list[str] = []
    surface = read_surface(root)
    total = 0
    for rel, raw in surface:
        for call in scan.find_exit_calls(rel, raw):
            total += 1
            if call.reason is None:
                viol.append(f"DIRECT_PROCESS_EXIT: {call.locator} {call.name}  ← {call.decl[:90]}")
    if total == 0:
        viol.append("ZERO_OBJECTS: 实算进程退出调用数为 0，采集面未被消费（VALIDATION_EVIDENCE.md:171）")
    return viol


def eval_a4(root: str) -> list[str]:
    """A4 判定逻辑：模块不读全局配置（AGENTS.md:89 + AGENTS.md:91）。

    两条同时成立才算通过：
      (i)   采集面内的 env 读取与登记面**双向**逐条相等（按 路径+变量名+出现次数）；
      (ii)  登记面内不得存在 `CONFLICT` 裁决的条目（冲突按 AGENTS.md:89 判红）。
    返回违规描述列表（空 = 通过）。
    """
    viol: list[str] = []
    surface = read_surface(root)
    found: dict[tuple[str, str], int] = {}
    dyn_found: dict[str, tuple[str, tuple[str, ...]]] = {}
    for rel, raw in surface:
        env = scan.find_env_reads(rel, raw)
        for read in env.literals:
            found[(rel, read.var)] = found.get((rel, read.var), 0) + 1
        for read in env.dynamic:
            dyn_found[rel] = (read.callee, read.known_args)
    if not found and not dyn_found:
        return ["ZERO_OBJECTS: 实算 env 读取数为 0（VALIDATION_EVIDENCE.md:171 第 1 条）"]

    reg = {(p, v): n for p, v, n, _w, _r in ENV_READ_REGISTRY}
    for extra in sorted(set(found) - set(reg)):
        viol.append(f"UNDECLARED_ENV_READ: {extra[0]} 读取 {extra[1]}（{found[extra]} 处）未在 ENV_READ_REGISTRY 登记")
    for missing in sorted(set(reg) - set(found)):
        viol.append(f"STALE_ENV_REGISTRY: ENV_READ_REGISTRY 登记了 {missing[0]} 的 {missing[1]}，代码中已不存在")
    for key, want in sorted(reg.items()):
        if key in found and found[key] != want:
            viol.append(f"ENV_READ_COUNT_DRIFT: {key[0]} 的 {key[1]} 期望 {want} 处，实测 {found[key]} 处")

    dyn_reg = {p: (c, a) for p, c, a, _w, _r in ENV_DYNAMIC_REGISTRY}
    for rel, got in sorted(dyn_found.items()):
        if rel not in dyn_reg:
            viol.append(f"UNDECLARED_DYNAMIC_ENV_READ: {rel} 的 {got[0]} 未在 ENV_DYNAMIC_REGISTRY 登记")
        elif dyn_reg[rel] != got:
            viol.append(f"DYNAMIC_ENV_DRIFT: {rel} 期望 {dyn_reg[rel]}，实测 {got}")
    for rel in sorted(set(dyn_reg) - set(dyn_found)):
        viol.append(f"STALE_DYNAMIC_ENV_REGISTRY: ENV_DYNAMIC_REGISTRY 登记了 {rel}，代码中已不存在")

    for rel, var, _n, verdict, _reason in ENV_READ_REGISTRY:
        if verdict == ENV_CONFLICT:
            viol.append(f"GLOBAL_CONFIG_CONFLICT: {rel} 的 {var} —— {_reason}")
    for rel, _callee, _args, verdict, reason in ENV_DYNAMIC_REGISTRY:
        if verdict == ENV_CONFLICT:
            viol.append(f"GLOBAL_CONFIG_CONFLICT: {rel} 的旋钮式 env 读取 —— {reason}")
    return viol


def eval_a5a(root: str) -> list[str]:
    """A5a 判定逻辑：降级记录必须**具名**且在册（PIPELINE_BLOCK.md:61）。

    采集面 = 模块适配层。命中的降级记录键集合与登记面双向相等即通过。
    返回违规描述列表（空 = 通过）。
    """
    viol: list[str] = []
    surface = read_surface(root)
    adapter = dict(surface).get(scan.ADAPTER_SOURCE)
    if adapter is None:
        return [f"ANCHOR_STALE: 采集面缺 {scan.ADAPTER_SOURCE}"]
    found = set(scan.find_literal_keys(scan.ADAPTER_SOURCE, adapter, scan.DEGRADATION_KEY_CANDIDATES))
    if not found:
        return ["ZERO_OBJECTS: 实算降级记录键数为 0（VALIDATION_EVIDENCE.md:171 第 1 条）"]
    reg = set(DEGRADATION_KEY_REGISTRY)
    for extra in sorted(found - reg):
        viol.append(f"UNDECLARED_DEGRADATION_KEY: {extra} 出现在模块适配层但未在 DEGRADATION_KEY_REGISTRY 登记")
    for missing in sorted(reg - found):
        viol.append(f"STALE_DEGRADATION_KEY: 登记的 {missing} 在模块适配层已不存在")
    return viol


def eval_a5b(root: str) -> list[str]:
    """A5b 判定逻辑：`optional=true` 的块必须可从机器源解析（PIPELINE_BLOCK.md:34/61）。

    合同点名 `optional` 字段与 `degraded_reason`，但两面机器源当前都没有 `optional` 键
    ⇒ 可降级块集合为空 ⇒ 依 VALIDATION_EVIDENCE.md:412 fail-closed 判红，**不按恒真通过**。
    返回违规描述列表（空 = 通过）。
    """
    viol: list[str] = []
    for rel in (ANCHORS["BLOCK_FLOW"], ANCHORS["PORTS_REGISTRY"]):
        path = os.path.join(root, rel.replace("/", os.sep))
        if not os.path.isfile(path):
            return [f"ANCHOR_STALE: 机器源缺失 {rel}"]
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
        if not re.search(r'"optional"\s*:', text):
            viol.append(
                f"DEGRADABLE_BLOCK_SOURCE_MISSING: {rel} 全树无 optional 字段，"
                f"合同 PIPELINE_BLOCK.md:34/61 点名的「optional=true 的块缺失须写 degraded_reason」"
                f"在机器源上不可解析（fail-closed，VALIDATION_EVIDENCE.md:412）"
            )
    return viol


def eval_a6(root: str) -> list[str]:
    """A6 判定逻辑：合同点名的 provenance 头部 KV 必须逐项出现在模块适配层。

    预期值来自 PIPELINE_BLOCK.md:60 的条款原文，不是当前代码的读数。
    返回违规描述列表（空 = 通过）。
    """
    viol: list[str] = []
    surface = read_surface(root)
    adapter = dict(surface).get(scan.ADAPTER_SOURCE)
    if adapter is None:
        return [f"ANCHOR_STALE: 采集面缺 {scan.ADAPTER_SOURCE}"]
    hits = scan.find_literal_keys(scan.ADAPTER_SOURCE, adapter, scan.CONTRACT_PROVENANCE_KV)
    missing = [k for k in scan.CONTRACT_PROVENANCE_KV if k not in hits]
    if len(hits) == 0:
        viol.append("ZERO_OBJECTS: 实算 provenance KV 命中数为 0（VALIDATION_EVIDENCE.md:171 第 1 条）")
    for key in missing:
        viol.append(f"PROVENANCE_KV_NOT_PASSTHROUGH: 合同 PIPELINE_BLOCK.md:60 点名的 {key} 未出现在 {scan.ADAPTER_SOURCE}")
    return viol


def eval_surface(root: str) -> tuple[int, int]:
    """A7 判定逻辑：零对象守卫的两条（VALIDATION_EVIDENCE.md:168-176）。

    返回 `(采集文件数, 采集面实算对象数)`。对象数 = 命名域/static 可变容器 ＋ 静态裸缓冲，
    两类都进 A1 的登记面比对，缺一类会让分母与实际消费数对不上。
    """
    surface = read_surface(root)
    objs = []
    raw_n = 0
    for rel, raw in surface:
        objs.extend(scan.find_static_objects(rel, raw))
        raw_n += len(scan.find_raw_buffers_tu(rel, raw))
    return len(surface), len(objs) + raw_n


# ─────────────────────────────────────────────────────────────────────────────
# 临时副本工具（VALIDATION_EVIDENCE.md:197「反例复跑必须隔离」）
# ─────────────────────────────────────────────────────────────────────────────


def make_temp_surface(mutate=None) -> str:
    """把采集面复制到临时目录并返回其根；绝不写仓库。

    `mutate(root)` 在副本上施加注入。副本保留仓库相对路径，并补齐
    `repo_root()` 所需的三个根锚（AGENTS.md / VERSION / 模块适配层）。
    """
    real = require_root()
    tmp = tempfile.mkdtemp(prefix="acsd_module_state_")
    for rel in scan.iter_scan_surface(real):
        dst = os.path.join(tmp, rel.replace("/", os.sep))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(os.path.join(real, rel.replace("/", os.sep)), dst)
    for anchor in ("AGENTS.md", "VERSION"):
        with open(os.path.join(tmp, anchor), "w", encoding="utf-8") as fh:
            fh.write("temporary copy for negative-case replay\n")
    if mutate is not None:
        mutate(tmp)
    return tmp


def append_to(path: str, text: str) -> None:
    """在副本里某个文件的末尾追加文本（真实形态的注入，不改动原仓库）。"""
    with open(path, "a", encoding="utf-8") as fh:
        fh.write(text)


def assert_new_violation_appeared(before: list[str], after: list[str], marker: str, neg_id: str) -> None:
    """负例的核心断言：**注入新增了违规项**，且新增项含指定标记。

    为什么不用「注入前必须绿」：本仓 A2 / A4 / A5b 有**如实登记的既存红项**
    （见文件头「本仓预期红项」表）。若要求注入前全绿，负例就会因为与本单无关的
    红项而恒红，反而失去判别力。判别力的正确形态是 S2「同时验证绿」+ S6
    「内容级负例」的合成：**同一判据在同一次复跑里，因为注入而多出新的红项**。
    """
    new = [v for v in after if v not in set(before)]
    assert new, (
        f"{neg_id} 无效：注入后判据没有新增任何违规项。"
        f" 注入前 {len(before)} 条，注入后 {len(after)} 条。"
        f" 注入前样例：{before[:3]}"
    )
    assert any(marker in v for v in new), f"{neg_id} 判红但未点名注入形态 {marker!r}：{new[:5]}"


# ═════════════════════════════════════════════════════════════════════════════
# A 组：正例
# ═════════════════════════════════════════════════════════════════════════════


def test_ModuleStateHygiene_non_degenerate_scan_surface():
    """A7 · Suite.Feature = ModuleStateHygiene.NonDegenerateScanSurface

    零对象守卫（`VALIDATION_EVIDENCE.md:168-176` 第 1、2 条）：声明非空且实算对象数 > 0；
    登记面声明的对象数与实际消费数一致。

    来源依据：`VALIDATION_EVIDENCE.md:171`「声明非空且实算对象数 > 0，否则红」、
    `:173`「声明的对象数必须与该判定实际消费的数一致」。
    """
    root = require_root()
    check_anchor_clauses(root)
    n_files, n_objs = eval_surface(root)

    assert n_files > 0, "ZERO_OBJECTS: 采集面文件数为 0"
    declared = len(MODULE_STATIC_OBJECTS) + len(RAW_STATIC_BUFFERS)
    assert n_objs > 0, "ZERO_OBJECTS: 实算静态对象数为 0"
    assert n_objs == declared, (
        f"OBJECT_COUNT_DRIFT: 实算静态对象 {n_objs} 条，登记面声明 {declared} 条"
        f"（VALIDATION_EVIDENCE.md:173 第 2 条）"
    )
    # 登记面自身的唯一性护栏：键 = `文件:符号:行`。同键重复即「用重复条目充数」，
    # 判红。**不得**把本断言删掉或放宽成 `>=`。
    static_keys = [(p, n, ln) for p, n, ln, _r in MODULE_STATIC_OBJECTS]
    assert len(set(static_keys)) == len(static_keys), (
        f"MODULE_STATIC_OBJECTS 有重复条目（同 文件:符号:行 出现多次）："
        f"{sorted(k for k in static_keys if static_keys.count(k) > 1)}"
    )
    raw_keys = [(p, n, ln) for p, n, ln, _r in RAW_STATIC_BUFFERS]
    assert len(set(raw_keys)) == len(raw_keys), (
        f"RAW_STATIC_BUFFERS 有重复条目（同 文件:符号:行 出现多次）："
        f"{sorted(k for k in raw_keys if raw_keys.count(k) > 1)}"
    )
    assert all(r.strip() for _p, _n, _l, r in MODULE_STATIC_OBJECTS), "登记面存在空理由条目（不得堆叠豁免）"
    assert all(r.strip() for _p, _n, _l, r in RAW_STATIC_BUFFERS), "登记面存在空理由条目（不得堆叠豁免）"


def test_ModuleStateHygiene_no_file_scope_frame_copy():
    """A1 · Suite.Feature = ModuleStateHygiene.NoFileScopeFrameCopy

    模块不得把整帧像素数据私藏成文件作用域 / 静态 / 长期成员。

    来源依据：`AGENTS.md:88`「模块经内存管线读块、写新块、消耗旧块，**不私藏大块数据长期副本**」；
    `docs/engineering/contracts/PIPELINE_BLOCK.md:13`「模块不私藏大块数据的长期副本」；
    `docs/engineering/standards/CODE.md:27`「cache 必须有 capacity/identity/invalidation/thread model」。

    本判据**不是**「扫不到就绿」：它要求采集面内每个静态可变容器与裸缓冲逐条登记，
    登记面多一条少一条都判红，且登记对象的尺寸来源与像素域写入逐条核。
    """
    root = require_root()
    check_anchor_clauses(root)
    viol = eval_a1(root)
    assert not viol, "A1 判红（模块状态卫生 / 大块数据长期副本）：\n  " + "\n  ".join(viol[:10])


def test_ModuleStateHygiene_no_private_thread_pool():
    """A2 · Suite.Feature = ModuleStateHygiene.NoPrivateThreadPool

    模块内不出现私建线程池，也不自行决定 worker 数。

    来源依据：`AGENTS.md:89`「模块不私建线程池」；
    `docs/engineering/standards/CONCURRENCY.md:37`「模块不硬编码 worker 数、不私建**长期**线程池」；
    `:35`「科学计算路径只用显式并行区（OpenMP / 线程池）」；
    `:24`「线程数：外部可配置，默认 min(可用核, 配置上限)；取值来源 = 配置」。

    任务书要求核实的结论已落在 `THREAD_SPAWN_ALLOWLIST`：
    `p1_parallel_for`（:2099）与 `p2_parallel_for`（:9273）的线程数来自 Runtime lease，
    **不是**硬编码；`coverage/src/upm.cpp` 与 `sampler.cpp` 的池同理（作用域内 join，非长期池）。

    本仓判红的 1 项：**auto 缺省路径未施加配置上限**——
    `lib/algorithms/coverage/include/astro/phase2/execution_options.h:23-26` 的
    `default_cpu_workers()` 返回裸 `std::thread::hardware_concurrency()`，
    而 `CONCURRENCY.md:24` 要求「默认 min(可用核, **配置上限**)」。
    ⚠️ 口径收窄：显式配置路径**存在且有效**（`execution_options.h:28-30` 取 `cpu_workers > 0`；
    `stage2_common.cpp:673-676` 从 JSON `execution.cpu_workers` 读），
    所以本项**不是**「模块不读配置」，只落在 `cpu_workers == 0` 的 auto 缺省分支上。
    """
    root = require_root()
    check_anchor_clauses(root)
    viol = eval_a2(root)
    assert not viol, "A2 判红（模块状态卫生 / 线程池）：\n  " + "\n  ".join(viol[:10])


def test_ModuleStateHygiene_no_direct_process_exit():
    """A3 · Suite.Feature = ModuleStateHygiene.NoDirectProcessExit

    模块代码不出现 `exit(` / `_exit(` / `_Exit(` / `abort()` / `quick_exit(` / `std::terminate`。

    来源依据：`AGENTS.md:89`「模块不直接退出进程」；
    交叉依据 `docs/engineering/standards/CODE.md:20`「异常边界 = C++ 侧；C ABI 失败时输出重置，
    单出口 cleanup/RAII」。

    本仓的 13 处 `abort()` 全部是 `P3FitsStream::abort()` 成员函数（定义在
    `lib/algorithms/fits_output/p3_output.cpp:1061`），由扫描器的同名消解规则剔除；
    成员声明/定义行 `void abort();` 亦不计。这两条假阳形态见文件头「已知假阳形态」1、2。
    """
    root = require_root()
    check_anchor_clauses(root)
    viol = eval_a3(root)
    assert not viol, "A3 判红（模块状态卫生 / 直接退出进程）：\n  " + "\n  ".join(viol[:10])


def test_ModuleStateHygiene_no_global_config_read():
    """A4 · Suite.Feature = ModuleStateHygiene.NoGlobalConfigRead

    模块读取环境变量必须逐条登记并按 AGENTS §7 裁决；取裁决为 CONFLICT 的判红。

    来源依据：`AGENTS.md:89`「模块不读全局配置」与 `AGENTS.md:91`「运行参数优先由 config 读取，
    **其次从运行环境自动获取**」。后者明确承认环境变量是合法的第二取值通道，
    因此本判据**不是**「出现 getenv 就红」——那会把观测开关与故障注入钩子一并误伤，
    属于「为了让测试绿而曲解条款」。判据是：**登记面双向完整 + 无 CONFLICT 条目**。

    本仓判红的 2 项，两种读法并列（详见 `ENV_READ_REGISTRY` 的逐条理由）：
    - `lib/algorithms/psf/src/dpsf_psf.cpp:101`（`DPSF_DIAG_PATH`）
    - `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:113`（`ACSD_DRIZZLE_TRACE`）

    判红点**收在后半句**：冲突不在「用了 env」（`AGENTS.md:91` 允许），
    而在这两个 env 的值**直接决定写盘路径**，且该路径**不在
    `lib/infrastructure/pipeline/module_ports.registry.json` 的任何端口声明里**
    ⇒ 撞 `AGENTS.md:89` 的「不写未声明文件」。
    零命中自证（`VALIDATION_EVIDENCE.md:218-223`）：同一检索式在已知应命中的
    `artifacts` 上有 86 处命中；`drizzle_lineage|leaf_internal|trace_selection|drizzle_trace|
    .csv|diag` 在该注册表 0 命中；换第二套检索式（`trace` / `DPSF` / `diag`）同样 0 命中；
    `trace` 在 `stage_block_flow.json` 亦 0 命中（该命令 exit=1，非工具不可用）。
    """
    root = require_root()
    check_anchor_clauses(root)
    viol = eval_a4(root)
    assert not viol, "A4 判红（模块状态卫生 / 全局配置读取）：\n  " + "\n  ".join(viol[:10])


def test_ModuleStateHygiene_degradation_record_registry():
    """A5a · Suite.Feature = ModuleStateHygiene.DegradationRecordRegistry

    `optional=true` 的块缺失时，消费方必须写**具名**降级记录；本判据核代码侧的具名记录面。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:61`「降级必须**显式**：
    `optional=true` 的块缺失时，消费方须写 `degraded_reason`」；`:34`「true 时缺失必须走
    **显式降级声明**（具名登记）」。

    已登记的形态不一致（不判红，登记备查）：合同只点名 `degraded_reason` 一个键，
    而代码里另有 8 种形态，其中 `degraded_scalar` / `degraded` / `degrade` /
    `noise_mask_degraded` / `sky_plane_degraded` / `mask_degraded` **不是**「reason 键」，
    是布尔/枚举式标记。按合同用语，这几项不构成 `degraded_reason` 的具名记录。
    """
    root = require_root()
    check_anchor_clauses(root)
    viol = eval_a5a(root)
    assert not viol, "A5a 判红（降级记录具名面）：\n  " + "\n  ".join(viol[:10])


def test_ModuleStateHygiene_degradable_block_source_resolvable():
    """A5b · Suite.Feature = ModuleStateHygiene.DegradableBlockSourceResolvable

    合同点名的 `optional=true` 块必须能从机器源解析出可降级块集合。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:34`（`optional` 是冻结字段）与
    `:61`（降级必须显式）；fail-closed 依据 `VALIDATION_EVIDENCE.md:412`
    「输入缺失…证据缺失或不可解析时判红；『文件不存在』按『有违规』处理」。

    **本仓预期判红**：`eng/contracts/block_flow/stage_block_flow.json` 的块记录只有
    `stage/block/lifecycle/produced_by/consumed_by`，`lifecycle` 取值域是
    `STAGE/EXTERNAL_IN/EXTERNAL_OUT/SHORT`，与合同冻结的 `short/frame/run` 不同域；
    `lib/infrastructure/pipeline/module_ports.registry.json` 全树无 `optional` 键。
    ⇒ 可降级块集合为空。这不是恒真通过，是 fail-closed 的红项，
    按 `AGENTS.md:89` 精神如实登记，不放宽判据、不 skip。
    """
    root = require_root()
    check_anchor_clauses(root)
    viol = eval_a5b(root)
    assert not viol, "A5b 判红（可降级块机器源不可解析）：\n  " + "\n  ".join(viol[:10])


def test_ModuleStateHygiene_provenance_kv_passthrough():
    """A6 · Suite.Feature = ModuleStateHygiene.ProvenanceKvPassthrough

    合同点名的 5 个 provenance 头部 KV 必须在模块适配层逐项出现（下游逐项透传）。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:60`「头部 KV（如 `frame_id`、
    `photometry_applied`、`k_photo`、`snr_path_effective`、`saturation_filter`）随块流转，
    下游逐项透传」。**预期集合来自条款本身，不是当前代码的读数**——
    扫描器里的 `CONTRACT_PROVENANCE_KV` 是照抄该行字面量，不随代码增删而变。

    **已知弱判据（如实登记）**：本判据只验「键名出现在模块适配层」，不验透传语义正确性
    （谁写、谁读、值是否逐项相同）。它是弱判据，不单独支撑任何结论；
    负例 B6 证明它不是恒真（删掉 `k_photo` 即判红）。按 `VALIDATION_EVIDENCE.md:441`
    「不得把恒真、空断言、不读真实对象的判据计入绿」，本条已在此声明其效力边界。
    """
    root = require_root()
    check_anchor_clauses(root)
    viol = eval_a6(root)
    assert not viol, "A6 判红（provenance KV 透传）：\n  " + "\n  ".join(viol[:10])


# ═════════════════════════════════════════════════════════════════════════════
# B 组：负例（注入后必须真变红；隔离在临时副本，绝不写仓库）
# ═════════════════════════════════════════════════════════════════════════════

INJECTED_FRAME_COPY = """

// ── 注入 B1：模块私藏整帧像素副本（AGENTS.md:88 违规形态）──
static std::vector<float> g_frame_cache;

static void injected_op_calibrate(const float* img, int img_w, int img_h) {
  g_frame_cache.assign((size_t)img_w * (size_t)img_h, 0.f);
  for (size_t i = 0; i < g_frame_cache.size(); ++i) g_frame_cache[i] = img[i];
}
"""

INJECTED_THREAD_POOL = """

// ── 注入 B2：模块私建线程池（AGENTS.md:89 / CONCURRENCY.md:37 违规形态）──
static void injected_op_mosaic(int n) {
  std::thread worker([] { volatile long long acc = 0; for (int i = 0; i < 1000; ++i) acc += i; });
  worker.join();
  (void)n;
}
"""

INJECTED_DIRECT_EXIT = """

// ── 注入 B3：模块直接退出进程（AGENTS.md:89 违规形态）──
static void injected_op_export_fail() {
  if (true) { std::_Exit(1); }
}
"""

INJECTED_GLOBAL_CFG = """

// ── 注入 B4：模块读全局配置取业务参数（AGENTS.md:89 违规形态）──
static int injected_op_calibrate_limit() {
  const char* v = std::getenv("ACSD_SOME_GLOBAL_CFG");
  return v ? std::atoi(v) : 8;
}
"""


def test_ModuleStateHygiene_neg_file_scope_frame_copy():
    """B1 · Suite.Feature = ModuleStateHygiene.NegFileScopeFrameCopy

    注入点 = **A1 判定自身的登记面比对与像素尺寸规则**（S6：不改共用逻辑 `_source_scan.py`）。
    注入物 = 任务书指定的真实违规形态：文件作用域 `static std::vector<float> g_frame_cache;`
    + 函数内 `g_frame_cache.assign((size_t)img_w * (size_t)img_h, 0.f);`。

    来源依据同 A1（`AGENTS.md:88`）。
    隔离依据：`VALIDATION_EVIDENCE.md:197`「反例复跑必须隔离：构造反例只允许在临时副本上改」。
    """
    root = require_root()
    check_anchor_clauses(root)
    before = eval_a1(root)

    def mutate(tmp: str) -> None:
        append_to(os.path.join(tmp, "lib/algorithms/noise_snr/src/module_entry.cpp"), INJECTED_FRAME_COPY)

    tmp = make_temp_surface(mutate)
    try:
        seen = scan.find_static_objects(
            "lib/algorithms/noise_snr/src/module_entry.cpp",
            scan.read_source(tmp, "lib/algorithms/noise_snr/src/module_entry.cpp"),
        )
        assert any(o.name == "g_frame_cache" for o in seen), "注入未生效：扫描面未读到 g_frame_cache"

        viol = eval_a1(tmp)
        assert_new_violation_appeared(before, viol, "g_frame_cache", "B1")
        # 三条判定规则中至少两条同时命中：登记面多出未登记对象 + 像素域写入
        assert any("UNDECLARED_STATIC_OBJECT" in v for v in viol), viol
        assert any("PIXEL_SIZED_STATIC_MUTATION" in v for v in viol), viol
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_ModuleStateHygiene_neg_private_thread_pool():
    """B2 · Suite.Feature = ModuleStateHygiene.NegPrivateThreadPool

    注入点 = **A2 判定自身的 `THREAD_SPAWN_ALLOWLIST` 比对**（S6）。
    注入物 = 函数内 `std::thread worker(...); worker.join();`。

    来源依据同 A2（`AGENTS.md:89`、`CONCURRENCY.md:37`）。
    """
    root = require_root()
    check_anchor_clauses(root)
    before = eval_a2(root)

    def mutate(tmp: str) -> None:
        append_to(os.path.join(tmp, "lib/algorithms/noise_snr/src/module_entry.cpp"), INJECTED_THREAD_POOL)

    tmp = make_temp_surface(mutate)
    try:
        viol = eval_a2(tmp)
        assert_new_violation_appeared(before, viol, "noise_snr/src/module_entry.cpp", "B2")
        assert any("UNREGISTERED_THREAD_SPAWN" in v for v in viol), viol
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_ModuleStateHygiene_neg_direct_exit():
    """B3 · Suite.Feature = ModuleStateHygiene.NegDirectExit

    注入点 = **A3 判定自身的同名消解规则**（S6：只换它，不换共用扫描器之外的逻辑）。
    注入物 = `std::_Exit(1);`。

    注意消解规则的边界：`abort` 有成员同名消解，`_Exit` 没有——本负例正是用来钉住
    「消解不得放宽成全体退出调用都放行」。若有人把消解规则扩到 `_Exit`，
    本负例会由红转绿并失败，这正是它的判别力。

    来源依据同 A3（`AGENTS.md:89`）。
    """
    root = require_root()
    check_anchor_clauses(root)
    before = eval_a3(root)

    def mutate(tmp: str) -> None:
        append_to(os.path.join(tmp, "lib/algorithms/noise_snr/src/module_entry.cpp"), INJECTED_DIRECT_EXIT)

    tmp = make_temp_surface(mutate)
    try:
        viol = eval_a3(tmp)
        assert_new_violation_appeared(before, viol, "_Exit", "B3")
        assert any("DIRECT_PROCESS_EXIT" in v for v in viol), viol
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_ModuleStateHygiene_neg_global_config_read():
    """B4 · Suite.Feature = ModuleStateHygiene.NegGlobalConfigRead

    注入点 = **A4 判定自身的 `ENV_READ_REGISTRY` 比对**（S6）。
    注入物 = 模块内 `std::getenv("ACSD_SOME_GLOBAL_CFG")` 取业务参数。

    来源依据同 A4（`AGENTS.md:89`）。
    """
    root = require_root()
    check_anchor_clauses(root)
    before = eval_a4(root)

    def mutate(tmp: str) -> None:
        append_to(os.path.join(tmp, "lib/algorithms/noise_snr/src/module_entry.cpp"), INJECTED_GLOBAL_CFG)

    tmp = make_temp_surface(mutate)
    try:
        viol = eval_a4(tmp)
        assert_new_violation_appeared(before, viol, "ACSD_SOME_GLOBAL_CFG", "B4")
        assert any("UNDECLARED_ENV_READ" in v for v in viol), viol
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_ModuleStateHygiene_neg_empty_scan_surface():
    """B5 · Suite.Feature = ModuleStateHygiene.NegEmptyScanSurface

    注入点 = **零对象守卫本身**（`VALIDATION_EVIDENCE.md:168-176` 第 1 条）。
    注入物 = 把采集面缩成一个**空目录** ＋ 一个 0 字节的模块适配层 ⇒ 实算对象数为 0。
    空集必须判红（`:176` 负例原文），不得按「没扫到东西=没问题」放行。

    来源依据：`VALIDATION_EVIDENCE.md:171`「声明非空且实算对象数 > 0，否则红」。
    """
    root = require_root()
    check_anchor_clauses(root)
    baseline = {name: eval_fn(root) for name, eval_fn in
                (("A1", eval_a1), ("A3", eval_a3), ("A4", eval_a4),
                 ("A5a", eval_a5a), ("A6", eval_a6))}

    tmp = tempfile.mkdtemp(prefix="acsd_module_state_empty_")
    try:
        os.makedirs(os.path.join(tmp, "lib", "algorithms"), exist_ok=True)
        os.makedirs(os.path.dirname(os.path.join(tmp, scan.ADAPTER_SOURCE)), exist_ok=True)
        for anchor in ("AGENTS.md", "VERSION"):
            with open(os.path.join(tmp, anchor), "w", encoding="utf-8") as fh:
                fh.write("empty scan surface\n")
        with open(os.path.join(tmp, scan.ADAPTER_SOURCE.replace("/", os.sep)), "w", encoding="utf-8") as fh:
            fh.write("")

        n_files, n_objs = eval_surface(tmp)
        assert n_objs == 0, f"B5 前置失败：空采集面实算对象数应为 0，实测 {n_objs}"

        for name, fn in (
            ("A1", eval_a1),
            ("A3", eval_a3),
            ("A4", eval_a4),
            ("A5a", eval_a5a),
            ("A6", eval_a6),
        ):
            viol = fn(tmp)
            assert viol, f"B5 无效：空扫描面下 {name} 仍报绿（空集必须判红）"
            assert any("ZERO_OBJECTS" in v or "ANCHOR_STALE" in v for v in viol), f"{name}: {viol[:3]}"
            assert_new_violation_appeared(baseline[name], viol, "ZERO_OBJECTS", f"B5/{name}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_ModuleStateHygiene_neg_provenance_drop():
    """B6 · Suite.Feature = ModuleStateHygiene.NegProvenanceDrop

    注入点 = **A6 判定自身的合同 KV 缺失比对**（S6）。
    注入物 = 从临时副本里删掉 `k_photo` 的透传字面量。

    来源依据：`docs/engineering/contracts/PIPELINE_BLOCK.md:60`（合同点名 `k_photo`）。

    断言里同时钉住对照：删 `k_photo` 报红、删 `saturation_filter` 也报红，
    证明判据按合同清单逐项生效，而不是「少一个就红」的粗糙比较。
    """
    root = require_root()
    check_anchor_clauses(root)
    before = eval_a6(root)

    def mutate(tmp: str) -> None:
        path = os.path.join(tmp, scan.ADAPTER_SOURCE.replace("/", os.sep))
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
        hits = [lineno for lineno, co, _fb in scan.iter_code_lines(text) if '"k_photo"' in co]
        assert hits, f"注入未生效：模块适配层里找不到 k_photo 字面量（实算 {len(hits)} 处）"
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(text.replace('"k_photo"', '"k_photo_RENAMED"'))

    tmp = make_temp_surface(mutate)
    try:
        viol = eval_a6(tmp)
        assert_new_violation_appeared(before, viol, "k_photo", "B6")
        assert any("k_photo" in v and "NOT_PASSTHROUGH" in v for v in viol), viol
        # 其余 4 项仍应命中（逐项生效，不是整体崩掉）
        assert not any("frame_id" in v for v in viol), viol
        assert not any("snr_path_effective" in v for v in viol), viol
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
