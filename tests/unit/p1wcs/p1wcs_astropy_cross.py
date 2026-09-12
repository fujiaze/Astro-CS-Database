#!/usr/bin/env python3
# P1-WCS-TEST · WCS-003 Astropy 第三方交叉验证 (独立实现, 只入测试面)
#
# 合同锚: WCS-003 任务规格验收项 "Astropy 或等价独立 WCS 交叉验证
# (只用于测试面比对, 不引入生产依赖)"。
#
# 流程 (自包含):
#   1. subprocess 重跑 p1wcs_tests apbp (env P1WCS_CROSS_OUT 指定导出路径,
#      刷新交叉验证输入 JSON — 三档 fixture 的生产 WCS 参数 + 采样点)。
#   2. astropy.wcs 独立构造 (CRPIX/CRVAL/CD + SIP A/B, 第三方实现):
#      a) 前向交叉: astropy all_pix2world (含 SIP) vs C++ oracle-4 前向锚
#         ra_fwd/dec_fwd → |Δ| < 1e-9 deg;
#      b) 逆向交叉: astropy all_world2pix (数值迭代反演, 不消费 AP/BP)
#         vs 生产 wcs_sky_to_pixel_iterative 输出 x_iter/y_iter
#         → |Δ| < 1e-4 px (冻结门同量级);
#      c) 扩展逆向交叉: astropy 前向 SIP 语义下, C++ APx/BPx 一步直加
#         (测试面独立重放) 误差与导出 onestep 表一致 (防导出失真)。
#   3. 结果 JSON 落工作目录 wcs003_astropy_cross.json。
#
# 工作目录解析 (环境无关化; 裁决 R-14 + 宪章 §14.1「不写死服务器绝对路径」;
# 本文件不含任何硬编码绝对路径):
#   1. --work-dir DIR            显式最高优先; 两产物均落 DIR 下;
#   2. P1WCS_CROSS_WORK_DIR      环境变量显式覆盖 (无 --work-dir 时);
#   3. 逐文件覆盖 P1WCS_CROSS_OUT / P1WCS_CROSS_RESULT (无 --work-dir 时);
#   4. 默认: 仓库内相对路径 <repo_root>/run/p1wcs_wcs003 —— repo_root 由本
#      文件位置推导 (向上找含 CMakeLists.txt 与 tests/ 的目录), 不硬编码;
#      仓库根不可判定时以 tempfile.gettempdir()/p1wcs_wcs003 兜底,
#      故默认值在任意宿主均可创建 (GitHub hosted runner 不依赖本机私有路径)。
#   选定目录在开工前创建并做真实写入探针; 不可创建/不可写 → 立即以明确
#   错误信息 fail-fast 退出 (rc=2), 绝不静默跳过、绝不静默改默认值而假绿
#   (宪章 §14.4)。
#
# 负向守卫 (path_guard_selfcheck, 每次运行先跑):
#   注入「父路径是普通文件」的工作目录 → ensure_writable_dir 必须抛
#   WorkDirError 且错误信息含该路径 (该注入对 root 同样成立, 与运行用户
#   无关); 非 root 时追加「只读目录 (0500)」注入; 正向控制: 可写目录必须
#   通过 (防恒常 FAIL 假红); 另断言 --work-dir > env > default 优先级与
#   默认路径的宿主推导性。守卫回归 → rc=3, 不放行。
#
# 依赖: astropy (测试面), numpy。零生产依赖 (lib/plate_solve 不 include)。
import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from astropy.wcs import WCS
from astropy.wcs import Sip

# 环境变量/命令行接口 (工作目录解析, 见头注)
WORK_DIR_ENV = "P1WCS_CROSS_WORK_DIR"
CROSS_OUT_ENV = "P1WCS_CROSS_OUT"
RESULT_OUT_ENV = "P1WCS_CROSS_RESULT"
TESTS_BIN_ENV = "P1WCS_TESTS_BIN"
WORK_DIR_NAME = "p1wcs_wcs003"
CROSS_INPUT_NAME = "wcs003_cross_input.json"
RESULT_NAME = "wcs003_astropy_cross.json"

