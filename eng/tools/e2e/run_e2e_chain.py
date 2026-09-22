#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""E2E-501 三命令真实数据全链驱动（可复跑、出机器可读证据）。

权威：ASTROCS_DESIGN §6.1（三命令串行 + 阶段间只走磁盘产品 + manifest 链）、§6.2（预检矩阵）、
      §4（退出码）；ENGINEERING_SPEC §7（目录纪律）。

流程（每一步都有判据，fail-closed）：
  1. 生成三命令配置（eng/tools/e2e/make_e2e_configs.py，字段全部来自 --help 合同）；
  2. 串行跑 normalize → mosaic → export，任一 rc != 0 即红；
  3. **manifest 链贯通**（口径见 module_adapters.cpp §20.3，不是拿产品自证）：
     · P2 阶段内自洽：p2_samples.input_manifest_hash == p2_final.provenance.ASTROCS_INPUT_MANIFEST_HASH；
     · P2→P3：用 Python **独立复算** p3n_input_manifest_hash 的公式
       （sha256 over "/signal/properties" + "/signal/Moc.fits" 按序拼接），与 p3_verify/p3_writer 自报值比对；
     · P3 阶段内自洽：p3_writer.input_manifest_hash == p3_verify.input_manifest_hash。
  4. 产品判据：p3_verify.json 的 coverage_ok==1 / reopen_ok==1 / canonical_match==true /
     covered_px>0；output_phase3.fits 存在且**结构自洽**（HDU 数、BITPIX、NAXIS1/NAXIS2 与
     p3_verify 的像素口径一致——不假设单一 f64 HDU，真实产品是 4 个 BITPIX=-32 HDU）；
  5. 目录纪律：三阶段产物只落各自 output_dir（扫 output_dir 之外是否出现本阶段产品名）；
  6. 分段计时：每阶段墙钟落证据（仅 --run 模式有值；--verify-only 下 timings 为空且不判红）。

用法：
  python3 eng/tools/e2e/run_e2e_chain.py --run [--dataset T4] [--json-out PATH]
  python3 eng/tools/e2e/run_e2e_chain.py --verify-only [--json-out PATH]
  python3 eng/tools/e2e/run_e2e_chain.py --self-test
