#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P-173 负例（TSan）：next_seq 取号非原子 ⇒ 数据竞争必须被判红。

为什么必须用 TSan：
  台账 P-173 = 「next_seq 非原子 static++」。这是**竞态**属性 —— 普通构建下非原子
  变异体大概率仍"通过"（重复名是小概率事件，且 C++ 数据竞争本身是 UB，可能被优化成
  任何结果），因此普通构建的"绿"**没有判别力**。TSan 对每次非原子访问做影子内存
  记录，能确定性报出 data race ⇒ 变异体必红、正本必绿。

判据（fail-closed，三态分明）:
  A 正本（未变异头）: TSan 运行 rc==0 且输出**不含** "data race"（证明探针/门不恒红）；
  B 变异体（seq++ 非原子）: 输出必须**含** "WARNING: ThreadSanitizer: data race"
    （该注入被检出 ⇒ 判红成立）；若无报告 ⇒ FAIL（变异体不可检出 = 本门无判别力）。
  C 不可判: 编译器不支持 -fsanitize=thread 或 TSan 运行库缺失 ⇒ exit 2，
    **不得**把"跑不起来"读成"通过"。
成本：只编译 1 个可执行文件（探针 TU，无第三方库依赖），不建全量 TSan 树。
用法: python3 tmp_path_tsan_negative.py <repo_root> <workdir>
"""
from __future__ import annotations

import os
import pathlib
import re
import shutil
import subprocess
import sys

PROBE_REL = "eng/tests/unit/aio/tmp_path_uniqueness_probe.cpp"
SRC_REL = "lib/infrastructure/aio/src"
TSAN_FLAGS = ["-std=c++17", "-O1", "-g", "-fsanitize=thread",
              "-fno-omit-frame-pointer", "-pthread"]
ATOMIC_PAT = re.compile(
    r"static std::atomic<uint64_t> seq\{0\};\s*"
    r"return seq\.fetch_add\(1, std::memory_order_seq_cst\) \+ 1;")


def build_and_run(repo: pathlib.Path, work: pathlib.Path, mutate: bool, tag: str):
    inc = work / tag / "include"
    inc.mkdir(parents=True, exist_ok=True)
    shutil.copytree(repo / SRC_REL, inc, dirs_exist_ok=True)
    header = inc / "aio_atomic_file.h"
    text = header.read_text(encoding="utf-8")
    if mutate:
        new, n = ATOMIC_PAT.subn("static uint64_t seq = 0;\n    return ++seq;", text)
        if n != 1:
            print("[fail-closed] 变异锚命中 %d 次（期望 1）—— next_seq 实现已变，"
                  "本负例需同步更新（不得静默跳过）" % n)
            sys.exit(2)
        header.write_text(new, encoding="utf-8")
    exe = work / tag / "probe"
    cmd = ["g++"] + TSAN_FLAGS + ["-I", str(inc), str(repo / PROBE_REL),
                                  "-o", str(exe)]
    c = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
    if c.returncode != 0:
        blob = (c.stderr or "") + (c.stdout or "")
        if "unrecognized" in blob or "not supported" in blob or "cannot find" in blob:
            print("[不可判] 本机编译器/运行库不支持 -fsanitize=thread ⇒ 无法取证\n" + blob[-400:])
            sys.exit(2)
        print("[fail-closed] TSan 编译失败:\n" + blob[-800:])
        sys.exit(2)
    env = dict(os.environ)
    env["TSAN_OPTIONS"] = "halt_on_error=0 exitcode=66"
    r = subprocess.run([str(exe), "8", "5000"], capture_output=True, text=True,
                       timeout=900, env=env)
    return r.returncode, (r.stdout or "") + (r.stderr or "")


def main(argv) -> int:
    if len(argv) < 3:
        print(__doc__)
        return 2
    repo, work = pathlib.Path(argv[1]).resolve(), pathlib.Path(argv[2])
    work.mkdir(parents=True, exist_ok=True)
    print("[info] TSan 负例：只编译 1 个可执行文件（探针 TU），不建全量 TSan 树")

    rc_a, out_a = build_and_run(repo, work, False, "clean")
    # C 态：TSan **运行库启动失败**（容器/内核不支持影子内存映射等）时，
    # 输出是 "FATAL: ThreadSanitizer: ..." 而非竞态报告 —— 必须判"不可判"，
    # 不得读成"没检出竞态"（与 P-163 的"未观测到即不可判"同一条纪律）。
    if "FATAL: ThreadSanitizer" in out_a:
        print("[不可判] TSan 运行库无法启动（非竞态报告）：\n" + out_a[-500:])
        return 2
    races_a = "data race" in out_a or "WARNING: ThreadSanitizer" in out_a
    if races_a or rc_a != 0:
        print("[FAIL] A 正本在 TSan 下不干净（rc=%d, race=%s）—— 正本必须先绿，"
              "否则负例无判别力" % (rc_a, races_a))
        print(out_a[-800:])
        return 1
    print("[ok] A 正本：TSan rc=0 且无 data race（门不恒红）")

    rc_b, out_b = build_and_run(repo, work, True, "mutated")
    if "FATAL: ThreadSanitizer" in out_b:
        print("[不可判] 变异体运行时 TSan 启动失败（非竞态报告）：\n" + out_b[-500:])
        return 2
    if "WARNING: ThreadSanitizer: data race" not in out_b:
        print("[FAIL] B 变异体（next_seq 非原子）未被 TSan 检出 ⇒ 判红失败"
              "（本门对 P-173 缺陷无判别力）；rc=%d" % rc_b)
        print(out_b[-800:])
        return 1
    print("[ok] B 变异体：TSan 报 data race（rc=%d）⇒ next_seq 非原子被检出" % rc_b)
    print("P173_TSAN_NEGATIVE_PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
