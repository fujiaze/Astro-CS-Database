#!/usr/bin/env python3
"""ARCH-001 生成器本体测试: 扫描次数不随匹配数增长 + 读失败 fail-fast。

`test_inventory.py::test_05` 只证明「磁盘产物 == 生成器输出」; 本文件直接对生成器本体
施加**负向注入**, 证明:
  (a) 生成器对每个符号模式只扫一遍树 —— 共享工作树下（多人并发写 tracked 文件）不再
      出现「清单计数取自另一时刻的重扫」漂移;
  (b) 源文件读取失败必须 fail-fast（宪章 §14.4）, 不得静默跳过 → 产生不完整清单,
      使 §12.3 机器一致性检查被静默削弱。
"""
import json, os, shutil, subprocess, sys, tempfile, textwrap, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GEN = os.path.join(REPO, "tools", "arch", "build_production_execution_inventory.py")

# 计数包装: runpy 执行生成器副本, 统计每个文件被 open() 读取的次数。
_WRAPPER = textwrap.dedent('''
    import builtins, collections, json, runpy, sys
    _opens = collections.Counter()
    _real = builtins.open
    def _counting(file, *a, **k):
        try:
            _opens[str(file)] += 1
        except Exception:
            pass
        return _real(file, *a, **k)
    builtins.open = _counting
    try:
        runpy.run_path(sys.argv[1], run_name="__main__")
        rc = 0
    except BaseException:
        import traceback; traceback.print_exc(); rc = 1
    finally:
        builtins.open = _real
    print("OPENS=" + json.dumps(dict(_opens)), file=sys.stderr)
    sys.exit(rc)
''')

_LOCK_SRC = "#include <mutex>\nstd::mutex g_mu;\nvoid f(){ std::lock_guard<std::mutex> g(g_mu); }\n"
_PLAIN_SRC = "int f%d(void){ return %d; }\n"


def _make_tree(td, n_lock, n_plain):
    """临时仓库树: tools/arch/gen.py(生成器副本) + lib/x/*.cpp + docs/architecture/。"""
    for sub in (("tools", "arch"), ("lib", "x"), ("docs", "architecture")):
        os.makedirs(os.path.join(td, *sub), exist_ok=True)
    shutil.copy(GEN, os.path.join(td, "tools", "arch", "gen.py"))
    for i in range(n_lock):
        with open(os.path.join(td, "lib", "x", "lock%02d.cpp" % i), "w") as f:
            f.write(_LOCK_SRC)
    for i in range(n_plain):
        with open(os.path.join(td, "lib", "x", "plain%02d.cpp" % i), "w") as f:
            f.write(_PLAIN_SRC % (i, i))
    return os.path.join(td, "tools", "arch", "gen.py")


def _run(td, gen):
    wrap = os.path.join(td, "wrap.py")
    with open(wrap, "w") as f:
        f.write(_WRAPPER)
    r = subprocess.run([sys.executable, wrap, gen], cwd=td, capture_output=True,
                       text=True, timeout=300)
    reads = {}
    for line in r.stderr.splitlines():
        if line.startswith("OPENS="):
            reads = json.loads(line[len("OPENS="):])
    return r, reads


class TestInventoryGenerator(unittest.TestCase):
    def test_01_scan_reads_do_not_scale_with_match_count(self):
        """每个模式只扫一遍树: 增大匹配文件数不得增加任何文件的读取次数。

        负向注入形态 = 同一临时树里放 K 个 lock 匹配文件; 旧实现对每个 lock 文件重复
        全树重扫 (1+K 次), 故 K=1 与 K=4 的读取次数不同 → 本测试必败。
        """
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            ga = _make_tree(a, n_lock=1, n_plain=3)     # 4 个 .cpp
            gb = _make_tree(b, n_lock=4, n_plain=0)     # 4 个 .cpp, 匹配数 4x
            ra, reads_a = _run(a, ga)
            rb, reads_b = _run(b, gb)
            self.assertEqual(ra.returncode, 0, ra.stderr[-800:])
            self.assertEqual(rb.returncode, 0, rb.stderr[-800:])
            ka = os.path.join(a, "lib", "x", "lock00.cpp")
            kb = os.path.join(b, "lib", "x", "lock00.cpp")
            self.assertIn(ka, reads_a, sorted(reads_a)[:8])
            self.assertIn(kb, reads_b, sorted(reads_b)[:8])
            self.assertEqual(
                reads_a[ka], reads_b[kb],
                "读取次数随匹配文件数增长 = 存在重复全树重扫 (K=1 -> %d 次, K=4 -> %d 次)"
                % (reads_a[ka], reads_b[kb]))

    def test_02_unreadable_source_fails_fast(self):
        """负向注入: 源文件不可读必须 fail-fast, 不得静默跳过产生不完整清单。"""
        with tempfile.TemporaryDirectory() as td:
            gen = _make_tree(td, n_lock=1, n_plain=1)
            broken = os.path.join(td, "lib", "x", "zz_broken.cpp")
            try:
                os.symlink(os.path.join(td, "lib", "x", "does_not_exist.cpp"), broken)
            except (OSError, NotImplementedError) as exc:   # 平台不支持符号链接
                self.skipTest("本平台不支持创建符号链接: %s" % exc)
            r, _ = _run(td, gen)
            self.assertNotEqual(r.returncode, 0,
                                "读失败被静默吞掉 → 清单不完整且无人知晓")
            self.assertIn("zz_broken.cpp", r.stderr, r.stderr[-800:])

    def test_03_regeneration_is_bitwise_stable(self):
        """同一棵树两次生成逐字节一致 (确定性 bitwise)。"""
        with tempfile.TemporaryDirectory() as td:
            gen = _make_tree(td, n_lock=2, n_plain=3)
            out = os.path.join(td, "docs", "architecture", "PRODUCTION_EXECUTION_INVENTORY.csv")
            r1, _ = _run(td, gen)
            self.assertEqual(r1.returncode, 0, r1.stderr[-800:])
            with open(out, "rb") as f:
                first = f.read()
            r2, _ = _run(td, gen)
            self.assertEqual(r2.returncode, 0, r2.stderr[-800:])
            with open(out, "rb") as f:
                second = f.read()
            self.assertEqual(first, second, "生成器必须逐字节幂等")


if __name__ == "__main__":
    unittest.main(verbosity=2)
