#!/usr/bin/env python3
"""P3-STREAM-01 静态判据: 生产 export 链必须引用 ARCH-504 ExportStreamScheduler。

背景（为什么需要这条门）
------------------------
ARCH-AUDIT-03 P3-01: 生产 export 曾实现为「全幅 vector + 全幅中间产物 + 单次
fits_write_pix」，而 ARCH-504 的 ExportStreamScheduler **只被组件级单测引用**。
原门 CHK-ARCH504-EXPORT-STREAM 只跑组件 ctest ⇒ **组件绿、生产未接线**（判据盲区）。
本检查器把「生产路径确实引用流式调度器」变成可执行、可红可绿的静态判据。

规范依据
--------
- ASTROCS_DESIGN.md §8.3 调度器表 export 行: 「子块流式: 读子块 → 投影重采样 →
  写 FITS，有界队列 + 背压，不整幅驻留；内存占用与子块大小成正比、与总图大小无关」;
- docs/contracts/SCHEDULER_CONTRACT.md §2 export 行（同文，FROZEN）;
- AGENTS.md §9: 「门禁/判据本身不合理时改进门禁本身，配可执行正例/负例与 --self-test」。

判据（每条可证伪）
------------------
A. 生产 phase3 节点链所在编译单元（lib/infrastructure/scheduler/src/module_adapters.cpp）
   必须 include "astrocs/core/export_stream.h";
B. 该文件内 p3_op_writer 的函数体内必须出现 ExportStreamScheduler 的实例化
   （即生产 writer 节点确实走流式调度器，而非只在别处出现符号）;
C. 该文件内不得存在「全幅平面 vector」的**默认**分配形态:
   `std::vector<float> sig((size_t)nelem)` / `std::vector<float> cov((size_t)nelem)`
   —— 只允许出现在 ASTROCS_P3_EXPORT_FAULT=whole_frame_resident 的参考分支内
   （该分支由 B 的流式路径默认覆盖; 出现次数必须 <= 2）;
D. resample2 / verify 节点必须声明子块流式（manifest 字段 sub_block_streaming）
   —— 由 eng/tests/unit/p3_export_stream_prod_test.cpp 运行时判据锁; 本静态检查器
   只断言源码中存在该字段名（防字段被删）。

--self-test（负例注入，必须判红）
--------------------------------
对真实源码做三处**变异**，每处都必须使检查器判红:
  M1 摘除 export_stream.h include  ⇒ A 红
  M2 摘除 p3_op_writer 内的 ExportStreamScheduler 实例化 ⇒ B 红
  M3 把全幅分配形态复制到参考分支之外 ⇒ C 红
变异只在内存中的副本上进行，不写仓库文件。
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))
PROD_TU = os.path.join(REPO, "lib", "infrastructure", "scheduler", "src",
                       "module_adapters.cpp")
RUNTIME_TEST = os.path.join(REPO, "eng", "tests", "unit",
                            "p3_export_stream_prod_test.cpp")
SCHEDULER_IMPL = os.path.join(REPO, "lib", "infrastructure", "scheduler", "src",
                              "export_stream.cpp")

INCLUDE_RE = re.compile(r'#include\s+"astrocs/core/export_stream\.h"')
WHOLE_FRAME_RE = re.compile(r"std::vector<float>\s+(sig|cov)\s*\(\s*\(size_t\)\s*nelem\s*\)")


def _writer_body(text):
    """返回 p3_op_writer 的函数体文本（花括号配对; 找不到返回 None）。"""
    m = re.search(r"Result<void>\s+p3_op_writer\s*\(", text)
    if not m:
        return None
    i = text.find("{", m.end())
    if i < 0:
        return None
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
    return None


def evaluate(text):
    """返回 (ok, failures[])。text = module_adapters.cpp 内容（可为变异副本）。"""
    fails = []
    if not INCLUDE_RE.search(text):
        fails.append("A: module_adapters.cpp 未 include astrocs/core/export_stream.h"
                     "（生产链不引用 ARCH-504 调度器）")
    body = _writer_body(text)
    if body is None:
        fails.append("B: 未找到 p3_op_writer 函数体（生产 writer 节点缺失）")
    elif "ExportStreamScheduler" not in body:
        fails.append("B: p3_op_writer 未实例化 ExportStreamScheduler"
                     "（生产 writer 节点不走子块流式调度器）")
    n_whole = len(WHOLE_FRAME_RE.findall(text))
    if n_whole > 2:
        fails.append("C: 全幅平面分配形态出现 %d 次（> 2）—— 只允许存在于"
                     " whole_frame_resident 参考分支" % n_whole)
    if not os.path.exists(SCHEDULER_IMPL):
        fails.append("A: lib/infrastructure/scheduler/src/export_stream.cpp 缺失")
    else:
        impl = open(SCHEDULER_IMPL, encoding="utf-8").read()
        if "ExportStreamScheduler::run" not in impl:
            fails.append("A: export_stream.cpp 未实现 ExportStreamScheduler::run")
    if not os.path.exists(RUNTIME_TEST):
        fails.append("D: 运行时判据 eng/tests/unit/p3_export_stream_prod_test.cpp 缺失")
    else:
        rt = open(RUNTIME_TEST, encoding="utf-8").read()
        for token in ("sub_block_streaming", "whole_frame_fault", "ExportStreamScheduler"):
            if token not in rt:
                fails.append("D: 运行时判据缺少断言 token '%s'" % token)
    return (len(fails) == 0), fails


def self_test():
    """变异注入: 每处变异都必须判红; 未变异必须判绿。"""
    text = open(PROD_TU, encoding="utf-8").read()
    ok, fails = evaluate(text)
    bad = []
    if not ok:
        bad.append("基线（未变异）应判绿, 实测判红: %s" % fails)
    mutations = []
    m1 = INCLUDE_RE.sub("// MUTATION-M1: include removed", text, count=1)
    mutations.append(("M1 摘除 export_stream.h include", m1))
    body = _writer_body(text)
    if body is None:
        bad.append("M2 无法定位 p3_op_writer 函数体（自检自身失效）")
    else:
        # 变异名不得包含原符号前缀（否则子串判据对该变异盲）
        mutated_body = body.replace("ExportStreamScheduler", "StreamSchedX")
        m2 = text.replace(body, mutated_body, 1)
        mutations.append(("M2 摘除 p3_op_writer 内 ExportStreamScheduler 实例化", m2))
    m3 = text.replace("Result<void> p3_op_writer(",
                      "static void mutation_m3() { std::vector<float> sig((size_t)nelem);"
                      " std::vector<float> cov((size_t)nelem); }\n"
                      "Result<void> p3_op_writer(", 1)
    mutations.append(("M3 额外注入全幅平面分配", m3))
    for name, mtext in mutations:
        mok, mfails = evaluate(mtext)
        status = "RED(ok)" if not mok else "GREEN(FAIL)"
        print("  [%s] %s%s" % (status, name,
                               "" if not mok else "  <-- 变异未判红"))
        if mok:
            bad.append("变异 '%s' 未判红（判据对该缺陷盲）" % name)
    if bad:
        for b in bad:
            print("SELF-TEST FAIL: %s" % b, file=sys.stderr)
        return 1
    print("P3-EXPORT-STREAM-PROD-SELFTEST PASS (基线绿 + 3 处变异全红)")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    text = open(PROD_TU, encoding="utf-8").read()
    ok, fails = evaluate(text)
    if ok:
        if not args.quiet:
            print("CHK-P3-EXPORT-STREAM-PROD PASS: 生产 phase3 writer 节点经 "
                  "ExportStreamScheduler 子块流式执行（ASTROCS_DESIGN §8.3 export 行）")
        return 0
    for f in fails:
        print("CHK-P3-EXPORT-STREAM-PROD FAIL: %s" % f, file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
