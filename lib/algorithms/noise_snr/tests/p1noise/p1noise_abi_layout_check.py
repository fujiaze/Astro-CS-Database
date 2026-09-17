#!/usr/bin/env python3
"""p1noise_abi_layout_check — MASK-002 (claim SC-009) C ABI 版本化证据门。

先例: lib/infrastructure/aio/tests/abi/aio_abi_layout_lock.py (SCI-FIX-AIO SC-006)。

判据（三条，全过 rc=0）:
  1. 布局锁定: 编译 p1noise_abi_layout_probe.cpp（生产头零改动）→ 打印的
     struct_size@0 / abi_version@4 / sizeof / 5 个掩膜参数偏移 / 3 个诊断偏移
     必须与 p1noise_abi_layout_lock.json **逐值相等**（改布局即红）;
  2. 版本常量在场: SNR_NOISE_CONFIG_ABI_VERSION / SNR_NOISE_MODEL_ABI_VERSION
     必须出现在探针输出且为本锁文件记录值;
  3. 突变自检 (真红证明): 在临时目录里把 ABI 头部宏的两个字段**交换顺序**
     （制造真实布局漂移）后用该头重编译探针 → 比对**必须失败**（门不是恒绿）。
用法: python3 p1noise_abi_layout_check.py [--self-test]
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def _repo_root(start):
    cur = start
    while True:
        if (os.path.isfile(os.path.join(cur, "ASTROCS_DESIGN.md"))
                and os.path.isfile(os.path.join(cur, "CMakeLists.txt"))):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            raise RuntimeError("repo root not found from %s" % start)
        cur = parent


REPO = _repo_root(HERE)
SNR_INC = os.path.join(REPO, "lib", "algorithms", "noise_snr", "cpp", "include")
PROBE = os.path.join(HERE, "p1noise_abi_layout_probe.cpp")
LOCK = os.path.join(HERE, "p1noise_abi_layout_lock.json")


def compile_and_run(inc_dir, tmp):
    exe = os.path.join(tmp, "probe")
    r = subprocess.run(["g++", "-std=c++17", "-O0", "-I" + inc_dir, PROBE, "-o", exe],
                       capture_output=True, text=True, timeout=600)
    if r.returncode != 0:
        return None, "compile failed: %s" % r.stderr[-400:]
    r = subprocess.run([exe], capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        return None, "probe rc=%d stderr=%s" % (r.returncode, r.stderr[-200:])
    try:
        return json.loads(r.stdout), None
    except Exception as exc:  # pragma: no cover
        return None, "probe stdout not JSON: %s (%s)" % (r.stdout[:200], exc)


def compare(got, want):
    """返回不一致项列表 (逐值, 含嵌套)。"""
    bad = []
    for k, v in want.items():
        if k not in got:
            bad.append("missing %s" % k)
        elif isinstance(v, dict):
            for kk, vv in v.items():
                if got[k].get(kk) != vv:
                    bad.append("%s.%s: got=%r want=%r" % (k, kk, got[k].get(kk), vv))
        elif got[k] != v:
            bad.append("%s: got=%r want=%r" % (k, got[k], v))
    return bad


def main():
    self_test = "--self-test" in sys.argv
    if not shutil.which("g++"):
        print("SKIP: 缺 g++，ABI 布局门跳过")
        return 0
    if not os.path.isfile(LOCK):
        print("FAIL: 缺锁定文件 %s" % LOCK)
        return 1
    with open(LOCK, encoding="utf-8") as f:
        want = json.load(f)
    tmp = tempfile.mkdtemp(prefix="p1noise_abi_")
    try:
        got, err = compile_and_run(SNR_INC, tmp)
        if got is None:
            print("FAIL: %s" % err)
            return 1
        bad = compare(got, want)
        if bad:
            print("FAIL: ABI 布局与锁定文件不一致 (%d 项)" % len(bad))
            for b in bad:
                print("  " + b)
            return 1
        print("ABI-LAYOUT PASS: config sizeof=%d model sizeof=%d, struct_size@0/abi_version@4, "
              "版本 config=%d model=%d"
              % (got["config"]["sizeof"], got["model"]["sizeof"],
                 got["config"]["abi_version"], got["model"]["abi_version"]))

        if not self_test:
            return 0

        # 突变自检: 头部字段顺序交换 ⇒ 偏移漂移 ⇒ 探针**必须红**
        # (编译期 static_assert 或运行期锁定比对, 二者任一即证明门有牙)
        mut_inc = os.path.join(tmp, "inc")
        os.makedirs(mut_inc, exist_ok=True)
        src = open(os.path.join(SNR_INC, "snr_estimator.h"), encoding="utf-8").read()
        needle = ("#define SNR_ABI_HEADER(TYPE, VERSION)      \\\n"
                  "    uint32_t struct_size; /* = sizeof(TYPE) @0 */ \\\n"
                  "    uint32_t abi_version; /* = VERSION @4 */")
        repl = ("#define SNR_ABI_HEADER(TYPE, VERSION)      \\\n"
                "    uint32_t abi_version; /* = VERSION @4 */ \\\n"
                "    uint32_t struct_size; /* = sizeof(TYPE) @0 */")
        if needle not in src:
            print("FAIL: 突变自检锚点缺失 (SNR_ABI_HEADER 宏已被改动)")
            return 1
        with open(os.path.join(mut_inc, "snr_estimator.h"), "w", encoding="utf-8") as f:
            f.write(src.replace(needle, repl))
        tmp2 = os.path.join(tmp, "mut")
        os.makedirs(tmp2, exist_ok=True)
        got_m, err_m = compile_and_run(mut_inc, tmp2)
        if got_m is None:
            # 编译期 static_assert 捕获漂移 = 门的更强形态 (布局不变量在编译期成立)
            if "static assertion failed" in (err_m or "") or "static_assert" in (err_m or ""):
                print("SELF-TEST PASS: 头部字段交换 → 编译期 static_assert 必红")
                return 0
            print("SELF-TEST FAIL: 突变头编译/运行失败且非 static_assert: %s" % err_m)
            return 1
        bad_m = compare(got_m, want)
        if not bad_m:
            print("SELF-TEST FAIL: 头部字段交换后布局门仍判绿 — 门恒绿, 无效")
            return 1
        print("SELF-TEST PASS: 头部字段交换 → 门必红 (%s)" % bad_m[0])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
