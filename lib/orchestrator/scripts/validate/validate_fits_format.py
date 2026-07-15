"""
validate_fits_format.py - FITS 格式验证脚本
功能: 用 astropy 验证导出 FITS 的格式正确性
用途: 全链路调试阶段验证 FITS 文件是否符合标准
调用: python validate_fits_format.py <fits_file> [--output <output_json>]
      python validate_fits_format.py --dir <directory> [--output <output_json>]

验证项:
  1. astropy.io.fits.open 打开不报错
  2. SIMPLE 卡片为逻辑值 T (bool 类型, 无引号)
  3. BITPIX 为 -32 (float32) 或 16 (uint16)
  4. NAXIS1/NAXIS2 与 data shape 匹配
  5. 像素数据为大端字节序 (data.dtype.byteorder == '>')
  6. EXTEND 卡片存在且为 T
  7. header 中没有非法字符 (控制字符)
  8. 每个卡片的关键字长度 <= 8 字符
"""

import argparse
import json
import logging
import os
import sys
import warnings
from datetime import datetime

import numpy as np
from astropy.io import fits
from astropy.utils.exceptions import AstropyWarning

# ============================================================================
# 日志配置
# ============================================================================

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_LOG_DIR = os.path.join(_SCRIPT_DIR, "logs")
os.makedirs(_LOG_DIR, exist_ok=True)

_LOG_FILE = os.path.join(
    _LOG_DIR, f"validate_fits_{datetime.now().strftime('%Y-%m-%d')}.log"
)

logging.basicConfig(
    level=logging.DEBUG,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.FileHandler(_LOG_FILE, encoding="utf-8"),
        logging.StreamHandler(sys.stderr),
    ],
)
logger = logging.getLogger(__name__)


# ============================================================================
# 单项验证函数
# ============================================================================

def _check_file_exists(fits_path):
    """前置检查: 文件存在性"""
    if os.path.isfile(fits_path):
        return {
            "name": "file_exists",
            "pass": True,
            "detail": "文件存在",
        }
    return {
        "name": "file_exists",
        "pass": False,
        "detail": f"文件不存在: {fits_path}",
    }


def _check_open_success(fits_path):
    """检查1: astropy.io.fits.open 打开不报错"""
    try:
        with fits.open(fits_path) as hdul:
            n_hdus = len(hdul)
        return {
            "name": "open_success",
            "pass": True,
            "detail": f"astropy 打开成功, {n_hdus} 个 HDU",
        }, n_hdus
    except Exception as e:
        return {
            "name": "open_success",
            "pass": False,
            "detail": f"打开失败: {type(e).__name__}: {e}",
        }, 0


def _check_simple_card(header):
    """检查2: SIMPLE 卡片为逻辑值 T (bool 类型, 无引号)"""
    try:
        simple_val = header["SIMPLE"]
    except KeyError:
        return {
            "name": "simple_card",
            "pass": False,
            "detail": "SIMPLE 卡片不存在",
        }

    is_bool = isinstance(simple_val, (bool, np.bool_))
    is_true = bool(simple_val) if is_bool else False

    if is_bool and is_true:
        logger.debug("SIMPLE 卡片: True (bool) OK")
        return {
            "name": "simple_card",
            "pass": True,
            "detail": "SIMPLE=True (bool)",
        }

    type_name = type(simple_val).__name__
    msg = f"SIMPLE={simple_val!r} (type={type_name}, 非逻辑值)"
    logger.warning(msg)
    return {
        "name": "simple_card",
        "pass": False,
        "detail": msg,
    }


def _check_bitpix(header):
    """检查3: BITPIX 为 -32 (float32) 或 16 (uint16)"""
    try:
        bitpix = header["BITPIX"]
    except KeyError:
        return {
            "name": "bitpix",
            "pass": False,
            "detail": "BITPIX 卡片不存在",
        }

    if bitpix in (-32, 16):
        logger.debug(f"BITPIX: {bitpix} OK")
        return {
            "name": "bitpix",
            "pass": True,
            "detail": f"BITPIX={bitpix}",
        }

    msg = f"BITPIX={bitpix} (期望 -32 或 16)"
    logger.warning(msg)
    return {
        "name": "bitpix",
        "pass": False,
        "detail": msg,
    }


