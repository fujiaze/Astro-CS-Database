#!/usr/bin/env python3
"""check_doc_symbols.py — T402 doc symbols checker

Checks: 文档中反引号符号、文件和 config key 均可解析；排除 archive 清单
        扫描面完整性：权威域目录缺失 / 扫到 0 份文档 ⇒ 判红（静默退化为恒真门的防线）
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
  python3 eng/tools/quality/contracts/check_doc_symbols.py              # 判据本体
  python3 eng/tools/quality/contracts/check_doc_symbols.py --self-test  # 正例绿/负例红夹具面
"""
import argparse, json, pathlib, re, sys, csv
import shutil, subprocess, tempfile

# 权威文档域（判定面 = 这些目录下的 *.md）。改判定面必须显式改本常量：
# 目录缺失或一份文档都没扫到 ⇒ 本门没有判据对象，此时判 PASS 就是恒真门。
#
# FINAL-07（lead-01/auth-dirs）：判定面随文档迁移改指**新三集**。
# 旧值 ("science","algorithms","architecture","contracts","modules","design") 是**迁移前**
# 的目录名 —— 迁移后除 science 外**五个目录的 .md 数全为 0**（实测：algorithms 0 / architecture 0 /
# contracts 0 / modules 0 / design 0，跟踪件分别 3/6/4/2/0 且**无一是 .md**）。
# 本门扫的判据对象是 **.md**，故旧值实际只扫到 docs/science 的 52 篇（+2 篇 extras 共 54 份，
# 去重 53 份），而 docs/ 下 .md 共 253 篇 ⇒ **约 196 篇文档从未受本门判过**，
# 即「门绿」与「文档合规」之间存在约 78% 的盲区（恒真门形态）。
# 新值 = 迁移后的三个一级目录，判定面 249 篇（去重）/253 份（含 extras 重复）。
AUTH_DIRS = ("science", "engineering", "detail")

_HEADER_CORPUS = {}


def _header_corpus(repo: pathlib.Path):
    """lib/**、lib/include/** 的公开头文本（**一次**读入）。

    原实现对**每个** token 都重新 rglob 全树 + 逐个 read_text ⇒
    判定面 O(tokens × headers)，实测把 CON-DOC-SYMBOLS 拖到 120s 超时
    （CI 侧 120s timeout 判 TIMEOUT，判据再对也拿不到结论）。现在只读一次。
    """
    key = str(repo.resolve())
    if key in _HEADER_CORPUS:
        return _HEADER_CORPUS[key]
    texts = []
    for base in ("lib", "include"):
        for h in (repo / base).rglob("*.h"):
            if "third_party" in str(h) or "archive" in str(h):
                continue
            try:
                texts.append(h.read_text(encoding="utf-8", errors="ignore"))
            except OSError:
                continue
    _HEADER_CORPUS[key] = "\n".join(texts)
    return _HEADER_CORPUS[key]


def _defined_in_public_header(repo: pathlib.Path, token: str) -> bool:
    """token 是否在 lib/** 公开头中真实定义(枚举常量/宏/标识符级核实)。"""
    return re.search(r"\b" + re.escape(token) + r"\b", _header_corpus(repo)) is not None


# ── 非 API 命名空间解析面 ────────────────────────────────────────────
# 事由：原判据把文档反引号里的**所有**标识符形态 token 都拿去和
# `docs/architecture/api_inventory.csv`（只登记**函数签名**）比对 ⇒ 文档状态词
# (`ALG_PROPOSED`/`TARGET_NORMATIVE`/`NOT_IMPLEMENTED`/`PENDING_SO07`)、文档简写
# (`PROJECT_SPEC`/`FREEZE_LIST`/`CONFLICT_MATRIX`)、科学量符号
# (`snr_v`/`snr_v²`/`snr_identity`) 全部被误判为"API 符号不存在"（40 条）。
# 修法（**收窄判据、不放宽**）：token 必须能解析到**已登记命名空间之一**；
# 解析不到的仍然判红。新增两个 fail-closed 检查：
#   DOC-SYMBOL-REGISTRY-EVIDENCE —— 登记项 evidence 必须存在且逐字含该 token；
#   DOC-SYMBOL-REGISTRY-STALE    —— 登记项必须在扫描面真实出现（登记只减不增）。
NAMESPACES_REL = "docs/architecture/doc_symbol_namespaces.json"
GLOSSARY_REL = "docs/GLOSSARY.md"