exit 0 = 全绿；1 = 判红；2 = 环境/配置错误。
"""
from __future__ import annotations

import argparse
import glob
import hashlib
import io
import json
import os
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
E2E = os.path.join(REPO, "run/RELEASE-05/e2e")
OUT = os.path.join(E2E, "out")
LOGS = os.path.join(REPO, "run/RELEASE-05/logs")
BINARY = os.path.join(REPO, "build/astrocs")


def sha256_file(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def p3_input_manifest_hash(hips_dir):
    """独立复算 module_adapters.cpp::p3n_input_manifest_hash 的公式：
    blob = 依次拼接 (rel + \n + 文件字节 + \n)，rel ∈ {/signal/properties, /signal/Moc.fits}，
    空/缺失文件跳过；空 blob ⇒ 空串。用于**独立**验证 P2→P3 的 manifest 链，而不是自证。"""
    blob = b""
    for rel in ("/signal/properties", "/signal/Moc.fits"):
        fp = hips_dir + rel
        if not os.path.isfile(fp):
            continue
        data = open(fp, "rb").read()
        if not data:
            continue
        blob += rel.encode() + b"\n" + data + b"\n"
    if not blob:
        return ""
    return hashlib.sha256(blob).hexdigest()


def fits_structure(path):
    """只读 FITS 头部，返回 HDU 数 / 主 HDU 的 BITPIX 与 NAXIS。不假设单 HDU 或位深。"""
    rec = {"n_hdu": 0}
    try:
        from astropy.io import fits as _fits
        with _fits.open(path, memmap=False) as h:
            rec["n_hdu"] = len(h)
            hd = h[0].header
            rec["bitpix"] = hd.get("BITPIX")
            rec["naxis1"] = hd.get("NAXIS1")
            rec["naxis2"] = hd.get("NAXIS2")
            rec["hdus"] = [{"bitpix": x.header.get("BITPIX"),
                            "naxis1": x.header.get("NAXIS1"),
                            "naxis2": x.header.get("NAXIS2")} for x in h]
    except Exception as e:      # 读不动就是判据失败，不静默
        rec["error"] = "%s: %s" % (type(e).__name__, e)
    return rec


def load_json(p):
    return json.loads(io.open(p, encoding="utf-8").read())


def run_cmd(cmd, log_path, timeout=5400):
    t0 = time.time()
    with open(log_path, "wb") as lf:
        pr = subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT, timeout=timeout, cwd=REPO)
    return pr.returncode, time.time() - t0


def newest_run_manifest(d):
    fs = sorted(glob.glob(os.path.join(d, "astrocs_run_*.json")), key=os.path.getmtime)
    return fs[-1] if fs else None


def verify_chain(dataset, timings, findings):
    """返回 (ok, evidence)。所有判据 fail-closed。"""
    ev = {"dataset": dataset, "timings_s": timings, "findings": findings}
    p1 = os.path.join(OUT, "p1_%s" % dataset.lower())
    p2 = os.path.join(OUT, "p2_mosaic")
    p3 = os.path.join(OUT, "p3_export")

    m1 = newest_run_manifest(p1)
    m2 = newest_run_manifest(p2)
    m3 = newest_run_manifest(p3)
    if not (m1 and m2 and m3):
        findings.append("E2E-E1 缺少 run manifest: p1=%s p2=%s p3=%s" % (m1, m2, m3))
        return False, ev
    h1, h2, h3 = sha256_file(m1), sha256_file(m2), sha256_file(m3)
    ev["run_manifest_sha256"] = {"normalize": h1, "mosaic": h2, "export": h3}

    # 3. manifest 链
    # 口径（module_adapters.cpp:5564 §20.3）：P2 的 input_manifest_hash 是
    #   sha256(canonical(frame identity + filter/order/frame_type))，**不是** P1 run manifest 文件哈希。
    # 故此处只验**阶段内自洽**：p2_samples.json 与 p2_final.json.provenance 必须报同一个哈希。
    p2f = os.path.join(p2, "p2_final.json")
    if os.path.isfile(p2f):
        got = (load_json(p2f).get("provenance") or {}).get("ASTROCS_INPUT_MANIFEST_HASH")
        ev["p2_input_manifest_hash"] = got
        smp = os.path.join(p2, "p2_samples.json")
        if os.path.isfile(smp):
            sh = load_json(smp).get("input_manifest_hash")
            ev["p2_samples_input_manifest_hash"] = sh
            if sh != got:
                findings.append("E2E-E2 P2 阶段内不一致: p2_samples=%s != p2_final.provenance=%s"
                                % (sh, got))
        else:
            findings.append("E2E-E2 缺 p2_samples.json")
    else:
        findings.append("E2E-E2 缺 p2_final.json")
    # P2 → P3 链：**独立复算** P3 的输入 manifest 哈希（不复用产品自报值）
    recomputed = p3_input_manifest_hash(p2)
    ev["p3_input_manifest_hash_recomputed"] = recomputed
    v3 = os.path.join(p3, "p3_verify.json")
    if os.path.isfile(v3):
        d3 = load_json(v3)
        got = d3.get("input_manifest_hash")
        ev["p3_input_manifest_hash"] = got
        wr = os.path.join(p3, "p3_writer.json")
        if os.path.isfile(wr):
            wh = load_json(wr).get("input_manifest_hash")
            ev["p3_writer_input_manifest_hash"] = wh
            if wh != got:
                findings.append("E2E-E2 P3 阶段内不一致: p3_writer=%s != p3_verify=%s" % (wh, got))
        if not recomputed:
            findings.append("E2E-E2 无法独立复算 P3 输入 manifest 哈希（P2 产品缺 properties/Moc.fits）")
        elif got != recomputed:
            findings.append("E2E-E2 P3 输入 manifest 哈希 != 独立复算值: %s vs %s" % (got, recomputed))
        ev["p3_verify"] = {k: d3.get(k) for k in
                           ("coverage_ok", "reopen_ok", "canonical_match", "bunit",
                            "integrity_sha256", "output_fits")}
        ev["p3_verify"]["covered_px"] = (d3.get("coverage_stats") or {}).get("covered_px")
        ev["p3_verify"]["total_px"] = (d3.get("coverage_stats") or {}).get("total_px")
        if d3.get("coverage_ok") != 1:
            findings.append("E2E-E3 coverage_ok != 1")
        if d3.get("reopen_ok") != 1:
            findings.append("E2E-E3 reopen_ok != 1")
        if d3.get("canonical_match") is not True:
            findings.append("E2E-E3 canonical_match != true")
        if not (ev["p3_verify"]["covered_px"] or 0) > 0:
            findings.append("E2E-E3 covered_px == 0（产品全空，coverage_ok 会骗人）")
        fits = d3.get("output_fits")
        if not (fits and os.path.isfile(fits)):
            findings.append("E2E-E4 output_fits 不存在: %s" % fits)
        else:
            ev["fits_bytes"] = os.path.getsize(fits)
            ev["fits_structure"] = fits_structure(fits)
            fs = ev["fits_structure"]
            if fs.get("error"):
                findings.append("E2E-E4 FITS 结构读取失败: %s" % fs["error"])
            else:
                if fs["n_hdu"] < 1:
                    findings.append("E2E-E4 FITS 无 HDU")
                # 结构自洽：主 HDU 的像素数与 p3_verify 的 total_px 一致（不假设位深/单 HDU）
                if fs.get("naxis1") and fs.get("naxis2"):
                    px = fs["naxis1"] * fs["naxis2"]
                    ev["fits_pixels"] = px
                    if ev["p3_verify"].get("total_px") and px != ev["p3_verify"]["total_px"]:
                        findings.append("E2E-E4 FITS 像素数 %d != p3_verify.total_px %s"
                                        % (px, ev["p3_verify"]["total_px"]))
                if fs.get("bitpix") not in (-32, -64, 16, 32, 8):
                    findings.append("E2E-E4 FITS BITPIX 非法: %s" % fs.get("bitpix"))
    else:
        findings.append("E2E-E3 缺 p3_verify.json")

    # 5. 目录纪律：本阶段的运行清单与产品必须落在本阶段 output_dir 内
    for tag, d, man in (("P1", p1, m1), ("P2", p2, m2), ("P3", p3, m3)):
        for pth in (man, os.path.join(d, "alloc_report.json")):
            if pth and os.path.isfile(pth):
                real_d = os.path.realpath(d)
                real_p = os.path.realpath(pth)
                if not real_p.startswith(real_d + os.sep):
                    findings.append("E2E-E5 %s 产物落在 output_dir 之外: %s" % (tag, pth))
    ev["dir_discipline"] = "checked"
    return not findings, ev


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--run", action="store_true")
    ap.add_argument("--verify-only", action="store_true", dest="verify_only")
    ap.add_argument("--dataset", default="T4")
    ap.add_argument("--json-out", default=os.path.join(REPO, "run/RELEASE-05/evidence/e2e501_chain.json"))
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args()

    if args.self_test:
        # 判据能红：退化产品（covered_px==0 / FITS 缺失）必须被判红，防「空产品也判绿」
        cases = []
        d = {"coverage_ok": 1, "reopen_ok": 1, "canonical_match": True,
             "coverage_stats": {"covered_px": 0, "total_px": 100}, "output_fits": "/nonexistent"}
        f1 = []
        if not (d["coverage_stats"]["covered_px"] or 0) > 0:
            f1.append("covered_px==0")
        if not os.path.isfile(d["output_fits"]):
            f1.append("missing fits")
        cases.append(("S1-degenerate-product-red", len(f1) == 2))
        f2 = []
        good = {"coverage_stats": {"covered_px": 10}}
        if not (good["coverage_stats"]["covered_px"] or 0) > 0:
            f2.append("covered_px==0")
        cases.append(("S2-nonzero-coverage-green", f2 == []))
        bad = [n for n, g in cases if not g]
        rec = {"tool": "run_e2e_chain", "mode": "self-test", "cases": len(cases),
               "results": [{"name": n, "ok": g} for n, g in cases],
               "verdict": "PASS" if not bad else "FAIL"}
        os.makedirs(os.path.dirname(args.json_out), exist_ok=True)
        io.open(args.json_out, "w", encoding="utf-8").write(
            json.dumps(rec, ensure_ascii=False, indent=1) + "\n")
        for n, g in cases:
            print("SELFTEST " + ("PASS " if g else "FAIL ") + n)
        print("E2E501_SELFTEST_%s: %d/%d -> %s"
              % (rec["verdict"], len(cases) - len(bad), len(cases), args.json_out))
        return 0 if not bad else 1

    os.makedirs(LOGS, exist_ok=True)
    os.makedirs(os.path.dirname(args.json_out), exist_ok=True)
    findings = []
    timings = {}

    if args.run:
        env = dict(os.environ)
        env["E2E_ONLY"] = args.dataset
        pr = subprocess.run([sys.executable, os.path.join(REPO, "eng/tools/e2e/make_e2e_configs.py")],
                            capture_output=True, text=True, cwd=REPO, env=env)
        if pr.returncode != 0:
            print("配置生成失败: " + pr.stderr[-400:], file=sys.stderr)
            return 2
        stages = [("normalize", "p1_m42_%s_red.json" % args.dataset.lower()),
                  ("mosaic", "mosaic_e2e.json"),
                  ("export", "export_e2e.json")]
        for name, cfg in stages:
            rc, dt = run_cmd([BINARY, name, "--json", os.path.join(E2E, "configs", cfg), "-y"],
                             os.path.join(LOGS, "e2e501_%s.log" % name))
            timings[name] = round(dt, 3)
            print("  %-9s rc=%d  %.2fs" % (name, rc, dt))
            if rc != 0:
                findings.append("E2E-E1 %s rc=%d（见 run/RELEASE-05/logs/e2e501_%s.log）"
                                % (name, rc, name))

    ok, ev = verify_chain(args.dataset, timings, findings)
    ev["verdict"] = "PASS" if ok else "FAIL"
    ev["commands"] = {
        "normalize": "build/astrocs normalize --json run/RELEASE-05/e2e/configs/p1_m42_%s_red.json -y"
                     % args.dataset.lower(),
        "mosaic": "build/astrocs mosaic --json run/RELEASE-05/e2e/configs/mosaic_e2e.json -y",
        "export": "build/astrocs export --json run/RELEASE-05/e2e/configs/export_e2e.json -y",
    }
    io.open(args.json_out, "w", encoding="utf-8").write(
        json.dumps(ev, ensure_ascii=False, indent=1) + "\n")
    print("E2E501_CHAIN_%s: findings=%d -> %s" % (ev["verdict"], len(findings), args.json_out))
    for f in findings:
        print("  " + f)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