def _check_naxis_shape(header, data):
    """检查4: NAXIS1/NAXIS2 与 data shape 匹配"""
    if data is None:
        return {
            "name": "naxis_shape",
            "pass": False,
            "detail": "data 为 None, 无法检查 NAXIS",
        }

    try:
        naxis = header.get("NAXIS", 0)
        naxis1 = header["NAXIS1"]
    except KeyError as e:
        return {
            "name": "naxis_shape",
            "pass": False,
            "detail": f"缺少 NAXIS 卡片: {e}",
        }

    detail_parts = [f"NAXIS={naxis}, NAXIS1={naxis1}"]
    if naxis >= 2:
        try:
            naxis2 = header["NAXIS2"]
            detail_parts.append(f"NAXIS2={naxis2}")
        except KeyError:
            return {
                "name": "naxis_shape",
                "pass": False,
                "detail": "NAXIS>=2 但缺少 NAXIS2 卡片",
            }
    else:
        naxis2 = None

    # 比对 shape (FITS 存储为列优先: NAXIS1=宽度=最后一维, NAXIS2=高度=倒数第二维)
    mismatches = []
    if data.ndim >= 1 and naxis1 != data.shape[-1]:
        mismatches.append(f"NAXIS1={naxis1} != shape[-1]={data.shape[-1]}")
    if naxis2 is not None and data.ndim >= 2 and naxis2 != data.shape[-2]:
        mismatches.append(f"NAXIS2={naxis2} != shape[-2]={data.shape[-2]}")

    detail_parts.append(f"data.shape={data.shape}")

    if not mismatches:
        logger.debug(f"NAXIS shape 匹配 OK ({', '.join(detail_parts)})")
        return {
            "name": "naxis_shape",
            "pass": True,
            "detail": ", ".join(detail_parts),
        }

    msg = "; ".join(mismatches) + f" ({', '.join(detail_parts)})"
    logger.warning(msg)
    return {
        "name": "naxis_shape",
        "pass": False,
        "detail": msg,
    }


def _check_byteorder(data):
    """检查5: 像素数据为大端字节序 (data.dtype.byteorder == '>')"""
    if data is None:
        return {
            "name": "byteorder",
            "pass": False,
            "detail": "data 为 None, 无法检查字节序",
        }

    byteorder = data.dtype.byteorder
    if byteorder == ">":
        logger.debug("字节序: > (大端) OK")
        return {
            "name": "byteorder",
            "pass": True,
            "detail": "data.dtype.byteorder='>' (大端, FITS标准)",
        }

    # '=' 表示 native 字节序, 在小端机器上等同 '<'
    if byteorder == "=":
        native = "<" if sys.byteorder == "little" else ">"
        msg = f"data.dtype.byteorder='=' (native={native}), 期望 '>'"
    else:
        msg = f"data.dtype.byteorder='{byteorder}', 期望 '>'"

    logger.warning(msg)
    return {
        "name": "byteorder",
        "pass": False,
        "detail": msg,
    }


def _check_extend_card(header):
    """检查6: EXTEND 卡片存在且为 T"""
    try:
        extend_val = header["EXTEND"]
    except KeyError:
        return {
            "name": "extend_card",
            "pass": False,
            "detail": "EXTEND 卡片不存在",
        }

    is_bool = isinstance(extend_val, (bool, np.bool_))
    is_true = bool(extend_val) if is_bool else False

    if is_bool and is_true:
        logger.debug("EXTEND: True (bool) OK")
        return {
            "name": "extend_card",
            "pass": True,
            "detail": "EXTEND=True (bool)",
        }

    type_name = type(extend_val).__name__
    msg = f"EXTEND={extend_val!r} (type={type_name}, 非逻辑值)"
    logger.warning(msg)
    return {
        "name": "extend_card",
        "pass": False,
        "detail": msg,
    }


def _check_no_illegal_chars(header):
    """检查7: header 中没有非法字符 (控制字符 ASCII < 32 或 > 126)"""
    illegal_chars = []
    n_cards = len(header)
    for i in range(n_cards):
        card = header.cards[i]
        card_str = str(card)
        for j, ch in enumerate(card_str):
            code = ord(ch)
            if code < 32 or code > 126:
                illegal_chars.append((i, j, code))

    if not illegal_chars:
        logger.debug(f"无非法字符 OK ({n_cards} 张卡片)")
        return {
            "name": "no_illegal_chars",
            "pass": True,
            "detail": f"检查 {n_cards} 张卡片, 无控制字符",
        }

    first = illegal_chars[0]
    detail = (
        f"发现 {len(illegal_chars)} 个非法字符, "
        f"首个: 卡片{first[0]} 位置{first[1]} ASCII={first[2]}"
    )
    logger.warning(detail)
    return {
        "name": "no_illegal_chars",
        "pass": False,
        "detail": detail,
    }


