#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_module_readmes.py — registry 模块页生成/校验（生成面唯一入口）。

依据：
  - 唯一源 = registry descriptor：lib/infrastructure/scheduler/src/module_adapters.cpp
    （ARCH-001 迁移后路径；迁移前 lib/core/src/module_adapters.cpp 已消失，本生成器
    **不**把它当回退——回退会把「证据源被搬走」伪装成「生成成功」）；
  - 授权面 = docs/DOCUMENT_INDEX.yaml（哪些页在册为 GENERATED，生成器才有权写）；
  - 结论证据源 = eng/ci/ledgers/module_page_evidence.json（可选；无条目即 NOT_VERIFIED）。

硬规矩（本文件存在的理由）：
  R1 必需证据源缺失/不可读/零 descriptor ⇒ exit 2，**不写盘**（在册页保持原样）；
  R2 结论字段（页状态/确定性/执行证据/容差）一律从证据源读取，读不到写 NOT_VERIFIED
     —— 既不是 PASS 也不是 FAIL；不再出现「等价已验 / 验收冻结」这类写死的断言；
  R3 只写自己所有的页：页内有本生成器的 GENERATED-ANCHOR 且记录 sha 与现行一致
     （OWNED），或该页不存在且在册 GENERATED（新建）。**无锚的页 = 人工内容，永不
     覆盖** ⇒ 下游对页面的手工修改不会被下一次生成静默回退；
  R4 每页带生成依据锚（生成器/源码 sha、再生命令、正文 sha、未验证字段清单），
     下游据此分辨「生成内容」与「人工内容」；
  R5 --check 只比对不写：OWNED 页与再生成结果不一致（含正文手改）⇒ exit 1。

用法：
  python3 eng/tools/quality/gen_module_readmes.py                # 写自己所有的页
  python3 eng/tools/quality/gen_module_readmes.py --check         # 门禁：漂移即红
  python3 eng/tools/quality/gen_module_readmes.py --adopt <module_id> [...]  # 显式收养
exit 0 = 成功/一致；1 = OWNED 页漂移；2 = 输入不可用（fail-closed）；3 = 收养被拒。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import subprocess
import sys

SOURCE_REL = "lib/infrastructure/scheduler/src/module_adapters.cpp"
LEGACY_SOURCE_REL = "lib/core/src/module_adapters.cpp"      # 已消失；仅用于报错提示
INDEX_REL = "docs/DOCUMENT_INDEX.yaml"
OUT_DIR_REL = "docs/modules/registry"
GENERATOR_REL = "eng/tools/quality/gen_module_readmes.py"
EVIDENCE_REL = "eng/ci/ledgers/module_page_evidence.json"
REGEN_CMD = "python3 eng/tools/quality/gen_module_readmes.py"

NOT_VERIFIED = "NOT_VERIFIED"
ANCHOR_MARK = "GENERATED-ANCHOR"
ANCHOR_RE = re.compile(r"<!--\s*" + ANCHOR_MARK + r"\s+(?P<fields>[^>]*?)-->")
KV_RE = re.compile(r"([A-Za-z][A-Za-z0-9_-]*)=(\"[^\"]*\"|\S+)")
CONTROL_FIELDS = ("status", "determinism", "verification", "tolerance")

# ---- 模板常量（模板事实，不是结论）------------------------------------------
# 判据：一行只要在断言「某个动作/性质已被验证」，它就不许写在这里，必须走证据源。
EXEC_NOTE = "资源门拒绝 heavy+serial 组合"
WORKER_NOTE = "ThreadBudget.max_workers(唯一取值源)"
ERROR_SECTION = (
    "错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；"
    "取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → "
    "exit 9，最高设计 §6.3；接线以实测为准）；模块内无 checkpoint（无断点续算）。"
)
TEMPLATE = """---
id: MOD-@@ID_DASH@@
version: 1.0.0
status: @@STATUS@@
owner: astrocs-core
source_commit: @@HEAD@@
upstream: [@@SCI@@, @@ALG@@, @@API@@]
downstream: [@@TEST@@]
---

@@ANCHOR@@

# 模块 @@MID@@

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

## 职责与明确非职责

Registry production 模块(唯一源=module_adapters.cpp descriptor)。职责由
SCI/ALG 合同定义(见链接); 不做 SCI/ALG 之外的扩展。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
@@PORTS@@

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

由 \x60@@API@@\x60 公共 API 定义(phase session extern "C"); 生命周期 create→validate→
run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=\x60@@MID@@\x60; execution_class=\x60@@EXEC@@\x60;
parallel_ok=@@PAR@@; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

\x60@@EXEC@@\x60; parallel=@@PAR_CN@@(@@EXEC_NOTE@@); worker 数=@@WORKER@@;
确定性=@@DET@@。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

@@ERRORS@@

## 独立 synthetic 验证命令与容差

测试标识=\x60@@TEST@@\x60（registry descriptor 单源）；执行证据=@@VER@@；容差=@@TOL@@。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 \x60@@ALG@@\x60 合同边界。
"""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def parse_anchor(anchor_text: str):
    m = ANCHOR_RE.search(anchor_text)
    if not m:
        return None
    out = {}
    for k, v in KV_RE.findall(m.group("fields")):
        out[k] = v[1:-1] if v.startswith('"') and v.endswith('"') else v
    return out


