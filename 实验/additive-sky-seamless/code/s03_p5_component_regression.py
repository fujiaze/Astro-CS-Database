#!/usr/bin/env python3
# 实验/additive-sky-seamless/code/s03_p5_component_regression.py
#
# S03（P5 断连重叠图逐分量 gauge）在**本实验单元**里的可复跑入口。
#
# 单一实现原则（AGENTS §13「复用」）：测量协议只保留一份实现，落在执行包
# `run/ACSDSDOC-02/S03/code/s03_p5_components.py`（合成世界与生产探针调用口径
# 逐字复用 `run/ACSDSDOC-01/T08/code/t08_common.py`，只读导入）。本文件是它的
# **薄入口**：把结果重定向到本单元的 results 目录，不改测量口径。
#
# 用法（在仓库根执行）：
#   python3 实验/additive-sky-seamless/code/s03_p5_component_regression.py \
#       --probe run/ACSDSDOC-02/S03/code/p2_sky_joint_probe_s03_post --tag post
#   # 负例（必须判红）
#   python3 实验/additive-sky-seamless/code/s03_p5_component_regression.py \
#       --probe run/ACSDSDOC-02/S03/code/p2_sky_joint_probe_s03_post \
#       --tag fault_global_anchor --fault global_anchor S9b_two_components
#   # 证据聚合（生成 docs/S03_P5_COMPONENT_evidence.exec.json）
#   python3 实验/additive-sky-seamless/code/s03_p5_component_evidence.py
import argparse
import os
import runpy
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
HARNESS = os.path.join(REPO, "run", "ACSDSDOC-02", "S03", "code", "s03_p5_components.py")


def main():
    if not os.path.exists(HARNESS):
        sys.stderr.write(f"missing harness: {HARNESS}\n")
        return 3
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("--probe", default=os.path.join(
        REPO, "run", "ACSDSDOC-02", "S03", "code", "p2_sky_joint_probe_s03_post"))
    ap.add_argument("--tag", default="post")
    ap.add_argument("--fault", default=None)
    ap.add_argument("scenarios", nargs="*")
    a, _ = ap.parse_known_args()
    sys.argv = [HARNESS, "--probe", a.probe, "--tag", a.tag]
    if a.fault:
        sys.argv += ["--fault", a.fault]
    sys.argv += a.scenarios
    # 单一实现：直接执行执行包的同一份脚本（同一逻辑、不同调用点）。
    runpy.run_path(HARNESS, run_name="__main__")
    return 0


if __name__ == "__main__":
    sys.exit(main())
