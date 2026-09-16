#!/usr/bin/env python3
"""AIO-OWNERSHIP: aio_image* owner 归类 + 禁止裸 free(aio_image*) + 算法目录禁产品 I/O。

依据（现行权威）:
- ASTROCS_DESIGN.md §9: "aio 是唯一 FITS/HiPS/manifest 读写边界; Phase1/2/3 复用同一
  套 AIO, 禁止各自复制 reader/writer"; §7.1 顶层结构把 aio 归 lib/infrastructure/。
- docs/plugins/infrastructure/17_aio.md §1/§6: aio 是唯一允许触碰磁盘产品的模块。
- ENGINEERING_SPEC.md §8: 每项检查必须有正例与负例(能红能绿)。

扫描规则:
1. AIOImageData 的释放必须经 canonical deleter: aio_free_image_data / dispose_image / ImagePtr(RAII)。
2. 禁止: std::free(p) / free(p) 作用于 aio_read*/aio_read_fits* 返回值。
3. AIO-OWN-002: lib/algorithms/** 生产源码禁止直接调用 FITS/HiPS 产品 I/O 原语
   (cfitsio fits_*/ff* 族 / aio_publish_* 发布原语); 必须经 lib/infrastructure/aio 边界。
   注意: 算法*调用* AIO 公开 C ABI(aio_read/aio_hips_*/aio_free_*)是正确方向, 不属违规;
   本规则只禁止算法侧自己复制 reader/writer。
4. 输出 owner 归类表。

命令行:
  python3 tools/check_aio_ownership.py              # 扫描真实仓库, rc=0/1
  python3 tools/check_aio_ownership.py --self-test  # 正例(干净合成树)+负例(注入)自检

exit 0 = PASS (无违规); 非 0 = 发现违规。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
ACQ = re.compile(r"\b(aio_read(?:_fits|_xisf|_header_only)?)\s*\(")
RAW_FREE = re.compile(r"(?:std::)?free\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
CANONICAL = re.compile(r"aio_free_image_data|dispose_image|ImagePtr|aio_free\s*\(")

# --- AIO-OWN-002: 算法目录生产源码禁止直接产品 I/O -------------------------
# 只匹配 cfitsio 真实原语族前缀, 不匹配同前缀的仓内局部标识符
# (fits_index_to_nested_local / fits_keywords / fits_reader 均为局部名, 非库调用)。
CFITSIO_FAMILY = (
    "fits_create", "fits_open", "fits_close", "fits_mov", "fits_write", "fits_read",
    "fits_update", "fits_delete", "fits_insert", "fits_copy", "fits_get", "fits_put",
    "fits_set", "fits_flush", "fits_file", "fits_img", "fits_hdr", "fits_col",
    "fits_find", "fits_mk", "fits_switch", "fits_resize", "fits_init", "fits_compress",
    "fits_imgpar", "fits_hd", "fits_test", "fits_report",
)
CFITSIO_CALL = re.compile(r"\b((?:%s)[a-z0-9_]*)\s*\(" % "|".join(CFITSIO_FAMILY))
# fitsfile 低层 I/O 前缀(ffopen/ffclos/ffrwlg/ffgpky 等): 以 ff + 大写字母区分局部名
FFIO_CALL = re.compile(r"\b(ff[a-z]*[A-Z][A-Za-z0-9]*)\s*\(")
# HiPS 发布层原语: 只应由 lib/infrastructure/aio 侧调用
HIPS_PUBLISH_CALL = re.compile(r"\b(aio_publish_[a-z][a-z0-9_]*)\s*\(")
ALG_ROOT_REL = "lib/algorithms"
ALG_SRC_EXT = (".c", ".cc", ".cpp", ".cxx")
# 生产源码排除: 共址测试 / vendored 第三方 / P1-HIPS 迁移目标域。
#   lib/algorithms/drizzle/hips/** = MODULE_MAP:P1-HIPS 目标目录(内为 aio_hips_writer 直写面
#   + aio_publish_* staging 面), 依据 ARCH-001 迁移清单第 10/11 行(lib/hips 归 drizzle legacy),
#   其退出 = AIO-001 收敛完成后删除该目录。此处显式登记豁免域, 只减不增。
ALG_EXCLUDE_PARTS = ("/tests/", "/third_party/", "/test/", "/algorithms/drizzle/hips/")


def alg_scan(root):
    """扫描 lib/algorithms/** 生产源码里的直接 FITS/HiPS 产品 I/O 调用。

    root 可指向真实仓库, 也可指向自检用合成树(目录结构同构)。
    返回 [(relpath, lineno, snippet, rule)]。
    """
    out = []
    base = root / ALG_ROOT_REL
    if not base.is_dir():
        return out
    for src in sorted(base.rglob("*")):
        if not src.is_file() or src.suffix not in ALG_SRC_EXT:
            continue
        posix = src.as_posix()
        if any(p in posix for p in ALG_EXCLUDE_PARTS):
            continue
        try:
            text = src.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        lines = text.splitlines()
        for rule, rx in (("cfitsio-direct-io", CFITSIO_CALL),
                         ("fitsfile-direct-io", FFIO_CALL),
                         ("hips-publish-direct", HIPS_PUBLISH_CALL)):
            for m in rx.finditer(text):
                line = text.count("\n", 0, m.start()) + 1
                snippet = lines[line - 1].strip() if 0 < line <= len(lines) else ""
                if snippet.startswith("//") or snippet.startswith("*") or snippet.startswith("/*"):
                    continue
                out.append((src.relative_to(root).as_posix(), line, snippet[:120], rule))
    return out


def scan(root=None):
    root = REPO if root is None else root
    violations, owners = [], {}
    for src in sorted((root / "lib").rglob("*.cpp")) + sorted((root / "cli").glob("*.cpp")):
        text = src.read_text(encoding="utf-8", errors="replace")
        for m in ACQ.finditer(text):
            var = None
            # 找赋值目标: AIOImageData* X = aio_read... 或 X = aio_read...
            line_start = text.rfind("\n", 0, m.start()) + 1
            line = text[line_start:m.start()]
            am = re.search(r"(\*|\w)\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*$", line)
            if am: var = am.group(2)
            else:
                am2 = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*aio_read", text[max(0, m.start()-120):m.start()])
                if am2: var = am2.group(1)
            owners.setdefault(m.group(1), set()).add(str(src))
            if var:
                # 检查该变量所有 free 调用
                for f in RAW_FREE.finditer(text):
                    if f.group(1) == var:
                        violations.append(f"{src}:{text.count(chr(10), 0, f.start())+1}: raw free({var}) on {m.group(1)} result — use canonical deleter")
    return violations, owners


def self_test():
    """正例+负例自检(ENGINEERING_SPEC §8): 合成同构树上注入算法目录 cfitsio 调用,
    检查器必须变红; 干净树上必须 0 命中。全部在 tempfile 内完成, 不写仓库。
    """
    import tempfile
    with tempfile.TemporaryDirectory(prefix="aio_own_selftest_") as td:
        root = pathlib.Path(td)
        mod = root / "lib" / "algorithms" / "selftest_mod"
        mod.mkdir(parents=True)
        probe = mod / "clean.cpp"
        probe.write_text("// 正例: 经 aio 边界的合法用法 + 同前缀局部名不得误报\n"
                         "extern \"C\" void* aio_read(const char*);\n"
                         "int fits_index_to_nested_local(int i) { return i; }\n"
                         "std::string fits_keywords(int d) { return \"\"; }\n"
                         "void f(const char* p) { (void)aio_read(p); "
                         "(void)fits_index_to_nested_local(1); (void)fits_keywords(2); }\n",
                         encoding="utf-8")
        pos = alg_scan(root)
        probe.write_text("// 负例: 算法目录直接 cfitsio(必须被抓)\n"
                         "int fits_create_file(void**, const char*, int*);\n"
                         "void g() { void* fp = 0; int st = 0; fits_create_file(&fp, \"a.fits\", &st); }\n",
                         encoding="utf-8")
        neg = alg_scan(root)
        ok_pos = (len(pos) == 0)
        ok_neg = (len(neg) >= 1)
        print("AIO-OWN-002 self-test: positive(clean tree) violations=%d [%s]; "
              "negative(injected fits_create_file) violations=%d [%s]"
              % (len(pos), "PASS" if ok_pos else "FAIL",
                 len(neg), "PASS" if ok_neg else "FAIL"))
        for x in pos:
            print("  false-positive: %s:%d: %s (%s)" % x)
        for x in neg:
            print("  injected hit: %s:%d: %s (%s)" % x)
        return 0 if (ok_pos and ok_neg) else 1


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if "--self-test" in argv:
        return self_test()
    violations, owners = scan()
    print("OWNER CLASSIFICATION (aio_image* acquisition -> files):")
    for acq, files in sorted(owners.items()):
        print(f"  {acq}: {len(files)} files")
        for f in sorted(files): print(f"    {f}")
    print(f"  total acquisition sites: {sum(len(v) for v in owners.values())}")
    alg = alg_scan(REPO)
    print("AIO-OWN-002 (lib/algorithms 生产源码禁直接 FITS/HiPS 产品 I/O; "
          "依据 ASTROCS_DESIGN §9 / 17_aio.md §1):")
    if alg:
        print(f"  AIO_OWNERSHIP_VIOLATION: {len(alg)} 处算法目录直接产品 I/O")
        for rel, ln, txt, rule in alg:
            print(f"    {rel}:{ln}: {txt}  [{rule}]")
    else:
        print("  AIO-OWN-002_PASS: lib/algorithms/** 直接 cfitsio/HiPS 发布原语 = 0")
    if violations:
        print("IO-002_FREE_VIOLATION:")
        for v in violations: print("  " + v)
        return 1
    print(f"IO-002_PASS: canonical deleter 全覆盖; 裸 free(aio_image*) = 0")
    return 1 if alg else 0


if __name__ == "__main__":
    raise SystemExit(main())