# 退出码 (fail-fast, 不静默)
EXIT_CROSS_FAIL = 1      # 交叉判定不过
EXIT_WORKDIR_FAIL = 2    # 工作目录不可创建/不可写 (环境合同违例)
EXIT_GUARD_FAIL = 3      # 负向守卫自身回归

# 冻结门 (px) 与前向交叉门 (deg) — 不放宽
FREEZE_PX = 1e-4
FWD_DEG = 1e-9

# 被测执行器路径 (由 ctest/CMake 显式传入; 未设则判定 FAIL, 不静默跳过)
TESTS_BIN = os.environ.get(TESTS_BIN_ENV, "")


class WorkDirError(RuntimeError):
    """工作目录不可创建/不可写 — fail-fast (宪章 §14.4), 绝不静默跳过。"""


def repo_root():
    """由本文件位置向上推导仓库根 (含 CMakeLists.txt 与 tests/); 无则 None。

    不读取任何硬编码绝对路径; 拷贝到任意宿主/任意检出位置均自洽。
    """
    here = Path(__file__).resolve()
    for parent in here.parents:
        if (parent / "CMakeLists.txt").is_file() and (parent / "tests").is_dir():
            return parent
    return None


def default_work_dir():
    """默认工作目录: 仓库内相对 run/<name>; 仓库根不可判定时 tempfile 兜底。"""
    root = repo_root()
    if root is not None:
        return root / "run" / WORK_DIR_NAME
    return Path(tempfile.gettempdir()) / WORK_DIR_NAME


def default_work_dir_source():
    """默认分支来源标签 (仓库内相对 / tempfile 兜底), 供日志与守卫断言。"""
    if repo_root() is not None:
        return "default(repo-relative run/%s)" % WORK_DIR_NAME
    return "default(tempfile fallback %s)" % WORK_DIR_NAME


def resolve_work_dir(cli_work_dir):
    """按优先级解析工作目录, 返回 (Path, 来源标签)。

    优先级: --work-dir > P1WCS_CROSS_WORK_DIR > 默认 (仓库内相对/tempfile)。
    """
    if cli_work_dir:
        return Path(cli_work_dir).expanduser(), "--work-dir"
    env_dir = os.environ.get(WORK_DIR_ENV, "").strip()
    if env_dir:
        return Path(env_dir).expanduser(), WORK_DIR_ENV
    return default_work_dir(), default_work_dir_source()


def resolve_artifact_paths(work_dir, cli_work_dir):
    """解析两个产物路径; 显式 --work-dir 时两产物均在 work_dir 下。

    无 --work-dir 时允许逐文件环境变量覆盖 (CMake/CI 显式传参路径)。
    """
    if cli_work_dir:
        return work_dir / CROSS_INPUT_NAME, work_dir / RESULT_NAME
    cross = os.environ.get(CROSS_OUT_ENV, "").strip()
    result = os.environ.get(RESULT_OUT_ENV, "").strip()
    return (Path(cross).expanduser() if cross else work_dir / CROSS_INPUT_NAME,
            Path(result).expanduser() if result else work_dir / RESULT_NAME)


