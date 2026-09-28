#!/usr/bin/env python3
"""ACSD CPU-005 provider 数值自测与路由表 — 验收运行器
eng/tests/cpu/dispatch/run_cpu005_route_checks.py

验收 (04_CPU_RESOURCE_TASKS.md CPU-005):
  1. 固定执行序 query→self_test→eligible→benchmark→select (cpu005_route_decision_test.cpp);
  2. 路由以 kernel_id 粒度 (同一 profile 各 kernel 独立选 provider, 无全局 preferred_isa);
  3. 伪造 profile (损坏 JSON / 字段类型篡改) → baseline 不抛异常;
  4. build/CPU/OS/benchmark hash 变化 → baseline@query;
  5. provider self_test hash 不符 (错误 hash) → baseline@self_test;
  6. 缺 benchmark (oracle:fail / 上游剔除) → baseline@benchmark;
  7. NaN/Inf/oracle-fail live rows → baseline (数值 mismatch);
  8. 低收益 (<3% 冻结门限) → baseline@select; 足够收益 → 选择;
  9. 路由表 JSON trace 显示每 kernel 实际 provider; 无全局 preferred_isa=avx512
     (源码静态 grep + 运行时断言)。

编译: 生产同源全量 TUs (cpu_routing 为本任务修改文件; 其余 b99fcd8 原样) +
       -Wall -Wextra -Wpedantic 严格零告警; 链接 -ldl -lpthread。
       -I 面 = 根 CMakeLists 里 astrocs_aio/astrocs_cpu 的 PUBLIC 面同集
       (lib/include + backend_host + crypto + third_party + aio/{include,src,
       third_party/cfitsio} + algorithms/shared); 完整性由 check_include_face()
       判据在编译前钉死, **不得靠 CPLUS_INCLUDE_PATH 之类调用环境兜**。
依赖: g++ (C++17), python3 标准库。
自检: python3 eng/tests/cpu/dispatch/run_cpu005_route_checks.py --self-test
       (正例: 真实声明面解析通过; 负例: 抽掉 aio 头面 / 未声明头的探针必须判红)
"""
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
TST = os.path.join(REPO, "eng", "tests", "cpu", "dispatch")
BH = os.path.join(REPO, "lib", "infrastructure", "benchmark", "backend_host")  # ROOT-008 迁移后路径
CRYPTO = os.path.join(REPO, "lib", "algorithms", "shared", "crypto")  # ROOT-008 迁移后路径
INC = os.path.join(REPO, "lib", "include")
TP = os.path.join(REPO, "lib", "third_party")
# ROOT-008 整合后 backend_host 的生产 TU 还会引 aio / algorithms-shared 的头
# （hardware_inspect.cpp → aio_atomic_file.h）。声明面必须与根 CMakeLists 的
# astrocs_aio/astrocs_cpu PUBLIC include 面同集，否则本 runner 在干净 checkout 上
# 首编译即失败——实测（2026-09-29）：缺 AIO_INCS 时 hardware_inspect.cpp 报
# "aio_atomic_file.h: 没有那个文件或目录"，此前是靠调用方 CPLUS_INCLUDE_PATH 兜过去的
# ——那是"环境变量兜编译面"的口子，已由 check_include_face() 判据钉死。
AIO = os.path.join(REPO, "lib", "infrastructure", "aio")
SHARED = os.path.join(REPO, "lib", "algorithms", "shared")
INCS = [
    INC,
    BH,
    CRYPTO,
    TP,
    os.path.join(AIO, "include"),
    os.path.join(AIO, "src"),
    os.path.join(AIO, "third_party", "cfitsio"),
    SHARED,
]

SRC = os.path.join(BH, "cpu_routing.cpp")       # 本任务改动
TEST_MAIN = os.path.join(TST, "cpu005_route_decision_test.cpp")
# 生产同源其余 TU (base b99fcd8 未改动; 全量链接证集成)
LIB_TUS = [
    "cpu_features.cpp", "hardware_inspect.cpp", "backend_loader.cpp",
    "baseline_backend.cpp", "profile_gen.cpp", "profile_gen_v2.cpp",
    "host_services.cpp", "bench_harness.cpp",
]
SHA_SRC = os.path.join(CRYPTO, "sha256.cpp")

FAILURES = []
COMPILE_STDERR = []


def log(msg):
    print(msg, flush=True)


def fail(msg):
    FAILURES.append(msg)
    log("FAIL: " + msg)


def run(cmd, cwd=REPO, timeout=300):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    log("$ " + " ".join(cmd) + f"\n  exit={r.returncode}")
    if r.stdout.strip():
        log("  stdout: " + r.stdout.strip()[-6000:])
    if r.stderr.strip():
        log("  stderr: " + r.stderr.strip()[-3000:])
    return r