def render_anchor(fields: dict) -> str:
    order = ("tool", "tool-sha256", "source", "source-sha256", "ownership",
             "evidence", "evidence-sha256", "regenerate", "not-verified",
             "body-sha256")
    parts = []
    for k in order:
        v = fields.get(k)
        if v is None:
            continue
        parts.append('%s="%s"' % (k, v) if k == "regenerate" else "%s=%s" % (k, v))
    return "<!-- %s %s -->" % (ANCHOR_MARK, " ".join(parts))


def strip_anchor(text: str):
    """拆出 (锚字段字典或 None, 去锚正文)。锚行 + 紧跟的一个空行同属锚块。"""
    m = ANCHOR_RE.search(text)
    if not m:
        return None, text
    line_start = text.rfind("\n", 0, m.start()) + 1
    line_end = text.find("\n", m.end())
    line_end = len(text) if line_end < 0 else line_end + 1
    if text[line_end:line_end + 1] == "\n":
        line_end += 1
    return parse_anchor(m.group(0)), text[:line_start] + text[line_end:]


# --------------------------------------------------------------------------- #
# 输入面
# --------------------------------------------------------------------------- #
def read_descriptors(src_path: pathlib.Path):
    """从唯一源提取 descriptor。返回 (descs, err)。"""
    try:
        src = src_path.read_text(encoding="utf-8")
    except OSError as exc:
        return None, "SOURCE_UNREADABLE: %s: %s" % (src_path, exc)
    pat = re.compile(r"ModuleDescriptor (\w+)_descriptor\(\) \{(.*?)\n\}", re.S)
    out = []
    for m in pat.finditer(src):
        body = m.group(2)

        def grab(key):
            mm = re.search(r'd\.%s = "([^"]*)"' % key, body)
            return mm.group(1) if mm else ""

        ports = []
        for pm in re.finditer(
                r'\{"([^"]+)",\s*"([^"]+)",\s*(true|false),\s*([^,}]+),\s*([^}]+)\}', body):
            ports.append({"name": pm.group(1), "data": pm.group(2),
                          "required": pm.group(3) == "true",
                          "unit": pm.group(4).strip(), "frame": pm.group(5).strip()})
        mid = grab("module_id")
        if mid:
            out.append({"fn": m.group(1), "module_id": mid,
                        "execution_class": grab("execution_class"),
                        "parallel_ok": "d.parallel_ok = true" in body,
                        "sci_id": grab("sci_id"), "alg_id": grab("alg_id"),
                        "api_id": grab("api_id"), "test_id": grab("test_id"),
                        "ports": ports})
    return out, None


def load_ownership(index_path: pathlib.Path):
    """授权面：docs/DOCUMENT_INDEX.yaml 在册 GENERATED 的 registry 页。返回 (set, err)。"""
    try:
        import yaml
    except ImportError as exc:                                     # pragma: no cover
        return None, "OWNERSHIP_INDEX_UNUSABLE: PyYAML 不可用: %s" % exc
    if not index_path.is_file():
        return None, "OWNERSHIP_INDEX_MISSING: %s（无授权面证据则无权写任何生成页）" % INDEX_REL
    try:
        doc = yaml.safe_load(index_path.read_text(encoding="utf-8")) or {}
    except Exception as exc:                                       # noqa: BLE001
        return None, "OWNERSHIP_INDEX_UNPARSABLE: %s: %s" % (INDEX_REL, exc)
    authorized = set()
    for entry in ((doc.get("doc_index") or {}).get("active") or []):
        p = str(entry.get("path") or "")
        if p.startswith(OUT_DIR_REL + "/") and p.endswith(".md") \
                and str(entry.get("status")) == "GENERATED":
            authorized.add(pathlib.Path(p).name)
    return authorized, None


