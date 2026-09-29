#!/usr/bin/env python3
"""P-174 第三态变异判红 harness（能红能绿）。

对 lib/infrastructure/aio/product_io 源码**副本**注入「回到旧行为」的实现错误
—— 删掉 rename 之后目录 fsync 失败时的 kNotDurable 赋值（该赋值是第三态的唯一
来源；删掉后 durability 退化为默认 kNotPublished，即旧口径「失败 ⇒ 无正式产品」）
—— 重新配置/编译探针，断言同一套断言（check_atomic_durability.py）必须 FAIL。
正控制：未变异源码同一套断言 rc == 0。

依据: docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md §4/§6;
      docs/detail/infrastructure/17_aio.md §4（P-174 三终态）。
用法: durability_mutation_and_check.py <repo_root> <workdir>
"""
import os
import shutil
import subprocess
import sys

# 变异锚点 = 第三态唯一赋值点（file 路径 + dir 路径各一处，共 2 次）。
MUT_OLD = "      res.durability = PublishDurability::kNotDurable;\n"
MUT_NEW = ""


def run(cmd, timeout=900):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, timeout=timeout)


def main(argv):
    if len(argv) != 3:
        print("usage: durability_mutation_and_check.py <repo_root> <workdir>")
        return 2
    repo_root = os.path.abspath(argv[1])
    workdir = os.path.abspath(argv[2])
    tests_dir = os.path.join(repo_root, "eng/tests/unit/aio")
    src_root = os.path.join(repo_root, "lib/infrastructure/aio/product_io")
    third = os.path.join(repo_root, "lib/third_party")
    checker = os.path.join(tests_dir, "check_atomic_durability.py")
    os.makedirs(workdir, exist_ok=True)

    def build_and_run(lib_dir, tag):
        build = os.path.join(workdir, "build_" + tag)
        shutil.rmtree(build, ignore_errors=True)
        cfg = run(["cmake", "-S", tests_dir, "-B", build,
                   "-DCMAKE_BUILD_TYPE=Release",
                   "-DAIO_PRODUCT_IO_LIB_DIR=" + lib_dir,
                   "-DAIO_PRODUCT_IO_THIRD_PARTY_DIR=" + third])
        if cfg.returncode != 0:
            return None, "cmake configure failed:\n" + cfg.stdout[-2000:]
        bld = run(["cmake", "--build", build, "--target",
                   "aio_atomic_durability_probe", "aio_fsync_dir_fail_interposer",
                   "-j", "8"])
        if bld.returncode != 0:
            return None, "build failed:\n" + bld.stdout[-2000:]
        probe = os.path.join(build, "aio_atomic_durability_probe")
        interposer = os.path.join(build, "libaio_fsync_dir_fail_interposer.so")
        rundir = os.path.join(workdir, "run_" + tag)
        shutil.rmtree(rundir, ignore_errors=True)
        r = run([sys.executable, checker, probe, interposer, rundir])
        return r.returncode, r.stdout

    # ── 正控制：未变异源码必须绿 ──────────────────────────────────────
    ctrl_rc, ctrl_out = build_and_run(src_root, "control")
    print("=== CONTROL (unmutated source) rc=%s ===" % ctrl_rc)
    print((ctrl_out or "").strip()[-2000:])
    if ctrl_rc != 0:
        print("CONTROL FAIL: unmutated source rc=%s (harness/负例自身坏了, 不是判红)"
              % ctrl_rc)
        return 1

    # ── 变异：删掉 kNotDurable 赋值（回到旧口径） ─────────────────────
    mut_root = os.path.join(workdir, "mut_no_notdurable", "product_io")
    shutil.rmtree(os.path.dirname(mut_root), ignore_errors=True)
    shutil.copytree(src_root, mut_root)
    target = os.path.join(mut_root, "src/atomic_publish.cpp")
    text = open(target, encoding="utf-8").read()
    hits = text.count(MUT_OLD)
    if hits != 2:
        print("ANCHOR-MISS: kNotDurable 赋值锚点命中 %d 次（期望 2: file + dir）" % hits)
        return 1
    open(target, "w", encoding="utf-8").write(text.replace(MUT_OLD, MUT_NEW))
    print("=== MUTATION: removed %d kNotDurable assignments (back to old behaviour) ==="
          % hits)

    mut_rc, mut_out = build_and_run(mut_root, "mut_no_notdurable")
    print("=== MUTATED (injected old behaviour) rc=%s ===" % mut_rc)
    print((mut_out or "").strip()[-3000:])
    if mut_rc is None:
        print("BUILD-FAIL under mutation (cannot judge red)")
        return 1
    if mut_rc == 0:
        print("MISSED: 删掉 kNotDurable 赋值后断言仍绿 => 第三态门无效")
        return 1
    print("P-174 DURABILITY MUTATION PASS: injected old behaviour is caught (rc=%s)"
          % mut_rc)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