def cc_cpp(src, out, extra=None):
    cmd = ["g++", "-std=c++17", "-O2", "-DNDEBUG", "-fPIC", "-Wall", "-Wextra",
           "-Wpedantic"] + [f"-I{i}" for i in INCS] + ["-c", src, "-o", out]
    if extra:
        cmd.extend(extra)
    r = run(cmd)
    if r.stderr:
        COMPILE_STDERR.append((os.path.basename(src), r.stderr))
    return r




# ── include 面完整性判据（唯一出处；缺项即判红） ─────────────────────────────
# 动机（2026-09-29 实测）：ROOT-008 目录整合后 backend_host 的生产 TU 新增了
# `#include "aio_atomic_file.h"` 一类依赖，而本 runner 的 -I 面未同步 ⇒ 干净
# checkout 上首编译即失败（hardware_inspect.cpp 报「aio_atomic_file.h: 没有那个
# 文件或目录」）；此前是靠调用方 `CPLUS_INCLUDE_PATH` 兜过去的，那是「编译面随
# 调用环境漂移」的口子。判据：从本 runner 编译的全部源文件出发，对**引号形式**的
# #include 求传递闭包（与 g++ 同一搜索规则：先所属文件目录，再按序 -I），凡
# 「仓库内确实存在该头文件、但当前声明面解析不到」者一律判红并点名。
QUOTED_INC = re.compile(r'^\s*#\s*include\s*"([^"]+)"')


def _repo_local_headers():
    """仓库 lib/** 下的头文件名 → 路径表（用于区分「缺 -I」与「外部/系统头」）。"""
    idx = {}
    for root, _dirs, files in os.walk(os.path.join(REPO, "lib")):
        for fn in files:
            if fn.endswith((".h", ".hpp", ".inc")):
                idx.setdefault(fn, []).append(os.path.join(root, fn))
    return idx


def check_include_face(incs, sources):
    """返回 (problems, skipped)。problems 非空 ⇒ 判红（缺 include 项）。"""
    repo_headers = _repo_local_headers()
    problems, skipped, seen, queue = [], [], set(), [os.path.abspath(s) for s in sources]
    while queue:
        path = queue.pop()
        if path in seen or not os.path.isfile(path):
            continue
        seen.add(path)
        base = os.path.dirname(path)
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            for ln, line in enumerate(fh, 1):
                m = QUOTED_INC.match(line)
                if not m:
                    continue
                name = m.group(1)
                hit = next((c for c in [os.path.join(base, name)] +
                            [os.path.join(i, name) for i in incs] if os.path.isfile(c)), None)
                if hit is None:
                    in_repo = repo_headers.get(os.path.basename(name), [])
                    if in_repo:
                        problems.append(
                            "%s:%d 引 \"%s\"：仓库内存在 %s，但当前 -I 面解析不到（缺 include 项）"
                            % (os.path.relpath(path, REPO), ln, name,
                               os.path.relpath(in_repo[0], REPO)))
                    else:
                        skipped.append("%s:%d \"%s\"（仓库内无此头, 交由系统搜索路径）"
                                       % (os.path.relpath(path, REPO), ln, name))
                else:
                    queue.append(os.path.abspath(hit))
    return problems, skipped


def all_sources():
    return [os.path.join(BH, tu) for tu in LIB_TUS] + [SRC, SHA_SRC, TEST_MAIN]


def self_test():
    """判据自检：真实声明面必须解析通过（正例）；抽掉一个 -I / 引入未声明头的
    探针必须判红（负例）——判据本身不得是恒真门。"""
    ok = True
    srcs = all_sources()
    problems, _ = check_include_face(INCS, srcs)
    if problems:
        log("SELF-TEST FAIL（正例）: 真实声明面被判不完整:\n  " + "\n  ".join(problems[:5]))
        ok = False
    # 负例1: 抽掉**全部 aio 头面**（aio_atomic_file.h 实际落在 aio/src）⇒ 必判红
    reduced = [i for i in INCS if not i.startswith(AIO + os.sep)]
    if not check_include_face(reduced, srcs)[0]:
        log("SELF-TEST FAIL（负例1）: 抽掉全部 aio 头面后仍判绿 ⇒ 判据恒真")
        ok = False
    probe_dir = tempfile.mkdtemp(prefix="cpu005_incface_")
    probe = os.path.join(probe_dir, "probe.cpp")
    with open(probe, "w", encoding="utf-8") as fh:
        fh.write('#include "aio_atomic_file.h"\nint probe_incface;\n')
    if not check_include_face([INC, BH], [probe])[0]:
        log("SELF-TEST FAIL（负例2）: 缺 -I 的探针未判红")
        ok = False
    log("INCLUDE-FACE SELF-TEST %s (声明面 %d 项; 正例源 %d 个; 负例 2 个)"
        % ("PASS" if ok else "FAIL", len(INCS), len(srcs)))
    return 0 if ok else 1