def _doc_stem_index(repo):
    """docs/**/*.md 词干集合（文档简写引用的解析域）。"""
    return {p.stem for p in repo.glob("docs/**/*.md")}


_DOC_SUFFIXES = (".md", ".markdown")


def _stem_resolves(token, stems):
    """token 能否解析到 docs/ 的某个文档词干。

    FINAL-07（lead-01/auth-dirs）补齐：文档引用**带扩展名**时（`gaia_xpsd_client.md`）
    原实现只拿**无扩展名**词干集（`p.stem`）比对 ⇒ 永远匹配不上；该 token 又因「无 `/`」
    进不了上面的文件引用分支（该分支要求 `"/" in token and "." in token`）⇒ 一篇
    **真实存在**的兄弟文档被判 DOC-BAD-SYMBOL（实测 docs/detail/README.md:45 两例，
    两篇均 test -e EXISTS 且 git ls-files 各 1 行）。

    这**不是豁免**：剥扩展名后仍必须在词干集里精确命中，指向不存在文档的 `FOO.md`
    照旧判红。属把实现补齐到 doc_symbol_namespaces.json auto_domains[0] 的自述
    （「docs/**/*.md 的词干」），判据不放宽。
    """
    cands = [token]
    for suf in _DOC_SUFFIXES:
        if token.endswith(suf):
            cands.append(token[: -len(suf)])
            break
    return any(any(c == s or s.endswith("_" + c) for s in stems) for c in cands)


def _glossary_tokens(repo):
    """docs/GLOSSARY.md 的术语表 token 集合（唯一术语权威）。"""
    p = repo / GLOSSARY_REL
    if not p.is_file():
        return set()
    text = p.read_text(encoding="utf-8", errors="ignore")
    toks = set()
    for line in text.splitlines():
        if not line.strip().startswith("|"):
            continue
        for cell in line.strip().strip("|").split("|"):
            toks.update(re.findall(r"[A-Za-z_][A-Za-z0-9_\u00b2\u00b3]*", cell))
    return toks


def _line_of(text: str, offset: int) -> int:
    """字符偏移 → 1 基行号（判词要能定位到 文件:行，偏移对人不友好）。"""
    return text.count("\n", 0, offset) + 1


