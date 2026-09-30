#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""裁决 ④「两序区分用例」runner —— 机制层"检测面更强"的可执行证据。

被验证的命题（docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md §4.1）:
  机制层 atomic_publish 的序 = 写 tmp -> flush/close/fsync -> **rename** ->
  **重开已发布对象验证** -> 失败撤销。它对"rename 成功之后目标才被改坏"这一情形
  有观测点，故必须**检出并撤销**；而 §4 的生产者序（校验先于 rename）在该窗口没有
  观测点，无法检出 —— 这就是"两序不等价、机制层检测面更强"。

判据（fail-closed）:
  A 干净基线: status == 0(kOk) 且 renamed==1 且 target_exists==1 且 bytes==期望长度
    （先证明探针/验证函数本身不恒真——否则 B 的判红没有意义）；
  B 注入基线: LD_PRELOAD=注入器 + ACSD_TEST_CORRUPT_AFTER_RENAME=1
    -> stderr 必须出现 EVENT CORRUPT-AFTER-RENAME（**注入确实发生**，否则 exit 2：
       注入器未生效时"没判红"不构成任何证据）
    -> 且 status != 0（检出）且 renamed==0 且 target_exists==0（已撤销，无可见半成品）。
用法:
  python3 check_verify_order.py <probe_exe> <interposer_so> <workdir>
exit 0 = 两序区分力成立；exit 1 = 判红失败（零鉴别力）；exit 2 = 输入/注入不可用。
"""
from __future__ import annotations

import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

EXPECTED_BYTES = len(b"verify-order-payload-v1")

# LD_PRELOAD 以空格/冒号分词，而本仓库工作目录含空格（".../Astro CS Database"）
# ⇒ 必须先注入器复制到**无空格**的临时目录再 preload；否则 ld.so 会把路径当多个
# 对象名逐个丢弃（实测："object '/workspace/Astro' ... ignored"），B 态会**静默空转**。
# 与 eng/tests/unit/aio/check_atomic_durability.py 的处置一致。
PRELOAD_SAFE_DIR = None


def safe_preload_path(interposer: str) -> str:
    global PRELOAD_SAFE_DIR
    if PRELOAD_SAFE_DIR is None:
        PRELOAD_SAFE_DIR = os.path.join(
            tempfile.gettempdir(), "aio_corrupt_after_rename_interposer.so")
    shutil.copy2(interposer, PRELOAD_SAFE_DIR)
    return PRELOAD_SAFE_DIR


def parse(out: str) -> dict:
    m = re.search(r"^RESULT (.*)$", out, re.M)
    if not m:
        raise SystemExit("[fail-closed] 探针未输出 RESULT 行:\n" + out[-800:])
    kv = {}
    for tok in m.group(1).split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            kv[k] = v
    return kv


def run(probe: str, workdir: str, env_extra: dict):
    shutil.rmtree(workdir, ignore_errors=True)
    pathlib.Path(workdir).mkdir(parents=True, exist_ok=True)  # 探针只 mkdir 末级
    env = dict(os.environ)
    env.update(env_extra)
    if not env_extra.get("LD_PRELOAD"):
        env.pop("LD_PRELOAD", None)   # A 态必须干净，不继承父环境注入
    p = subprocess.run([probe, workdir], capture_output=True, text=True, env=env,
                       timeout=300)
    return p


def main(argv) -> int:
    if len(argv) < 4:
        print(__doc__)
        return 2
    probe, interposer, workdir = argv[1], argv[2], argv[3]
    for p in (probe, interposer):
        if not pathlib.Path(p).is_file():
            print("[fail-closed] 缺输入: %s" % p)
            return 2
    fails = []

    # A 干净基线
    pa = run(probe, workdir + "/clean", {})
    ka = parse(pa.stdout)
    if (ka.get("status") != "0" or ka.get("renamed") != "1"
            or ka.get("target_exists") != "1" or int(ka.get("bytes", "0")) != EXPECTED_BYTES):
        fails.append("A 干净基线不成立（验证函数可能恒真 ⇒ B 无意义）: %s" % ka)
    else:
        print("[ok] A 干净基线: status=0 renamed=1 target_exists=1 bytes=%d"
              % EXPECTED_BYTES)

    # B 注入基线（rename 之后改坏目标）
    pb = run(probe, workdir + "/corrupt",
             {"LD_PRELOAD": safe_preload_path(interposer),
              "ACSD_TEST_CORRUPT_AFTER_RENAME": "1"})
    kb = parse(pb.stdout)
    if "EVENT CORRUPT-AFTER-RENAME" not in pb.stderr:
        print("[fail-closed] 注入未发生（stderr 无 EVENT CORRUPT-AFTER-RENAME）——"
              "该情形下任何结论都不成立")
        print(pb.stderr[-600:])
        return 2
    print("[ok] B 注入发生: EVENT CORRUPT-AFTER-RENAME 已记录（目标被追加坏字节）")
    if kb.get("status") == "0":
        fails.append("B 机制层未检出 rename 之后的损坏（status=0）—— 与 §4.1 命题矛盾")
    if kb.get("renamed") != "0":
        fails.append("B 检出后未撤销（renamed=%s）—— 不得留可见半成品"
                     % kb.get("renamed"))
    if kb.get("target_exists") != "0":
        fails.append("B 撤销后目标仍存在（target_exists=%s）" % kb.get("target_exists"))
    if kb.get("target_exists") == "0" and kb.get("renamed") == "0" and kb.get("status") != "0":
        print("[ok] B 检出并撤销: status=%s renamed=%s target_exists=%s"
              % (kb.get("status"), kb.get("renamed"), kb.get("target_exists")))
        print("      => 机制层（rename 后验证）检出该故障；生产者序（校验先于 rename）"
              "在该窗口无观测点 ⇒ 两序不等价、机制层检测面更强（§4.1）")

    if fails:
        for f in fails:
            print("[FAIL] " + f, file=sys.stderr)
        print("VERIFY_ORDER_FAIL")
        return 1
    print("VERIFY_ORDER_PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
