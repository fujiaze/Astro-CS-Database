#!/usr/bin/env python3
"""RT-004 验收测试：唯一共享 executor（CPU heavy + 有界 I/O）。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md RT-004):
  - 私池静态扫描：生产路径(lib/infrastructure/scheduler/src + lib/include/astrocs/core)无 std::thread
    私建永久池；executor.cpp 是全仓唯一共享 executor 池实现（Impl 持有
    vector<thread> 创建 CPU/I/O worker，析构 stop+notify+join 完整回收生命周期，
    无 detach 常驻线程/UAF）；scheduler.cpp 的 CORE-006 基线 bounded per-run
    join pool (每次 run 内创建并在 run 结束前全部 join 回收) 属 Scheduler 自身
    调度协调，非"模块私建永久池"，予以显式豁免并在 known_limits 记录（不静默）。
  - 并发任务不超预算：budget=4 enqueue 8 任务 → Σactive ≤ 4。
  - worker 不空转：队列有工作时 worker 立即被 cv 唤醒执行（不忙等轮询）；
    静态断言 executor.cpp worker_loop 用 condition_variable 且无 sleep_for 忙等。
  - 取消能唤醒等待：cancel() 置位后 notify_all；排队任务丢弃、幂等、新 enqueue 拒绝；
    IoExecutor 有界队列满 → enqueue 返回 false。

方法 (独立 harness, 照 eng/tests/runtime/test_rt003_budget_wiring.py 先例):
  Python unittest 内嵌 C++ driver，g++ 真实编译链接 lib/infrastructure/scheduler/src 源码
  (executor.cpp/context.cpp/artifact.cpp) + lib/include/astrocs/core 头，运行断言；
  另以源码静态扫描断言生产路径无私建永久池、worker 无忙等轮询。
"""
from __future__ import annotations

import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[4]
INC = REPO / "lib" / "include"
CORE = REPO / "lib" / "infrastructure" / "scheduler" / "src"