def _load_symbol_namespaces(repo, findings):
    """读命名空间登记表并逐条校验 evidence（fail-closed）。返回 (tokens, ok)。"""
    p = repo / NAMESPACES_REL
    if not p.is_file():
        findings.append({"id": "DOC-SYMBOL-REGISTRY-MISSING", "severity": "P1",
                         "file": NAMESPACES_REL, "symbol": NAMESPACES_REL,
                         "observed": "命名空间登记表不存在", "expected": "exists"})
        return set(), False
    try:
        doc = json.loads(p.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        findings.append({"id": "DOC-SYMBOL-REGISTRY-BAD-JSON", "severity": "P1",
                         "file": NAMESPACES_REL, "symbol": NAMESPACES_REL,
                         "observed": "不可解析: %s" % exc, "expected": "valid JSON"})
        return set(), False
    toks, ok = set(), True
    for ent in doc.get("registered_symbols", []):
        tok = ent.get("token")
        ev = ent.get("evidence", "")
        if not tok or not ev:
            findings.append({"id": "DOC-SYMBOL-REGISTRY-EVIDENCE", "severity": "P1",
                             "file": NAMESPACES_REL, "symbol": str(tok),
                             "observed": "登记项缺 token/evidence", "expected": "两者齐备"})
            ok = False
            continue
        rel, _, line_s = ev.rpartition(":")
        evp = repo / (rel or ev)
        if not evp.is_file():
            findings.append({"id": "DOC-SYMBOL-REGISTRY-EVIDENCE", "severity": "P1",
                             "file": NAMESPACES_REL, "symbol": tok,
                             "observed": "evidence 文件不存在 %s" % ev, "expected": "exists"})
            ok = False
            continue
        evtext = evp.read_text(encoding="utf-8", errors="ignore")
        if tok not in evtext:
            findings.append({"id": "DOC-SYMBOL-REGISTRY-EVIDENCE", "severity": "P1",
                             "file": NAMESPACES_REL, "symbol": tok,
                             "observed": "evidence 文件未逐字含该 token: %s" % ev,
                             "expected": "token 出现在 evidence"})
            ok = False
            continue
        toks.add(tok)
    return toks, ok


# ── 内置夹具面（--self-test）：正例判绿 + 负例判红 + 恢复判绿 ────────────────
# 夹具全部落临时目录（**不写仓库正文**）；子进程跑**真实 CLI** —— 判据怎么判，
# 夹具面就怎么判。路径一律拼接构造，不在源码里写死仓库正文里的 docs 路径字面量。
_FIXTURE_DOC = "FIXTURE_SAMPLE.md"


def _dp(*parts):
    """夹具仓库相对路径（拼接构造）。"""
    return str(pathlib.PurePosixPath("docs", *parts))


def _registry(entries):
    return {"schema": "astrocs/doc-symbol-namespaces/v1", "auto_domains": [],
            "registered_symbols": list(entries)}


def _fixture(root, name, *, doc_tokens=(), registry=None, skip_registry=False,
             domains=None, extra_files=()):
    """造一个最小夹具仓库，返回其根目录。"""
    base = pathlib.Path(root) / name
    (base / "docs" / "architecture").mkdir(parents=True, exist_ok=True)
    for d in (AUTH_DIRS if domains is None else domains):
        (base / "docs" / d).mkdir(parents=True, exist_ok=True)
    if not skip_registry:
        reg = _registry([]) if registry is None else registry
        (base / "docs" / "architecture" / "doc_symbol_namespaces.json").write_text(
            json.dumps(reg, ensure_ascii=False), encoding="utf-8")
    if doc_tokens:
        p = base / "docs" / "science" / _FIXTURE_DOC
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("# 夹具文档\n\n正文：" + "、".join("`%s`" % t for t in doc_tokens) + "\n",
                     encoding="utf-8")
    for rel, text in extra_files:
        p = base / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text, encoding="utf-8")
    return base