def main():
    tmp = tempfile.mkdtemp(prefix="cpu005_dispatch_")
    log(f"repo={REPO} tmp={tmp}")

    # 0) include 面完整性（先于编译：缺项即判红，不得靠调用环境兜）
    problems, skipped = check_include_face(INCS, all_sources())
    if skipped:
        log("  note: %d 条引号 include 不在仓库内（交由系统搜索路径）" % len(skipped))
    if problems:
        for p in problems:
            fail("include 面不完整: " + p)
        return 1

    # 1) 全量 TU 严格编译 (含本任务 cpu_routing.cpp 零告警证据)
    objs = []
    for tu in LIB_TUS:
        obj = os.path.join(tmp, os.path.splitext(tu)[0] + ".o")
        r = cc_cpp(os.path.join(BH, tu), obj)
        if r.returncode != 0:
            fail(f"生产 TU 编译失败: {tu}")
            return 1
        objs.append(obj)
    obj_route = os.path.join(tmp, "cpu_routing.o")
    r = cc_cpp(SRC, obj_route)
    if r.returncode != 0:
        fail("cpu_routing.cpp 编译 (本任务修改文件)")
        return 1
    objs.append(obj_route)
    obj_sha = os.path.join(tmp, "sha256.o")
    r = cc_cpp(SHA_SRC, obj_sha)
    if r.returncode != 0:
        fail("sha256.cpp 编译")
        return 1
    objs.append(obj_sha)
    # 严格告警检查: 任一 TU 编译 stderr 含 warning:/error: 即失败 (编译纪律)
    for srcname, errtext in COMPILE_STDERR:
        if "warning:" in errtext or "error:" in errtext:
            fail(f"严格编译存在 warning/error: {srcname}\n{errtext[-2000:]}")
            return 1

    # 2) 编译测试 main + 链接
    obj_main = os.path.join(tmp, "cpu005_route_decision_test.o")
    r = cc_cpp(TEST_MAIN, obj_main)
    if r.returncode != 0:
        fail("测试 main 编译")
        return 1
    exe = os.path.join(tmp, "cpu005_route_decision_test")
    r = run(["g++", "-std=c++17", "-O2", "-DNDEBUG",
             obj_main] + objs + ["-o", exe, "-ldl", "-lpthread"], timeout=300)
    if r.returncode != 0:
        fail("测试链接")
        return 1

    # 3) 运行验收矩阵
    r = run([exe], timeout=120)
    if r.returncode != 0:
        fail("route 决策测试二进制退出非 0")
        return 1
    if "CPU-005 ROUTE TESTS PASS" not in r.stdout:
        fail("route 决策测试未宣告 PASS")
        return 1
    # trace 表须显示实际 provider
    if "TABLE_BEGIN" not in r.stdout or '"hips-bulk-transform"' not in r.stdout:
        fail("路由表 trace 未打印")
    if '"provider": "avx512"' not in r.stdout:
        fail("trace 未显示 hips avx512 实际 provider")

    # 4) 静态断言: 无全局 preferred_isa=avx512 (源码面, 仅代码非注释行)
    #    扫描对象: 生产路由源码 + 测试 C++ (runner 自身代码不含该赋值, 无须自扫)
    scan = [os.path.join(BH, "cpu_routing.h"), os.path.join(BH, "cpu_routing.cpp"),
            TEST_MAIN]
    for f in scan:
        with open(f, "r", encoding="utf-8", errors="replace") as fh:
            for ln, line in enumerate(fh, 1):
                code = line.split("//", 1)[0].strip()
                if not code:
                    continue   # 纯注释/空行
                if re.search(r"preferred_isa\s*=\s*[\"']?avx512", code):
                    fail(f"{os.path.relpath(f, REPO)}:{ln}: 全局 preferred_isa=avx512 赋值")
    # 全 CPU provider/backend_host 面 grep (跨文件全局 ISA 声明禁止; 注释除外)
    r = run(["grep", "-rn", "preferred_isa", os.path.join(BH),
             os.path.join(REPO, "lib", "infrastructure", "benchmark", "cpu")], timeout=60)
    if r.returncode == 0 and r.stdout.strip():
        hits = [l for l in r.stdout.splitlines()
                if l.split("//", 1)[0].strip() and
                re.search(r"preferred_isa\s*=\s*[\"']?avx512", l.split("//", 1)[0])]
        if hits:
            fail("backend_host/providers 出现全局 preferred_isa 赋值:\n" + "\n".join(hits))
    # baseline_avx512 对照已由 CPU-002/003/004 oracle 覆盖; 此处查路由层无硬编码首选 ISA

    if FAILURES:
        log(f"\nCPU-005 CHECKS FAIL ({len(FAILURES)})")
        return 1
    log("\nCPU-005 CHECKS ALL PASS")
    return 0


if __name__ == "__main__":
    if "--self-test" in sys.argv[1:]:
        sys.exit(self_test())
    sys.exit(main())