def load_evidence(ledger_path: pathlib.Path):
    """结论证据源（可选）。返回 (entries, 描述串, err)。文件不存在不是输入错误。"""
    if not ledger_path.is_file():
        return {}, "%s(absent)" % EVIDENCE_REL, None
    try:
        doc = json.loads(ledger_path.read_text(encoding="utf-8"))
    except Exception as exc:                                       # noqa: BLE001
        return None, "", "EVIDENCE_UNPARSABLE: %s: %s" % (EVIDENCE_REL, exc)
    return (dict(doc.get("entries") or {}),
            "%s sha256=%s" % (EVIDENCE_REL, sha256_file(ledger_path)[:16]), None)


def evidence_value(root: pathlib.Path, entries: dict, module_id: str, field: str,
                   evidence_desc: str):
    """读一个结论字段：仅当条目含 value+command+evidence 且证据文件 sha 对得上才算读到。

    读不到 ⇒ NOT_VERIFIED（既不是 PASS 也不是 FAIL）。返回 (文本, 是否读到, 是否未验证)。
    """
    why = "证据源 %s 无 %s.%s 条目" % (evidence_desc, module_id, field)
    ent = (entries.get(module_id) or {}).get(field)
    if not isinstance(ent, dict):
        return "%s（%s）" % (NOT_VERIFIED, why), False, True
    value = str(ent.get("value") or "").strip()
    command = str(ent.get("command") or "").strip()
    ev = str(ent.get("evidence") or "").strip()
    if not value or not command or not ev:
        return ("%s（条目字段不全：需 value+command+evidence；%s）" % (NOT_VERIFIED, why),
                False, True)
    ev_path = root / ev
    if not ev_path.is_file():
        return ("%s（证据文件不存在: %s；%s）" % (NOT_VERIFIED, ev, why), False, True)
    want = str(ent.get("sha256") or "").strip()
    if want and sha256_file(ev_path)[:len(want)] != want:
        return ("%s（证据文件 sha256 与条目不符: %s；%s）" % (NOT_VERIFIED, ev, why),
                False, True)
    return ("%s（证据源 %s；命令 %s；证据文件 %s）" % (value, evidence_desc, command, ev),
            True, False)


def git_head(root: pathlib.Path):
    try:
        r = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, capture_output=True,
                           text=True, timeout=30)
    except Exception:                                              # noqa: BLE001
        return None
    head = r.stdout.strip()
    return head if re.fullmatch(r"[0-9a-f]{40}", head) else None


