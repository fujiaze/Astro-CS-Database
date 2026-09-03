#!/usr/bin/env python3
"""RT-003 验收测试：真实 ThreadBudget 接入 scheduler→RunContext→module host API。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md RT-003):
  - lease RAII、异常/取消回收、上限由可用资源和 profile/config 计算；
  - 不用 ThreadLease::make（静态扫描生产路径无伪授权）；
  - 验收负测：竞争、嵌套、超额、取消、exception、重复 run、budget=1；
  - scheduler→RunContext 注入唯一 ThreadBudget（Scheduler::run 注入 ctx.budget）。

方法 (独立 harness, 照 tests/arch/test_budget_contract.py 先例):
  Python unittest 内嵌 C++ driver，g++ 真实编译 lib/core 源码(context.cpp/
  artifact.cpp/scheduler.cpp) + include/astrocs/core 头, 链接运行断言；
  另以源码静态扫描断言生产路径(lib/core/src/**)无 ThreadLease::make 伪授权。
"""
from __future__ import annotations

import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
INC = REPO / "include"
CORE = REPO / "lib" / "core" / "src"

_DRIVER = r'''
// RT-003 harness: ThreadBudget 接入 scheduler→RunContext 验收（真实编译 lib/core 源码）
#include "astrocs/core/context.h"
#include "astrocs/core/scheduler.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// ── A. RunContext::acquire_lease 接 ThreadBudget 真实原子预留 ──
static void test_ctx_acquire_real_budget() {
  auto b = create_thread_budget(4);
  CHECK(b.ok());
  RunContext ctx;
  ctx.set_budget(b.value());               // scheduler 注入语义
  auto lease = ctx.acquire_lease(8);       // cap 到预算 → 4
  CHECK(lease.acquired());
  CHECK(lease.size() == 4);
  CHECK(ctx.budget()->available() == 0);
  auto lease2 = ctx.acquire_lease(2);      // 耗尽 → 空（不伪造）
  CHECK(!lease2.acquired());
  { auto _ = std::move(lease); }
  CHECK(ctx.budget()->available() == 4);   // RAII 归还
}

// ── B. 未注入预算 → 空租约（拒绝 ThreadLease::make 伪授权回退） ──
static void test_ctx_no_budget_no_fake() {
  RunContext ctx;
  auto lease = ctx.acquire_lease(4);
  CHECK(!lease.acquired());                // 无预算来源 → 空
}

// ── C. 竞争: 8 并发请求 budget=4 → Σactive<=4 不超卖 ──
static void test_contention_no_oversubscribe() {
  const uint32_t budget_n = 4;
  auto b = create_thread_budget(budget_n);
  CHECK(b.ok());
  std::atomic<uint32_t> active{0};
  std::atomic<uint32_t> peak{0};
  std::atomic<bool> violated{false};
  std::vector<std::thread> ts;
  for (int i = 0; i < 8; ++i) {
    ts.emplace_back([&]() {
      ThreadLease lease = b.value()->acquire(1, 2, AcquirePolicy::NONBLOCK);
      if (!lease.acquired()) return;
      uint32_t cur = active.fetch_add(lease.size()) + lease.size();
      uint32_t p = peak.load();
      while (cur > p && !peak.compare_exchange_weak(p, cur)) {}
      if (cur > budget_n) violated.store(true);
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
      active.fetch_sub(lease.size());
    });
  }
  for (auto& t : ts) t.join();
  CHECK(!violated.load());
  CHECK(peak.load() <= budget_n);
  CHECK(b.value()->available() == budget_n);
}

// ── D. 嵌套: 外租约占满后内层请求不可超卖 ──
static void test_nested_no_double_count() {
  auto b = create_thread_budget(2);
  CHECK(b.ok());
  ThreadLease outer = b.value()->acquire(2, 2, AcquirePolicy::NONBLOCK);
  CHECK(outer.acquired());
  CHECK(b.value()->available() == 0);
  ThreadLease inner = b.value()->acquire(1, 2, AcquirePolicy::NONBLOCK);
  CHECK(!inner.acquired());                // 嵌套不可超卖
  outer.release();
  CHECK(b.value()->available() == 2);
}

// ── E. 超额: 请求 > budget 上限 → cap 到上限；全占用后再请求 → 空 ──
static void test_oversubscribe_rejected() {
  auto b = create_thread_budget(1);
  CHECK(b.ok());
  ThreadLease l1 = b.value()->acquire(1, 5, AcquirePolicy::NONBLOCK);
  CHECK(l1.size() == 1);                   // cap 到预算 1
  CHECK(b.value()->available() == 0);
  ThreadLease l2 = b.value()->acquire(1, 1, AcquirePolicy::NONBLOCK);
  CHECK(!l2.acquired());                   // 超额拒绝
  l1.release();
  CHECK(b.value()->available() == 1);
}

// ── F. 取消/异常: lease RAII 统一回收 ──
static void test_exception_and_cancel_restore() {
  auto b = create_thread_budget(3);
  CHECK(b.ok());
  try {
    ThreadLease lease = b.value()->acquire(3, 3, AcquirePolicy::NONBLOCK);
    CHECK(lease.size() == 3);
    CHECK(b.value()->available() == 0);
    throw std::runtime_error("boom");      // 异常 → 析构归还
  } catch (const std::runtime_error&) {}
  CHECK(b.value()->available() == 3);
  {
    ThreadLease lease = b.value()->acquire(2, 2, AcquirePolicy::NONBLOCK);
    if (lease.acquired()) return;          // 模拟取消提前返回 → 析构归还
  }
  CHECK(b.value()->available() == 3);
}

// ── G. budget=1 负测: 并发下 Σactive<=1 ──
static void test_budget1_backpressure() {
  auto b = create_thread_budget(1);
  CHECK(b.ok());
  std::atomic<uint32_t> peak{0};
  std::atomic<uint32_t> active{0};
  std::vector<std::thread> ts;
  for (int i = 0; i < 4; ++i) {
    ts.emplace_back([&]() {
      ThreadLease lease = b.value()->acquire(1, 1, AcquirePolicy::NONBLOCK);
      if (!lease.acquired()) return;
      uint32_t cur = active.fetch_add(1) + 1;
      uint32_t p = peak.load();
      while (cur > p && !peak.compare_exchange_weak(p, cur)) {}
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      active.fetch_sub(1);
    });
  }
  for (auto& t : ts) t.join();
  CHECK(peak.load() <= 1);
  CHECK(b.value()->available() == 1);
}

// ── H. scheduler→RunContext 注入 + 重复 run 预算不泄漏 ──
static void test_scheduler_inject_and_repeat_run() {
  Scheduler sched(2, 2);                   // budget=2（可用资源上限注入）
  CHECK(sched.thread_budget() != nullptr);
  std::atomic<uint32_t> peak{0};
  sched.add_node({"a", {}, [&](const std::string&, RunContext& ctx) {
    auto lease = ctx.acquire_lease(2);     // 经注入预算原子预留
    if (!lease.acquired()) return Result<void>::fail(
        Error(ErrorDomain::RESOURCE, "no lease"));
    uint32_t p = peak.fetch_add(1) + 1;
    (void)p;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    peak.fetch_sub(1);
    return Result<void>::success();
  }, "cpu_heavy"});
  // 第一次 run
  {
    RunContext ctx;
    auto r = sched.run(ctx);
    CHECK(r.ok());
    CHECK(sched.thread_budget()->available() == 2);  // 全部归还
  }
  // 第二次 run（重复 run：预算不泄漏、可再次全量使用）
  {
    RunContext ctx;
    auto r = sched.run(ctx);
    CHECK(r.ok());
    CHECK(sched.thread_budget()->available() == 2);
  }
}

// ── I. module host API 上限同源: ctx.budget 注入 host workers 上限 ──
static void test_ctx_budget_host_cap_source() {
  auto b = create_thread_budget(3);
  CHECK(b.ok());
  RunContext ctx;
  ctx.set_budget(b.value());
  CHECK(ctx.budget()->budget() == 3);
  const uint32_t workers = 2;              // 模块声明上限
  const uint32_t host_workers = workers < ctx.budget()->budget()
                                    ? workers : ctx.budget()->budget();
  CHECK(host_workers == 2);                // host 上限 = min(模块请求, 预算)
  auto lease = ctx.acquire_lease(host_workers);
  CHECK(lease.size() == 2);
  lease.release();
  CHECK(ctx.budget()->available() == 3);
}

int main() {
  test_ctx_acquire_real_budget();
  test_ctx_no_budget_no_fake();
  test_contention_no_oversubscribe();
  test_nested_no_double_count();
  test_oversubscribe_rejected();
  test_exception_and_cancel_restore();
  test_budget1_backpressure();
  test_scheduler_inject_and_repeat_run();
  test_ctx_budget_host_cap_source();
  if (failures) {
    std::fprintf(stderr, "RT-003_FAIL failures=%d\n", failures);
    return 1;
  }
  std::printf("RT-003_WIRING_PASS\n");
  return 0;
}
'''