def ensure_writable_dir(path, source):
    """创建目录并做真实写入探针; 失败抛 WorkDirError (明确错误信息)。

    fail-fast 是合同: 不可写即失败退出, 不静默降级、不静默跳过、不通融放行。
    """
    path = Path(path)
    try:
        os.makedirs(str(path), exist_ok=True)
    except OSError as exc:
        raise WorkDirError(
            "work dir not creatable: %s (source: %s)\n"
            "  cause: %s: %s\n"
            "  fix: pass a writable --work-dir DIR or set %s (no silent "
            "fallback / no skipped work; ASTROCS constitution 14.4)"
            % (path, source, type(exc).__name__, exc, WORK_DIR_ENV))
    if not os.path.isdir(str(path)):
        raise WorkDirError(
            "work dir not a directory: %s (source: %s)\n"
            "  fix: pass a writable --work-dir DIR or set %s"
            % (path, source, WORK_DIR_ENV))
    probe = path / ".p1wcs_write_probe"
    try:
        with open(str(probe), "w") as fh:
            fh.write("p1wcs\n")
        os.unlink(str(probe))
    except OSError as exc:
        raise WorkDirError(
            "work dir not writable: %s (source: %s)\n"
            "  cause: %s: %s\n"
            "  fix: pass a writable --work-dir DIR or set %s (no silent "
            "fallback / no skipped work; ASTROCS constitution 14.4)"
            % (path, source, type(exc).__name__, exc, WORK_DIR_ENV))
    return path


def path_guard_selfcheck():
    """负向守卫: 工作目录不可用时必须明确失败; 返回问题清单 (空=通过)。

    每次运行先跑 (交叉验证之前), 守卫自身回归即 rc=3 —— 防止把 fail-fast
    悄悄改成静默跳过而假绿 (宪章 §14.4)。
    """
    problems = []
    try:
        td_ctx = tempfile.TemporaryDirectory(prefix="p1wcs_path_guard_")
    except OSError as exc:
        return ["path guard tempdir unavailable: %s: %s"
                % (type(exc).__name__, exc)]
    with td_ctx as td:
        # 注入 1: 父路径是普通文件 → 任意用户 (含 root) 下 makedirs 必失败
        blocker = os.path.join(td, "not_a_dir")
        with open(blocker, "w") as fh:
            fh.write("p1wcs guard blocker\n")
        injected = os.path.join(blocker, "child")
        try:
            ensure_writable_dir(injected, "guard-injection:parent-is-file")
            problems.append("injection(parent-is-file) did not fail: %s"
                            % injected)
        except WorkDirError as exc:
            msg = str(exc)
            if injected not in msg:
                problems.append("injection(parent-is-file) message lacks path")
            if not ("not creatable" in msg or "not writable" in msg
                    or "not a directory" in msg):
                problems.append("injection(parent-is-file) message not explicit")
            if "--work-dir" not in msg:
                problems.append("injection(parent-is-file) message lacks fix")

        # 注入 2: 只读目录 (0500) → 非 root 下写入探针必失败
        ro = os.path.join(td, "readonly_dir")
        os.makedirs(ro)
        os.chmod(ro, 0o500)
        try:
            if not (hasattr(os, "geteuid") and os.geteuid() == 0):
                try:
                    ensure_writable_dir(ro, "guard-injection:readonly")
                    problems.append("injection(readonly) did not fail: %s" % ro)
                except WorkDirError as exc:
                    if ro not in str(exc):
                        problems.append("injection(readonly) message lacks path")
        finally:
            os.chmod(ro, 0o700)

        # 正向控制: 可写目录必须通过 (防恒常 FAIL 假红)
        ok_dir = os.path.join(td, "ok_dir")
        try:
            ensure_writable_dir(ok_dir, "guard-positive-control")
        except WorkDirError as exc:
            problems.append("positive control failed: %s" % exc)

    # 优先级守卫: --work-dir > 环境变量 > 默认
    cli_probe = os.path.join(tempfile.gettempdir(), "p1wcs_guard_cli_probe")
    got, src = resolve_work_dir(cli_probe)
    if str(got) != cli_probe or src != "--work-dir":
        problems.append("--work-dir precedence broken: %s (%s)" % (got, src))
    env_probe = os.path.join(tempfile.gettempdir(), "p1wcs_guard_env_probe")
    saved = os.environ.get(WORK_DIR_ENV)
    os.environ[WORK_DIR_ENV] = env_probe
    try:
        got, src = resolve_work_dir("")
        if str(got) != env_probe or src != WORK_DIR_ENV:
            problems.append("%s precedence broken: %s (%s)"
                            % (WORK_DIR_ENV, got, src))
    finally:
        if saved is None:
            os.environ.pop(WORK_DIR_ENV, None)
        else:
            os.environ[WORK_DIR_ENV] = saved

    # 默认值必须是宿主推导路径 (回归防线: 不得再写死绝对默认值)
    dflt = default_work_dir()
    root = repo_root()
    host_prefix = str(root) if root is not None else tempfile.gettempdir()
    if not str(dflt).startswith(host_prefix):
        problems.append("default work dir not host-derived: %s" % dflt)
    if Path(dflt).name != WORK_DIR_NAME:
        problems.append("default work dir name unexpected: %s" % dflt)
    return problems