_DRIVER = r'''
// RT-004 harness: 唯一共享 executor（CPU heavy + 有界 I/O）真实编译链接验收
#include "astrocs/core/executor.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
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

// ── A. 并发不超预算: budget=4 enqueue 8 → Σactive≤4 ──
static void test_budget4_enqueue8_no_oversubscribe() {
  auto b = create_thread_budget(4);
  CHECK(b.ok());
  auto ex = create_cpu_heavy_executor(b.value());
  CHECK(ex.ok());
  CHECK(ex.value()->worker_count() == 4);
  std::atomic<uint32_t> active{0};
  std::atomic<uint32_t> peak{0};
  std::atomic<uint32_t> done{0};
  for (int i = 0; i < 8; ++i) {
    ex.value()->enqueue([&](RunContext&) {
      uint32_t cur = active.fetch_add(1) + 1;
      uint32_t p = peak.load();
      while (cur > p && !peak.compare_exchange_weak(p, cur)) {}
      if (cur > 4) { std::fprintf(stderr, "OVERSHOOT cur=%u\n", cur); }
      std::this_thread::sleep_for(std::chrono::milliseconds(40));
      active.fetch_sub(1);
      done.fetch_add(1);
    });
  }
  ex.value()->wait_all();
  CHECK(done.load() == 8);       // 全部执行完
  CHECK(peak.load() <= 4);       // Σactive ≤ budget 不超卖
  CHECK(active.load() == 0);
  CHECK(b.value()->available() == 4);  // lease 全部归还
}

// ── B. worker 不空转: 队列有工作 → 任务在毫秒级被唤醒执行 ──
static void test_worker_no_idle_when_work() {
  auto b = create_thread_budget(2);
  CHECK(b.ok());
  auto ex = create_cpu_heavy_executor(b.value());
  CHECK(ex.ok());
  // 连续 30 轮: enqueue 一个任务 → 等待其 started（cv 唤醒证明，非轮询空转）
  std::atomic<uint32_t> done{0};
  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < 30; ++i) {
    std::mutex mtx;
    std::condition_variable cv;
    bool started = false;
    ex.value()->enqueue([&](RunContext&) {
      { std::lock_guard<std::mutex> lk(mtx); started = true; }
      cv.notify_one();
    });
    {
      std::unique_lock<std::mutex> lk(mtx);
      if (!cv.wait_for(lk, std::chrono::seconds(1),
                       [&] { return started; })) {
        std::fprintf(stderr, "TIMEOUT waiting worker wakeup round %d\n", i);
        ++failures;
        break;
      }
    }
    done.fetch_add(1);
  }
  ex.value()->wait_all();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0).count();
  CHECK(done.load() == 30);
  // 30 轮任务（含 2 次 wait_all 间调度）应在数秒内完成——若 worker 忙等轮询
  // 也会快，故空转主要由静态断言(无 sleep_for 忙等)兜底；此处仅证唤醒即时。
  std::printf("  worker wakeup 30 rounds elapsed_ms=%lld\n",
              static_cast<long long>(elapsed_ms));
}

// ── C. 取消能唤醒等待: CPU executor ──
// 长任务占住 worker + 排队任务；cancel() → 排队任务丢弃、幂等、新 enqueue 拒绝。
static void test_cpu_cancel_wakes_and_idempotent() {
  auto b = create_thread_budget(1);   // 单 worker，排队任务必然等待
  CHECK(b.ok());
  auto ex = create_cpu_heavy_executor(b.value());
  CHECK(ex.ok());

  std::mutex gm;
  std::condition_variable gcv;
  bool gate_open = false;
  std::atomic<uint32_t> long_started{0};
  std::atomic<uint32_t> queued_done{0};
  std::atomic<uint32_t> after_cancel_done{0};

  ex.value()->enqueue([&](RunContext&) {   // 长任务: 等 gate
    long_started.store(1);
    std::unique_lock<std::mutex> lk(gm);
    gcv.wait(lk, [&] { return gate_open; });
  });
  // 等长任务真正开始（占住唯一 worker + lease）
  {
    for (int i = 0; i < 5000 && long_started.load() == 0; ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(long_started.load() == 1);

  // 排队任务（无法执行，因预算耗尽/worker 忙）
  for (int i = 0; i < 3; ++i)
    ex.value()->enqueue([&](RunContext&) { queued_done.fetch_add(1); });

  ex.value()->cancel();      // 唤醒等待的 worker（排队任务丢弃）
  ex.value()->cancel();      // 幂等: 第二次调用无副作用
  ex.value()->enqueue([&](RunContext&) { after_cancel_done.fetch_add(1); });  // 取消后拒绝

  // 释放长任务 gate → 长任务完成
  {
    std::lock_guard<std::mutex> lk(gm);
    gate_open = true;
  }
  gcv.notify_all();
  ex.value()->wait_all();    // 取消后尽快返回（长任务完成即返回）

  CHECK(queued_done.load() == 0);        // 排队任务被取消丢弃，未执行
  CHECK(after_cancel_done.load() == 0);  // cancel 后 enqueue 被拒绝
}

// ── D. IoExecutor: 有界队列满 → enqueue 返回 false；取消唤醒 ──
static void test_io_bounded_queue_full_returns_false() {
  // max_concurrency=1, queue_capacity=2
  auto io = create_io_executor(1, 2);
  CHECK(io.ok());
  CHECK(io.value()->max_concurrency() == 1);
  CHECK(io.value()->queue_capacity() == 2);

  std::mutex gm;
  std::condition_variable gcv;
  bool gate_open = false;
  std::atomic<uint32_t> long_started{0};
  std::atomic<uint32_t> done{0};

  // 任务 1: 占住唯一 worker
  CHECK(io.value()->enqueue(IoTaskClass::IO_READ, [&](RunContext&) {
    long_started.store(1);
    std::unique_lock<std::mutex> lk(gm);
    gcv.wait(lk, [&] { return gate_open; });
    done.fetch_add(1);
  }));
  for (int i = 0; i < 5000 && long_started.load() == 0; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  CHECK(long_started.load() == 1);

  // 任务 2、3: 入队（队列 2/2 满）
  CHECK(io.value()->enqueue(IoTaskClass::IO_WRITE, [&](RunContext&) { done.fetch_add(1); }));
  CHECK(io.value()->enqueue(IoTaskClass::METADATA, [&](RunContext&) { done.fetch_add(1); }));
  CHECK(io.value()->queued() == 2);
  // 任务 4: 队列满 → false（不阻塞、不丢失语义由调用方处理）
  CHECK(!io.value()->enqueue(IoTaskClass::SHORT_SERIAL,
                             [&](RunContext&) { done.fetch_add(1); }));

  // 释放 gate → 全部排队任务执行
  {
    std::lock_guard<std::mutex> lk(gm);
    gate_open = true;
  }
  gcv.notify_all();
  io.value()->wait_all();
  CHECK(done.load() == 3);   // 长任务 + 2 排队任务；第 4 个被拒未执行
}

// ── E. IoExecutor: cancel 唤醒等待 worker（幂等） ──
static void test_io_cancel_wakes_and_idempotent() {
  auto io = create_io_executor(1, 4);
  CHECK(io.ok());

  std::mutex gm;
  std::condition_variable gcv;
  bool gate_open = false;
  std::atomic<uint32_t> long_started{0};
  std::atomic<uint32_t> queued_done{0};

  CHECK(io.value()->enqueue(IoTaskClass::IO_READ, [&](RunContext&) {
    long_started.store(1);
    std::unique_lock<std::mutex> lk(gm);
    gcv.wait(lk, [&] { return gate_open; });
  }));
  for (int i = 0; i < 5000 && long_started.load() == 0; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  CHECK(long_started.load() == 1);
  for (int i = 0; i < 2; ++i)
    CHECK(io.value()->enqueue(IoTaskClass::IO_WRITE,
                              [&](RunContext&) { queued_done.fetch_add(1); }));

  io.value()->cancel();     // 唤醒等待 worker；排队任务丢弃
  io.value()->cancel();     // 幂等
  CHECK(!io.value()->enqueue(IoTaskClass::IO_READ,
                             [&](RunContext&) {}));   // 取消后拒绝

  {
    std::lock_guard<std::mutex> lk(gm);
    gate_open = true;
  }
  gcv.notify_all();
  io.value()->wait_all();   // 取消后尽快返回
  CHECK(queued_done.load() == 0);  // 排队任务被取消丢弃
}

// ── F. 异常安全: 任务抛异常不杀 worker；后续任务仍执行 ──
static void test_exception_safety_worker_survives() {
  auto b = create_thread_budget(2);
  CHECK(b.ok());
  auto ex = create_cpu_heavy_executor(b.value());
  CHECK(ex.ok());
  std::atomic<uint32_t> done{0};
  ex.value()->enqueue([&](RunContext&) { throw std::runtime_error("boom"); });
  for (int i = 0; i < 4; ++i)
    ex.value()->enqueue([&](RunContext&) { done.fetch_add(1); });
  ex.value()->wait_all();
  CHECK(done.load() == 4);  // worker 未被异常杀死，后续任务全部执行
  CHECK(b.value()->available() == 2);  // lease 经 RAII 异常路径归还
}

// ── G. 工厂负测: 空 budget / 0 并发 → fail ──
static void test_factory_negative() {
  auto b0 = create_cpu_heavy_executor(nullptr);
  CHECK(!b0.ok());
  auto bad = create_io_executor(0, 4);
  CHECK(!bad.ok());
  auto bad2 = create_io_executor(2, 0);
  CHECK(!bad2.ok());
}

int main() {
  test_budget4_enqueue8_no_oversubscribe();
  test_worker_no_idle_when_work();
  test_cpu_cancel_wakes_and_idempotent();
  test_io_bounded_queue_full_returns_false();
  test_io_cancel_wakes_and_idempotent();
  test_exception_safety_worker_survives();
  test_factory_negative();
  if (failures) {
    std::fprintf(stderr, "RT-004_FAIL failures=%d\n", failures);
    return 1;
  }
  std::printf("RT-004_EXECUTOR_PASS\n");
  return 0;
}
'''

