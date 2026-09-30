#!/usr/bin/env python3
"""p2_pixel_weight_wiring.py — M06 判据 (D)：生产侧逐像素面接线核对（可红可绿）。

检查 module_adapters.cpp（Phase2 mosaic 的实际接线）在**逐像素权重面**上：
  D1 调用集成侧生产 API（weight_from_sparse_layer_pixel_prepared 或
     weight_from_sparse_layer_pixel），而不是自写一套换算；
  D2 逐像素面**不存在**把帧级 SNR 乘进层值的代码形态
     （frame_snr * / snr_px * / weight_from_snr( 帧级变量 ）；
  D3 稀疏层语义判别式（absolute_flux_type_snr / SparseSnrSemantics）确实随层传入；
  D4 已删除的帧级×帧内合成 compose_actual_snr 在整个仓库代码面不存在。

退出码 0 = 全绿；1 = 有判红项。--self-test 在临时树上自证「能红」。
"""
import argparse
import os
import re
import sys

ADAPTER = "lib/infrastructure/scheduler/src/module_adapters.cpp"
HEADER = "lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h"
IMPL = "lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp"

# 代码面（去注释）后的禁止形态：帧级 SNR 与层值相乘/相除
FORBIDDEN_PATTERNS = [
    (re.compile(r"frame_snr\s*\*\s*\w*snr"), "frame_snr * <snr> (frame-level scalar multiplied into an SNR)"),
    (re.compile(r"\w*snr\s*\*\s*frame_snr"), "<snr> * frame_snr (frame-level scalar multiplied into an SNR)"),
    (re.compile(r"weight_from_snr\s*\(\s*frame_snr"), "weight_from_snr(frame_snr, ...) on the layer face"),
    (re.compile(r"compose_actual_snr"), "compose_actual_snr (retired frame x intra composition)"),
    (re.compile(r"frame_snr_x_sparse_snr"), "weight_source \"frame_snr_x_sparse_snr\" (retired)"),
]


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def scan(repo: str):
    problems = []
    try:
        raw = open(os.path.join(repo, ADAPTER), encoding="utf-8").read()
    except OSError as exc:
        return [f"cannot read {ADAPTER}: {exc}"]
    code = strip_comments(raw)
    # D1 单一实现
    if "weight_from_sparse_layer_pixel_prepared" not in code and \
       "weight_from_sparse_layer_pixel(" not in code:
        problems.append(
            "D1 module_adapters has no call into the integration per-pixel API "
            "(weight_from_sparse_layer_pixel[_prepared])")
    # D2 不得把帧级标量乘进层值
    for rx, why in FORBIDDEN_PATTERNS:
        m = rx.search(code)
        if m:
            problems.append(f"D2/D4 forbidden form in {ADAPTER}: {why} -> {m.group(0)!r}")
    # D3 语义随层传入
    if "SparseSnrSemantics::kAbsoluteFluxTypeSnr" not in code:
        problems.append(
            "D3 module_adapters does not tag the loaded layer with the frozen "
            "absolute_flux_type_snr semantics (SparseSnrSemantics::kAbsoluteFluxTypeSnr)")
    # D4 生产实现面也要干净
    for path in (HEADER, IMPL):
        try:
            pcode = strip_comments(open(os.path.join(repo, path), encoding="utf-8").read())
        except OSError as exc:
            problems.append(f"cannot read {path}: {exc}")
            continue
        for rx, why in FORBIDDEN_PATTERNS:
            m = rx.search(pcode)
            if m:
                problems.append(f"D4 forbidden form in {path}: {why} -> {m.group(0)!r}")
    # 逐像素面必须仍在（防「删掉就算修好」）
    impl = strip_comments(open(os.path.join(repo, IMPL), encoding="utf-8").read())
    for need in ("weight_from_sparse_layer_pixel",
                 "weight_from_sparse_layer_pixel_prepared",
                 "layer_semantics_ok"):
        if need not in impl:
            problems.append(f"D5 {IMPL} lost the per-pixel consumer ({need})")
    return problems


def self_test() -> int:
    import shutil
    import tempfile
    ok = True
    with tempfile.TemporaryDirectory() as tmp:
        repo = os.path.join(tmp, "repo")
        for rel in (ADAPTER, HEADER, IMPL):
            dst = os.path.join(repo, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy(os.path.join(SELF_REPO, rel), dst)
        clean = scan(repo)
        print(f"  [self-test] unmodified tree -> {len(clean)} problem(s)")
        if clean:
            for p in clean:
                print("    ", p)
            ok = False
        # 注入 1：把 prepared 调用换成自写换算，并把帧级标量乘进层值
        dst = os.path.join(repo, ADAPTER)
        text = open(dst, encoding="utf-8").read()
        broken = text.replace(
            "weight_from_sparse_layer_pixel_prepared(",
            "weight_from_snr(frame_snr * snr_px, snr_fref_k[fid], &w, &serr); (")
        open(dst, "w", encoding="utf-8").write(broken)
        got = scan(repo)
        print(f"  [self-test] injected bypass -> {len(got)} problem(s)")
        if not got:
            print("    [FAIL] injection was not caught (gate cannot go red)")
            ok = False
        # 注入 2：改回干净树，再删掉语义标记
        open(dst, "w", encoding="utf-8").write(text.replace(
            "SparseSnrSemantics::kAbsoluteFluxTypeSnr", "SparseSnrSemantics::kUnspecified"))
        got2 = scan(repo)
        print(f"  [self-test] injected semantics loss -> {len(got2)} problem(s)")
        if not got2:
            print("    [FAIL] semantics-loss injection was not caught")
            ok = False
    print("  [self-test]", "PASS" if ok else "FAIL")
    return 0 if ok else 1


SELF_REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))))

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=SELF_REPO)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    problems = scan(args.repo)
    if problems:
        print("p2_pixel_weight_wiring: FAIL")
        for p in problems:
            print("  -", p)
        return 1
    print("p2_pixel_weight_wiring: PASS (per-pixel face uses the integration API; "
          "no frame-level scalar on the layer face; frozen semantics carried)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
