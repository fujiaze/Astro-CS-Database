# -*- coding: utf-8 -*-
"""
FSyn Loader - F_syn 结果加载器
功能: 从光谱积分器输出的 JSON 文件加载 F_syn 合成流量结果
用途: gradient_estimator 模块的数据入口，读取 SpectrumIntegrator.save_results 写出的
      JSON 文件，解析为星点列表供梯度拟合器(GradientFitter)使用
依赖: Python 标准库 json, logging
调用: from fsyn_loader import FSynLoader
      stars = FSynLoader.load("f_syn_results.json")
      metadata, stars = FSynLoader.load_with_metadata("f_syn_results.json")

JSON 格式 (由 SpectrumIntegrator.save_results 生成):
  顶层: filter_name, qe_name, wl_step, spectrum_source, n_stars,
        ra_center, dec_center, radius_deg, stars[]
  stars[] 每项: source_id, ra, dec, mag_g, f_syn

处理规则:
  1. 校验 "stars" 字段存在，否则抛出 ValueError
  2. 每颗星提取 ra/dec/mag_g/f_syn/source_id 并做类型转换
  3. 过滤 f_syn <= 0 的无效积分结果
"""

from __future__ import annotations

import json
import logging
import os
from typing import Dict, List, Tuple

logger = logging.getLogger(__name__)


class FSynLoader:
    """F_syn 结果加载器，解析光谱积分器输出的 JSON 文件

    将 SpectrumIntegrator.save_results 写出的 JSON 解析为星点列表，
    每颗星含 ra/dec/mag_g/f_syn/source_id，并过滤 f_syn <= 0 的无效积分结果。
    """

    @staticmethod
    def load(json_path: str) -> List[dict]:
        """从 JSON 文件加载 F_syn 结果

        Args:
            json_path: JSON 文件路径

        Returns:
            list[dict]，每项含 ra(float), dec(float), mag_g(float),
            f_syn(float), source_id(int)

        Raises:
            FileNotFoundError: 文件不存在
            ValueError: JSON 缺少 stars 字段
        """
        _metadata, stars = FSynLoader.load_with_metadata(json_path)
        return stars

    @staticmethod
    def load_with_metadata(json_path: str) -> Tuple[dict, List[dict]]:
        """加载 JSON 并返回元数据和星列表

        Args:
            json_path: JSON 文件路径

        Returns:
            (metadata_dict, stars_list)
            metadata_dict 含 filter_name, qe_name, n_stars, ra_center 等
            stars_list 每项含 ra(float), dec(float), mag_g(float),
            f_syn(float), source_id(int)

        Raises:
            FileNotFoundError: 文件不存在
            ValueError: JSON 缺少 stars 字段
        """
        logger.info("加载 F_syn 结果: %s", json_path)
        with open(json_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        if "stars" not in data:
            raise ValueError("JSON 缺少 stars 字段，格式不符")

        # 元数据: 顶层除 stars 外的全部字段
        metadata: Dict = {k: v for k, v in data.items() if k != "stars"}

        raw_stars = data["stars"]
        n_raw = len(raw_stars)

        stars: List[dict] = []
        n_invalid = 0
        for s in raw_stars:
            f_syn = float(s["f_syn"])
            # 过滤无效积分结果
            if f_syn <= 0.0:
                n_invalid += 1
                continue
            stars.append({
                "ra": float(s["ra"]),
                "dec": float(s["dec"]),
                "mag_g": float(s["mag_g"]),
                "f_syn": f_syn,
                "source_id": int(s["source_id"]),
            })

        filter_name = metadata.get("filter_name", "未知")
        logger.info(
            "F_syn 结果加载完成: 滤光片=%s, 原始星数=%d, 无效(f_syn<=0)=%d, 有效星数=%d",
            filter_name, n_raw, n_invalid, len(stars),
        )
        return metadata, stars


# ======================================================================
# 模块验证 (使用光谱积分器生成的 f_syn_galactic_center.json)
# ======================================================================
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(name)s: %(message)s")

    print("=" * 60)
    print("FSynLoader 模块验证")
    print("=" * 60)

    # gradient_estimator/python -> photometric_calib -> spectrum_integrator/python
    verify_json = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "..", "..", "spectrum_integrator", "python", "f_syn_galactic_center.json",
    )
    verify_json = os.path.normpath(verify_json)
    print("验证文件: %s" % verify_json)

    all_pass = True

    # ---- 验证 1: load() 返回非空列表 ----
    print("\n[验证 1] FSynLoader.load() 返回非空列表")
    stars = FSynLoader.load(verify_json)
    nonempty_ok = len(stars) > 0
    print("  返回星数=%d" % len(stars))
    print("  [%s] 返回非空列表" % ("PASS" if nonempty_ok else "FAIL"))
    all_pass = all_pass and nonempty_ok

    # ---- 验证 2: 每项含 ra, dec, mag_g, f_syn, source_id ----
    print("\n[验证 2] 每项含 ra, dec, mag_g, f_syn, source_id")
    required_fields = ("ra", "dec", "mag_g", "f_syn", "source_id")
    fields_ok = all(all(k in s for k in required_fields) for s in stars)
    type_ok = all(
        isinstance(s["ra"], float)
        and isinstance(s["dec"], float)
        and isinstance(s["mag_g"], float)
        and isinstance(s["f_syn"], float)
        and isinstance(s["source_id"], int)
        for s in stars
    )
    print("  字段完整=%s, 类型正确=%s" % (fields_ok, type_ok))
    print("  示例: ra=%.4f, dec=%.4f, mag_g=%.2f, f_syn=%.6e, source_id=%d"
          % (stars[0]["ra"], stars[0]["dec"], stars[0]["mag_g"],
             stars[0]["f_syn"], stars[0]["source_id"]))
    print("  [%s] 字段与类型正确" % ("PASS" if fields_ok and type_ok else "FAIL"))
    all_pass = all_pass and fields_ok and type_ok

    # ---- 验证 3: 所有 f_syn > 0 ----
    print("\n[验证 3] 所有 f_syn > 0")
    positive_ok = all(s["f_syn"] > 0.0 for s in stars)
    f_vals = [s["f_syn"] for s in stars]
    print("  F_syn 范围=[%.6e, %.6e]" % (min(f_vals), max(f_vals)))
    print("  [%s] 全部 f_syn > 0" % ("PASS" if positive_ok else "FAIL"))
    all_pass = all_pass and positive_ok

    # ---- 验证 4: load_with_metadata() 返回 metadata 含 filter_name="Baader R" ----
    print("\n[验证 4] load_with_metadata() 返回 metadata 含 filter_name='Baader R'")
    metadata, _ = FSynLoader.load_with_metadata(verify_json)
    filter_ok = metadata.get("filter_name") == "Baader R"
    print("  filter_name=%s" % metadata.get("filter_name"))
    print("  metadata 键: %s" % list(metadata.keys()))
    print("  [%s] filter_name='Baader R'" % ("PASS" if filter_ok else "FAIL"))
    all_pass = all_pass and filter_ok

    print("\n" + "=" * 60)
    print("验证结果: %s" % ("全部通过" if all_pass else "存在失败项"))
    print("=" * 60)
