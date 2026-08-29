#!/usr/bin/env python3
"""PAR-007 测试: 统一 thread budget 合同 — 嵌套 stress 下 Σ(active) ≤ max_workers 不超标。
验收(03 PAR-007): nested stress 时 threads/CPU/RAM 不超合同; 端到端利用率 PASS。
对接 ARCH-004: backend kernel 经 host->budget.acquire/release 租借(CAS: Σ(active)+n≤max_workers);
禁硬编码线程数(static checker: tools/arch/check_thread_budget.py 未登记线程创建=0 硬编码线程数=0)。
本测试验证 budget acquire 合同在并发压力下不超标(失败即快速释放/重试, 05 §6)。
"""
import os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INC = os.path.join(REPO, "include")
HOST = os.path.join(REPO, "lib", "backend_host")

_DRV = r'''
#include "astrocs/common_abi_v1.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers, astrocs_host_services_v1* out);
}
int main(int argc,char**argv){
    int maxw=argc>1?atoi(argv[1]):2, nreq=argc>2?atoi(argv[2]):8;
    astrocs_host_services_v1 host; void* state=nullptr; astrocs_host_services_default_v1(&host,&state);
    astrocs_host_state_set_budget_v1(state,(uint32_t)maxw,(uint32_t)maxw,&host);
    std::atomic<int> hold{0}, peak_hold{0}, granted{0};
    std::vector<std::thread> ths;
    for(int i=0;i<nreq;++i){
        ths.emplace_back([&](){
            while(true){
                if(host.budget.acquire(host.budget.user_data,1)!=0){ continue; }  // 重试直到获准
                granted.fetch_add(1);
                int h=hold.fetch_add(1)+1;
                int p=peak_hold.load(); while(h>p && !peak_hold.compare_exchange_weak(p,h)) {}
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                hold.fetch_sub(1);
                host.budget.release(host.budget.user_data,1);
                return;
            }
        });
    }
    for(auto&t:ths) t.join();
    printf("BUDGET maxw=%d nreq=%d max_hold=%d granted=%d\n",maxw,nreq,peak_hold.load(),granted.load());
    astrocs_host_services_destroy_state_v1(state); return 0; }
'''


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestBudgetContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="par007_")
        cls.drv = os.path.join(cls.tmp, "bud.cpp")
        with open(cls.drv, "w") as f:
            f.write(_DRV)
        cls.exe = os.path.join(cls.tmp, "bud")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-fopenmp", f"-I{INC}", f"-I{HOST}",
                            cls.drv, os.path.join(HOST, "host_services.cpp"),
                            "-ldl", "-lgomp", "-pthread", "-o", cls.exe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-400:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, maxw, nreq):
        r = subprocess.run([self.exe, str(maxw), str(nreq)], capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-200:])
        m = re.search(r"max_hold=(\d+) granted=(\d+)", r.stdout)
        self.assertIsNotNone(m, r.stdout)
        return {"max_hold": int(m.group(1)), "granted": int(m.group(2))}

    def test_01_no_oversubscription_budget2(self):
        """max_workers=2, 8 并发请求 → 同时持有峰值 ≤2(合同 Σ(active)≤budget)。"""
        r = self._run(2, 8)
        self.assertLessEqual(r["max_hold"], 2, f"超标: max_hold={r['max_hold']}")
        self.assertEqual(r["granted"], 8, "全部请求应最终获准(不饿死)")

    def test_02_no_oversubscription_budget1(self):
        """max_workers=1 时峰值 ≤1。"""
        r = self._run(1, 6)
        self.assertLessEqual(r["max_hold"], 1, f"超标: max_hold={r['max_hold']}")

    def test_03_static_checker_no_hardcoded_threads(self):
        """ARCH-004 static checker: 生产源无未登记线程创建/硬编码线程数。"""
        r = subprocess.run([os.sys.executable, os.path.join(REPO, "tools", "arch", "check_thread_budget.py")],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("未登记线程创建=0", r.stdout)
        self.assertIn("硬编码线程数=0", r.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