# ── 静态扫描断言 (私池 + 忙等) ──
# 扫描范围: 生产路径 = lib/infrastructure/scheduler/src/*.cpp + lib/include/astrocs/core/*.h
# 语义 (RT-004): 模块/节点不得 std::thread 私建永久池。executor.cpp 是全仓唯一
# 共享 executor 池实现，允许创建常驻 worker（CpuHeavyExecutor/IoExecutor 构造，
# Impl 持有 vector<thread>，析构 stop+notify+join 完整回收生命周期，无 UAF）。
# scheduler.cpp 含 CORE-006 基线 bounded per-run join pool（每次 run 内创建并在
# run 结束前全部 join 回收，非永久驻留）——属 Scheduler 自身调度协调线程而非
# "模块私建永久池"，显式豁免并记录（见 known_limits）。
CPP_PROD_ROOTS = [CORE]
HDR_PROD_ROOT = INC / "acsd" / "core"

# scheduler.cpp CORE-006 基线 bounded per-run join pool 豁免（run 内 join 回收）
_SCHEDULER_JOIN_POOL_OK = {"scheduler.cpp"}

# 忙等轮询特征: worker_loop 空等不得使用 sleep_for 自旋（空转）；须 cv.wait。
# executor.cpp 内允许 sleep_for 吗？不允许——worker 空等只经 condition_variable。
# 任务执行体内 sleep 属任务代码，不在 executor.cpp。

DETACH_RE = re.compile(r"\.detach\s*\(")
THREAD_CREATE_RE = re.compile(r"std::(?:j?thread)\s*\(")
ASYNC_RE = re.compile(r"std::async\s*\(")
BUSY_SLEEP_RE = re.compile(r"std::this_thread::sleep_for")


def _cpp_files():
    files = []
    for root in CPP_PROD_ROOTS:
        files.extend(sorted(root.glob("*.cpp")))
    files.extend(sorted(HDR_PROD_ROOT.glob("*.h")))
    return files