def _fixture_run(root, name, **kw):
    base = _fixture(root, name, **kw)
    out = base / "selftest_out.json"
    proc = subprocess.run(
        [sys.executable, str(pathlib.Path(__file__).resolve()),
         "--repo", str(base), "--out-json", str(out)],
        cwd=str(root), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if not out.is_file():
        return proc.returncode, None, proc.stdout
    return proc.returncode, json.loads(out.read_text(encoding="utf-8")), proc.stdout


def self_test():
    """正例判绿 / 负例判红 / 恢复判绿；任一例不符预期 ⇒ rc=1。"""
    common_files = (
        ("fixture_evidence.py", "FIXTURE_REGISTERED_TOKEN = 1\n"),
        ("lib/include/fixture_case.h", "#define FIXTURE_HEADER_CONST 1\n"),
        (_dp("architecture", "api_inventory.csv"),
         "symbol,signature\nFIXTURE_API_SYMBOL,int fixture_api_symbol(void)\n"),
        (_dp("GLOSSARY.md"),
         "| 术语 | 定义 |\n|---|---|\n| FIXTURE_GLOSSARY_TERM | 夹具术语 |\n"),
    )
    live_tokens = ("FIXTURE_REGISTERED_TOKEN", "FIXTURE_HEADER_CONST",
                   "FIXTURE_API_SYMBOL", "FIXTURE_GLOSSARY_TERM")
    reg_ok = _registry([{"token": "FIXTURE_REGISTERED_TOKEN", "namespace": "fixture",
                         "evidence": "fixture_evidence.py:1",
                         "reason": "夹具登记项：evidence 文件逐字含该 token"}])
    reg_stale = _registry([
        {"token": "FIXTURE_REGISTERED_TOKEN", "namespace": "fixture",
         "evidence": "fixture_evidence.py:1", "reason": "夹具登记项"},
        {"token": "FIXTURE_STALE_TOKEN", "namespace": "fixture",
         "evidence": "fixture_stale_evidence.py:1", "reason": "夹具登记项"}])
    cases = (
        ("正例-四类命名空间全可解析", 0, None,
         dict(doc_tokens=live_tokens, registry=reg_ok, extra_files=common_files)),
        ("负例-悬空符号", 1, "DOC-BAD-SYMBOL",
         dict(doc_tokens=("FIXTURE_DANGLING_SYMBOL",), registry=reg_ok,
              extra_files=common_files)),
        ("负例-登记项不再命中(STALE)", 1, "DOC-SYMBOL-REGISTRY-STALE",
         dict(doc_tokens=("FIXTURE_REGISTERED_TOKEN",), registry=reg_stale,
              extra_files=common_files + (("fixture_stale_evidence.py",
                                           "FIXTURE_STALE_TOKEN = 1\n"),))),
        ("负例-evidence 未逐字含 token", 1, "DOC-SYMBOL-REGISTRY-EVIDENCE",
         dict(doc_tokens=("FIXTURE_EV_TOKEN",),
              registry=_registry([{"token": "FIXTURE_EV_TOKEN", "namespace": "fixture",
                                   "evidence": "fixture_evidence.py:1",
                                   "reason": "夹具登记项"}]),
              extra_files=common_files)),
        ("负例-evidence 文件不存在", 1, "DOC-SYMBOL-REGISTRY-EVIDENCE",
         dict(doc_tokens=("FIXTURE_EV_FILE_TOKEN",),
              registry=_registry([{"token": "FIXTURE_EV_FILE_TOKEN", "namespace": "fixture",
                                   "evidence": "fixture_absent.py:1",
                                   "reason": "夹具登记项"}]),
              extra_files=common_files)),
        ("负例-登记表缺失", 1, "DOC-SYMBOL-REGISTRY-MISSING",
         dict(doc_tokens=("FIXTURE_REGISTERED_TOKEN",), skip_registry=True,
              extra_files=common_files)),
        ("负例-权威域目录缺失", 1, "DOC-SYMBOL-SCAN-SURFACE",
         dict(doc_tokens=(), registry=reg_ok, domains=(), extra_files=common_files)),
        ("负例-扫描面 0 份文档", 1, "DOC-SYMBOL-SCAN-SURFACE",
         dict(doc_tokens=(), registry=_registry([]), extra_files=())),
        ("恢复-正例重跑仍绿", 0, None,
         dict(doc_tokens=live_tokens, registry=reg_ok, extra_files=common_files)),
    )
    root = tempfile.mkdtemp(prefix="doc-symbols-selftest-")
    bad = []
    try:
        for i, (name, want_rc, want_id, kw) in enumerate(cases):
            rc, result, out = _fixture_run(root, "case%02d" % i, **kw)
            if result is None:
                bad.append("%s：未产出 JSON（rc=%s）\n%s" % (name, rc, (out or "")[-400:]))
                print("BAD  %s：无 JSON 输出" % name)
                continue
            ids = [f.get("id") for f in result.get("findings", [])]
            ok = rc == want_rc and result.get("status") == ("PASS" if want_rc == 0 else "FAIL")
            if ok and want_id is None:
                ok = not ids          # 正例必须一条 finding 都没有（防误红）
            if ok and want_id is not None:
                ok = want_id in ids   # 负例必须命中**指定**判词（防"红了但不是这条"）
            print("%s %s rc=%s findings=%s" % ("OK  " if ok else "BAD ", name, rc, ids))
            if not ok:
                bad.append("%s：期望 rc=%s finding=%s，实得 rc=%s findings=%s"
                           % (name, want_rc, want_id, rc, ids))
    finally:
        shutil.rmtree(root, ignore_errors=True)
    if bad:
        print("DOC-SYMBOL-SELFTEST_FAIL: %d/%d 例不符预期" % (len(bad), len(cases)))
        for item in bad:
            print("  - " + item)
        return 1
    print("DOC-SYMBOL-SELFTEST_PASS: %d/%d 例（正例/恢复 2 例须绿，负例 %d 例须红且命中指定判词）"
          % (len(cases), len(cases), len(cases) - 2))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    ap.add_argument("--self-test", dest="self_test", action="store_true",
                    help="跑内置正例/负例夹具面（全部符合预期 = 0，任一例不符预期 = 1）")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    repo = pathlib.Path(args.repo)
    findings = []
    status = "PASS"
    doc_stems = _doc_stem_index(repo)
    glossary_toks = _glossary_tokens(repo)
    ns_tokens, ns_ok = _load_symbol_namespaces(repo, findings)
    if not ns_ok:
        status = "FAIL"
    seen_tokens = set()
    # Scan docs/**/*.md excluding archive/history
    # Only authoritative docs per 05 L1 classification
    docs = []
    for d in AUTH_DIRS:
        docs.extend([p for p in (repo / "docs" / d).rglob("*.md") if "archive" not in str(p)])
    # 扫描面完整性守卫（fail-closed；静默退化防线）：权威域目录缺失、或一份文档都没扫到
    # ⇒ 本门没有判据对象。此时给 PASS 等于恒真门（真值无效应时度量不归零，见 AGENTS.md
    # §5/§9）：docs/ 一次重构、域清单改个名，全部悬空符号都会**静默**停止受判。
    # 判定面收缩必须显式改 AUTH_DIRS（可见的改口径），不得靠"扫不到"变绿。
    missing_domains = [d for d in AUTH_DIRS if not (repo / "docs" / d).is_dir()]
    if missing_domains or not docs:
        findings.append({"id": "DOC-SYMBOL-SCAN-SURFACE", "severity": "P1",
                         "file": "docs",
                         "symbol": ",".join(missing_domains) or "docs_scanned=0",
                         "observed": ("权威域目录缺失: " + ",".join(missing_domains))
                                     if missing_domains else "扫描面为 0 份文档",
                         # 域个数从 AUTH_DIRS **导出**，不写死 —— 旧实现写死「六个」，
                         # 迁移后 AUTH_DIRS 已是三集，写死会把判词说成与判定面不符的话。
                         "expected": "AUTH_DIRS 的 %d 个权威域目录齐备，且至少扫到 1 份 md"
                                     % len(AUTH_DIRS)})
        status = "FAIL"
    # Also include top-level docs that are authoritative: TRACEABILITY, PUBLIC_API etc handled via contracts
    # Only add if exists (fixtures may not have)
    #
    # FINAL-07（lead-01/auth-dirs）去重：迁移后 science 与 engineering **都在** AUTH_DIRS 内，
    # 这两篇早被上面的 rglob 收进扫描面，再 append 等于**同一份文档被扫两遍** ——
    # 后果是 docs_scanned 虚高、同一 (file,line,symbol) 的 finding **成对重复**。
    # 去重**不缩小判定面**（去重前后是同一集合的不同元素），只消除重复计数。
    # 按解析后真实路径去重，避免同一文件经不同拼写混入两次。
    for extra in [repo / "docs/engineering/PUBLIC_API.md", repo / "docs/science/DATA_SEMANTICS.md"]:
        if extra.exists() and extra.resolve() not in {p.resolve() for p in docs}:
            docs.append(extra)
    # Extract backtick symbols like `p2_integrate_pixel` or `docs/...` or `lib/...`
    backtick_re = re.compile(r'`([^`]+)`')
    # Load known symbols from API inventory
    api_syms = set()
    try:
        inv = list(csv.DictReader(open(repo/"docs/architecture/api_inventory.csv", encoding="utf-8")))
        api_syms = set(r["symbol"] for r in inv)
    except: pass
    # Load known files
    # 原实现对**整仓** rglob("*")（含 build/ run/ .git/ 问题扫描/），
    # 187 份文档 × 全树遍历 ⇒ 单跑 4 分钟，CI 120s timeout 直接判 TIMEOUT。
    # 判定面只需要"文档可能引用的仓库文件"，排除重型产物目录后同判据更快。
    # 注意：`run/` 与 `reports/` **必须保留**在遍历面内 —— 文档常把运行产物写成
    # 任务目录相对路径（如 `spec/x.json` → `run/quality/<task>/spec/x.json`），
    # 下方"按 / 后缀唯一匹配"的兜底解析需要它们。只剪真正体量巨大的目录。
    _skip = (".git", "build", "问题扫描", "__pycache__", "AstroCS.wiki",
             "GaiaDR3", "GaiaDR3SP")
    # 2026-09-21 ROOT-CONSOLIDATION：原根目录 "BASS DR3"（58 MB / 367 文件）迁入
    # testdata/BASS_DR3/，按前缀继续排除以**保持原排除面不增不减**
    # （testdata/ 其余内容仍在遍历面内；HST_M16 仅 3 文件，不排除）。
    _skip_prefix = ("testdata/BASS_DR3/",)
    known_files = set()
    for p in repo.rglob("*"):
        if not p.is_file():
            continue
        rel = p.relative_to(repo).as_posix()
        if rel.split("/", 1)[0] in _skip or rel.startswith(_skip_prefix):
            continue
        known_files.add(rel)
    for doc in docs:
        text = doc.read_text(encoding="utf-8", errors="ignore")
        for m in backtick_re.finditer(text):
            token = m.group(1).strip()
            # Skip obvious non-symbol tokens (plain english, too short, contains spaces)
            if " " in token and "/" not in token: continue
            if token.startswith("http"): continue
            # 多行反引号是代码/正文块, 不是文件路径引用 — 跳过。
            # 不跳过时整段文本进入 os.stat(Errno 36 file name too long)
            # 或误判 DOC-BAD-FILE(Windows CI R8 实测 4 例)。
            if "\n" in token: continue
            # Skip composite file lists like rejection.cpp/integrate.cpp
            if "/" in token and ".cpp" in token:
                parts = token.split("/")
                if any(".cpp" in p for p in parts) and len(parts) == 2 and token.count(".") == 2:
                    continue
            # Skip glob patterns
            if "*" in token:
                continue
            # Check if token looks like file path
            if "/" in token and "." in token:
                # File reference: check exists or is doc-relative
                if token.endswith(".md") or token.endswith(".h") or token.endswith(".cpp") or token.endswith(".json"):
                    # Try alternative: token may be relative like eng/tools/stage2.cpp -> lib/algorithms/coverage/tools/stage2.cpp
                    found = token in known_files or (repo / token).exists()
                    if not found:
                        # Try lib/algorithms/coverage/tools/ prefix
                        alt = repo / "lib/algorithms/coverage" / token
                        if alt.exists():
                            found = True
                        alt2 = repo / "lib" / token
                        if alt2.exists():
                            found = True
                    if not found:
                        # 文档常写模块内相对路径(如 healpix_drizzle/xxx.cpp,
                        # 真实位于 lib/algorithms/drizzle/healpix_drizzle/) — 以
                        # known_files 后缀匹配兜底(消 Windows CI R8 实测误报)。
                        # rglob 相对路径在 Windows 是 backslash — 统一正斜杠
                        # 再匹配(否则 Linux 过 Windows 挂, R9 34178712916 实证)。
                        if not found and any(k.replace("\\", "/").endswith("/" + token) for k in known_files):
                            found = True
                    if not found:
                        # 占位符路径(如 <out_hips>/diagnostics.json)非真实引用
                        if "<" in token or ">" in token:
                            found = True
                    if not found:
                        # 三种形态**明确不是**"文件必须存在"的判据对象：
                        #   ① 历史引证：token 自身带"已删除/已归档/deleted"标记
                        #      （如历史控制包目录下的 tasks/x.md）
                        #      —— 文档已声明该文件不存在，拿"存在"判它是错口径；
                        #   ② 花括号展开/通配（`docs/design/PHASE{1,2,3}_.md`）；
                        #   ③ 含空格的命令行（`grep -c gate2 eng/ci/checks.json`）。
                        # 三者都在 token 自身可判定，无需外部豁免表。
                        _hist = any(k in token for k in ("已删除", "已归档", "deleted", "removed"))
                        _glob = ("{" in token and "}" in token) or "*" in token or "?" in token
                        _cmd = " " in token
                        if _hist or _glob or _cmd:
                            found = True
                    if not found:
                        # Allow if is a non-retention namespace doc.
                        # `run/**` 加入非保留命名空间 —— run/ 是 ENGINEERING_SPEC
                        # §7 / AGENTS.md 明定的「运行产物、不入库」空间（gitignore），
                        # 对 run/ 里的产物断言"文件存在"本身是错口径（与 archive/
                        # third_party 同类）。判据只对**在版本库保留面内**的路径生效。
                        _nonret = ("archive", "third_party", "run/")
                        if not any(k in token for k in _nonret):
                            findings.append({"id":"DOC-BAD-FILE","severity":"P1","file":str(doc.relative_to(repo)),"line":_line_of(text, m.start()),"symbol":token,"observed":"file not found","expected":"exists"})
                            status="FAIL"
                continue
            # Skip pure header filenames (contain .h)
            if token.endswith(".h") or token.endswith(".cpp") or token.endswith(".hpp"):
                continue
            # Skip module short names (no dot, not API prefix)
            if token in {"snr_estimator","calibration","phase2","healpix_drizzle","astro_image_io","acr","photometric_calib","plate_solve","dynamic_psf","star_detector","orchestrator","healpix_db","common","gaia_client"}:
                continue
            # Skip C file refs like gaia_client.c
            if token.endswith(".c"):
                continue
            # Skip file:line refs like dpsf_psf.cpp:368
            if ":" in token and (token.endswith(tuple(str(i) for i in range(10))) or token.split(":")[-1].isdigit()):
                continue
            # Skip composite file lists with /
            if "/" in token and token.count("/") == 1 and "." in token and "," not in token:
                # Single file path - already handled as file reference
                pass
            # Composite file list like rejection.cpp/integrate.cpp -> skip
            if "/" in token and ".cpp" in token:
                parts = token.split("/")
                if any(".cpp" in p for p in parts):
                    continue
            # Skip dll/so names
            if token.endswith((".dll",".so",".json")):
                continue
            # Skip composite file lists like a/b/c.h
            if token.count("/") >= 2 and token.count(",") == 0 and "." not in token.split("/")[-1].split(",")[0]:
                # Heuristic: if token looks like path but contains multiple slashes without clear file, skip detailed check
                pass
            # Check if token looks like symbol (contains _ and no spaces)
            # 文档词干 / 术语词典 / 登记命名空间 命中即视为文档引用或科学量,
            # 不进 API-inventory 判据（api_inventory 只登记函数签名）。收窄判据面。
            seen_tokens.add(token)
            if (_stem_resolves(token, doc_stems) or token in glossary_toks
                    or token in ns_tokens):
                continue
            if re.match(r'^[A-Za-z_][\w:]*$', token.replace('.', '')):
                # Heuristic: if token contains _ and is plausible API symbol, check against inventory
                if "_" in token and len(token) >= 3:
                    # Allow known symbols or check if substring matches
                    if token not in api_syms and not any(token in s for s in api_syms) and not any(s in token for s in api_syms):
                        # Check if token is actually a known file stem or config key - skip if not API-like prefix
                        # Skip known status/error codes (not API symbols)
                        if token in {"AC_ERR_PARAM","AC_OK","AC_ERR_MEMORY","AC_ERR_INTERNAL","NO_DATA","NO_CANDIDATES","INVALID_INPUT","INVALID_CONFIGURATION","INVALID_METHOD","AC_ERR","TIMEOUT","CANCELLED","OK","ALL_REJECTED","ZERO_VALID_WEIGHT","UNDERDETERMINED","P2_INTEGRATE_OK","P2_INTEGRATE_NO_CANDIDATES","P2_STATUS_OK","MOFFAT4_FWHM_FACTOR","DPSF_ERR_PARAM","NO_SOLUTION","PC_API","P2_API","AC_API","SNR_API","CC_EXPORT","DPSF_EXPORT","SDET_EXPORT","IPV_API","AIO_EXPORT","HIO_EXPORT","THREAD_BUDGET_EXEMPT","BASELINE_OPCODE_PASS","LD_LIBRARY_PATH","backend_math_contract.h","PLAN_ONLY"}:
                            continue
                        # Only flag UPPER_CASE or known API prefix; lowercase vars like t_light/hp_res are sci params not API symbols
                        #
                        # FINAL-07（lead-01/auth-dirs）补齐：token 带**小写扩展名**时
                        # `token.isupper()` 恒为 False（`FOO_BAR.md` 的 `md` 是小写）⇒
                        # 哪怕词干是纯 UPPER_CASE、且**仓内根本不存在**，该 token 也永不进
                        # is_api_like ⇒ **静默免判**。实测 `GHOST_DOC_NO_SUCH_FILE.md` 判绿
                        # （可证伪性用例④）。判据本意是「UPPER_CASE 形态的符号」⇒ 判形态前
                        # 先剥扩展名。此处只**收严**（增红方向），不放宽任何一格。
                        _shape = token.rsplit(".", 1)[0] if "." in token else token
                        is_api_like = ((token.isupper() or _shape.isupper()) and "_" in _shape
                                       and len(_shape) >= 6) or token.startswith(("p2_","aio_","ac_","cc_","dpsf_","sdet_","ipv_","pc_","snr_","gaia_"))
                        if is_api_like:
                            # 公开头内真实定义的枚举常量/宏是合法公开符号
                            # (api_inventory 只登记函数签名) — 库头全文核实放行
                            # (Windows CI R8 实测误报 AIO_HIPS_RD_IVAR/SNR)。
                            if _defined_in_public_header(repo, token):
                                continue
                            findings.append({"id":"DOC-BAD-SYMBOL","severity":"P1","file":str(doc.relative_to(repo)),"line":_line_of(text, m.start()),"symbol":token,"observed":"symbol not in API inventory","expected":"exists"})
                            status="FAIL"
    # Check archive symbols should not be in active docs (exclude archive docs themselves)
    # 登记项只减不增守卫 — 登记了却在扫描面从未出现的 token 判红
    # (防止把登记表当只增不减的豁免表; 条目随文档改写失效即须同步清除)。
    stale = sorted(t for t in ns_tokens if t not in seen_tokens)
    if stale:
        findings.append({"id": "DOC-SYMBOL-REGISTRY-STALE", "severity": "P1",
                         "file": NAMESPACES_REL, "symbol": ",".join(stale),
                         "observed": "登记项未在扫描面出现 (登记表只减不增)",
                         "expected": "删除失效登记或修正 token"})
    if findings:
        status = "FAIL"
    result = {"tool":"check_doc_symbols","status":status,"docs_scanned":len(docs),
              "namespaces_domains":{"document_stems":len(doc_stems),
                                    "glossary_tokens":len(glossary_toks),
                                    "registered_symbols":len(ns_tokens)},
              "findings":findings,"passed": status=="PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    # FINAL-07（前台交办）：判词必须**任何模式下**都上流。原实现有 --out-json 就只写
    # 文件、stdout 一条判词都没有 ⇒ CI 里 CHK-DANGLING / CON-DOC-SYMBOLS 判红却无法
    # 定位（汇总层只能记"检查器未打印可识别判词"）。JSON 面完全不变，这里只补逐条
    # 「文件:行 符号 判定」；PASS 打一行摘要。
    if status == "FAIL":
        print("CON-DOC-SYMBOLS_FAIL: %d finding(s) / %d doc(s) scanned"
              % (len(findings), len(docs)))
        for f in findings:
            print("  - %s:%s [%s/%s] symbol=%s observed=%s expected=%s"
                  % (f.get("file", "?"), f.get("line", 1), f.get("id", "?"),
                     f.get("severity", "?"), f.get("symbol", "?"),
                     f.get("observed", ""), f.get("expected", "")))
    else:
        print("CON-DOC-SYMBOLS_PASS: docs_scanned=%d namespaces=%s"
              % (len(docs), result["namespaces_domains"]))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_doc_symbols" tests="{len(docs)}" failures="{failures}"><testcase classname="docs" name="symbols"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
