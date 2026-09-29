#!/usr/bin/env python3
"""P-174 三终态（已发布且持久化 / 已发布但持久化未确认 / 未发布）可执行正负例。

用例（同一断言集跑两遍，差别只在 LD_PRELOAD 注入）:
  A 正例  : 无 preload 正常发布 file / dir
            => status=OK、renamed=1、durability=DURABLE、目标可见、无 tmp 残留
  B 负例  : LD_PRELOAD=fsync_dir_fail_interposer + ASTROCS_FAIL_DIR_FSYNC=1
            （只让 rename **之后**落在目录 fd 上的 fsync 失败；文件 fd 与
             rename 前 staging 树的 fsync 放行）
            => status=ERR_IO、renamed=1、durability=NOT_DURABLE、
               **已发布对象仍在**（第三态 kNotDurable 的存在证据）、无 tmp 残留
  两例的 status/durability 必须不同（负例不得空转）。

判红（源码变异）：把实现里的 kNotDurable 赋值删掉/改回只置 kErrIo 后重跑本脚本，
B 例必须 FAIL（durability 会退化为 NOT_PUBLISHED）。

权威依据: docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md §4（步序正本）/§6（错误语义）;
          docs/detail/infrastructure/17_aio.md §4（P-174 三终态）;
          docs/ASTROCS_DESIGN.md §10（原子发布链）。
用法: check_atomic_durability.py <probe_exe> <interposer.so> <workdir>
"""
import os
import shutil
import subprocess
import sys
import tempfile

# LD_PRELOAD 以空格/冒号分词，而本仓库工作目录含空格
# （".../Astro CS Database"）⇒ 先把注入器复制到无空格的临时目录再 preload，
# 否则 ld.so 会把它当多个对象名丢弃（负例会**静默空转**）。
PRELOAD_SAFE_DIR = None

MODES = ("file", "dir")

# 正例：已发布且持久化已确认。
POSITIVE = dict(status="OK", renamed="1", durability="DURABLE", visible="1",
                target_exists="1", tmp_residue="0", consistent="1")
# 负例：已发布但持久化未确认（第三态）——目标可见且不可回滚。
INJECTED = dict(status="ERR_IO", renamed="1", durability="NOT_DURABLE", visible="1",
                target_exists="1", tmp_residue="0", consistent="1")


def safe_preload_path(interposer):
    """把注入器复制到无空格路径（gettempdir()）再 preload；返回该路径。"""
    global PRELOAD_SAFE_DIR
    if PRELOAD_SAFE_DIR is None:
        PRELOAD_SAFE_DIR = os.path.join(tempfile.gettempdir(),
                                        "aio_p174_fsync_dir_fail_interposer.so")
    shutil.copy2(interposer, PRELOAD_SAFE_DIR)
    return PRELOAD_SAFE_DIR


def run_probe(probe, mode, workdir, preload):
    shutil.rmtree(workdir, ignore_errors=True)
    os.makedirs(workdir)
    env = dict(os.environ)
    if preload:
        env["LD_PRELOAD"] = preload
        env["ASTROCS_FAIL_DIR_FSYNC"] = "1"
    else:
        env.pop("LD_PRELOAD", None)
        env.pop("ASTROCS_FAIL_DIR_FSYNC", None)
    p = subprocess.run([probe, mode, workdir], stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, text=True, env=env, timeout=120)
    return p


def parse(raw):
    fields = None
    for line in raw.splitlines():
        if line.startswith("RESULT "):
            fields = {}
            for tok in line[len("RESULT "):].split():
                key, _, val = tok.partition("=")
                fields[key] = val
        elif line.startswith("TARGET ") and fields is not None:
            # 路径可含空格 ⇒ 独立行整体取值，不做空格分词。
            fields["target"] = line[len("TARGET "):]
        elif line.startswith("MESSAGE ") and fields is not None:
            fields["message"] = line[len("MESSAGE "):]
    return fields