# --------------------------------------------------------------------------- #
# 页面合成
# --------------------------------------------------------------------------- #
def compose(root: pathlib.Path, d: dict, ctx: dict):
    """合成一页（锚含正文 sha）。返回 (文本, 未验证字段列表)。"""
    mid = d["module_id"]
    nv = []
    status_txt, _, nv_status = evidence_value(root, ctx["entries"], mid, "status",
                                              ctx["evidence_desc"])
    det, _, nv_det = evidence_value(root, ctx["entries"], mid, "determinism",
                                    ctx["evidence_desc"])
    ver, _, nv_ver = evidence_value(root, ctx["entries"], mid, "verification",
                                    ctx["evidence_desc"])
    tol, _, nv_tol = evidence_value(root, ctx["entries"], mid, "tolerance",
                                    ctx["evidence_desc"])
    for flag, name in ((nv_status, "status"), (nv_det, "determinism"),
                       (nv_ver, "verification"), (nv_tol, "tolerance")):
        if flag:
            nv.append(name)
    status = NOT_VERIFIED if nv_status else status_txt.split("（")[0]
    ports_md = "\n".join(
        "| \x60%s\x60 | \x60%s\x60 | %s | \x60%s\x60 | \x60%s\x60 |"
        % (p["name"], p["data"], "必" if p["required"] else "可", p["unit"], p["frame"])
        for p in d["ports"]) or "_(descriptor 未声明端口)_"

    base_anchor = {
        "tool": GENERATOR_REL,
        "tool-sha256": sha256_file(root / GENERATOR_REL),
        "source": SOURCE_REL,
        "source-sha256": sha256_file(root / SOURCE_REL),
        "ownership": "%s status=GENERATED" % INDEX_REL,
        "evidence": EVIDENCE_REL,
        "evidence-sha256": (ctx["evidence_desc"].split("sha256=")[1]
                            if "sha256=" in ctx["evidence_desc"] else "-"),
        "regenerate": REGEN_CMD,
        "not-verified": ",".join(nv) or "-",
    }

    def render(anchor_text: str) -> str:
        subs = {
            "@@ID_DASH@@": mid.replace(".", "-"), "@@STATUS@@": status,
            "@@HEAD@@": ctx["head"] or NOT_VERIFIED, "@@SCI@@": d["sci_id"],
            "@@ALG@@": d["alg_id"], "@@API@@": d["api_id"], "@@TEST@@": d["test_id"],
            "@@ANCHOR@@": anchor_text, "@@MID@@": mid, "@@PORTS@@": ports_md,
            "@@EXEC@@": d["execution_class"], "@@PAR@@": str(d["parallel_ok"]),
            "@@PAR_CN@@": "是" if d["parallel_ok"] else "否",
            "@@EXEC_NOTE@@": EXEC_NOTE, "@@WORKER@@": WORKER_NOTE, "@@DET@@": det,
            "@@ERRORS@@": ERROR_SECTION, "@@VER@@": ver, "@@TOL@@": tol,
        }
        text = TEMPLATE
        for k, v in subs.items():
            text = text.replace(k, v)
        return text

    probe = render(render_anchor(dict(base_anchor, **{"body-sha256": "0" * 64})))
    body_sha = sha256_bytes(strip_anchor(probe)[1].encode("utf-8"))
    text = render(render_anchor(dict(base_anchor, **{"body-sha256": body_sha})))
    return text, nv


def page_name(module_id: str) -> str:
    return module_id + ".md"


def verify_page(root: pathlib.Path, path: pathlib.Path):
    """校验锚与正文。state ∈ OWNED_OK / DRIFT / UNOWNED / BROKEN。"""
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        return "BROKEN", "unreadable: %s" % exc
    fields, plain = strip_anchor(text)
    if not fields:
        return "UNOWNED", "无 %s 锚（人工内容）" % ANCHOR_MARK
    if fields.get("tool") != GENERATOR_REL:
        return "BROKEN", "锚声明的生成器不是本文件: %s" % fields.get("tool")
    if fields.get("source") != SOURCE_REL:
        return "BROKEN", "锚声明的证据源不是 %s: %s" % (SOURCE_REL, fields.get("source"))
    if fields.get("tool-sha256") != sha256_file(root / GENERATOR_REL):
        return "DRIFT", "生成器已变更，页需再生成（tool-sha256 不符）"
    if fields.get("source-sha256") != sha256_file(root / SOURCE_REL):
        return "DRIFT", "证据源已变更，页需再生成（source-sha256 不符）"
    if fields.get("body-sha256") != sha256_bytes(plain.encode("utf-8")):
        return "DRIFT", "正文被改（body-sha256 不符）"
    return "OWNED_OK", "锚有效"