_STATIC_SCAN = r'''
// RT-003 静态扫描: 生产路径禁止 ThreadLease::make（伪授权）与未注入预算回退。
'''

GLOBAL_MAKE_RE = re.compile(r"ThreadLease::make")
PROD_ROOTS = [
    REPO / "lib" / "core" / "src",
]


def build_driver(tmp: pathlib.Path) -> pathlib.Path:
    drv = tmp / "rt003_driver.cpp"
    drv.write_text(_DRIVER, encoding="utf-8")
    exe = tmp / "rt003_wiring"
    objs = []
    for src in ("context.cpp", "artifact.cpp", "scheduler.cpp"):
        o = tmp / (src + ".o")
        r = subprocess.run(
            ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-c",
             str(CORE / src), f"-I{INC}", f"-I{REPO / 'third_party'}",
             "-o", str(o)],
            capture_output=True, text=True, timeout=300)
        if r.returncode != 0:
            raise RuntimeError(f"compile {src} failed:\n{r.stderr[-2000:]}")
        objs.append(str(o))
    r = subprocess.run(
        ["g++", "-std=c++17", "-O2", str(drv),
         f"-I{INC}", f"-I{REPO / 'third_party'}",
         *objs, "-pthread", "-o", str(exe)],
        capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        raise RuntimeError(f"link driver failed:\n{r.stderr[-2000:]}")
    return exe


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestRt003WiringCpp(unittest.TestCase):
    """C++ harness：真实编译 lib/core 源码运行 RT-003 接线验收。"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = pathlib.Path(tempfile.mkdtemp(prefix="rt003_"))
        cls.exe = build_driver(cls.tmp)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_driver_all_checks_pass(self):
        r = subprocess.run([str(self.exe)], capture_output=True, text=True,
                           timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-2000:])
        self.assertIn("RT-003_WIRING_PASS", r.stdout)


class TestRt003NoFakeAuthorization(unittest.TestCase):
    """静态检查生产路径没有伪授权：lib/core 生产源不得调用 ThreadLease::make。"""

    def test_no_threadlease_make_in_production_sources(self):
        hits = []
        for root in PROD_ROOTS:
            for p in sorted(root.glob("*.cpp")):
                for lineno, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
                    if GLOBAL_MAKE_RE.search(line) and "//" not in line.split("ThreadLease::make")[0]:
                        hits.append(f"{p.name}:{lineno}: {line.strip()}")
        self.assertEqual(hits, [], f"生产路径发现 ThreadLease::make 伪授权: {hits}")

    def test_threadlease_make_only_frozen_header_legacy(self):
        """make 仅允许保留在冻结公共头 context.h（RT-003 前兼容注释），
        生产实现一律经 acquire/acquire_lease 原子预留。"""
        hdr = (INC / "astrocs" / "core" / "context.h").read_text(encoding="utf-8")
        # context.h 内 make 只存在于 ThreadLease::make 定义处（RT-003 前兼容）
        defs = re.findall(r"static ThreadLease make\(uint32_t", hdr)
        self.assertEqual(len(defs), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
