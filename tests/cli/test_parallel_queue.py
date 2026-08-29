#!/usr/bin/env python3
"""PAR-001 测试: 有界队列/backpressure/error/cancel + I/O与compute overlap + 无全局串行锁。
验收(03 PAR-001): lock contention test、queue saturation、failure drain; CPU compute 不被 writer 饿死。"""
import os, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PHASE2_INC = os.path.join(REPO, "lib", "phase2", "include")


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestParallelQueue(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="par001_")
        cls.drv = os.path.join(cls.tmp, "par_driver.cpp")
        with open(cls.drv, "w") as f:
            f.write(r'''
#include "astro/phase2/async_io.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
using namespace astro::phase2;
using Clk = std::chrono::steady_clock;
static double now_ms(Clk::time_point t0){ return std::chrono::duration<double,std::milli>(Clk::now()-t0).count(); }
int main(int argc, char** argv){
    const std::string c = (argc>1)?argv[1]:"";
    if (c=="capacity"){          // 容量推导
        std::printf("cap=%zu\n", bounded_queue_capacity(1024*1024, 1024)); // 1024
        std::printf("cap0=%zu\n", bounded_queue_capacity(0, 1024));         // at least 1
        std::printf("capz=%zu\n", bounded_queue_capacity(1024*1024, 0));    // at least 1
    }
    else if (c=="overlap"){      // I/O(生产)与 compute(消费) overlap
        // 生产: 每项 sleep 模拟读 I/O。计算只 sleep 少量。若串行 => 总时 ≈ N*(Tio+Tcmp)。
        // 若 overlap => 总时 ≈ max(N*Tio, N*Tcmp) 而非加和。
        const int N=20; const long Tio=4, Tcmp=2;  // ms/项
        BoundedAsyncQueue<int> q(4);
        std::atomic<bool> done{false};
        std::thread prod([&]{
            for (int i=0;i<N;++i){ q.push(i); std::this_thread::sleep_for(std::chrono::milliseconds(Tio)); }
            q.close();
        });
        std::atomic<long> consumed{0};
        std::vector<std::thread> cmp;
        for (int w=0;w<2;++w) cmp.emplace_back([&,w]{
            while (auto v = q.pop()){ std::this_thread::sleep_for(std::chrono::milliseconds(Tcmp)); consumed.fetch_add(1); }
        });
        auto t0=Clk::now();
        prod.join(); for (auto& t:cmp) t.join();
        double el=now_ms(t0);
        double serial = N*(Tio+Tcmp);      // 若完全串行
        double ideal  = (double)N*Tio;     // 仅 I/O 时间下界
        std::printf("consumed=%ld elapsed=%.1f serial_est=%.1f io_floor=%.1f ratio=%.2f\n",
            consumed.load(), el, serial, ideal, el/serial);
    }
    else if (c=="saturation"){   // 队列满背压: 生产阻塞
        BoundedAsyncQueue<int> q(2);   // 容量2
        std::thread prod([&]{
            for (int i=0;i<4;++i){ q.push(i); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
            q.close();
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));  // 不做任何消费
        std::size_t sz = q.size();
        std::printf("size=%zu cap=%zu\n", sz, q.capacity());
        // 确保无死锁: 正常退出
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        while (auto v=q.pop()){ /* 排空 */ }
        prod.join();
        std::printf("drained\n");
    }
    else if (c=="failure-drain"){ // 生产者错误→cancel 传播, 消费者 drain, 无死锁
        BoundedAsyncQueue<int> q(4);
        std::thread prod([&]{
            for (int i=0;i<6;++i){ if (!q.push(i)) break; }
            q.cancel("io_read_failed");   // 错误传播
        });
        std::atomic<long> got{0}; std::atomic<bool> saw_err{false};
        std::thread cmp([&]{
            while (auto v=q.pop()){ got.fetch_add(1); }
            if (q.has_error()){ saw_err.store(true); }
        });
        prod.join(); cmp.join();
        std::printf("got=%ld saw_err=%d err=%s\n", got.load(), (int)saw_err.load(), q.error().c_str());
    }
    else if (c=="cancel-wake"){  // cancel 唤醒两端阻塞者(无死锁)
        BoundedAsyncQueue<int> q(4);
        std::thread prod([&]{ for (int i=0;i<100;++i){ if(!q.push(i)) break; } });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        q.cancel("cancel_now");   // 消费者阻塞在 pop, 生产者可能在 push
        std::thread cmp([&]{ int n=0; while (auto v=q.pop()) n++; });
        prod.join(); cmp.join();
        std::printf("cancel_ok\n");
    }
    else { std::printf("unknown-case\n"); return 2; }
    return 0;
}
''')
        cls.exe = os.path.join(cls.tmp, "par_driver")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-pthread",
                            f"-I{PHASE2_INC}", cls.drv, "-o", cls.exe],
                           capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, case):
        o = subprocess.run([self.exe, case], capture_output=True, text=True, timeout=60)
        self.assertEqual(o.returncode, 0, o.stderr)
        return o.stdout.strip()

    def test_01_capacity_derived(self):
        """容量由 memory_budget/item_bytes 推导(禁 0)。"""
        d = self._run("capacity")
        self.assertIn("cap=1024", d)
        self.assertIn("cap0=1", d)   # budget 0 → at least 1
        self.assertIn("capz=1", d)   # item_bytes 0 → at least 1

    def test_02_io_compute_overlap(self):
        """I/O 与 compute 可 overlap: 总时 < 串行求和(证明并行非全局串行锁)。"""
        o = self._run("overlap")
        d = dict(kv.split("=") for kv in o.replace(" ", " ").split() if "=" in kv)
        # consumed=20 实际在单独的 token, 再解析
        vals = {}
        for tok in o.replace(" ", " ").split():
            if "=" in tok: k,v=tok.split("=",1); vals[k]=v
        self.assertGreater(int(vals["consumed"]), 0, o)
        ratio = float(vals["ratio"])
        self.assertLess(ratio, 0.9, f"overlap 应 < 串行(ratio={ratio}); {o}")

    def test_03_queue_saturation_backpressure(self):
        """有界队列满时背压, 不无界增长, 无死锁, 可排出。"""
        o = self._run("saturation")
        self.assertIn("drained", o)
        vals = dict(tok.split("=") for tok in o.replace(" ", " ").split() if "=" in tok)
        self.assertLessEqual(int(vals["size"]), int(vals["cap"]), f"size 应 ≤ cap: {o}")

    def test_04_failure_drain_and_error_propagation(self):
        """生产错误→cancel 传播; 消费者 drain 收到错误; 无死锁。"""
        o = self._run("failure-drain")
        vals = dict(tok.split("=") for tok in o.replace(" ", " ").split() if "=" in tok)
        self.assertEqual(vals["saw_err"], "1", f"消费者须看到错误: {o}")
        self.assertIn("io_read_failed", vals["err"], o)

    def test_05_cancel_wakes_both_no_deadlock(self):
        """cancel 唤醒阻塞在 push/pop 的双方, 无死锁。"""
        o = self._run("cancel-wake")
        self.assertIn("cancel_ok", o)

    def test_06_no_global_serial_lock_construct(self):
        """生产队列使用每队列独立 mutex(非单一全局锁): header 无 static/global mutex。"""
        hdr = os.path.join(PHASE2_INC, "astro", "phase2", "async_io.h")
        content = open(hdr, encoding="utf-8").read()
        # 无 static std::mutex / 全局锁对象(每实例 mutex_ 内聚于类)
        self.assertNotIn("static std::mutex", content)
        self.assertNotIn("static std::condition_variable", content)


if __name__ == "__main__":
    unittest.main(verbosity=2)
