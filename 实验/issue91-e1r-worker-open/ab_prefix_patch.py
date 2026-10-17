#!/usr/bin/env python3
"""E1-R A/B：把**同样**的故障注入管线打到修复前（HEAD=014273357）的
p3_op_resample worker 上，但**保留旧的静默 return 语义**（不记录失败、无条件计数）。

目的：证明"worker sampler 打开失败被当作正常完成"这一缺陷在修复前真实存在
（exit 0 + 发布 FITS），而修复后同一注入必然失败。这不是生产代码，只是 A/B 对照。

用法: python3 ab_prefix_patch.py <tree_root>
"""
from __future__ import annotations

import os
import sys

OLD_WORKER = """  auto worker = [&](long b0, long b1) {
    P3Sampler w_samp{};
    std::string wserr;
    if (p3_sampler_open_ex(g.hips_dir.c_str(), &w_samp, nullptr, nullptr, &wserr) !=
        P3_RS_OK)
      return;"""

NEW_WORKER = """  // ── E1-R A/B 注入面（仅对照用；**不含**错误传播修复）────────────────────
  const char* rt001_fault = std::getenv("ACSD_RT001_FAULT");
  int fault_worker_open_k = -1;
  if (rt001_fault) {
    static const char kTok[] = "resample_worker_open_fail";
    if (std::strncmp(rt001_fault, kTok, sizeof(kTok) - 1) == 0) {
      fault_worker_open_k = 1;
      const char* rest = rt001_fault + (sizeof(kTok) - 1);
      if (*rest == ':' || *rest == '=') {
        char* end = nullptr;
        const long v = std::strtol(rest + 1, &end, 10);
        if (end != rest + 1 && v >= 0 && v <= 4096) fault_worker_open_k = (int)v;
      }
    }
  }
  auto worker = [&](long b0, long b1, uint32_t wk) {
    P3Sampler w_samp{};
    std::string wserr;
    const std::string wdir =
        (fault_worker_open_k >= 0 && (int)wk == fault_worker_open_k)
            ? (g.hips_dir + "/__rt001_worker_open_fault__")
            : g.hips_dir;
    if (p3_sampler_open_ex(wdir.c_str(), &w_samp, nullptr, nullptr, &wserr) !=
        P3_RS_OK)
      return;   // 旧语义: 静默 return, 不记录失败"""

OLD_TAIL = """    p3_sampler_close(&w_samp);
  };"""
NEW_TAIL = """    p3_sampler_close(&w_samp);
  };"""

OLD_BAND = """  auto band_task = [&](long b0, long b1, RunContext&) {"""
NEW_BAND = """  auto band_task = [&](long b0, long b1, RunContext&, uint32_t wk) {"""

OLD_CALL = """    worker(b0, b1);
    band_active.fetch_sub(1);
    band_executed.fetch_add(1);"""
NEW_CALL = """    worker(b0, b1, wk);          // 旧语义: 返回值被丢弃
    band_active.fetch_sub(1);
    band_executed.fetch_add(1);  // 旧语义: 无条件计入完成"""

OLD_ENQ = """        band_pool->enqueue([&band_task, b0, b1](RunContext& c) {
          band_task(b0, b1, c);
        });"""
NEW_ENQ = """        band_pool->enqueue([&band_task, b0, b1, k](RunContext& c) {
          band_task(b0, b1, c, k);
        });"""

OLD_DEF = """        band_task(b0, b1, no_ctx);"""
NEW_DEF = """        band_task(b0, b1, no_ctx, k);"""

OLD_SER = """    band_task(0, nblocks, no_ctx);"""
NEW_SER = """    band_task(0, nblocks, no_ctx, 0u);"""


def main() -> int:
    root = sys.argv[1]
    path = os.path.join(root, "lib/infrastructure/scheduler/src/module_adapters.cpp")
    text = open(path, encoding="utf-8").read()
    for old, new, name in ((OLD_WORKER, NEW_WORKER, "worker"),
                           (OLD_BAND, NEW_BAND, "band_sig"),
                           (OLD_CALL, NEW_CALL, "band_call"),
                           (OLD_ENQ, NEW_ENQ, "enqueue"),
                           (OLD_DEF, NEW_DEF, "defensive"),
                           (OLD_SER, NEW_SER, "serial")):
        if text.count(old) != 1:
            print(f"ANCHOR_MISS[{name}]: found {text.count(old)}", file=sys.stderr)
            return 2
        text = text.replace(old, new)
    open(path, "w", encoding="utf-8").write(text)
    print("ab prefix patch applied (injection only, old silent-return semantics)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
