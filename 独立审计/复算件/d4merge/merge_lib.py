# -*- coding: utf-8 -*-
"""D4 装配器：把机械穷举层＋四路判读层＋四链路报告常数条目合并去重成总台账主表。
不新增判读：每行的取值／出处／处置一律取自已落盘成稿；复核结论覆盖报告结论。"""
import csv, io, os, re, collections, json

BASE = r"产出/"
OUT = os.path.join(BASE, "复算/d4merge")

# ---------- 路径规范化索引 ----------
TRACKED = [l.strip().replace("\\", "/") for l in io.open(os.path.join(OUT, "tracked_files.txt"), encoding="utf-8")]
BY_BASE = collections.defaultdict(list)
for p in TRACKED:
    BY_BASE[os.path.basename(p)].append(p)

KNOWN_DIR = {
    "defaults.json": "eng/packaging/config/defaults.json",
    "filters.json": "eng/packaging/config/filters.json",
    "runtime_resources.json": "eng/packaging/config/runtime_resources.json",
    "config_registry.json": "eng/packaging/config/config_registry.json",
    "resource_gate_v1.json": "eng/contracts/resource_gate_v1.json",
    "module_adapters.cpp": "lib/infrastructure/scheduler/src/module_adapters.cpp",
    "rejection.cpp": "lib/algorithms/noise_snr/cpp/src/rejection.cpp",
    "noise_model.cpp": "lib/algorithms/noise_snr/cpp/src/noise_model.cpp",
    "snr_science.cpp": "lib/algorithms/noise_snr/cpp/src/snr_science.cpp",
    "snr_estimator.cpp": "lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp",
    "snr_estimator.h": "lib/algorithms/noise_snr/cpp/include/snr_estimator.h",
    "star_matcher.cpp": "lib/algorithms/photometry/cpp/src/star_matcher.cpp",
    "spatial_gain.cpp": "lib/algorithms/photometry/cpp/src/spatial_gain.cpp",
    "spatial_gain.h": "lib/algorithms/photometry/cpp/src/spatial_gain.h",
    "pc_api.cpp": "lib/algorithms/photometry/cpp/src/pc_api.cpp",
    "frame_photometry_fit.h": "lib/algorithms/photometry/cpp/src/frame_photometry_fit.h",
    "spectrum_integrator.cpp": "lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp",
    "upm.cpp": "lib/algorithms/coverage/src/upm.cpp",
    "upm.h": "lib/algorithms/coverage/include/astro/phase2/upm.h",
    "sky_plane.cpp": "lib/algorithms/coverage/src/sky_plane.cpp",
    "sampler.cpp": "lib/algorithms/coverage/src/sampler.cpp",
    "drizzle_engine.cpp": "lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp",
    "spherical_overlap.cpp": "lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp",
    "astro_sphere_sink.h": "lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.h",
    "astro_sphere_sink.cpp": "lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp",
    "hp_drizzle_api.cpp": "lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.cpp",
    "sdet_api.cpp": "lib/algorithms/star_detection/src/sdet_api.cpp",
    "ipv_select.cpp": "lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp",
    "resource_gate.h": "lib/infrastructure/cli/resource_gate.h",
    "resource_events.h": "lib/infrastructure/cli/resource_events.h",
    "resource_recorder.h": "lib/infrastructure/cli/resource_recorder.h",
    "memory_report.h": "lib/infrastructure/cli/memory_report.h",
    "memory_growth.h": "lib/infrastructure/cli/memory_growth.h",
    "monitor.h": "lib/infrastructure/cli/monitor.h",
    "commands.cpp": "lib/infrastructure/cli/commands.cpp",
    "session_commands.h": "lib/infrastructure/cli/session_commands.h",
    "parser.cpp": "lib/infrastructure/cli/parser.cpp",
    "disk_gate.h": "lib/infrastructure/cli/disk_gate.h",
    "jsonl.h": "lib/infrastructure/cli/jsonl.h",
    "process.cpp": "lib/infrastructure/cli/process.cpp",
    "astrocs_process.h": "lib/infrastructure/cli/astrocs_process.h",
    "v6_runtime_contract.h": "lib/infrastructure/cli/v6_runtime_contract.h",
    "module.yaml": "lib/infrastructure/cli/module.yaml",
    "phase1_product.h": "lib/infrastructure/pipeline/phase1_product.h",
}

FILE_TOK = (r"(?:cpp|hpp|h|json|md|py|sh|in|cmake|yaml|yml|tsv|csv|txt)")
POS_RE = re.compile(r"(?:[A-Za-z0-9_\-\u4e00-\u9fff./]+/)?[A-Za-z0-9_\-\u4e00-\u9fff]+\." +
                   FILE_TOK + r"(?::\d+(?:[-,]\d+)?(?:[,\s]+:\d+)*)?")


def norm_path(tok):
    """把成稿里的路径片段规范成仓根相对全路径:行。"""
    if ":" in tok:
        path, line = tok.split(":", 1)
    else:
        path, line = tok, ""
    path = path.replace("\\", "/").lstrip("./")
    if not path.startswith(("lib/", "eng/", "docs/", "实验/", "testdata/", "artifacts/", "run/",
                            "根", "产出/")):
        if path in KNOWN_DIR:
            path = KNOWN_DIR[path]
        else:
            cands = BY_BASE.get(os.path.basename(path))
            if cands and len(cands) == 1:
                path = cands[0]
            elif cands:
                # 多命中：按 .cpp/.h 归属目录族优先
                pick = [c for c in cands if "/cpp/" in c or "/src/" in c] or cands
                path = pick[0]
            else:
                return None
    return (path + ":" + line) if line else path