# --------------------------------------------------------------------------- #
def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parents[3]))
    ap.add_argument("--check", action="store_true",
                    help="只比对不写：OWNED 页漂移即 exit 1")
    ap.add_argument("--adopt", nargs="*", default=[],
                    help="显式收养的在册模块页（module_id；仅限 index 在册 GENERATED）")
    args = ap.parse_args()
    root = pathlib.Path(args.root).resolve()
    out_dir = root / OUT_DIR_REL

    descs, err = read_descriptors(root / SOURCE_REL)
    if err:
        hint = ""
        if (root / LEGACY_SOURCE_REL).exists():
            hint = "（旧路径 %s 存在 ⇒ 生成器常量与迁移事实不一致）" % LEGACY_SOURCE_REL
        print("MODULE_README_GEN_INPUT_MISSING: %s%s" % (err, hint), file=sys.stderr)
        print("MODULE_README_GEN_FAIL: 未写盘（%s 保持原样）" % OUT_DIR_REL, file=sys.stderr)
        return 2
    prod = [d for d in descs if d["module_id"].startswith("astrocs.phase")]
    if not prod:
        print("MODULE_README_GEN_INPUT_MISSING: %s 解析出 0 个 production descriptor"
              "（零命中不是「清单本来就该是空的」的理由）" % SOURCE_REL, file=sys.stderr)
        return 2

    authorized, err = load_ownership(root / INDEX_REL)
    if err:
        print("MODULE_README_GEN_INPUT_MISSING: %s" % err, file=sys.stderr)
        print("MODULE_README_GEN_FAIL: 未写盘（无授权面证据则不写任何生成页）", file=sys.stderr)
        return 2

    entries, ev_desc, err = load_evidence(root / EVIDENCE_REL)
    if err:
        print("MODULE_README_GEN_INPUT_MISSING: %s" % err, file=sys.stderr)
        return 2
    ctx = {"head": git_head(root), "entries": entries, "evidence_desc": ev_desc}

    for mid in args.adopt:
        if page_name(mid) not in authorized:
            print("MODULE_README_GEN_ADOPT_REFUSED: %s 不在授权面（%s 未在册 GENERATED）"
                  % (mid, INDEX_REL), file=sys.stderr)
            return 3

    written, skipped, drifted, broken, conflicts = [], [], [], [], []
    nv_total = 0
    for d in sorted(prod, key=lambda x: x["module_id"]):
        path = out_dir / page_name(d["module_id"])
        text, nv = compose(root, d, ctx)
        nv_total += len(nv)
        if not path.exists():
            if page_name(d["module_id"]) not in authorized:
                skipped.append((d["module_id"], "不在授权面（index 未在册 GENERATED）"))
                continue
            if args.check:
                drifted.append((d["module_id"], "在册生成页缺失"))
            else:
                out_dir.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
                written.append(d["module_id"])
            continue
        state, detail = verify_page(root, path)
        if state == "UNOWNED":
            if d["module_id"] in args.adopt:
                if args.check:
                    drifted.append((d["module_id"], "待收养：页无锚"))
                else:
                    path.write_text(text, encoding="utf-8")
                    written.append(d["module_id"] + "(adopted)")
                continue
            if page_name(d["module_id"]) in authorized:
                conflicts.append(d["module_id"])
            skipped.append((d["module_id"], "无锚 ⇒ 人工内容，不覆盖"))
            continue
        if state == "BROKEN":
            broken.append((d["module_id"], detail))
            continue
        if state == "DRIFT":
            if args.check:
                drifted.append((d["module_id"], detail))
            else:
                # 证据源/生成器已变更 ⇒ 在册页陈旧：重生成（这是生成器的正常路径）
                path.write_text(text, encoding="utf-8")
                written.append(d["module_id"])
            continue
        if path.read_text(encoding="utf-8") != text:
            if args.check:
                drifted.append((d["module_id"], "与再生成结果不一致"))
            else:
                path.write_text(text, encoding="utf-8")
                written.append(d["module_id"])

    desc_ids = {d["module_id"] for d in prod}
    orphan_auth = sorted(p for p in authorized if p[:-3] not in desc_ids)

    for label, rows in (("UNOWNED", skipped), ("BROKEN", broken), ("DRIFT", drifted)):
        for mid, why in rows:
            print("MODULE_README_GEN_%s: %s — %s" % (label, mid, why))
    for mid in orphan_auth:
        print("MODULE_README_GEN_AUTHORIZED_WITHOUT_DESCRIPTOR: %s"
              "（在册 GENERATED 但 %s 无对应 descriptor ⇒ 需人工裁决）" % (mid, SOURCE_REL))
    if conflicts:
        print("MODULE_README_GEN_OWNERSHIP_CONFLICT n=%d: %s"
              "（index 标 GENERATED，但页内无锚且含人工内容 ⇒ 生成器不覆盖；"
              "请人工裁决：改 index 状态为 ACTIVE_*，或 --adopt 吸收）"
              % (len(conflicts), ",".join(conflicts)))
    print("MODULE_README_GEN_ANCHOR_HINT: 锚在=生成内容，锚不在=人工内容；"
          "本生成器永不覆盖无锚页")

    if broken:
        return 2
    if drifted:
        print("MODULE_README_GEN_DRIFT n=%d" % len(drifted), file=sys.stderr)
        return 1
    print("MODULE_README_GEN_OK written=%d skipped_unowned=%d "
          "ownership_conflicts=%d not_verified_fields=%d evidence=%s"
          % (len(written), len(skipped), len(conflicts), nv_total, ev_desc))
    return 0


if __name__ == "__main__":
    sys.exit(main())
