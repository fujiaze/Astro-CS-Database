#!/usr/bin/env python3
"""Issue #106 配置/schema 判据测试（JSON Schema 层）。

用 `jsonschema` 以仓内正本 `eng/contracts/schemas/phase_config_normalize.schema.json`
校验 `photometry.fit.response` / `photometry.frame_overrides[].response` 的形状:
  * 正例: 参数化窄带（fit 级 / 逐帧 override）、measured 曲线、旧路径不变;
  * 反例: 未知 mode、缺 center_nm/fwhm_nm、fwhm<=0、未知 profile/version、
    未知键、measured 缺 curve_name。

**注意**: 同层 `response` + 旧 `filter` 并存是**运行期**规则（RESPONSE_SOURCE_CONFLICT）,
不是 schema 层可表达的; 该判据由生产节点 module_adapters 执行。

用法:
    python3 eng/tests/unit/narrowband_response_config_test.py
"""
from __future__ import annotations

import json
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SCHEMA = os.path.join(REPO, "eng", "contracts", "schemas",
                      "phase_config_normalize.schema.json")

try:
    import jsonschema
except ImportError:  # pragma: no cover
    print("SKIP: jsonschema not installed")
    sys.exit(0)

with open(SCHEMA, "r", encoding="utf-8") as fh:
    SCHEMA_DOC = json.load(fh)

VALIDATOR = jsonschema.Draft202012Validator(SCHEMA_DOC)

n_fail = 0
n_check = 0


def base_config(photometry: dict) -> dict:
    # 平铺单块简写形态（schema 的 else 分支）: schema_version + input_lights
    # + output_dir 必填。
    return {
        "schema_version": "1",
        "input_lights": ["/data/light1.fits"],
        "output_dir": "/tmp/out",
        "photometry": photometry,
    }


def check(ok: bool, what: str) -> None:
    global n_fail, n_check
    n_check += 1
    print(f"[{'PASS' if ok else 'FAIL'}] {what}")
    if not ok:
        n_fail += 1


def accepts(cfg: dict) -> bool:
    return VALIDATOR.is_valid(cfg)


def main() -> int:
    print("=" * 78)
    print("Issue #106 配置/schema 判据（phase_config_normalize.schema.json）")
    print("=" * 78)

    # ── 正例 ────────────────────────────────────────────────────────────
    check(accepts(base_config({"fit": {
        "enabled": True,
        "gaia_data_dir": "/gaia/GaiaDR3SP",
        "qe_name": "KAF-16803",
        "response": {"mode": "parametric_narrowband", "center_nm": 656.28,
                     "fwhm_nm": 3.0, "profile": "top_hat_approx",
                     "profile_version": 1},
    }})), "fit 级参数化窄带被接受")

    check(accepts(base_config({"fit": {
        "enabled": True,
        "gaia_data_dir": "/gaia/GaiaDR3SP",
        "qe_name": "Ideal QE curve",
        "response": {"mode": "measured_curve", "curve_name": "Baader 7nm H-alpha"},
    }})), "fit 级 measured 曲线被接受")

    check(accepts(base_config({"fit": {
        "enabled": True, "gaia_data_dir": "/g", "qe_name": "KAF-16803",
        "filter": "Baader_R",
    }})), "旧路径（fit.filter）仍被接受（行为逐位不变）")

    check(accepts(base_config({"fit": {
        "enabled": True, "gaia_data_dir": "/g", "qe_name": "KAF-16803",
    },
        "frame_overrides": [
            {"input": "/data/light1.fits",
             "response": {"mode": "parametric_narrowband", "center_nm": 500.0,
                          "fwhm_nm": 5.0}},
        ]})), "逐帧 override 的参数化窄带被接受")

    check(accepts(base_config({"fit": {
        "enabled": True, "gaia_data_dir": "/g", "qe_name": "KAF-16803",
    },
        "frame_overrides": [
            {"input": "/data/light1.fits", "filter": "Baader_R"},
        ]})), "逐帧 override 的旧 filter 仍被接受")

    # ── 反例 ────────────────────────────────────────────────────────────
    bad = [
        ("未知 mode", {"mode": "gaussian", "center_nm": 656.28, "fwhm_nm": 3.0}),
        ("参数化缺 center_nm", {"mode": "parametric_narrowband", "fwhm_nm": 3.0}),
        ("参数化缺 fwhm_nm", {"mode": "parametric_narrowband", "center_nm": 656.28}),
        ("fwhm<=0", {"mode": "parametric_narrowband", "center_nm": 656.28,
                     "fwhm_nm": 0.0}),
        ("center_nm<=0", {"mode": "parametric_narrowband", "center_nm": -1.0,
                          "fwhm_nm": 3.0}),
        ("未知 profile", {"mode": "parametric_narrowband", "center_nm": 656.28,
                          "fwhm_nm": 3.0, "profile": "gauss_1nm"}),
        ("未知 profile_version", {"mode": "parametric_narrowband",
                                  "center_nm": 656.28, "fwhm_nm": 3.0,
                                  "profile_version": 2}),
        ("measured 缺 curve_name", {"mode": "measured_curve"}),
        ("response 内未知键", {"mode": "parametric_narrowband", "center_nm": 656.28,
                               "fwhm_nm": 3.0, "od5": 5}),
        ("response 缺 mode", {"center_nm": 656.28, "fwhm_nm": 3.0}),
    ]
    for what, resp in bad:
        check(not accepts(base_config({"fit": {
            "enabled": True, "gaia_data_dir": "/g", "qe_name": "KAF-16803",
            "response": resp,
        }})), f"反例被拒绝: {what}")

    print("-" * 78)
    print(f"checks {n_check}, failures {n_fail}")
    if n_fail:
        print("CONFIG SCHEMA TEST FAILED")
        return 1
    print("CONFIG SCHEMA TEST PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