def extract_positions(*texts):
    seen, res = set(), []
    for t in texts:
        if not t:
            continue
        for m in POS_RE.finditer(t):
            s = m.group(0).strip()
            n = norm_path(s)
            if n and len(n) > 6 and n not in seen:
                seen.add(n)
                res.append(n)
    return res


# ---------- 处置归一（五档） ----------
def disp_norm(s):
    s0 = (s or "").strip()
    lead = re.match(r"^[①②③④]\s*", s0)
    core = s0[1:].strip() if lead else s0
    tag = {"①": "文献值", "②": "公式导出", "③": "实验标定", "④": "待确认"}.get(
        s0[0] if s0[:1] in "①②③④" else "")
    if tag:
        return tag
    if s0.startswith("不适用"):
        return "不适用"
    for k, v in [("文献值", "文献值"), ("公式导出", "公式导出"), ("可公式导出", "公式导出"),
                 ("可由", "公式导出"), ("实验标定", "实验标定"), ("需实验标定", "实验标定"),
                 ("待确认", "待确认"), ("暂时无法", "待确认"), ("暂时确认", "待确认"),
                 ("有出处", "文献值"), ("登记出处", "文献值")]:
        if s0.startswith(k):
            return v
    for k, v in [("不适用", "不适用"), ("文献", "文献值"), ("公式", "公式导出"),
                 ("标定", "实验标定"), ("待确认", "待确认")]:
        if k in s0:
            return v
    return "待确认"


# ---------- 类别（常数｜公式｜算法｜容差） ----------
TOL_PAT = re.compile(r"(?i)tol|epsilon|eps|容差|threshold|thresh|margin|guard|precision|resid")
ALG_PAT = re.compile(r"(?i)mode|path|order|policy|recipe|iter|converg|clip|reject|solver|算法|流程|策略|branch")
FORM_PAT = re.compile(r"√|∫|·|log|pi|π|\d\s*/\s*\d|导出式|按式|式\(|恒等|定义式|换算式")


FORM_PAT = re.compile(r"√|∫|π|⟨δ⟩|=|≈|·|—\s*式")


def is_formula_value(val):
    """现行值本身是表达式（而非一个数／一条字符串断言）才判「公式」。"""
    v = (val or "").strip()
    if not v:
        return False
    if re.search(r"字符串|断言|格式串|文案", v):
        return False
    if re.fullmatch(r"[-+0-9.eEx_×,()\[\]\s;；/｜:a-zA-Zμ°″'\.\-]+", v) and not re.search(r"[=∫√·≈]", v):
        return False
    return bool(re.search(r"[=∫√≈]|·|π|/σ|/g\b|式", v))


def classify(sym, val, unit, coord, note, disptxt):
    """类别＝常数｜公式｜算法｜容差；只在现行值是表达式时判公式。"""
    s = (sym or "")
    if is_formula_value(val):
        return "公式"
    if TOL_PAT.search(s):
        return "容差"
    if ALG_PAT.search(s):
        return "算法"
    if TOL_PAT.search(note or "") and ("容差" in (note or "") or "epsilon" in (note or "").lower()):
        return "容差"
    return "常数"


# ---------- 判读层载入 ----------
def load(p):
    with io.open(os.path.join(BASE, p), encoding="utf-8-sig", newline="") as f:
        rows = list(csv.reader(f))
    return rows[0], rows[1:]


HDR = None


def judged(name):
    h, b = load("raw/AUD-402-%s.csv" % name)
    return b


J_SRC = {"判读-A1": "AUD-402-判读-A1", "判读-A2": "AUD-402-判读-A2",
         "判读-A3": "AUD-402-判读-A3", "判读-BD1": "AUD-402-判读-BD1"}

CONFLICT_KW = ["多侧不一致", "取值不一致", "同名不同值", "两侧取值", "多侧异", "不一致", "打脸",
               "互斥", "值集不相交", "两套", "三套", "分叉", "双份复制", "唯一数值源被旁路",
               "零登记", "无登记", "未登记", "静默", "恒不触发", "恒真", "假绿", "fail-open",
               "反向", "违禁", "登记面缺口", "六种", "六侧", "两处入口", "相反", "循环引用"]
CASE_ID = re.compile(r"\b(?:F|E|O|U|X|L|D|N|W|R|V|CL|G)-[0-9A-Za-z]+(?:[Ss])?\b|L[0-9]{1,2}\b|CL-[0-9A-Z]+")


SCORE_KW = ["⇒", "vs", "互斥", "相反", "打脸", "不等", "不一致", "值集不相交", "两套", "三套"]


def conflict_excerpt(text, kws, width=150):
    """取备注/出处里最能说明"哪两侧不等"的那一句。"""
    if not text:
        return ""
    cands = [s.strip() for s in re.split(r"[；。\n]", text) if any(k in s for k in kws)]
    if not cands:
        return ""
    cands.sort(key=lambda s: -sum(1 for k in SCORE_KW if k in s))
    return cands[0][:width]


def find_cases(*texts):
    got = set()
    for t in texts:
        for m in CASE_ID.finditer(t or ""):
            s = m.group(0).strip("-")
            if s.upper() in ("L", "F", "E", "U", "X"):
                continue
            got.add(s)
    return sorted(got)