def _check_keyword_length(header):
    """检查8: 每个卡片的关键字长度 <= 8 字符"""
    long_keywords = []
    n_cards = len(header)
    for i in range(n_cards):
        card = header.cards[i]
        keyword = card.keyword
        if len(keyword) > 8:
            long_keywords.append((i, keyword, len(keyword)))

    if not long_keywords:
        logger.debug(f"关键字长度 OK ({n_cards} 张卡片)")
        return {
            "name": "keyword_length",
            "pass": True,
            "detail": f"检查 {n_cards} 张卡片, 关键字长度均 <= 8",
        }

    first = long_keywords[0]
    detail = (
        f"发现 {len(long_keywords)} 个超长关键字, "
        f"首个: '{first[1]}' (len={first[2]})"
    )
    logger.warning(detail)
    return {
        "name": "keyword_length",
        "pass": False,
        "detail": detail,
    }


# ============================================================================
# 核心验证逻辑
# ============================================================================

def validate_fits(fits_path):
    """验证单个 FITS 文件格式.

    Parameters
    ----------
    fits_path : str
        FITS 文件路径.

    Returns
    -------
    dict
        验证结果, 包含 fits_file / overall_pass / checks / warnings / errors.
    """
    fits_path = os.path.abspath(fits_path)
    logger.info(f"开始验证: {fits_path}")

    checks = []
    warnings_list = []
    errors_list = []

    # 前置检查: 文件存在
    check = _check_file_exists(fits_path)
    checks.append(check)
    if not check["pass"]:
        errors_list.append(check["detail"])
        logger.error(check["detail"])
        return _build_result(fits_path, checks, warnings_list, errors_list)

    # 检查1: astropy 打开不报错 (同时捕获 astropy 警告)
    with warnings.catch_warnings(record=True) as caught_warnings:
        warnings.simplefilter("always", AstropyWarning)
        open_check, n_hdus = _check_open_success(fits_path)
    checks.append(open_check)

    # 收集 astropy 警告
    for w in caught_warnings:
        wmsg = f"{w.category.__name__}: {w.message}"
        warnings_list.append(wmsg)
        logger.debug(f"astropy 警告: {wmsg}")

    if not open_check["pass"]:
        errors_list.append(open_check["detail"])
        logger.error(f"打开失败, 跳过后续检查: {open_check['detail']}")
        return _build_result(fits_path, checks, warnings_list, errors_list)

    # 打开 FITS 进行后续检查 (2-8)
    try:
        with warnings.catch_warnings(record=True) as caught_warnings:
            warnings.simplefilter("always", AstropyWarning)
            with fits.open(fits_path) as hdul:
                hdu = hdul[0]
                header = hdu.header
                data = hdu.data

                # 检查2: SIMPLE 卡片
                checks.append(_check_simple_card(header))

                # 检查3: BITPIX
                checks.append(_check_bitpix(header))

                # 检查4: NAXIS1/NAXIS2 与 shape 匹配
                checks.append(_check_naxis_shape(header, data))

                # 检查5: 大端字节序
                checks.append(_check_byteorder(data))

                # 检查6: EXTEND 卡片
                checks.append(_check_extend_card(header))

                # 检查7: 无非法字符
                checks.append(_check_no_illegal_chars(header))

                # 检查8: 关键字长度 <= 8
                checks.append(_check_keyword_length(header))

        # 收集后续 astropy 警告
        for w in caught_warnings:
            wmsg = f"{w.category.__name__}: {w.message}"
            if wmsg not in warnings_list:
                warnings_list.append(wmsg)
                logger.debug(f"astropy 警告: {wmsg}")

    except Exception as e:
        msg = f"验证过程异常: {type(e).__name__}: {e}"
        logger.error(msg, exc_info=True)
        errors_list.append(msg)
        return _build_result(fits_path, checks, warnings_list, errors_list)

    # 汇总错误
    for c in checks:
        if not c["pass"]:
            errors_list.append(f"{c['name']}: {c['detail']}")

    overall_pass = all(c["pass"] for c in checks)
    status = "PASS" if overall_pass else "FAIL"
    logger.info(f"验证完成: {fits_path} -> {status}")

    return _build_result(fits_path, checks, warnings_list, errors_list)


