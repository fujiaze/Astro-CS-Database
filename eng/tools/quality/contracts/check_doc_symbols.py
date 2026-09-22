#!/usr/bin/env python3
"""check_doc_symbols.py — T402 doc symbols checker

Checks: 文档中反引号符号、文件和 config key 均可解析；排除 archive 清单
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
"""
import argparse, json, pathlib, re, sys, csv


_HEADER_CORPUS = {}


def _header_corpus(repo: pathlib.Path):
    """lib/**、lib/include/** 的公开头文本（**一次**读入）。

    W4-A3: 原实现对**每个** token 都重新 rglob 全树 + 逐个 read_text ⇒
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


# ── W4-A3：非 API 命名空间解析面 ────────────────────────────────────────────
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


def _stem_resolves(token, stems):
    return any(token == s or s.endswith("_" + token) for s in stems)


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    args = ap.parse_args()
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
    auth_dirs = ["science","algorithms","architecture","contracts","modules","design"]
    docs = []
    for d in auth_dirs:
        docs.extend([p for p in (repo / "docs" / d).rglob("*.md") if "archive" not in str(p)])
    # Also include top-level docs that are authoritative: TRACEABILITY, PUBLIC_API etc handled via contracts
    # Only add if exists (fixtures may not have)
    for extra in [repo / "docs/contracts/PUBLIC_API.md", repo / "docs/contracts/DATA_SEMANTICS.md"]:
        if extra.exists():
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
    # W4-A3: 原实现对**整仓** rglob("*")（含 build/ run/ .git/ 问题扫描/），
    # 187 份文档 × 全树遍历 ⇒ 单跑 4 分钟，CI 120s timeout 直接判 TIMEOUT。
    # 判定面只需要"文档可能引用的仓库文件"，排除重型产物目录后同判据更快。
    # 注意：`run/` 与 `reports/` **必须保留**在遍历面内 —— 文档常把运行产物写成
    # 任务目录相对路径（如 `spec/x.json` → `run/v6/<task>/spec/x.json`），
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
                        # W4-A3: 三种形态**明确不是**"文件必须存在"的判据对象：
                        #   ① 历史引证：token 自身带"已删除/已归档/deleted"标记
                        #      （如 `工程控制/旧 V6 控制包（ROOT-007 已删除）/tasks/x.md`）
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
                        # W4-A3: `run/**` 加入非保留命名空间 —— run/ 是 ENGINEERING_SPEC
                        # §7 / AGENTS.md 明定的「运行产物、不入库」空间（gitignore），
                        # 对 run/ 里的产物断言"文件存在"本身是错口径（与 archive/
                        # third_party 同类）。判据只对**在版本库保留面内**的路径生效。
                        _nonret = ("archive", "third_party", "run/")
                        if not any(k in token for k in _nonret):
                            findings.append({"id":"DOC-BAD-FILE","severity":"P1","file":str(doc.relative_to(repo)),"symbol":token,"observed":"file not found","expected":"exists"})
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
            # W4-A3: 文档词干 / 术语词典 / 登记命名空间 命中即视为文档引用或科学量,
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
                        is_api_like = (token.isupper() and "_" in token and len(token) >= 6) or token.startswith(("p2_","aio_","ac_","cc_","dpsf_","sdet_","ipv_","pc_","snr_","gaia_"))
                        if is_api_like:
                            # 公开头内真实定义的枚举常量/宏是合法公开符号
                            # (api_inventory 只登记函数签名) — 库头全文核实放行
                            # (Windows CI R8 实测误报 AIO_HIPS_RD_IVAR/SNR)。
                            if _defined_in_public_header(repo, token):
                                continue
                            findings.append({"id":"DOC-BAD-SYMBOL","severity":"P1","file":str(doc.relative_to(repo)),"symbol":token,"observed":"symbol not in API inventory","expected":"exists"})
                            status="FAIL"
    # Check archive symbols should not be in active docs (exclude archive docs themselves)
    # W4-A3: 登记项只减不增守卫 — 登记了却在扫描面从未出现的 token 判红
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
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_doc_symbols" tests="{len(docs)}" failures="{failures}"><testcase classname="docs" name="symbols"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
