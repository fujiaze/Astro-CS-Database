#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ipv_dead_params_selfcheck.py - P27 死字段防误用机器锁的阴性对照

证明 ipv_dead_params_lock.py 不是恒真的空门:
  P0 基线            : 未变异的影子树 => 锁必须 PASS (绿)
  P1 新接线死字段    : 在 solve_post_select 里读 params.polygon_sides
                       => 锁必须 FAIL 且报 polygon_sides 新引用点
  P2 真消费被删      : 删掉 estimate_mag_lim_iterative 对 params.m_lim_safety
                       的真实读取 => 锁必须 FAIL 且报引用点数不足
  P3 死匹配器被接线  : 在生产链函数里调用 polygon_match(...)
                       => 锁必须 FAIL 且报 dead_symbol
  P4 常数被换成配置  : 把 solve_post_select 的 const int n_target = 60;
                       改成 params.img_n_target => 锁必须 FAIL (冻结字面量 + 新引用点)
  P5 恢复            : 复原影子树 => 锁必须重新 PASS (绿)

影子树只复制清单 source_files + 两个配置头, 不触碰主工作树, 不写任何仓库文件。
用法: ipv_dead_params_selfcheck.py [--src <树根>] [--manifest <json>] [--work <临时目录>]
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SRC = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", ".."))
DEFAULT_MANIFEST = os.path.join(HERE, "ipv_dead_params_manifest.json")
LOCK = os.path.join(HERE, "ipv_dead_params_lock.py")


def shadow_copy(src, manifest, work):
    """按清单把 source_files + 配置头复制到影子树。"""
    import json
    man = json.load(open(manifest, encoding="utf-8"))
    rels = list(man["source_files"])
    for spec in man["config_structs"].values():
        rels.append(spec["header"])
    for rel in rels:
        s = os.path.join(src, rel)
        d = os.path.join(work, rel)
        os.makedirs(os.path.dirname(d), exist_ok=True)
        shutil.copyfile(s, d)
    return man


def patch(work, rel, old, new):
    p = os.path.join(work, rel)
    text = open(p, encoding="utf-8").read()
    if text.count(old) != 1:
        raise RuntimeError("变异锚点不唯一 (%d): %s\n%s" % (text.count(old), rel, old))
    open(p, "w", encoding="utf-8").write(text.replace(old, new))


def run_lock(work, manifest):
    proc = subprocess.run([sys.executable, LOCK, "--src", work,
                           "--manifest", manifest, "--quiet"],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          timeout=180)
    return proc.returncode, proc.stdout.decode("utf-8", "replace")


def main(argv=None):
    ap = argparse.ArgumentParser(description="P27 死字段锁阴性对照")
    ap.add_argument("--src", default=os.environ.get("P27_SRC", DEFAULT_SRC))
    ap.add_argument("--manifest", default=DEFAULT_MANIFEST)
    ap.add_argument("--work", default=None)
    args = ap.parse_args(argv)

    work = args.work or tempfile.mkdtemp(prefix="p27_selfcheck_")
    os.makedirs(work, exist_ok=True)
    failures = []
    results = []

    def case(tag, mutate, expect_red, expect_substr):
        for rel in list(os.listdir(work)):
            p = os.path.join(work, rel)
            shutil.rmtree(p) if os.path.isdir(p) else os.remove(p)
        shadow_copy(args.src, args.manifest, work)
        try:
            if mutate:
                mutate()
            rc, out = run_lock(work, args.manifest)
        except Exception as exc:  # noqa: BLE001
            failures.append("%s: 执行异常 %s" % (tag, exc))
            results.append((tag, "ERROR", str(exc)))
            return
        red = (rc != 0)
        ok = (red == expect_red)
        if expect_red and expect_substr and expect_substr not in out:
            ok = False
        results.append((tag, "RED" if red else "GREEN",
                        "OK" if ok else "MISMATCH"))
        if not ok:
            failures.append("%s: 期望 %s, 实际 %s (rc=%d)"
                            % (tag, "RED" if expect_red else "GREEN",
                               "RED" if red else "GREEN", rc))
            print("---- %s 输出 (尾 20 行) ----" % tag)
            for ln in out.strip().split("\n")[-20:]:
                print("   " + ln)
        else:
            if expect_red:
                hit = [ln.strip() for ln in out.split("\n")
                       if expect_substr and expect_substr in ln]
                print("[%s] RED (符合预期): %s" % (tag, hit[0] if hit else ""))
            else:
                print("[%s] GREEN (符合预期)" % tag)

    SOL = "lib/plate_solve/cpp/ipv/src/ipv_solver.cpp"
    SEL = "lib/plate_solve/cpp/ipv/src/ipv_select.cpp"

    case("P0-基线-必须绿", None, False, None)

    ANCHOR = "    const int n_target = 60;  // 与 solve_from_memory 的 {60} 一致"

    def m1():
        patch(work, SOL, ANCHOR,
              ANCHOR + "\n"
              "    volatile int p27_neg = params.polygon_sides; (void)p27_neg;")
    case("P1-死字段新接线-必须红", m1, True, "polygon_sides")

    def m2():
        patch(work, SEL,
              "    const double safety   = (params.m_lim_safety > 0.0) ? params.m_lim_safety : 3.0;",
              "    const double safety   = 3.0;")
    case("P2-真消费被删-必须红", m2, True, "m_lim_safety")

    def m3():
        patch(work, SOL, ANCHOR,
              ANCHOR + "\n"
              "    { if (false) { KVectorIndex p27_kv{};"
              " (void)polygon_match(selection.U, selection.W, p27_kv, params, 1.0); } }")
    case("P3-死匹配器被接线-必须红", m3, True, "dead_symbol polygon_match")

    def m4():
        patch(work, SOL, ANCHOR,
              "    const int n_target = params.img_n_target;")
    case("P4-常数换成配置-必须红", m4, True, "n_target=60")

    case("P5-恢复-必须绿", None, False, None)

    print()
    print("=" * 78)
    for tag, got, verdict in results:
        print("  %-34s %-6s %s" % (tag, got, verdict))
    print("=" * 78)
    if failures:
        print("P27_DEAD_PARAMS_SELFCHECK FAIL (%d 项):" % len(failures))
        for f in failures:
            print("  - " + f)
        return 1
    print("P27_DEAD_PARAMS_SELFCHECK PASS: 锁对 4 类反例均变红, 恢复后变绿 (非恒真)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