def validate_directory(dir_path):
    """批量验证目录下所有 FITS 文件.

    Parameters
    ----------
    dir_path : str
        目录路径.

    Returns
    -------
    list of dict
        每个文件的验证结果列表.
    """
    dir_path = os.path.abspath(dir_path)
    logger.info(f"批量验证目录: {dir_path}")

    if not os.path.isdir(dir_path):
        logger.error(f"目录不存在: {dir_path}")
        return []

    fits_files = []
    for name in sorted(os.listdir(dir_path)):
        lower = name.lower()
        if lower.endswith(".fits") or lower.endswith(".fit"):
            fits_files.append(os.path.join(dir_path, name))

    logger.info(f"找到 {len(fits_files)} 个 FITS 文件")

    results = []
    for fp in fits_files:
        result = validate_fits(fp)
        results.append(result)

    n_pass = sum(1 for r in results if r["overall_pass"])
    n_fail = len(results) - n_pass
    logger.info(
        f"批量验证完成: {n_pass} PASS, {n_fail} FAIL, 共 {len(results)} 个文件"
    )

    return results


# ============================================================================
# 输出工具
# ============================================================================

def _build_result(fits_path, checks, warnings_list, errors_list):
    """构建单文件验证结果字典."""
    overall_pass = all(c["pass"] for c in checks) if checks else False
    return {
        "fits_file": fits_path,
        "overall_pass": overall_pass,
        "checks": checks,
        "warnings": warnings_list,
        "errors": errors_list,
    }


def write_json(result, output_path):
    """以 UTF-8 编码写入 JSON."""
    out_dir = os.path.dirname(os.path.abspath(output_path))
    os.makedirs(out_dir, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=4)
    logger.info(f"JSON 输出: {output_path}")


def _print_summary(result):
    """打印单文件验证摘要到 stdout."""
    status = "PASS" if result["overall_pass"] else "FAIL"
    n_pass = sum(1 for c in result["checks"] if c["pass"])
    n_total = len(result["checks"])
    print(f"验证结果: {status} ({n_pass}/{n_total} 项通过)")
    if result["warnings"]:
        print(f"警告: {len(result['warnings'])} 个")
        for w in result["warnings"]:
            print(f"  [WARN] {w}")
    if result["errors"]:
        print(f"错误: {len(result['errors'])} 个")
        for e in result["errors"]:
            print(f"  [ERROR] {e}")


# ============================================================================
# 命令行入口
# ============================================================================

def main(argv=None):
    parser = argparse.ArgumentParser(
        description="FITS 格式验证脚本 - 用 astropy 验证导出 FITS 的格式正确性",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
示例:
  python validate_fits_format.py output/pipeline_debug/frame1/0_read_fits.fits
  python validate_fits_format.py 1_calibrate.fits --output result.json
  python validate_fits_format.py --dir output/pipeline_debug/frame1/
""",
    )
    parser.add_argument(
        "fits_file", nargs="?", help="待验证的 FITS 文件路径"
    )
    parser.add_argument(
        "--dir", dest="directory",
        help="批量验证目录下所有 FITS 文件",
    )
    parser.add_argument(
        "--output", dest="output",
        help="JSON 输出路径 (不指定则输出到 FITS 同目录的 validate_fits.json)",
    )
    args = parser.parse_args(argv)

    if not args.fits_file and not args.directory:
        parser.error("必须指定 <fits_file> 或 --dir <directory>")

    if args.fits_file and args.directory:
        parser.error("不能同时指定 <fits_file> 和 --dir")

    # ---- 批量模式 ----
    if args.directory:
        results = validate_directory(args.directory)
        if not results:
            print("未找到任何 FITS 文件", file=sys.stderr)
            return 1

        n_pass = sum(1 for r in results if r["overall_pass"])
        n_fail = len(results) - n_pass

        if args.output:
            output_path = args.output
        else:
            output_path = os.path.join(
                os.path.abspath(args.directory), "validate_fits.json"
            )

        batch_result = {
            "directory": os.path.abspath(args.directory),
            "total_files": len(results),
            "n_pass": n_pass,
            "n_fail": n_fail,
            "overall_pass": n_fail == 0,
            "files": results,
        }
        write_json(batch_result, output_path)

        print(f"批量验证: {n_pass}/{len(results)} PASS, {n_fail} FAIL")
        for r in results:
            status = "PASS" if r["overall_pass"] else "FAIL"
            print(f"  [{status}] {os.path.basename(r['fits_file'])}")
        print(f"JSON 输出: {output_path}")

        return 0 if n_fail == 0 else 1

    # ---- 单文件模式 ----
    fits_path = os.path.abspath(args.fits_file)
    result = validate_fits(fits_path)

    if args.output:
        output_path = args.output
    else:
        output_path = os.path.join(
            os.path.dirname(fits_path), "validate_fits.json"
        )

    write_json(result, output_path)
    _print_summary(result)
    print(f"JSON 输出: {output_path}")

    return 0 if result["overall_pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