def check(fields, want, case):
    bad = []
    for key in sorted(want):
        got = fields.get(key)
        if got != want[key]:
            bad.append("  FAIL case=%s field=%s want=%s got=%r"
                       % (case, key, want[key], got))
    return bad


def published_object_present(mode, target):
    """独立于探针自报字段的复核：目标路径及其内容仍在（第三态证据）。"""
    if mode == "file":
        if not os.path.isfile(target):
            return False
        with open(target, "rb") as f:
            return f.read() == b"hello-durability-v1"
    tile = os.path.join(target, "norder6", "tile.fits")
    props = os.path.join(target, "properties")
    if not (os.path.isfile(tile) and os.path.isfile(props)):
        return False
    with open(tile, "rb") as f:
        return f.read() == b"tile-durability-v1"


def main(argv):
    if len(argv) != 4:
        print("usage: check_atomic_durability.py <probe> <interposer.so> <workdir>")
        return 2
    probe, interposer, workdir = (os.path.abspath(a) for a in argv[1:4])
    for path, what in ((probe, "probe"), (interposer, "interposer")):
        if not os.path.exists(path):
            print("FAIL missing %s: %s" % (what, path))
            return 1
    shutil.rmtree(workdir, ignore_errors=True)

    fails = []
    see = {}
    for mode in MODES:
        for case, want in (("positive", POSITIVE), ("injected", INJECTED)):
            preload = "" if case == "positive" else safe_preload_path(interposer)
            wd = os.path.join(workdir, case, mode)
            p = run_probe(probe, mode, wd, preload)
            raw = (p.stdout or "") + (p.stderr or "")
            print("=== case=%s mode=%s rc=%d preload=%s ===" %
                  (case, mode, p.returncode, "yes" if preload else "no"))
            print(raw.rstrip() or "(no output)")
            fields = parse(p.stdout or "")
            if p.returncode != 0 or fields is None:
                fails.append("  FAIL case=%s mode=%s: rc=%d, no RESULT line"
                             % (case, mode, p.returncode))
                continue
            if "cannot be preloaded" in raw:
                fails.append("  FAIL case=%s mode=%s: LD_PRELOAD 未生效（ld.so 丢弃了注入器）"
                             " => 负例空转" % (case, mode))
            if case == "injected":
                # 非空转证据：注入器确实在 rename 之后的目录 fd 上拦到了 fsync。
                if "EVENT FSYNC-DIR-FAIL" not in raw:
                    fails.append("  FAIL case=injected mode=%s: 注入器未触发目录 fsync 失败"
                                 " => 负例空转" % mode)
            if fields.get("mode") != mode:
                fails.append("  FAIL case=%s: mode field=%r" % (case, fields.get("mode")))
            fails += check(fields, want, "%s/%s" % (case, mode))
            # P-174 第三态的存在证据：注入后已发布对象**仍在**（不得被清理）。
            target = fields.get("target", "")
            if case == "injected":
                if not published_object_present(mode, target):
                    fails.append("  FAIL case=%s mode=%s: published object gone at %s "
                                 "(third state lost)" % (case, mode, target))
            elif not published_object_present(mode, target):
                fails.append("  FAIL case=%s mode=%s: positive publish not visible at %s"
                             % (case, mode, target))
            see[(case, mode)] = fields

    # 负例不得空转：注入与不注入必须给出不同的 status 与 durability。
    for mode in MODES:
        pos = see.get(("positive", mode), {})
        inj = see.get(("injected", mode), {})
        if pos.get("status") == inj.get("status"):
            fails.append("  FAIL mode=%s: injected status == positive status (%r) "
                         "=> negative case is vacuous"
                         % (mode, pos.get("status")))
        if pos.get("durability") == inj.get("durability"):
            fails.append("  FAIL mode=%s: injected durability == positive durability (%r)"
                         % (mode, pos.get("durability")))

    if fails:
        print("\n".join(fails))
        print("P-174 DURABILITY FAIL: %d assertion(s)" % len(fails))
        return 1
    print("P-174 DURABILITY PASS: three terminal states verified "
          "(OK/DURABLE + ERR_IO/NOT_DURABLE with published object still present)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
