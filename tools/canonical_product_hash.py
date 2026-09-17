#!/usr/bin/env python3
"""规范产品哈希（canonical product hash）—— AstroCS 可复现性判据的唯一实现。

问题（DET-001 / SMOKE-001 D6）
------------------------------
AstroCS 产物里存在**合法但易变**的元数据：

* FITS RUNID（每次运行的运行标识）与 CHECKSUM（覆盖整个 HDU 的 1 补码校验和
  的 ASCII 编码；只要头部任何一字节变化就必须重算），以及 DATASUM 卡注释里
  标准推荐的 "updated <ISO-8601>" 时间戳；
* HiPS properties 的 hips_creation_date / hips_release_date（IVOA HiPS 要求的
  创建/发布日期）；
* 溯源 sidecar 的 run_id 以及它记录的**兄弟产物文件级 sha256**。

因此「同输入两次运行 -> 文件级 sha256 一致」这条判据对上述产物**原理上不可达**：
把时间戳与运行标识从产品里删掉会破坏标准要求或溯源链。正确做法是把两件事分开：

============  ==========================  ==============================
判据          口径                        用途
============  ==========================  ==============================
完整性面      integrity_sha256            文件字节是否被改动/损坏（对拍、
（integrity） = 整文件 sha256             传输校验、manifest 登记）
可复现面      canonical_sha256            像素数据 + 科学元数据 的规范
（canonical） = 本工具                   序列化 sha256；用于可复现性验收
============  ==========================  ==============================

口径分层与外部依据（详见 run/PROJECT-GOVERNANCE-01/DET-001/自证摘要.md §D6）
----------------------------------------------------------------------------
* FITS 4.0 §4.4.2.7 + 附录 K：CHECKSUM 覆盖**整个 HDU（含头记录）**，
  DATASUM 只覆盖数据单元（"excluding the header records"）。
* cfitsio §5.8.1 / checksum.c；astropy 默认在打开/写出时**剥离**
  CHECKSUM/DATASUM 卡。
* 可复现构建（reproducible-builds.org：Definition / Timestamps /
  Stripping of unreproducible information / Commandments）：时间戳、构建 ID、
  路径、UUID/随机数属变异源，做法是剥离或归一化，并把环境记到独立记录里。
* 天文同类管线的通行做法是**规范化比较 + 显式忽略表**：
  JWST regtest/conftest.py ignore_keywords = ["DATE","CAL_VER","CAL_VCS",
  "CRDS_VER","CRDS_CTX","NAXIS1","TFORM*"]；drizzlepac tests/hap/base_test.py
  ['origin','filename','date','iraf-tlm','fitsdate','upwtim','wcscdate',
  'upwcsver','pywcsver','history','prod_ver','rulefile']；
  romancal conftest.py ignore_metadata_paths（"always variable values.
  These include versions, dates, logs, etc."）。
* 部署内既有裁决: tools/quality/compare_products.py（P26 T3, 负责人裁决 4）已把
  hips_creation_date/hips_update_date/hips_release_date + created_utc/generated_utc/
  timestamp_utc/run_id/started_utc/ended_utc 列为墙钟字段, 把 FITS
  DATE/RUNID/CHECKSUM/DATASUM 列为头忽略项, 并明确**不忽略** DATE-OBS。本 spec v1 的
  排除清单与之一致, 并额外登记两个 derived_integrity 键（sha256 / product_sha256）:
  本工具产出的是**单一指纹**而非逐文件树比对, 派生摘要必须显式处理。
* 天文界**不存在**通用 canonical product hash 规范（IVOA ProvenanceDM 模型
  中 checksum/hash 零命中；DataLink 用标识符寻址）=> 本项目必须自行定义，
  并把它与完整性校验分开陈述（本文件即该定义）。

不得掩盖科学差异（本文件的设计约束）
------------------------------------
1. 排除清单**是穷举的、版本化的**：只有在 EXCLUDED_* 中列名的键/卡才被排除；
   任何未列名的差异都会改变 canonical_sha256。
2. 排除项**逐条带依据**（--spec 可机读输出），并且每次哈希都会在报告里回显
   实际命中的排除项（excluded_keys_hit / excluded_cards），不做静默丢弃。
3. 数据单元以**原始字节 sha256**（256 位）参与，严格强于 32 位 DATASUM；同时
   把 DATASUM 卡值当作**交叉校验不变量**（不符即硬错，exit 3）。

用法
----
    # 机读口径（排除清单 + 依据 + 版本）
    python3 tools/canonical_product_hash.py --spec

    # 单文件
    python3 tools/canonical_product_hash.py hash <path> [--base <dir>]

    # 一次运行的全部登记产物（从 run manifest 读 artifacts[]）
    python3 tools/canonical_product_hash.py dir <run_manifest.json>

    # 判据入口：两次运行的登记产物逐条比较 canonical_sha256
    python3 tools/canonical_product_hash.py compare <manifestA> <manifestB> [--out <json>]

    # 可执行负例面（ENGINEERING_SPEC §8）：合成夹具 + 正/反例断言
    python3 tools/canonical_product_hash.py --self-test

退出码: 0 通过/一致 · 1 自检失败 · 2 判据不一致 · 3 输入/完整性错误。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import sys
from typing import Any, Dict, List, Optional, Tuple

SPEC_ID = "astrocs.canonical-product-hash/v1"
DOMAIN = (SPEC_ID + "\n").encode("utf-8")

# ── 排除清单（版本化；改条目必须升 SPEC_ID 的主版本号）──────────────────────
# 类别: wall_clock（墙钟时间） / invocation_id（每次调用唯一标识） /
#       derived_integrity（兄弟产物的文件级摘要, 其易变性由该产物自身的
#       canonical hash 覆盖, 保留会重复传播易变） / telemetry（性能遥测）
EXCLUDED_FITS_CARDS: List[Dict[str, str]] = [
    {
        "key": "CHECKSUM",
        "category": "derived_integrity",
        "reason": "值 = 整个 HDU（头记录 + 数据单元）1 补码校验和的 ASCII 编码; "
                  "头部任何一字节变化（含其它卡的注释时间戳）都必须重算 => 它把全部易变元数据"
                  "放大进指纹。完整性由 integrity_sha256 与本工具自算的数据摘要覆盖。",
        "evidence": "FITS 4.0 §4.4.2.7 'accumulated over the entire FITS HDU' + 附录 K 步骤 2; "
                    "cfitsio §5.8.1; astropy 默认在打开/写出时剥离该卡",
    },
    {
        "key": "DATASUM",
        "category": "derived_integrity",
        "reason": "值 = 数据单元 1 补码校验和, 本工具已用数据单元原始字节 sha256（256 位）覆盖, "
                  "更强; 该卡注释按标准**推荐**写入 ISO-8601 更新时刻。卡值保留为交叉校验不变量"
                  "（不符即硬错）。",
        "evidence": "FITS 4.0 §4.4.2.7 'excluding the header records'; 同节推荐在注释写 ISO-8601 "
                    "Datetime; cfitsio checksum.c ffgstm 取当前系统时刻",
    },
    {
        "key": "DATE",
        "category": "wall_clock",
        "reason": "HDU/文件创建时刻（写盘墙钟）; 与观测、算法、像素无关, 每次运行必变。",
        "evidence": "FITS 4.0 §4.4.2.1 'the date on which the HDU was created'; JWST regtest "
                    "ignore_keywords 含 DATE; romancal ignore_metadata_paths 含 roman.meta.date / "
                    "roman.meta.file_date; drizzlepac ignore_keywords 含 'date','fitsdate'",
    },
    {
        "key": "RUNID",
        "category": "invocation_id",
        "reason": "AstroCS 每次运行的运行标识（8 字节短哈希）; 不携带科学信息, 属溯源面。",
        "evidence": "SLSA v1.2 把 invocationId 放在 runDetails.metadata、产物身份为 subject.digest; "
                    "reproducible-builds.org Commandments: 随机数/UUID 不得进产物",
    },
]

EXCLUDED_JSON_KEYS: List[Dict[str, str]] = [
    {
        "key": "run_id",
        "category": "invocation_id",
        "reason": "每次运行的运行标识; 属溯源面, 不属产品内容。",
        "evidence": "SLSA v1.2 runDetails.metadata.invocationId 与 subject.digest 分列; romancal "
                    "ignore_metadata_paths 忽略 cal_logs/individual_image_meta",
    },
    {
        "key": "elapsed_sec",
        "category": "telemetry",
        "reason": "节点墙钟耗时; 随机器负载/并行度变化, 无科学信息量。",
        "evidence": "间接: reproducible-builds.org Version information（'incremental build counter' 类"
                    "反例）; SLSA 把非主产物列为 byproducts。**无直接点名条款, 属类推依据**",
    },
    {
        "key": "created_utc",
        "category": "wall_clock",
        "reason": "记录创建时刻。",
        "evidence": "同 DATE（FITS 4.0 §4.4.2.1 类比）; SLSA startedOn/finishedOn 属 metadata",
    },
    {
        "key": "started_utc",
        "category": "wall_clock",
        "reason": "节点开始时刻。",
        "evidence": "同 created_utc",
    },
    {
        "key": "finished_utc",
        "category": "wall_clock",
        "reason": "节点结束/运行结束时刻。",
        "evidence": "同 created_utc",
    },
    {
        "key": "hips_creation_date",
        "category": "wall_clock",
        "reason": "IVOA HiPS properties 要求的创建时刻; 属标准要求字段, 不可删除, 只能从可复现面排除。",
        "evidence": "IVOA HiPS 1.0 §4.2 properties 必备键（创建日期）; 与 JWST/drizzlepac 忽略 "
                    "DATE/fitsdate 同款处置",
    },
    {
        "key": "hips_update_date",
        "category": "wall_clock",
        "reason": "IVOA HiPS properties 的更新日期; 部署内既有墙钟清单已登记。",
        "evidence": "tools/quality/compare_products.py CLOCK_KEYS_TEXT（负责人裁决 4）",
    },
    {
        "key": "generated_utc",
        "category": "wall_clock",
        "reason": "报告/溯源面通用生成时刻。",
        "evidence": "tools/quality/compare_products.py CLOCK_KEYS_TEXT（负责人裁决 4）",
    },
    {
        "key": "timestamp_utc",
        "category": "wall_clock",
        "reason": "报告/溯源面通用时刻戳。",
        "evidence": "tools/quality/compare_products.py CLOCK_KEYS_TEXT（负责人裁决 4）",
    },
    {
        "key": "ended_utc",
        "category": "wall_clock",
        "reason": "节点/运行结束时刻（compare_products 用 ended_utc, 本仓节点 trace 用 finished_utc, 两者同义）。",
        "evidence": "tools/quality/compare_products.py CLOCK_KEYS_TEXT（负责人裁决 4）",
    },
    {
        "key": "hips_release_date",
        "category": "wall_clock",
        "reason": "IVOA HiPS properties 的发布日期; 随运行日变化。",
        "evidence": "IVOA HiPS 1.0 §4.2 properties 键; 同 hips_creation_date",
    },
    {
        "key": "sha256",
        "category": "derived_integrity",
        "reason": "兄弟产物（FITS 主产物）的**文件级**摘要; 其易变性已由该产物自身的 canonical "
                  "hash 覆盖（且更强）, 保留会把 RUNID/CHECKSUM 的易变传播进 sidecar。",
        "evidence": "分层原则: 完整性面（本字段）与可复现面（本工具）分离; astropy 剥离 "
                    "CHECKSUM/DATASUM 同款「派生物不参与内容比较」",
    },
    {
        "key": "product_sha256",
        "category": "derived_integrity",
        "reason": "同 sha256（p3_writer/p3_verify 记录 output_phase3.fits 的文件级摘要）。",
        "evidence": "同 sha256",
    },
    {
        "key": "integrity_sha256",
        "category": "derived_integrity",
        "reason": "DET-001 起 p3_writer/p3_verify 的**文件级**摘要字段名（原 sha256/product_sha256）;"
                  " 其易变性由被引用产物自身的 canonical hash 覆盖（更强）。",
        "evidence": "同 sha256; 命名分层见 tools/canonical_product_hash.py 头注释",
    },
]

EXCLUDED_PROPERTIES_KEYS: List[Dict[str, str]] = [
    {"key": "hips_creation_date", "category": "wall_clock",
     "reason": "同 JSON 侧。", "evidence": "IVOA HiPS 1.0 properties 必备键"},
    {"key": "hips_release_date", "category": "wall_clock",
     "reason": "同 JSON 侧。", "evidence": "IVOA HiPS 1.0 properties 键"},
    {"key": "hips_update_date", "category": "wall_clock",
     "reason": "同 JSON 侧。",
     "evidence": "tools/quality/compare_products.py CLOCK_KEYS_TEXT（负责人裁决 4）"},
]

# 不参与卡片比较的 FITS 结构卡（非内容关键字）
FITS_STRUCTURAL_SKIP = ("END",)


class CanonicalHashError(RuntimeError):
    pass


def _sha256_bytes(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


def _sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ── FITS ────────────────────────────────────────────────────────────────────
def _is_fits(path: pathlib.Path) -> bool:
    try:
        with open(path, "rb") as f:
            head = f.read(80)
    except OSError:
        return False
    return head[:8].decode("ascii", "replace") in ("SIMPLE  ", "XTENSION")


def _apply_subs(text: bytes, subs) -> bytes:
    for old, new in subs or ():
        text = text.replace(old.encode("utf-8"), new.encode("utf-8"))
    return text


def canonical_fits(path: pathlib.Path, subs=None) -> Dict[str, Any]:
    """FITS 规范哈希: 逐 HDU 取 (保留卡原始文本, 按 keyword 排序) + 数据单元原始字节。"""
    from astropy.io import fits  # 本地 import: 非 FITS 路径不依赖 astropy

    raw = path.read_bytes()
    parts: List[bytes] = [DOMAIN]
    excluded = {e["key"].upper() for e in EXCLUDED_FITS_CARDS}
    hdus: List[Dict[str, Any]] = []
    warnings: List[str] = []
    excluded_hits: List[str] = []
    try:
        hdul_cm = fits.open(path, mode="readonly", memmap=False)
    except Exception as exc:
        raise CanonicalHashError("cannot parse FITS %s: %s" % (path, exc))
    with hdul_cm as hdul:
        for idx, hdu in enumerate(hdul):
            fi = hdu.fileinfo()
            hdr_loc = int(fi["hdrLoc"])
            dat_loc = int(fi["datLoc"])
            dat_span = int(fi["datSpan"])
            # 头部: 直接从文件读 80 字节记录（保真, 与语言/浮点格式化无关）
            cards_raw = []
            off = hdr_loc
            order = 0
            while off + 80 <= dat_loc:
                rec = raw[off:off + 80]
                off += 80
                kw = rec[:8].decode("ascii", "replace").strip()
                if kw in FITS_STRUCTURAL_SKIP or rec.strip(b" \x00") == b"":
                    continue
                if kw.upper() in excluded:
                    excluded_hits.append("HDU%d:%s" % (idx, kw))
                    continue
                cards_raw.append((kw.upper(), order, rec.rstrip(b" \x00")))
                order += 1
            cards_raw.sort(key=lambda t: (t[0], t[1]))
            data = raw[dat_loc:dat_loc + dat_span]
            data_sha = _sha256_bytes(data)
            parts.append(("HDU %d\n" % idx).encode("utf-8"))
            for _kw, _o, text in cards_raw:
                parts.append(b"CARD " + _apply_subs(text, subs) + b"\n")
            parts.append(("DATA sha256=%s bytes=%d\n" % (data_sha, len(data))).encode("utf-8"))
            # 交叉校验不变量: DATASUM / CHECKSUM 若存在必须自洽
            ds_verify = ck_verify = None
            ds_card = hdu.header.get("DATASUM")
            try:
                ds_verify = int(hdu.verify_datasum())
            except Exception as exc:  # pragma: no cover - 防御
                warnings.append("HDU%d verify_datasum failed: %s" % (idx, exc))
            try:
                ck_verify = int(hdu.verify_checksum())
            except Exception as exc:  # pragma: no cover - 防御
                warnings.append("HDU%d verify_checksum failed: %s" % (idx, exc))
            if ds_verify == 0:
                raise CanonicalHashError(
                    "integrity violation: HDU%d DATASUM card (%s) != data unit checksum"
                    % (idx, ds_card))
            if ck_verify == 0:
                raise CanonicalHashError(
                    "integrity violation: HDU%d CHECKSUM card does not verify" % idx)
            if ds_verify == -1:
                warnings.append("HDU%d has no DATASUM card (integrity cross-check unavailable)"
                                % idx)
            hdus.append({
                "index": idx,
                "cards_retained": len(cards_raw),
                "data_sha256": data_sha,
                "data_bytes": len(data),
                "datasum_card": ds_card,
                "datasum_verify": ds_verify,
                "checksum_verify": ck_verify,
            })
    return {
        "format": "fits",
        "canonical_sha256": _sha256_bytes(b"".join(parts)),
        "hdus": hdus,
        "excluded_cards": sorted(set(excluded_hits)),
        "excluded_keys_hit": sorted({h.split(":", 1)[1] for h in excluded_hits}),
        "warnings": warnings,
    }


# ── JSON ────────────────────────────────────────────────────────────────────
def _prune_json(obj: Any, excluded: set, hits: List[str], prefix: str = "") -> Any:
    if isinstance(obj, dict):
        out = {}
        for k, v in obj.items():
            if k in excluded:
                hits.append((prefix + "/" + k) if prefix else k)
                continue
            out[k] = _prune_json(v, excluded, hits, (prefix + "/" + k) if prefix else k)
        return out
    if isinstance(obj, list):
        return [_prune_json(v, excluded, hits, prefix + "[]") for v in obj]
    return obj


def _apply_subs_json(obj: Any, subs) -> Any:
    if isinstance(obj, str):
        for old, new in subs or ():
            obj = obj.replace(old, new)
        return obj
    if isinstance(obj, dict):
        return {k: _apply_subs_json(v, subs) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_apply_subs_json(v, subs) for v in obj]
    return obj


def _json_escape(s: str) -> str:
    out = ['"']
    for ch in s:
        o = ord(ch)
        if ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif o < 0x20:
            out.append("\\u%04x" % o)
        else:
            out.append(ch)
    out.append('"')
    return "".join(out)


def _num_text(v: Any) -> str:
    """跨语言一致的数值文本: 整数 -> 十进制; 浮点 -> %.17g（C/Python 同义）。"""
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, int):
        return str(v)
    return "%.17g" % v


def canonical_json_text(obj: Any) -> str:
    """规范 JSON 文本（canonical-product-hash/v1）:

    * 对象键按 UTF-8 字节序升序;
    * 字符串仅转义 '"'、'\\' 与控制字符(\\u00xx), 非 ASCII 原样保留;
    * 数字: 整数十进制; 浮点 %.17g（与 C++ snprintf("%.17g") 逐字一致）;
    * 无空白。
    """
    if obj is None:
        return "null"
    if isinstance(obj, bool):
        return "true" if obj else "false"
    if isinstance(obj, (int, float)):
        return _num_text(obj)
    if isinstance(obj, str):
        return _json_escape(obj)
    if isinstance(obj, list):
        return "[" + ",".join(canonical_json_text(v) for v in obj) + "]"
    if isinstance(obj, dict):
        items = sorted(obj.items(), key=lambda kv: kv[0].encode("utf-8"))
        return "{" + ",".join(_json_escape(k) + ":" + canonical_json_text(v)
                              for k, v in items) + "}"
    raise CanonicalHashError("unsupported JSON value type: %r" % type(obj))


def canonical_json(path: pathlib.Path, excluded_keys: Optional[set] = None,
                   subs=None) -> Dict[str, Any]:
    excluded = excluded_keys if excluded_keys is not None else {e["key"] for e in EXCLUDED_JSON_KEYS}
    doc = json.loads(path.read_text(encoding="utf-8"))
    hits: List[str] = []
    pruned = _prune_json(doc, excluded, hits)
    pruned = _apply_subs_json(pruned, subs)
    body = canonical_json_text(pruned).encode("utf-8")
    return {
        "format": "json",
        "canonical_sha256": _sha256_bytes(DOMAIN + b"JSON\n" + body),
        "excluded_keys_hit": sorted(hits),
        "warnings": [],
    }


# ── HiPS properties（key=value 行文本）───────────────────────────────────────
EXCLUDED_PROPERTIES_SET = {e["key"] for e in EXCLUDED_PROPERTIES_KEYS}


def _is_properties(path: pathlib.Path) -> bool:
    if path.name != "properties":
        return False
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return False
    for line in text.splitlines():
        if line.strip() and not line.lstrip().startswith("#") and "=" not in line:
            return False
    return "=" in text


def canonical_properties(path: pathlib.Path, subs=None) -> Dict[str, Any]:
    hits: List[str] = []
    kept: List[Tuple[str, str]] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        s = line.strip()
        if not s or s.startswith("#") or "=" not in s:
            continue
        k, v = s.split("=", 1)
        k = k.strip()
        v = v.strip()
        if k in EXCLUDED_PROPERTIES_SET:
            hits.append(k)
            continue
        for old, new in subs or ():
            v = v.replace(old, new)
        kept.append((k, v))
    kept.sort()
    body = "".join("%s=%s\n" % (k, v) for k, v in kept).encode("utf-8")
    return {
        "format": "hips-properties",
        "canonical_sha256": _sha256_bytes(DOMAIN + b"PROPERTIES\n" + body),
        "excluded_keys_hit": sorted(hits),
        "warnings": [],
    }


# ── 其它 ────────────────────────────────────────────────────────────────────
def canonical_raw(path: pathlib.Path) -> Dict[str, Any]:
    return {
        "format": "raw",
        "canonical_sha256": _sha256_bytes(DOMAIN + b"RAW\n" + path.read_bytes()),
        "excluded_keys_hit": [],
        "warnings": ["no format-specific canonicalization for this file type; "
                     "canonical hash falls back to raw bytes"],
    }


def hash_file(path: pathlib.Path, base: Optional[pathlib.Path] = None,
              subs=None) -> Dict[str, Any]:
    """subs: [(old, new)] 显式归一化替换（如 A/B 输出目录前缀 -> <OUT>）。
    每次哈希都会把 subs 回显到报告的 normalizations 字段, 不做静默归一化。"""
    if not path.is_file():
        raise CanonicalHashError("not a regular file: %s" % path)
    if _is_fits(path):
        res = canonical_fits(path, subs)
    elif path.suffix.lower() == ".json":
        res = canonical_json(path, subs=subs)
    elif _is_properties(path):
        res = canonical_properties(path, subs)
    else:
        res = canonical_raw(path)
    res["normalizations"] = ["%s -> %s" % (o, n) for o, n in (subs or ())]
    res["path"] = str(path)
    if base is not None:
        try:
            res["rel_path"] = str(path.resolve().relative_to(base.resolve()))
        except ValueError:
            res["rel_path"] = path.name
    res["size_bytes"] = path.stat().st_size
    res["integrity_sha256"] = _sha256_file(path)
    res["spec"] = SPEC_ID
    return res


# ── 目录 / manifest / 判据 ───────────────────────────────────────────────────
def _default_base(paths: List[str]) -> Optional[pathlib.Path]:
    if not paths:
        return None
    try:
        return pathlib.Path(os.path.commonpath(
            [str(pathlib.Path(p).resolve().parent) for p in paths]))
    except ValueError:
        return None


def read_manifest_artifacts(manifest: pathlib.Path) -> List[pathlib.Path]:
    doc = json.loads(manifest.read_text(encoding="utf-8"))
    arts = doc.get("artifacts")
    if not isinstance(arts, list):
        raise CanonicalHashError("manifest has no artifacts[]: %s" % manifest)
    out = []
    for a in arts:
        p = a.get("path") if isinstance(a, dict) else None
        if p:
            out.append(pathlib.Path(p))
    return out


def cmd_hash(args: argparse.Namespace) -> int:
    base = pathlib.Path(args.base) if args.base else None
    try:
        res = hash_file(pathlib.Path(args.path), base)
    except CanonicalHashError as exc:
        print(json.dumps({"error": str(exc)}, ensure_ascii=False), file=sys.stderr)
        return 3
    print(json.dumps(res, indent=1, ensure_ascii=False, sort_keys=True))
    return 0


def cmd_dir(args: argparse.Namespace) -> int:
    paths = read_manifest_artifacts(pathlib.Path(args.manifest))
    base = _default_base([str(p) for p in paths])
    rows = []
    rc = 0
    for p in paths:
        try:
            rows.append(hash_file(p, base))
        except CanonicalHashError as exc:
            rows.append({"path": str(p), "error": str(exc)})
            rc = 3
    print(json.dumps({"spec": SPEC_ID, "manifest": args.manifest, "artifacts": rows},
                     indent=1, ensure_ascii=False, sort_keys=True))
    return rc


def cmd_compare(args: argparse.Namespace) -> int:
    """判据入口: 两次运行的登记产物逐条比较 canonical_sha256。"""
    ma, mb = pathlib.Path(args.manifest_a), pathlib.Path(args.manifest_b)
    pa, pb = read_manifest_artifacts(ma), read_manifest_artifacts(mb)
    ba = pathlib.Path(args.base_a) if args.base_a else _default_base([str(p) for p in pa])
    bb = pathlib.Path(args.base_b) if args.base_b else _default_base([str(p) for p in pb])

    def index(paths, base):
        out = {}
        for p in paths:
            rel = str(p.resolve().relative_to(base.resolve())) if base else p.name
            out[rel] = p
        return out

    subs_a = [(str(ba), "<OUT>")] if (ba and args.normalize_output_dir) else []
    subs_b = [(str(bb), "<OUT>")] if (bb and args.normalize_output_dir) else []

    ia, ib = index(pa, ba), index(pb, bb)
    only_a = sorted(set(ia) - set(ib))
    only_b = sorted(set(ib) - set(ia))
    rows = []
    bad = 0
    err = 0
    for rel in sorted(set(ia) & set(ib)):
        try:
            ra = hash_file(ia[rel], ba, subs_a)
            rb = hash_file(ib[rel], bb, subs_b)
        except CanonicalHashError as exc:
            rows.append({"artifact": rel, "error": str(exc)})
            err += 1
            continue
        same_canon = ra["canonical_sha256"] == rb["canonical_sha256"]
        same_file = ra["integrity_sha256"] == rb["integrity_sha256"]
        if not same_canon:
            bad += 1
        rows.append({
            "artifact": rel,
            "format": ra["format"],
            "canonical_sha256_a": ra["canonical_sha256"],
            "canonical_sha256_b": rb["canonical_sha256"],
            "canonical_match": same_canon,
            "integrity_sha256_a": ra["integrity_sha256"],
            "integrity_sha256_b": rb["integrity_sha256"],
            "integrity_match": same_file,
            "excluded_a": ra.get("excluded_keys_hit", []),
            "excluded_b": rb.get("excluded_keys_hit", []),
        })
    report = {
        "spec": SPEC_ID,
        "manifest_a": str(ma), "manifest_b": str(mb),
        "artifacts_compared": len(rows),
        "canonical_mismatches": bad,
        "errors": err,
        "only_in_a": only_a, "only_in_b": only_b,
        "verdict": "PASS" if (bad == 0 and err == 0 and not only_a and not only_b) else "FAIL",
        "artifacts": rows,
    }
    text = json.dumps(report, indent=1, ensure_ascii=False, sort_keys=True)
    if args.out:
        pathlib.Path(args.out).write_text(text + "\n", encoding="utf-8")
    print(text)
    if only_a or only_b or err:
        return 2
    return 0 if bad == 0 else 2


# ── 口径导出 ────────────────────────────────────────────────────────────────
def cmd_spec(_args: argparse.Namespace) -> int:
    print(json.dumps({
        "spec": SPEC_ID,
        "domain_prefix": DOMAIN.decode("utf-8"),
        "canonicalization": {
            "fits": "per HDU: retained header cards (raw 80-byte card text, trailing blanks "
                    "stripped, sorted by (KEYWORD, occurrence index)) + data-unit raw bytes "
                    "sha256; DATASUM/CHECKSUM cards cross-checked as invariants",
            "json": "parse; drop keys in excluded_json_keys (recursively); serialize with the "
                    "cross-language canonical JSON text form (keys sorted by UTF-8 byte order, "
                    "no whitespace, ints decimal, floats %.17g, minimal string escaping); sha256",
            "hips-properties": "key=value lines; drop excluded keys; sort by key; sha256",
            "other": "raw bytes sha256 (reported as format=raw with a warning)",
        },
        "excluded_fits_cards": EXCLUDED_FITS_CARDS,
        "excluded_json_keys": EXCLUDED_JSON_KEYS,
        "excluded_properties_keys": EXCLUDED_PROPERTIES_KEYS,
        "never_excluded": [
            "DATE-OBS / MJD-OBS / TELAPSE / EXPOSURE / EXPTIME（观测时刻与曝光）",
            "FILTER / INSTRUME / TELESCOP / OBJECT / OBSERVER",
            "BUNIT / BSCALE / BZERO / PHOTSCAL / PHOTAPPL / PHOTDEGRADE",
            "CRPIX* / CRVAL* / CD*_* / CDELT* / CTYPE* / CUNIT* / SIP 系数",
            "像素数据（数据单元原始字节）",
            "nside / nested / pixfrac / precision_mode / n_healpix_pixels / "
            "n_source_pixels 等算法 provenance",
        ],
        "invariants": [
            "DATASUM card (if present) must equal the data-unit 1's complement checksum -> exit 3",
            "CHECKSUM card (if present) must verify -> exit 3",
            "any difference outside the exclusion lists changes canonical_sha256",
        ],
    }, indent=1, ensure_ascii=False, sort_keys=True))
    return 0


# ── 可执行负例面 ────────────────────────────────────────────────────────────
def _write_synth_fits(path: pathlib.Path, crval1: float, pixel0: float, runid: str,
                      date: str) -> None:
    import numpy as np
    from astropy.io import fits
    data = np.zeros((4, 4), dtype=">f4")
    data[0, 0] = pixel0
    hdu = fits.PrimaryHDU(data)
    hdu.header["CRVAL1"] = crval1
    hdu.header["CRVAL2"] = 34.0
    hdu.header["BUNIT"] = "ADU"
    hdu.header["DATE-OBS"] = "2026-01-01T00:00:00"
    hdu.header["RUNID"] = runid
    hdu.header["DATE"] = date
    hdu.writeto(path, overwrite=True, checksum=True)


def self_test() -> int:
    import tempfile
    failures: List[str] = []
    checks = 0

    def expect(cond: bool, msg: str) -> None:
        nonlocal checks
        checks += 1
        if not cond:
            failures.append(msg)

    with tempfile.TemporaryDirectory(prefix="cph_selftest_") as td:
        tmp = pathlib.Path(td)
        # ── 正例 1: 仅易变元数据不同（RUNID/DATE/CHECKSUM/DATASUM 注释）=> 必须不变
        f1, f2 = tmp / "a.fits", tmp / "b.fits"
        _write_synth_fits(f1, 210.0, 1.0, "aaaaaaaaaaaa", "2026-01-01T00:00:00")
        _write_synth_fits(f2, 210.0, 1.0, "bbbbbbbbbbbb", "2026-06-06T12:00:00")
        h1, h2 = hash_file(f1), hash_file(f2)
        expect(h1["integrity_sha256"] != h2["integrity_sha256"],
               "positive control: file-level sha256 MUST differ when RUNID/DATE differ")
        expect(h1["canonical_sha256"] == h2["canonical_sha256"],
               "excluded-only difference MUST NOT change canonical hash")
        expect("RUNID" in h1["excluded_keys_hit"] and "DATE" in h1["excluded_keys_hit"],
               "exclusion hits must be reported")

        # ── 反例 1: 改一个像素 => 必须变
        f3 = tmp / "c.fits"
        _write_synth_fits(f3, 210.0, 2.0, "aaaaaaaaaaaa", "2026-01-01T00:00:00")
        expect(hash_file(f3)["canonical_sha256"] != h1["canonical_sha256"],
               "one changed pixel MUST change canonical hash")

        # ── 反例 2: 改一个科学卡（CRVAL1）=> 必须变
        f4 = tmp / "d.fits"
        _write_synth_fits(f4, 210.5, 1.0, "aaaaaaaaaaaa", "2026-01-01T00:00:00")
        expect(hash_file(f4)["canonical_sha256"] != h1["canonical_sha256"],
               "one changed science card (CRVAL1) MUST change canonical hash")

        # ── 反例 3: 篡改 DATASUM 卡值 => 必须硬错（不静默通过）
        bad = tmp / "bad.fits"
        raw = bytearray(f1.read_bytes())
        i = raw.find(b"DATASUM ")
        expect(i >= 0, "synthetic FITS must carry DATASUM")
        if i >= 0:
            # 等长改写（80 字节卡）：结构不变, 只有卡值被篡改
            card = (b"DATASUM = '123456789'          / data unit checksum updated "
                    b"2026-01-01T00:00:00")
            raw[i:i + 80] = card.ljust(80)[:80]
            bad.write_bytes(bytes(raw))
            try:
                hash_file(bad)
                expect(False, "tampered DATASUM MUST raise (fail-closed)")
            except CanonicalHashError:
                expect(True, "")

        # ── JSON: 排除键变化不变 / 科学值变化必变
        j1, j2, j3 = tmp / "j1.json", tmp / "j2.json", tmp / "j3.json"
        j1.write_text(json.dumps({"schema": "X", "run_id": "aaa", "photscal": 1.0,
                                  "elapsed_sec": 0.1, "sha256": "0" * 64,
                                  "fluxes": [1.0, 2.0]}), encoding="utf-8")
        j2.write_text(json.dumps({"schema": "X", "run_id": "bbb", "photscal": 1.0,
                                  "elapsed_sec": 9.9, "sha256": "1" * 64,
                                  "fluxes": [1.0, 2.0]}), encoding="utf-8")
        j3.write_text(json.dumps({"schema": "X", "run_id": "aaa", "photscal": 1.0000001,
                                  "elapsed_sec": 0.1, "sha256": "0" * 64,
                                  "fluxes": [1.0, 2.0]}), encoding="utf-8")
        expect(hash_file(j1)["canonical_sha256"] == hash_file(j2)["canonical_sha256"],
               "excluded JSON keys only MUST NOT change canonical hash")
        expect(hash_file(j3)["canonical_sha256"] != hash_file(j1)["canonical_sha256"],
               "changed scientific JSON value MUST change canonical hash")

        # ── HiPS properties: 时间戳变化不变 / 科学键变化必变
        p1 = tmp / "properties"
        p2 = tmp / "p2" / "properties"
        p3 = tmp / "p3" / "properties"
        for d in (p2.parent, p3.parent):
            d.mkdir(parents=True, exist_ok=True)
        body = ("creator_did=astrocs/phase1\nhips_order=0\nhips_pixel_scale=412.25\n"
                "hips_creation_date=%s\nhips_release_date=%s\nobs_filter=R\n")
        p1.write_text(body % ("2026-01-01T00:00:00Z", "2026-01-01"), encoding="utf-8")
        p2.write_text(body % ("2026-09-17T12:34:56Z", "2026-09-17"), encoding="utf-8")
        p3.write_text((body % ("2026-01-01T00:00:00Z", "2026-01-01")).replace(
            "hips_pixel_scale=412.25", "hips_pixel_scale=412.26"), encoding="utf-8")
        expect(hash_file(p1)["canonical_sha256"] == hash_file(p2)["canonical_sha256"],
               "creation-date-only change MUST NOT change canonical hash")
        expect(hash_file(p3)["canonical_sha256"] != hash_file(p1)["canonical_sha256"],
               "changed scientific properties key MUST change canonical hash")

    if failures:
        print("CANONICAL_HASH_SELFTEST_FAIL: checks=%d failures=%d" % (checks, len(failures)))
        for f in failures:
            print("  - " + f)
        return 1
    print("CANONICAL_HASH_SELFTEST_PASS: checks=%d (positive+negative control both live)" % checks)
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="AstroCS canonical product hash (DET-001)")
    p.add_argument("--spec", action="store_true", help="print the versioned canonicalization spec")
    p.add_argument("--self-test", action="store_true", help="run the executable self-test")
    sub = p.add_subparsers(dest="cmd")
    h = sub.add_parser("hash", help="hash a single product file")
    h.add_argument("path")
    h.add_argument("--base", default=None, help="directory to relativize the path against")
    h.set_defaults(func=cmd_hash)
    d = sub.add_parser("dir", help="hash every registered artifact of a run manifest")
    d.add_argument("manifest")
    d.set_defaults(func=cmd_dir)
    c = sub.add_parser("compare", help="compare registered artifacts of two runs (verdict gate)")
    c.add_argument("manifest_a")
    c.add_argument("manifest_b")
    c.add_argument("--base-a", default=None)
    c.add_argument("--base-b", default=None)
    c.add_argument("--normalize-output-dir", action="store_true",
                   help="replace each run's output_dir prefix by <OUT> before hashing "
                        "(recorded in the report as a normalization, not an exclusion)")
    c.add_argument("--out", default=None)
    c.set_defaults(func=cmd_compare)
    return p


def main(argv: Optional[List[str]] = None) -> int:
    args = build_parser().parse_args(argv)
    if args.spec:
        return cmd_spec(args)
    if args.self_test:
        return self_test()
    if not getattr(args, "func", None):
        build_parser().print_help()
        return 3
    try:
        return int(args.func(args))
    except CanonicalHashError as exc:
        print(json.dumps({"error": str(exc)}, ensure_ascii=False), file=sys.stderr)
        return 3
    except FileNotFoundError as exc:
        print(json.dumps({"error": str(exc)}, ensure_ascii=False), file=sys.stderr)
        return 3
    except OSError as exc:
        print(json.dumps({"error": str(exc)}, ensure_ascii=False), file=sys.stderr)
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