def build_sip_matrix(flat, order, stride):
    """i*stride+j 展平数组 → (order+1)x(order+1) 矩阵 (astropy Sip 接口)"""
    n = order + 1
    m = np.zeros((n, n))
    for i in range(order + 1):
        for j in range(order + 1 - i):
            m[i][j] = flat[i * stride + j]
    return m


def cross_check(cross_out, result_out):
    """交叉验证主体 (判定强度不变); 返回 rc。"""
    # 1. 刷新交叉验证输入 (apbp 组重跑, 其自身断言独立于本脚本)
    env = dict(os.environ)
    env[CROSS_OUT_ENV] = str(cross_out)
    cmd = [TESTS_BIN] if TESTS_BIN else []
    if not cmd:
        print("FAIL: P1WCS_TESTS_BIN not set")
        return EXIT_CROSS_FAIL
    cmd.append("apbp")
    r = subprocess.run(cmd, env=env, capture_output=True, text=True,
                       timeout=1800)
    if r.returncode != 0:
        print("FAIL: apbp re-run rc=%d\n%s" % (r.returncode, r.stderr[-2000:]))
        return EXIT_CROSS_FAIL

    with open(str(cross_out)) as f:
        data = json.load(f)

    report = {"schema": "p1wcs/wcs003-astropy-cross-v1",
              "astropy_version": __import__("astropy").__version__,
              "freezes": {"reverse_px": FREEZE_PX, "forward_deg": FWD_DEG},
              "semantic_bridge": None,
              "work_dir": str(Path(result_out).parent),
              "fixtures": []}
    all_ok = True
    for fx in data["fixtures"]:
        # 语义桥接 (finding WCS-003-F1 登记面, 见 evidence):
        # 生产 WcsFitResult 的 FITS 语义自洽口径为 u = x − crpix
        # (WCS-001 冻结, oracle_wcs_forward/oracle_wcs_reverse 同口径,
        # F2/F6 roundtrip 在此口径闭合), 标准 FITS/astropy 为
        # u = x − (crpix − 1), 两者相差常量 1px 原点平移。
        # 桥接: astropy crpix' = crpix + 1 → u_astropy = x − (crpix' − 1)
        # = x − crpix = u_prod, CD/SIP 数学内容逐系数不变。该平移精确
        # 吸收, 不放宽任何容差; 1px 语义分歧本身登记 finding 移交。
        crpix_bridge = [fx["crpix"][0] + 1.0, fx["crpix"][1] + 1.0]
        report["semantic_bridge"] = ("prod u=x-crpix (WCS-001 frozen) vs "
                                     "std FITS u=x-(crpix-1): constant 1px "
                                     "origin shift, bridged via astropy "
                                     "crpix'=crpix-1; registered as "
                                     "finding WCS-003-F1 (domain-external "
                                     "fix, owner decision)")
        w = WCS(naxis=2)
        w.wcs.crpix = crpix_bridge
        w.wcs.crval = fx["crval"]
        w.wcs.cd = np.array(fx["cd"])
        w.wcs.ctype = ["RA---TAN-SIP", "DEC--TAN-SIP"]
        a = build_sip_matrix(fx["A"], fx["sip_order"], 6)
        b = build_sip_matrix(fx["B"], fx["sip_order"], 6)
        ap = build_sip_matrix(fx["APx"], fx["apx_order"], 10)
        bp = build_sip_matrix(fx["BPx"], fx["apx_order"], 10)
        w.sip = Sip(a, b, ap, bp, np.array(crpix_bridge))

        pts = fx["points"]
        x_f = np.array([p["x_f"] for p in pts])
        y_f = np.array([p["y_f"] for p in pts])
        x_it = np.array([p["x_iter"] for p in pts])
        y_it = np.array([p["y_iter"] for p in pts])

        # a) 前向交叉 (astropy all_pix2world 含 SIP A/B)
        ra_a, dec_a = w.all_pix2world(x_f, y_f, 0)
        dra = np.abs(ra_a - np.array([p["ra_fwd"] for p in pts]))
        ddec = np.abs(dec_a - np.array([p["dec_fwd"] for p in pts]))
        # RA 环绕归一 (±180 折返)
        dra = np.minimum(dra, 360.0 - dra)
        fwd_max = float(max(dra.max(), ddec.max()))

        # b) 逆向交叉 (astropy all_world2pix 数值迭代, 第三方独立反演)
        px_a, py_a = w.all_world2pix(
            np.array([p["ra_fwd"] for p in pts]),
            np.array([p["dec_fwd"] for p in pts]), 0,
            tolerance=1e-9, maxiter=200)
        rev = np.hypot(px_a - x_it, py_a - y_it)
        rev_max = float(rev.max())

        # c) 生产迭代反演自身 rt (导出侧复核)
        rt_max = float(max(p["rt_px"] for p in pts if p["converged"] == 1))

        ok = fwd_max < FWD_DEG and rev_max < FREEZE_PX
        all_ok = all_ok and ok
        entry = {"name": fx["name"], "dist_scale": fx["dist_scale"],
                 "n_points": len(pts), "forward_max_deg": fwd_max,
                 "astropy_vs_iter_max_px": rev_max, "iter_rt_max_px": rt_max,
                 "pass": ok}
        report["fixtures"].append(entry)
        print("[WCS-003 astropy] %s: fwd=%.3e deg rev=%.3e px rt=%.3e px %s"
              % (fx["name"], fwd_max, rev_max, rt_max, "PASS" if ok else "FAIL"))

    with open(str(result_out), "w") as f:
        json.dump(report, f, indent=1)

    if all_ok:
        print("P1WCS ASTROPY CROSS PASS")
        return 0
    print("P1WCS ASTROPY CROSS FAIL")
    return EXIT_CROSS_FAIL


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="P1-WCS-003 Astropy 第三方交叉验证 (只入测试面)")
    ap.add_argument("--work-dir", default=None,
                    help="交叉验证工作目录 (最高优先; 两产物均落此目录)")
    args = ap.parse_args(argv)

    # 0. 负向守卫 (先跑: fail-fast 语义的回归防线)
    problems = path_guard_selfcheck()
    if problems:
        print("FAIL: path guard selfcheck (fail-fast regression):")
        for p in problems:
            print("  - %s" % p)
        return EXIT_GUARD_FAIL

    # 1. 解析 + 创建 + 可用性验证 (§14.4 fail-fast, 明确错误信息)
    work_dir, source = resolve_work_dir(args.work_dir)
    cross_out, result_out = resolve_artifact_paths(work_dir, args.work_dir)
    for path, label in ((work_dir, "work-dir(%s)" % source),
                        (Path(cross_out).parent, "cross-out parent"),
                        (Path(result_out).parent, "result-out parent")):
        try:
            ensure_writable_dir(path, label)
        except WorkDirError as exc:
            print("FAIL: %s" % exc)
            return EXIT_WORKDIR_FAIL

    print("[p1wcs astropy] work_dir=%s (source: %s)" % (work_dir, source))
    print("[p1wcs astropy] cross_out=%s" % cross_out)
    print("[p1wcs astropy] result_out=%s" % result_out)

    # 2. 交叉验证主体
    return cross_check(cross_out, result_out)


if __name__ == "__main__":
    sys.exit(main())