def build_driver(tmp: pathlib.Path) -> pathlib.Path:
    drv = tmp / "rt004_driver.cpp"
    drv.write_text(_DRIVER, encoding="utf-8")
    exe = tmp / "rt004_executor"
    objs = []
    for src in ("executor.cpp", "context.cpp", "artifact.cpp"):
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
class TestRt004ExecutorCpp(unittest.TestCase):
    """C++ harness：真实编译链接 executor.cpp 运行 RT-004 验收。"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = pathlib.Path(tempfile.mkdtemp(prefix="rt004_"))
        cls.exe = build_driver(cls.tmp)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_driver_all_checks_pass(self):
        r = subprocess.run([str(self.exe)], capture_output=True, text=True,
                           timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-2000:])
        self.assertIn("RT-004_EXECUTOR_PASS", r.stdout)


class TestRt004NoPrivatePermanentPool(unittest.TestCase):
    """私池静态扫描：生产路径无 std::thread 私建永久池（executor.cpp 唯一豁免）。"""

    def test_no_detach_worker_anywhere(self):
        """全生产路径不得使用 detach 常驻线程（无法 join 回收 → UAF 风险；
        唯一共享池 executor.cpp 用 Impl vector<thread> + join 生命周期管理）。"""
        hits = []
        for p in _cpp_files():
            for lineno, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
                if DETACH_RE.search(line) and not line.strip().startswith("//"):
                    hits.append(f"{p.name}:{lineno}: {line.strip()}")
        self.assertEqual(hits, [],
                         f"生产路径发现 detach 线程（应 join 回收）: {hits}")

    def test_executor_worker_threads_present(self):
        """executor.cpp 两处 worker 线程创建（CPU/I/O 各一）必须存在（唯一共享池）。"""
        text = (CORE / "executor.cpp").read_text(encoding="utf-8")
        create_lines = [ln for ln in text.splitlines()
                        if "emplace_back([this]() { worker_loop(); })" in ln
                        or "emplace_back([this]() { io_worker_loop(); })" in ln]
        self.assertGreaterEqual(len(create_lines), 2,
                                "executor.cpp 应有两处 worker 创建(CPU+I/O)")

    def test_no_async_bypass_in_production(self):
        """std::async 绕过 budget → 生产路径拒绝（模块不得经 async 私建并行）。"""
        hits = []
        for p in _cpp_files():
            for lineno, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
                if ASYNC_RE.search(line) and not line.strip().startswith("//"):
                    hits.append(f"{p.name}:{lineno}: {line.strip()}")
        self.assertEqual(hits, [], f"生产路径发现 std::async 绕过预算: {hits}")

    def test_no_permanent_thread_pool_member_in_headers(self):
        """头文件不得持有 std::vector<std::thread> 等永久池成员（模块自建池拒绝）。"""
        hits = []
        for p in sorted(HDR_PROD_ROOT.glob("*.h")):
            text = p.read_text(encoding="utf-8")
            # 类成员声明（行内不以 // 注释开头）持有线程容器 → 永久池
            for lineno, line in enumerate(text.splitlines(), 1):
                if re.search(r"std::vector\s*<\s*std::(j?thread)\s*>", line) \
                        and not line.strip().startswith("//"):
                    hits.append(f"{p.name}:{lineno}: {line.strip()}")
        self.assertEqual(hits, [], f"头文件发现永久线程池成员: {hits}")

    def test_scheduler_join_pool_is_bounded_and_reclaimed(self):
        """scheduler.cpp CORE-006 基线 pool 必须是有界 + run 内 join 回收（豁免依据）。

        若 scheduler.cpp 出现 detach 型常驻池（脱离生命周期）则豁免失效 → FAIL。
        当前仅允许: 局部 vector<std::thread> pool + 全部 join()（run 内回收）。
        """
        text = (CORE / "scheduler.cpp").read_text(encoding="utf-8")
        if ".detach(" in text:
            self.fail("scheduler.cpp 出现 detach 常驻池，超出 CORE-006 基线豁免")
        if "std::vector<std::thread>" not in text:
            return  # 无 pool（将来消灭 per-run 池后此豁免自然空转）
        self.assertIn("pool.emplace_back", text)
        self.assertIn("t.join()", text)


class TestRt004WorkerNoBusySpin(unittest.TestCase):
    """worker 不空转静态断言：executor.cpp worker_loop 用 cv.wait，无忙等 sleep_for。"""

    def test_executor_cpp_uses_cv_not_busy_sleep(self):
        text = (CORE / "executor.cpp").read_text(encoding="utf-8")
        self.assertIn("cv.wait(", text)       # worker 空等阻塞在 CV（不空转）
        self.assertIn("cv.notify_all()", text)
        self.assertIn("cv.notify_one()", text)
        # executor.cpp 内部不允许 sleep_for 忙等（任务体内 sleep 属任务代码）
        for lineno, line in enumerate(text.splitlines(), 1):
            if BUSY_SLEEP_RE.search(line) and not line.strip().startswith("//"):
                self.fail(f"executor.cpp:{lineno} 出现 sleep_for 忙等轮询: {line.strip()}")

    def test_worker_loops_block_on_empty_queue(self):
        text = (CORE / "executor.cpp").read_text(encoding="utf-8")
        # worker_loop 空队列谓词: 队列空且未取消/未停止 → 阻塞等待
        self.assertRegex(text, r"impl_->tasks\.empty\(\)")
        self.assertRegex(text, r"cv\.wait\(lock, \[this\]")


# ── RT-004-POOL-01 运行时池回收 harness（Linux：/proc/self/task 线程计数）──
# 判据：三个阶段调度器每次 run() 的池必须在 run 返回前全部 join 回收 —— run 返回后
# **有限时间（kReclaimDeadlineMs）内**进程线程数回到基线（连续 5 轮不累积）；同时 run
# 期间线程数峰值必须超过基线（非退化证据：池线程确实起过，判据不是恒真门）。
#
# FLAKE-01（2026-09-23）判据观测面订正：原实现读 join 返回**瞬间**的 /proc/self/task
# 条目数，满负载下会假红（实测 LEAK normalize round=2 threads_after=5 base=4，单跑
# 复现不出）。一手证据 run/FLAKE-01/evidence/tjoin_window.cpp：pthread_join 返回 ≠ 内核
# 任务条目已消失（清 tid futex 在 do_exit 早期，摘除线程组条目在之后的 release_task），
# 空载 3000 轮 0 次滞后；宿主 load≈13 时 20000 轮中 1.08% 滞后，p50=3.8ms /
# p99=8.6ms / max=15.4ms。⇒ 判据改为"有界等待回到基线"（窗口 2000ms = 实测 max 的
# 130 倍），并由 check_wait_discriminates_real_leak 自检保证真泄漏仍判红（不放宽判据）。
_POOL_DRIVER = r'''
// RT-004-POOL-01 harness: 三个阶段调度器的池回收（无泄漏线程）运行时验收
#include "astrocs/core/export_stream.h"
#include "astrocs/core/mosaic_window.h"
#include "astrocs/core/normalize_workflow.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <dirent.h>
#include <functional>
#include <string>
#include <thread>
#include <vector>

using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

// /proc/self/task 条目数（含 "." ".."；基线/结束后同口径比较）
static int thread_count() {
  DIR* d = ::opendir("/proc/self/task");
  if (!d) return -1;
  int n = 0;
  while (::readdir(d) != nullptr) ++n;
  ::closedir(d);
  return n;
}

// 回收判据的**观测窗口**（FLAKE-01 实测依据，非固定 sleep）：
//   pthread_join 返回 ≠ 内核任务条目已从 /proc/self/task 消失 —— 线程退出时
//   清 CLONE_CHILD_CLEARTID 的 futex 在 do_exit 早期（mm_release），而任务从
//   线程组摘除（__unhash_process）在之后的 release_task；两者之间 join 已返回
//   但条目仍在枚举里。实测（run/FLAKE-01/evidence/tjoin_window.cpp，20000 轮、
//   宿主 load≈13）：滞后出现率 1.08%，滞后时长 p50=3.8ms / p90=6.0ms /
//   p99=8.6ms / max=15.4ms；空载 3000 轮 0 次。
//   ⇒ 判据不得是"join 返回瞬间的读数"，而必须是"run 返回后**有限时间**内回到
//   基线"。窗口 2000ms 为实测最大值的 130 倍：真泄漏（永不 join/detach 常驻）
//   不可能在窗口内回落，判别力不减（见 check_wait_discriminates_real_leak）。
static constexpr int kReclaimDeadlineMs = 2000;

// 有界等待：线程数回落到 want，或超时后返回最后读数（超时值 ⇒ 调用方判红）。
static int wait_thread_count(int want, int deadline_ms) {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(deadline_ms);
  int n = thread_count();
  while (n != want && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::microseconds(200));
    n = thread_count();
  }
  return n;
}

// 判别力自检（能红能绿）：有界等待不得把**真泄漏**放过去。
//   红：故意 detach 一个常驻线程 ⇒ 有界等待超时后读数仍 != 基线（必须判红）；
//   绿：该线程退出后同一等待必须回到基线。
//   若把判据退化成"等待足够久就算回收"，本自检立刻判红。
static void check_wait_discriminates_real_leak(int base) {
  std::atomic<bool> stop{false};
  std::thread leaked([&stop] {
    while (!stop.load(std::memory_order_relaxed))
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  });
  leaked.detach();                                  // 真泄漏：无人回收
  const int during = wait_thread_count(base, 300);
  if (during == base) {
    std::fprintf(stderr,
                 "DISCRIMINATION_FAIL: 常驻泄漏线程未被判红 (base=%d during=%d)\n",
                 base, during);
    ++failures;
  }
  std::printf("  reclaim-wait leak-detected=%d (base=%d during=%d)\n",
              during != base ? 1 : 0, base, during);
  stop.store(true);
  const int after = wait_thread_count(base, kReclaimDeadlineMs);
  CHECK(after == base);                             // 线程退出后必须回到基线
}

// 采样线程：run() 进行中记录线程数峰值（"池确实起过"的非退化证据）
struct PeakSampler {
  std::atomic<bool> stop{false};
  std::atomic<bool> ready{false};
  std::atomic<int> peak{0};
  std::thread th;
  void start() {
    th = std::thread([this] {
      ready.store(true);
      while (!stop.load()) {
        const int n = thread_count();
        int p = peak.load();
        while (n > p && !peak.compare_exchange_weak(p, n)) {}
        std::this_thread::sleep_for(std::chrono::microseconds(200));
      }
    });
    while (!ready.load()) std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
  void finish() {
    stop.store(true);
    if (th.joinable()) th.join();
  }
};

static NormalizeFrame make_frame(std::uint64_t fid, int compute_ms) {
  NormalizeFrame f;
  f.frame_id = fid;
  f.prefetch_keys.push_back("gaia_" + std::to_string(fid % 3));
  NormalizeNode n;
  n.id = "compute";
  n.consumes = {prefetch_block_name(f.prefetch_keys[0])};
  n.run = [compute_ms](BlockFrame&) {
    const auto t0 = std::chrono::steady_clock::now();
    volatile double acc = 0.0;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - t0).count() < compute_ms) {
      for (int i = 0; i < 2000; ++i) acc += 1.0 / (i + 1.0);
    }
    return true;
  };
  f.nodes.push_back(n);
  return f;
}

static MosaicFrameInput make_mosaic_frame(std::uint64_t fid, int n_tiles) {
  MosaicFrameInput f;
  f.frame_id = fid;
  for (int t = 0; t < n_tiles; ++t)
    f.tile_bytes[static_cast<std::uint64_t>(t)] = 4096u + static_cast<std::uint64_t>(t % 17);
  return f;
}

static double ref_pixel(int x, int y) {
  return 100.0 + static_cast<double>((x * 3 + y * 7) % 53);
}

// 5 轮 run：每轮 run 返回后线程数必须回到基线（池已全部 join 回收）；
// 且 run 期间峰值 >= 基线 + 2（非退化：池线程确实起过）。
// base_no_sampler = 无采样线程时的进程线程数；采样线程存活期间基线 = base_no_sampler + 1。
static void check_reclaim(const char* name, int base_no_sampler, int min_pool_threads,
                          const std::function<void(int)>& run_once) {
  PeakSampler sampler;
  sampler.start();
  const int base = thread_count();
  CHECK(base == base_no_sampler + 1);
  for (int i = 0; i < 5; ++i) {
    run_once(i);
    // 判据 = "run 返回后**有限时间内**回到基线"，不是 join 返回瞬间的瞬时读数
    // （依据见 wait_thread_count：内核任务条目的清除滞后于 pthread_join 返回）。
    const int after = wait_thread_count(base, kReclaimDeadlineMs);
    if (after != base) {
      std::fprintf(stderr,
                   "LEAK %s round=%d threads_after=%d base=%d (deadline=%dms)\n",
                   name, i, after, base, kReclaimDeadlineMs);
      ++failures;
    }
  }
  const int peak = sampler.peak.load();
  sampler.finish();
  const int final = wait_thread_count(base_no_sampler, kReclaimDeadlineMs);
  std::printf("  pool %-9s base=%d peak=%d final=%d (pool_threads>=%d)\n", name, base, peak,
              final, min_pool_threads);
  CHECK(peak >= base + min_pool_threads);   // 非退化：run 期间确实建了池
  CHECK(final == base_no_sampler);          // 采样线程回收后回到进程基线
}

int main() {
  const int base0 = thread_count();
  std::printf("RT-004-POOL thread baseline=%d\n", base0);

  // 判别力自检（能红能绿）：有界等待对"真泄漏"必须仍判红 —— 先跑，避免
  // "等待窗口把判据等没了" 的退化（AGENTS.md §9：门禁不许静默退化）。
  check_wait_discriminates_real_leak(base0);

  // A. normalize：4 帧 worker + 1 预取线程（cfg 注入），5 轮 run 各自回收
  check_reclaim("normalize", base0, 2, [](int round) {
    NormalizeWorkflowConfig cfg;
    cfg.workers = 4;
    cfg.prefetch_enabled = true;
    cfg.prefetch_threads = 1;
    NormalizeWorkflowScheduler s(cfg);
    s.set_prefetch_loader([](const std::string& k) { return "catalog:" + k; });
    for (int i = 0; i < 12; ++i)
      s.add_frame(make_frame(700 + static_cast<std::uint64_t>(i) +
                                 static_cast<std::uint64_t>(round) * 100, 2));
    auto out = s.run();
    CHECK(out.size() == 12);
    for (const auto& o : out) CHECK(o.ok);
  });

  // B. mosaic：4 个窗口 worker，5 轮 run 各自回收
  check_reclaim("mosaic", base0, 2, [](int) {
    MosaicWindowConfig cfg;
    cfg.workers = 4;
    cfg.window_tiles = 8;
    MosaicWindowScheduler s(cfg);
    for (int i = 0; i < 4; ++i)
      s.add_frame(make_mosaic_frame(300 + static_cast<std::uint64_t>(i), 2048));
    auto out = s.run();
    CHECK(out.size() == 256);
    for (const auto& o : out) CHECK(o.ok);
  });

  // C. export：读/写各 1 + 4 个 compute worker，5 轮 run 各自回收
  check_reclaim("export", base0, 2, [](int round) {
    ExportStreamConfig c;
    c.workers = 4;
    c.sub_block_px = 128;
    c.queue_depth = 4;
    c.output_path = std::string(RT004_POOL_TMP) + "/rt004_pool_w" + std::to_string(round) + ".fits";
    c.wcs_header = "SIMPLE  =                    T\nNAXIS   =                    2\n";
    c.properties = "ASTROCS PROVENANCE\nPROJECT = ACSD\n";
    ExportStreamScheduler s(c);
    s.set_image(512, 512);
    s.set_pixel_fn(ref_pixel);
    auto o = s.run();
    CHECK(o.ok);
  });

  if (failures) {
    std::fprintf(stderr, "RT-004_POOL_FAIL failures=%d\n", failures);
    return 1;
  }
  std::printf("RT-004_POOL_RECLAIM_PASS\n");
  return 0;
}
'''

# 池形态判据（RT-004-POOL-01）——
# 判据意图：线程池必须**有界、可回收、生命周期可控**。三个阶段调度器 (ARCH-502/503/504)
# 的池因此不得是头文件成员（对象级常驻、生命周期不可控），而必须是 run() 作用域内
# 创建、run 返回前全部 join 回收的**有界**池（线程数 = 配置注入的 cfg.workers /
# cfg.prefetch_threads）。本判据与既有判据同向、不放松：
#   test_no_permanent_thread_pool_member_in_headers（头文件无私池成员）、
#   test_no_detach_worker_anywhere（不 detach）、test_no_async_bypass_in_production
#   （不 std::async 绕过预算）、test_scheduler_join_pool_is_bounded_and_reclaimed
#   （scheduler.cpp CORE-006 的同一 per-run join 形态）。
SCHED_SRC = CORE                       # lib/infrastructure/scheduler/src
SCHED_POOL_FILES = ("normalize_workflow.cpp", "mosaic_window.cpp", "export_stream.cpp")
SCHED_POOL_HEADERS = ("normalize_workflow.h", "mosaic_window.h", "export_stream.h")
SCHED_POOL_SRC_DEPS = ("normalize_workflow.cpp", "mosaic_window.cpp", "export_stream.cpp",
                       "block_frame.cpp")
POOL_DECL_RE = re.compile(r"std::vector\s*<\s*std::thread\s*>\s*(\w+)")
POOL_JOIN_RE = re.compile(
    r"for\s*\(\s*auto&\s*t\s*:\s*(\w+)\s*\)\s*if\s*\(\s*t\.joinable\(\)\s*\)\s*t\.join\(\)\s*;")
RUN_DEF_RE = re.compile(r"::run\s*\(\s*\)\s*\{")


def scheduler_pool_violations(name, text):
    """判据：调度器 worker 池必须 run() 作用域有界 + run 返回前全部 join 回收。

    红条件（任一即判红）：
      ① detach：线程脱离生命周期 ⇒ 不可回收；
      ② 池声明出现在 run() 定义之前（文件级/成员/构造期常驻池 ⇒ 生命周期不可控）；
      ③ 建池但无同名池的 join 回收循环（spawn 后不 join ⇒ 泄漏）；
      ④ 无任何池声明（判据非退化要求：调度器必须有界建池）。
    绿条件：池声明在 run() 之后 + emplace_back 建池 + 同名池 join 循环 + 无 detach。
    "在 run() 之内" 由位置 + 运行时线程回收判据（TestRt004SchedulerPoolReclaimedAtRuntime）
    共同锁定：静态判据防形态回退，运行时判据证实际回收。
    """
    v = []
    lines = text.splitlines()
    run_line = next((i for i, ln in enumerate(lines, 1) if RUN_DEF_RE.search(ln)), None)
    if run_line is None:
        return [f"{name}: 未找到 run() 定义（判据无法适用 ⇒ 判红，避免空转）"]
    decls = {}
    for lineno, line in enumerate(lines, 1):
        if line.strip().startswith("//"):
            continue
        if ".detach(" in line:
            v.append(f"{name}:{lineno}: 出现 detach（线程脱离生命周期，无法回收）")
        m = POOL_DECL_RE.search(line)
        if m:
            decls[m.group(1)] = lineno
            if lineno <= run_line:
                v.append(f"{name}:{lineno}: 池 {m.group(1)} 声明在 run() 之前"
                         f"（常驻/成员池，生命周期不可控）")
    if not decls:
        v.append(f"{name}: 未找到 run 作用域池声明（判据非退化要求）")
    joined = {m.group(1) for m in POOL_JOIN_RE.finditer(text)}
    spawned = set(re.findall(r"(\w+)\.emplace_back\s*\(", text))
    for var, lineno in decls.items():
        if var not in spawned:
            v.append(f"{name}:{lineno}: 池 {var} 未见 emplace_back（未真正建池/形态改变）")
        if var not in joined:
            v.append(f"{name}:{lineno}: 池 {var} 未在 run 内 join 回收（泄漏线程）")
    return v


def build_pool_driver(tmp, pool_srcs, exe_name="rt004_pool"):
    """编译池回收 harness（真实链接三个调度器实现，非 mock）。"""
    drv = tmp / "rt004_pool_driver.cpp"
    drv.write_text(_POOL_DRIVER, encoding="utf-8")
    exe = tmp / exe_name
    cmd = ["g++", "-std=c++17", "-O0", f'-DRT004_POOL_TMP="{tmp}"',
           f"-I{INC}", f"-I{REPO / 'third_party'}",
           f"-I{REPO / 'lib' / 'infrastructure' / 'aio' / 'src'}",
           f"-I{REPO / 'lib' / 'algorithms' / 'shared'}",
           str(drv), *[str(p) for p in pool_srcs], "-pthread", "-o", str(exe)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    if r.returncode != 0:
        raise RuntimeError(f"build pool driver failed:\n{r.stderr[-2000:]}")
    return exe


class TestRt004SchedulerPoolRunScoped(unittest.TestCase):
    """ARCH-502/503/504 池形态：run 作用域有界池 + run 内 join 回收（RT-004-POOL-01）。"""

    def test_scheduler_headers_hold_no_thread_container(self):
        """三个阶段调度器头文件不得持有线程容器（永久池成员）。"""
        hits = []
        for h in SCHED_POOL_HEADERS:
            for lineno, line in enumerate(
                    (HDR_PROD_ROOT / h).read_text(encoding="utf-8").splitlines(), 1):
                if POOL_DECL_RE.search(line) and not line.strip().startswith("//"):
                    hits.append(f"{h}:{lineno}: {line.strip()}")
        self.assertEqual(hits, [], f"调度器头文件发现线程容器成员: {hits}")

    def test_scheduler_pools_are_run_scoped_and_joined(self):
        """三个实现文件的池必须 run() 作用域有界 + run 返回前 join 回收。"""
        for name in SCHED_POOL_FILES:
            text = (SCHED_SRC / name).read_text(encoding="utf-8")
            self.assertEqual(scheduler_pool_violations(name, text), [],
                             f"{name} 池形态违反 run 作用域回收判据")

    def test_criterion_goes_red_on_injected_non_reclaim(self):
        """判据自测（能红能绿）：三种注入必须判红，原样必须判绿。"""
        for name in SCHED_POOL_FILES:
            text = (SCHED_SRC / name).read_text(encoding="utf-8")
            self.assertEqual(scheduler_pool_violations(name, text), [],
                             f"{name} 原样必须判绿")
            m = POOL_JOIN_RE.search(text)
            self.assertIsNotNone(m, f"{name} 未找到 join 回收循环（注入点）")
            # 注入①：join → detach
            mut = text.replace(m.group(0), m.group(0).replace("t.join()", "t.detach()"))
            self.assertNotEqual(mut, text)
            v = scheduler_pool_violations(name, mut)
            self.assertTrue(any("detach" in x for x in v), f"{name} detach 注入必须判红: {v}")
            # 注入②：删除 join 回收（故意不 join）
            mut = text.replace(m.group(0), "")
            self.assertNotEqual(mut, text)
            v = scheduler_pool_violations(name, mut)
            self.assertTrue(any("join" in x for x in v), f"{name} 不 join 注入必须判红: {v}")
            # 注入③：池声明移出 run()（常驻池形态）
            d = POOL_DECL_RE.search(text)
            self.assertIsNotNone(d, f"{name} 未找到池声明（注入点）")
            mut = d.group(0) + "\n" + text.replace(d.group(0), "")
            v = scheduler_pool_violations(name, mut)
            self.assertTrue(any("run() 之前" in x for x in v),
                            f"{name} 池移出 run() 必须判红: {v}")


@unittest.skipUnless(shutil.which("g++") and sys.platform.startswith("linux"),
                     "需要 g++ 与 /proc/self/task 线程计数（Linux）")
class TestRt004SchedulerPoolReclaimedAtRuntime(unittest.TestCase):
    """运行时判据：run 返回后线程数回到基线（池已回收）+ 峰值证明池确实起过。"""

    @classmethod
    def setUpClass(cls):
        cls.tmp = pathlib.Path(tempfile.mkdtemp(prefix="rt004_pool_"))
        cls.exe = build_pool_driver(cls.tmp, [SCHED_SRC / n for n in SCHED_POOL_SRC_DEPS])

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_run_scoped_pools_reclaimed_no_thread_leak(self):
        r = subprocess.run([str(self.exe)], capture_output=True, text=True, timeout=600)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertIn("RT-004_POOL_RECLAIM_PASS", r.stdout)
        # 非退化：三个阶段调度器都必须被观测到池线程峰值（否则判据是恒真门）
        self.assertEqual(r.stdout.count("pool "), 3, r.stdout)

    def test_injected_missing_join_is_red(self):
        """负例（必红）：删掉 normalize run() 的 join 回收 ⇒ 池不回收 ⇒ 判红。

        注入后池向量析构时线程仍 joinable ⇒ std::terminate（rc≠0）；若线程存活则
        线程数不回落（LEAK）并 rc≠0。两种失败模式都算判据红。
        """
        src = (SCHED_SRC / "normalize_workflow.cpp").read_text(encoding="utf-8")
        m = POOL_JOIN_RE.search(src)
        self.assertIsNotNone(m, "未找到 join 回收循环（注入点）")
        mut = src.replace(m.group(0), "  // 负例注入：故意不 join（池线程不回收）")
        self.assertNotEqual(mut, src, "注入点未命中（负例必须真注入）")
        mut_path = self.tmp / "mut_normalize_workflow.cpp"
        mut_path.write_text(mut, encoding="utf-8")
        srcs = [mut_path if n == "normalize_workflow.cpp" else SCHED_SRC / n
                for n in SCHED_POOL_SRC_DEPS]
        exe = build_pool_driver(self.tmp, srcs, exe_name="rt004_pool_mut")
        r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=600)
        self.assertNotEqual(r.returncode, 0,
                            "注入「不 join」后必须判红（rc=0 ⇒ 判据失效）\n"
                            + r.stdout[-1000:] + r.stderr[-1000:])
        self.assertNotIn("RT-004_POOL_RECLAIM_PASS", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
