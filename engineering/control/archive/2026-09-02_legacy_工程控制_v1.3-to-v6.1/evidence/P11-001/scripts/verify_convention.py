"""
P11-001 坐标约定冻结验证脚本.

验证 COORDINATE_CONVENTION.md 中冻结的约定与代码实际状态一致.
仅做读取检查, 不修改任何代码.

用法: python verify_convention.py
退出码: 0=全部通过, 1=有失败
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[4]  # evidence/P11-001/scripts/ -> repo root


def check(condition: bool, name: str, detail: str = "") -> bool:
    status = "PASS" if condition else "FAIL"
    msg = f"[{status}] {name}"
    if detail:
        msg += f" — {detail}"
    print(msg)
    return condition


# ---------------------------------------------------------------------------
# S2 内部 U 坐标: 中心点 cx = img_w/2.0 (无 +0.5)
# ---------------------------------------------------------------------------

def test_internal_center_no_plus_half() -> bool:
    f = REPO_ROOT / "lib/plate_solve/cpp/ipv/src/ipv_select.cpp"
    if not f.exists():
        return check(False, "S2_center_no_plus_half", f"file not found: {f}")
    text = f.read_text(encoding="utf-8", errors="replace")
    # 期望: double cx = img_w / 2.0, cy = img_h / 2.0;
    has = ("img_w / 2.0" in text and "img_h / 2.0" in text)
    # 不应有 width/2.0 + 0.5 在 select.cpp 中
    no_plus = "+ 0.5" not in text.split("gnomonic_forward_proj")[0] if "gnomonic_forward_proj" in text else True
    return check(has and no_plus, "S2_center_no_plus_half",
                 "cx=img_w/2.0 cy=img_h/2.0 (无 +0.5)" if has and no_plus else "中心点公式不符")


# ---------------------------------------------------------------------------
# S2 内部 U 坐标: Y 反转 U.y = -(det_y - cy)
# ---------------------------------------------------------------------------

def test_internal_y_flip() -> bool:
    f = REPO_ROOT / "lib/plate_solve/cpp/ipv/src/ipv_select.cpp"
    text = f.read_text(encoding="utf-8", errors="replace")
    has = "-(det_y[idx] - cy)" in text or "-(det_y[ idx ] - cy)" in text
    return check(has, "S2_y_flip", "U.y = -(det_y - cy)" if has else "Y 反转公式缺失")


# ---------------------------------------------------------------------------
# S3 CRPIX 1-based: width/2.0 + 0.5
# ---------------------------------------------------------------------------

def test_crpix_1based() -> bool:
    f = REPO_ROOT / "lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp"
    text = f.read_text(encoding="utf-8", errors="replace")
    has = ("img_width  / 2.0 + 0.5" in text or "img_width/2.0 + 0.5" in text
           or "img_width / 2.0 + 0.5" in text)
    has2 = ("img_height / 2.0 + 0.5" in text or "img_height/2.0 + 0.5" in text)
    return check(has and has2, "S3_crpix_1based",
                 "CRPIX = width/2.0 + 0.5" if has and has2 else "CRPIX 公式不符")


# ---------------------------------------------------------------------------
# S3 CD 矩阵无 1/cos(Dec) 因子: TRANS / 3600
# ---------------------------------------------------------------------------

def test_cd_no_cosdec_factor() -> bool:
    f = REPO_ROOT / "lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp"
    text = f.read_text(encoding="utf-8", errors="replace")
    has = ("trans.x10 / 3600.0" in text and "trans.x01 / 3600.0" in text)
    return check(has, "S3_cd_no_cosdec", "CD = TRANS/3600 (无 cos(Dec) 因子)" if has else "CD 公式不符")


# ---------------------------------------------------------------------------
# S3 Y-up → Y-down 转换: CD.cd12/cd22 取反
# ---------------------------------------------------------------------------

def test_y_up_to_down_conversion() -> bool:
    f = REPO_ROOT / "lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp"
    text = f.read_text(encoding="utf-8", errors="replace")
    has_cd12_neg = "cd.cd12 = -result->cd.cd12" in text or "cd12 = -result" in text
    has_cd22_neg = "cd.cd22 = -result->cd.cd22" in text or "cd22 = -result" in text
    return check(has_cd12_neg and has_cd22_neg, "S3_y_up_to_down",
                 "CD.cd12/cd22 取反" if has_cd12_neg and has_cd22_neg else "Y-up→Y-down 转换缺失")


# ---------------------------------------------------------------------------
# S3 SIP 符号调整: A * (-1)^j, B * -(-1)^j
# ---------------------------------------------------------------------------

def test_sip_sign_adjustment() -> bool:
    f = REPO_ROOT / "lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp"
    text = f.read_text(encoding="utf-8", errors="replace")
    has_sign_in = "sign_in" in text and "(-1)^j" in text.replace(" ", "")
    has_a_adj = "A[idx]  *= sign_in" in text or "A[idx] *= sign_in" in text
    has_b_adj = "B[idx]  *= -sign_in" in text or "B[idx] *= -sign_in" in text
    return check(has_sign_in and has_a_adj and has_b_adj, "S3_sip_sign",
                 "A *= (-1)^j, B *= -(-1)^j" if all([has_sign_in, has_a_adj, has_b_adj]) else "SIP 符号调整缺失")


# ---------------------------------------------------------------------------
# S3 CRVAL/CRPIX 不变 (Y-flip 后)
# ---------------------------------------------------------------------------

def test_crval_crpix_unchanged() -> bool:
    f = REPO_ROOT / "lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp"
    text = f.read_text(encoding="utf-8", errors="replace")
    # 期望在 Y-flip 段落附近有 CRVAL/CRPIX 不变的注释
    # 检查 CRVAL 不被取反
    has_no_crval_neg = "crval1 = -" not in text and "crval2 = -" not in text
    has_no_crpix_neg = "crpix[0] = -" not in text and "crpix[1] = -" not in text
    return check(has_no_crval_neg and has_no_crpix_neg, "S3_crval_crpix_unchanged",
                 "CRVAL/CRPIX 不被取反" if has_no_crval_neg and has_no_crpix_neg else "CRVAL/CRPIX 被修改")


# ---------------------------------------------------------------------------
# astro_image_io shape = (height, width)
# ---------------------------------------------------------------------------

def test_image_shape_height_width() -> bool:
    f = REPO_ROOT / "lib/astro_image_io/python/astro_image_io.py"
    text = f.read_text(encoding="utf-8", errors="replace")
    has = "return (self.height, self.width)" in text or "return (self._height, self._width)" in text
    return check(has, "aio_shape_h_w", "shape=(height,width)" if has else "shape 约定不符")


# ---------------------------------------------------------------------------
# astro_image_io CRPIX 原样读写 (无 ±1 转换)
# ---------------------------------------------------------------------------

def test_aio_crpix_passthrough() -> bool:
    f = REPO_ROOT / "lib/astro_image_io/src/aio_fits.cpp"
    text = f.read_text(encoding="utf-8", errors="replace")
    # 期望: wcs.crpix1 = kw_float("CRPIX1"); 不做 +1 或 -1
    has = 'wcs.crpix1 = kw_float("CRPIX1")' in text or 'wcs.crpix1=kw_float("CRPIX1")' in text
    no_plus1 = "crpix1 + 1" not in text and "crpix1 - 1" not in text
    return check(has and no_plus1, "aio_crpix_passthrough",
                 "CRPIX 原样读写 (无 ±1)" if has and no_plus1 else "CRPIX 有 ±1 转换")


# ---------------------------------------------------------------------------
# astro_image_io has_wcs 判定一致
# ---------------------------------------------------------------------------

def test_has_wcs_logic() -> bool:
    f = REPO_ROOT / "lib/astro_image_io/src/aio_fits.cpp"
    text = f.read_text(encoding="utf-8", errors="replace")
    has = ("ctype1[0] != '\\0'" in text and "ctype2[0] != '\\0'" in text
           and "1e-15" in text)
    return check(has, "aio_has_wcs", "CTYPE 非空 + CD |val|>1e-15" if has else "has_wcs 逻辑不符")


# ---------------------------------------------------------------------------
# Photometric wcs_transform: x - (crpix1 - 1.0) 模式
# ---------------------------------------------------------------------------

def test_photometric_crpix_pattern() -> bool:
    f = REPO_ROOT / "lib/photometric_calib/cpp/src/wcs_transform.cpp"
    if not f.exists():
        return check(False, "photometric_crpix_pattern", f"file not found: {f}")
    text = f.read_text(encoding="utf-8", errors="replace")
    has = "m_crpix1 - 1.0" in text and "m_crpix2 - 1.0" in text
    return check(has, "photometric_crpix_pattern",
                 "dx = x - (crpix1 - 1.0)" if has else "CRPIX 1-based 偏移模式缺失")


# ---------------------------------------------------------------------------
# Photometric CD 直接应用 (无显式 cos(Dec))
# ---------------------------------------------------------------------------

def test_photometric_no_explicit_cosdec() -> bool:
    d = REPO_ROOT / "lib/photometric_calib"
    if not d.exists():
        return check(False, "photometric_no_cosdec", f"dir not found: {d}")
    # 搜索 cos(dec) / cos_dec / cosDec / cosd(dec) 在整个 photometric_calib 目录
    found = False
    for p in d.rglob("*.cpp"):
        text = p.read_text(encoding="utf-8", errors="replace")
        for line in text.splitlines():
            low = line.lower().replace(" ", "")
            if ("cos(dec)" in low or "cos_dec" in low or "cosdec" in low or "cosd(dec)" in low
                or "cos(m_crval2" in low):
                # 排除注释行
                if not line.strip().startswith("//") and not line.strip().startswith("*"):
                    found = True
                    break
        if found:
            break
    return check(not found, "photometric_no_explicit_cosdec",
                 "无显式 cos(Dec) 乘法" if not found else "发现显式 cos(Dec) 乘法")


# ---------------------------------------------------------------------------
# Drizzle wcs_sip 与 Photometric 一致 (CRPIX 1-based)
# ---------------------------------------------------------------------------

def test_drizzle_crpix_pattern() -> bool:
    f = REPO_ROOT / "lib/healpix_db/healpix_drizzle/wcs_sip.cpp"
    if not f.exists():
        return check(False, "drizzle_crpix_pattern", f"file not found: {f}")
    text = f.read_text(encoding="utf-8", errors="replace")
    # Drizzle 使用 m_wcs.crpix[0] - 1.0 (数组索引), Photometric 使用 m_crpix1 - 1.0 (成员变量)
    # 约定一致: CRPIX 1-based, 减 1 得到 0-based 偏移
    has = ("crpix[0] - 1.0" in text or "crpix1 - 1.0" in text or "crpix1-1.0" in text)
    has_comment = "1-based" in text and "0-based" in text
    return check(has and has_comment, "drizzle_crpix_pattern",
                 "dx = x - (crpix[0] - 1.0), CRPIX 1-based 注释存在" if has and has_comment else "CRPIX 1-based 偏移模式缺失")


# ---------------------------------------------------------------------------
# 接口注释: ipv_itertrans.h:80 U 坐标定义
# ---------------------------------------------------------------------------

def test_itertrans_u_comment() -> bool:
    f = REPO_ROOT / "lib/plate_solve/cpp/ipv/include/ipv_itertrans.h"
    text = f.read_text(encoding="utf-8", errors="replace")
    has = "U - 图像侧星点" in text or "U 坐标" in text or "Y 轴向上" in text
    return check(has, "iface_itertrans_u_comment",
                 "U 坐标定义注释存在" if has else "U 坐标注释缺失")


# ---------------------------------------------------------------------------
# 接口注释: ipv_sip.h cos(dec) 公式注释
# ---------------------------------------------------------------------------

def test_sip_cosdec_comment() -> bool:
    f = REPO_ROOT / "lib/plate_solve/cpp/ipv/include/ipv_sip.h"
    text = f.read_text(encoding="utf-8", errors="replace")
    has = "cos(dec)" in text or "cos(dec)" in text.lower()
    return check(has, "iface_sip_cosdec_comment",
                 "cos(dec) 公式注释存在" if has else "cos(dec) 注释缺失")


# ---------------------------------------------------------------------------
# 接口注释: ipv_api.h CRPIX 1-based
# ---------------------------------------------------------------------------

def test_api_crpix_comment() -> bool:
    f = REPO_ROOT / "lib/plate_solve/cpp/ipv/include/ipv_api.h"
    text = f.read_text(encoding="utf-8", errors="replace")
    has = "1-based" in text or "1based" in text.lower()
    return check(has, "iface_api_crpix_comment",
                 "CRPIX 1-based 注释存在" if has else "1-based 注释缺失")


# ---------------------------------------------------------------------------
# 接口注释: wcs_transform.h 坐标约定块
# ---------------------------------------------------------------------------

def test_wcs_transform_convention_block() -> bool:
    f = REPO_ROOT / "lib/photometric_calib/cpp/src/wcs_transform.h"
    if not f.exists():
        return check(False, "iface_wcs_transform_block", f"file not found: {f}")
    text = f.read_text(encoding="utf-8", errors="replace")
    has = "CRPIX" in text and "1-based" in text and "0-based" in text
    return check(has, "iface_wcs_transform_block",
                 "坐标约定注释块存在" if has else "坐标约定注释块缺失")


# ---------------------------------------------------------------------------
# 禁止捷径: 不得先改符号 (本次任务未修改代码)
# ---------------------------------------------------------------------------

def test_no_code_modification() -> bool:
    """验证本任务未修改任何代码文件 (仅创建文档)."""
    # 通过 git status 检查
    import subprocess
    try:
        r = subprocess.run(
            ["git", "diff", "--name-only"],
            cwd=str(REPO_ROOT), capture_output=True, text=True, timeout=10
        )
        # 期望: 没有代码文件被修改 (只允许 evidence/P11-001/ 下新增)
        modified = [l for l in r.stdout.strip().splitlines() if l.strip()]
        code_files = [f for f in modified
                      if not f.startswith("engineering_v1.2/evidence/P11-001")
                      and not f.startswith("engineering_v1.2/control/")]
        return check(len(code_files) == 0, "forbidden_no_code_modification",
                     f"无代码修改 (modified={modified})" if not code_files
                     else f"代码文件被修改: {code_files}")
    except Exception as e:
        return check(False, "forbidden_no_code_modification", f"git diff 失败: {e}")


# ---------------------------------------------------------------------------
# COORDINATE_CONVENTION.md 存在
# ---------------------------------------------------------------------------

def test_convention_doc_exists() -> bool:
    f = REPO_ROOT / "engineering_v1.2/evidence/P11-001/COORDINATE_CONVENTION.md"
    exists = f.exists()
    size = f.stat().st_size if exists else 0
    return check(exists and size > 5000, "deliverable_convention_doc",
                 f"COORDINATE_CONVENTION.md 存在 ({size} bytes)" if exists else "文档不存在")


# ---------------------------------------------------------------------------
# 主测试入口
# ---------------------------------------------------------------------------

def main() -> int:
    print("=" * 70)
    print("P11-001 坐标约定冻结验证")
    print("=" * 70)

    tests = [
        # S2 内部 U 坐标
        test_internal_center_no_plus_half,
        test_internal_y_flip,
        # S3 FITS WCS
        test_crpix_1based,
        test_cd_no_cosdec_factor,
        test_y_up_to_down_conversion,
        test_sip_sign_adjustment,
        test_crval_crpix_unchanged,
        # astro_image_io
        test_image_shape_height_width,
        test_aio_crpix_passthrough,
        test_has_wcs_logic,
        # Photometric
        test_photometric_crpix_pattern,
        test_photometric_no_explicit_cosdec,
        # Drizzle
        test_drizzle_crpix_pattern,
        # 接口注释
        test_itertrans_u_comment,
        test_sip_cosdec_comment,
        test_api_crpix_comment,
        test_wcs_transform_convention_block,
        # 禁止捷径
        test_no_code_modification,
        # 交付物
        test_convention_doc_exists,
    ]

    passed = 0
    failed = 0
    for t in tests:
        try:
            if t():
                passed += 1
            else:
                failed += 1
        except Exception as e:
            print(f"[FAIL] {t.__name__} — 异常: {e}")
            failed += 1

    print("=" * 70)
    print(f"总计: {passed + failed}  通过: {passed}  失败: {failed}")
    print("=" * 70)
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
