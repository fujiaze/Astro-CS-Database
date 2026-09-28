#!/usr/bin/env python3
"""p3_publish_order_negative.py — P-205/P-206 负例（注入旧行为必须判红）。

判据面（与 eng/tests/backend/test_p3_output.py 的 test_12/test_13 同一判词）:
  P-205: 发布序 = fsync → 校验(DATASUM) → sha256(tmp) → 原子 rename。
         旧序（rename → sha256 → 重开校验）必须判红：
           (a) RENAME 之前不得存在对 tmp 的**读**打开；
           (b) RENAME 之后出现对已发布路径的读打开。
  P-206: verify 逐 HDU 对拍 DATASUM/CHECKSUM。
         旧 verify（不读 checksum 键）对「只改 DATASUM 卡一位」零鉴别力 ⇒ 判红。

做法（fail-closed，不依赖 git 状态）: 把当前 p3_output.cpp 复制到临时目录，按
下面的**逆向变异**还原旧实现，用同一编译命令编出 mutant 探针，跑同一组判词。
- 变异任一步的模式匹配失败 ⇒ 直接报错退出 2（不静默通过）。
- 期望：当前源全绿（判词成立）且 mutant 全红（判词被触发）；任一不成立即退出 1。

用法: python3 eng/tests/backend/p3_publish_order_negative.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
HOST = os.path.join(REPO, "lib", "phase3_session")
PROJ = os.path.join(REPO, "lib", "algorithms", "projection")
FITS = os.path.join(REPO, "lib", "algorithms", "fits_output")
AIO = os.path.join(REPO, "lib", "infrastructure", "aio")
CFITSIO = os.path.join(AIO, "third_party", "cfitsio")
BACKEND = os.path.join(REPO, "eng", "tests", "backend")
SRC = os.path.join(FITS, "p3_output.cpp")
SKIP_C = re.compile(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                    r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                    r"imcopy|imarith|tabcompile|sortcol|tabselect")

INCS = [f"-I{os.path.join(REPO, 'lib', 'include')}", f"-I{HOST}", f"-I{FITS}",
        f"-I{PROJ}", f"-I{os.path.join(REPO, 'lib', 'algorithms', 'shared')}",
        f"-I{os.path.join(REPO, 'lib', 'algorithms', 'shared', 'crypto')}",
        f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
        f"-I{CFITSIO}"]


def strip_once(text, old, new, what):
    """fail-closed 替换：必须恰好命中一次，否则报错退出。"""
    n = text.count(old)
    if n != 1:
        sys.stderr.write(f"[negative] 变异锚点 '{what}' 命中 {n} 次 (期望 1) — "
                         f"p3_output.cpp 结构已变，必须同步更新负例\n")
        sys.exit(2)
    return text.replace(old, new)


def mutate_to_old_order(text):
    """把新发布序逆向还原为旧序: rename 提前，校验/哈希改读已发布路径。"""
    # ① 校验对象改回「已发布路径」
    text = strip_once(text, "p3_output_verify_ex(\n            tmp.c_str(), wcs,",
                      "p3_output_verify_ex(\n            output_path, wcs,",
                      "verify(tmp)->verify(output)")
    # ② 哈希对象改回「已发布路径」
    text = strip_once(text, "sha256_file_checked(tmp.c_str(), &pub_sha)",
                      "sha256_file_checked(output_path, &pub_sha)",
                      "sha256(tmp)->sha256(output)")
    # ③ rename 块整体前移到「③ 校验」之前
    start = text.index("    // ⑤ 原子 rename")
    end = text.index("    if (result) {\n        std::snprintf(result->sha256")
    rename_block = text[start:end]
    text = text[:start] + text[end:]
    anchor = "    // ③ 校验（rename 之前，针对 tmp）"
    i = text.index(anchor)
    text = text[:i] + rename_block + text[i:]
    return text


def mutate_checksum_off(text):
    """P-206 反向变异：去掉 verify 里的逐 HDU DATASUM/CHECKSUM 对拍调用。"""
    n = text.count("if (!verify_hdu_chksum(f, nullptr)) chksumok = 0;")
    if n != 3:
        sys.stderr.write("[negative] P-206 变异锚点命中 %d 次 (期望 3, "
                         "PRIMARY/COVERAGE/VARIANCE+IVAR) — 结构已变，须同步更新负例\n" % n)
        sys.exit(2)
    return text.replace("if (!verify_hdu_chksum(f, nullptr)) chksumok = 0;", "(void)0;")


def build_cfitsio_objs(tmp):
    objs = []
    for f in sorted(os.listdir(CFITSIO)):
        if not f.endswith(".c") or SKIP_C.search(f):
            continue
        o = os.path.join(tmp, f[:-2] + ".o")
        subprocess.run(["gcc", "-O2", "-w", f"-I{CFITSIO}", "-c",
                        os.path.join(CFITSIO, f), "-o", o], check=True,
                       capture_output=True, timeout=300)
        objs.append(o)
    return objs


def build_probe(tmp, objs, p3_output_cpp, exe):
    srcs = [os.path.join(BACKEND, "p3_output_fsync_probe.cpp"), p3_output_cpp,
            os.path.join(PROJ, "p3_wcs.cpp"),
            os.path.join(REPO, "lib", "algorithms", "shared", "crypto", "sha256.cpp"),
            os.path.join(AIO, "src", "aio_fits.cpp"), os.path.join(AIO, "src", "aio_api.cpp"),
            os.path.join(AIO, "src", "aio_log.cpp")]
    r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *INCS,
                        *srcs, *objs, "-lz", "-lzstd", "-llz4", "-o", exe],
                       capture_output=True, text=True, timeout=900)
    if r.returncode != 0:
        sys.stderr.write(r.stderr[-2000:])
        sys.exit(2)
    return exe


def build_interposer(tmp):
    so = os.path.join(tmp, "interposer.so")
    r = subprocess.run(["g++", "-shared", "-fPIC", "-O2", "-w",
                        os.path.join(BACKEND, "p3_output_fsync_interposer.cpp"),
                        "-ldl", "-o", so], capture_output=True, text=True, timeout=180)
    if r.returncode != 0:
        sys.stderr.write(r.stderr[-2000:])
        sys.exit(2)
    return so


def run(probe, args, preload=None):
    env = os.environ.copy()
    env.pop("ASTROCS_HASH_FAIL_INJECT", None)
    env.pop("ASTROCS_FAIL_FSYNC", None)
    if preload:
        env["LD_PRELOAD"] = preload
    r = subprocess.run([probe, *[str(a) for a in args]], capture_output=True,
                       text=True, timeout=300, env=env)
    return r.returncode, r.stdout.strip(), r.stderr


def parse_events(stderr_text):
    evs = []
    for line in stderr_text.splitlines():
        if line.startswith("EVENT "):
            _, kind, arg = line.split(" ", 2)
            evs.append((kind, arg))
    return evs


def check_p205(probe, so, tmp, tag):
    """返回 (判词, 是否被抓到)。抓到 = 该实现违反 P-205 发布序。"""
    out = os.path.join(tmp, f"order_{tag}.fits")
    rc, o, err = run(probe, ["write", out, 32, 20, 9], preload=so)
    if rc != 0 or not o.startswith("OK"):
        return f"write 未成功 (rc={rc} out={o!r}) — 事件序不可判", True
    evs = parse_events(err)
    want_tmp = os.path.basename(out) + "."
    want_final = os.path.basename(out)

    def name_of(a):
        return os.path.basename(a.split(" fd=")[0])

    def mode_of(a):
        return a.split("mode=")[-1]

    renames = [i for i, (k, a) in enumerate(evs)
               if k == "RENAME" and os.path.basename(a).startswith(want_tmp)]
    if not renames:
        return "未见 RENAME(tmp→out)", True
    cut = min(renames)
    pre_tmp_reads = [i for i, (k, a) in enumerate(evs[:cut])
                     if k == "FOPEN" and name_of(a).startswith(want_tmp)
                     and "r" in mode_of(a)]
    post_final_reads = [i for i, (k, a) in enumerate(evs[cut:])
                        if k == "FOPEN" and name_of(a) == want_final
                        and "r" in mode_of(a)]
    if not pre_tmp_reads:
        return "RENAME 之前无对 tmp 的校验/sha256 读", True
    if post_final_reads:
        return "RENAME 之后仍读已发布路径 (哈希/校验在发布后)", True
    return "发布序 = fsync→校验→sha256(tmp)→rename", False


def check_p206(probe, tmp, tag):
    """返回 (判词, 是否被抓到)。抓到 = 该 verify 对 DATASUM 篡改零鉴别力。"""
    out = os.path.join(tmp, f"chk_{tag}.fits")
    rc, o, err = run(probe, ["write", out, 24, 16, 11])
    if rc != 0 or not o.startswith("OK"):
        return f"write 未成功 (rc={rc} out={o!r})", True
    rc, o, err = run(probe, ["verify_path", out, 24, 16, 11])
    if not o.startswith("OK reopen=1"):
        return f"基线 verify 未绿: {o!r}", True
    raw = bytearray(open(out, "rb").read())
    m = re.search(rb"DATASUM\s*=\s*'(\d)", bytes(raw))
    if m is None:
        return "未找到 DATASUM 卡", True
    i = m.start(1)
    raw[i] = ord('0') if raw[i] != ord('0') else ord('1')
    tampered = os.path.join(tmp, f"chk_{tag}_tampered.fits")
    with open(tampered, "wb") as fh:
        fh.write(bytes(raw))
    rc, o, err = run(probe, ["verify_path", tampered, 24, 16, 11])
    if o.startswith("OK reopen=0"):
        return "DATASUM 篡改被 verify 检出 (reopen=0)", False
    return f"DATASUM 篡改未被检出: {o!r} (零鉴别力)", True


def main():
    tmp = tempfile.mkdtemp(prefix="p3neg_")
    try:
        print("[negative] 编译 cfitsio 对象 ...", flush=True)
        objs = build_cfitsio_objs(tmp)
        so = build_interposer(tmp)
        cur_cpp = os.path.join(tmp, "p3_output_current.cpp")
        shutil.copyfile(SRC, cur_cpp)
        mut_cpp = os.path.join(tmp, "p3_output_mutant.cpp")
        with open(SRC, encoding="utf-8") as fh:
            text = fh.read()
        with open(mut_cpp, "w", encoding="utf-8") as fh:
            fh.write(mutate_to_old_order(text))
        ck_cpp = os.path.join(tmp, "p3_output_ckoff.cpp")
        with open(ck_cpp, "w", encoding="utf-8") as fh:
            fh.write(mutate_checksum_off(text))
        probe_cur = build_probe(tmp, objs, cur_cpp, os.path.join(tmp, "probe_current"))
        probe_mut = build_probe(tmp, objs, mut_cpp, os.path.join(tmp, "probe_mutant"))
        probe_ck = build_probe(tmp, objs, ck_cpp, os.path.join(tmp, "probe_ckoff"))

        # (探针, 期望 P-205 被抓, 期望 P-206 被抓)
        variants = (("current", probe_cur, False, False),
                    ("mut_order", probe_mut, True, False),
                    ("mut_ckoff", probe_ck, False, True))
        bad = False
        for tag, probe, want205, want206 in variants:
            w205, c205 = check_p205(probe, so, tmp, tag)
            w206, c206 = check_p206(probe, tmp, tag)
            for name, words, caught, want in (("P-205", w205, c205, want205),
                                              ("P-206", w206, c206, want206)):
                verdict = "RED(被抓)" if caught else "GREEN(通过)"
                ok = (caught == want)
                bad = bad or not ok
                print(f"[{tag:9s}] {name}: {verdict} — {words}"
                      f"{'' if ok else '   <<< 期望不符'}")
        print("判词: current 两项 GREEN；mut_order 仅 P-205 判红；mut_ckoff 仅 P-206 判红 "
              "⇒ 两条判词各自非退化（能红能绿）"
              if not bad else "判词: 判别力不成立 (见上)")
        return 1 if bad else 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
