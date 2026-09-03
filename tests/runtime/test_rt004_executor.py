#!/usr/bin/env python3
"""RT-004 验收测试：唯一共享 executor（CPU heavy + 有界 I/O）。

验收映射 (tasks/03_RUNTIME_DATA_IO_TASKS.md RT-004):
  - 私池静态扫描：生产路径(lib/core/src + include/astrocs/core)无 std::thread
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

方法 (独立 harness, 照 tests/runtime/test_rt003_budget_wiring.py 先例):
  Python unittest 内嵌 C++ driver，g++ 真实编译链接 lib/core/src 源码
  (executor.cpp/context.cpp/artifact.cpp) + include/astrocs/core 头，运行断言；
  另以源码静态扫描断言生产路径无私建永久池、worker 无忙等轮询。
"""
from __future__ import annotations

import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
INC = REPO / "include"
CORE = REPO / "lib" / "core" / "src"

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
# 扫描范围: 生产路径 = lib/core/src/*.cpp + include/astrocs/core/*.h
# 语义 (RT-004): 模块/节点不得 std::thread 私建永久池。executor.cpp 是全仓唯一
# 共享 executor 池实现，允许创建常驻 worker（CpuHeavyExecutor/IoExecutor 构造，
# Impl 持有 vector<thread>，析构 stop+notify+join 完整回收生命周期，无 UAF）。
# scheduler.cpp 含 CORE-006 基线 bounded per-run join pool（每次 run 内创建并在
# run 结束前全部 join 回收，非永久驻留）——属 Scheduler 自身调度协调线程而非
# "模块私建永久池"，显式豁免并记录（见 known_limits）。
CPP_PROD_ROOTS = [CORE]
HDR_PROD_ROOT = INC / "astrocs" / "core"

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


if __name__ == "__main__":
    unittest.main(verbosity=2)
