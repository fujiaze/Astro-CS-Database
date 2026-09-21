#!/usr/bin/env python3
"""FIX-406: 三阶段 × 三取消点 SIGTERM/SIGINT 取消矩阵（exit 9 + incomplete manifest + 无半成品）。

权威（逐条）:
  * ASTROCS_DESIGN.md §7.2「取消（Ctrl-C）：协作取消 → 关 writer → 写 incomplete
    manifest → 删/隔离临时产物 → exit 9」+ 退出码表「9 = 用户取消或超时」；
  * ASTROCS_DESIGN.md §10「所有产品走临时区 → 校验 → 原子改名发布 → 最后落完成清单；
    没有完成清单就不算成功对象；失败/取消时清理临时产物」；
  * GAP_AUDIT G3-15「SIGTERM 全阶段 exit 9 路径未覆盖」（本任务闭合）。

取消点定义（三命令同构；钩子均为既有/新增**测试钩子**，生产零影响）:
  ① startup  进程入口窗 ASTROCS_TEST_SLEEP_MS（subcommand.h；读配置之前）
              ⇒ 期望: rc=9 + final(status=cancelled) + **不写任何 manifest**（本次
                 运行尚未建立 output_dir/run_context，禁造假清单）+ 无产物;
  ② compute  Runtime 计算窗 ASTROCS_TEST_PIPELINE_SLEEP_MS（runtime_client.cpp；
              取消经 cancel_watch → rt->cancel() 送达调度器安全点）
              ⇒ 期望: rc=9 + incomplete manifest（原因 = cancelled by user）;
  ③ write    写盘窗 ASTROCS_TEST_WRITE_SLEEP_MS（commands.cpp；产物收集/哈希完成、
              run manifest/运行图落盘之前）
              ⇒ 期望: rc=9 + incomplete manifest + **artifacts 非空**（证明取消点
                 确实落在产物已写、完成清单未落之间；判据非退化）。

三取消点证据可区分（非退化）: startup 无 manifest / compute 有 incomplete manifest /
write 有 incomplete manifest 且 artifacts>0 —— 若钩子失效（例如全部退化成 startup
行为），本矩阵必红。

Windows 等价路径: CTRL_C_EVENT / CTRL_BREAK_EVENT 由 TestWindowsConsoleCtrl 承载
（仅在 Windows 上执行；Python 无法程序化投递 CTRL_CLOSE_EVENT —— 见该类的平台限制
登记）。Linux 侧另有 cancel_token.h 结构化登记门，保证 Windows 处理函数与两平台
安装点不被静默删除。

复跑:
  python3 -m unittest tests.cli.test_fix406_sigterm_cancel -v
  ASTROCS_FIX406_FIXTURES=run/FIX-406/fixtures python3 -m unittest ...  # 复用已建夹具
"""
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, REPO)

from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402
from tests.cli.test_phase123_pipeline import (  # noqa: E402
    DRIZZLE, EXE, WCS_EXPLICIT, _aio_srcs, _cfitsio_objs, _common_incs)

PHASES = ("normalize", "mosaic", "export")
# (取消点名, 钩子环境变量) —— 三个取消点各自的**唯一**注入面
CANCEL_POINTS = (
    ("startup", "ASTROCS_TEST_SLEEP_MS"),
    ("compute", "ASTROCS_TEST_PIPELINE_SLEEP_MS"),
    ("write", "ASTROCS_TEST_WRITE_SLEEP_MS"),
)
SLEEP_MS = "6000"          # 注入窗（信号在 1.2s 到达，窗内必达）
SIGNAL_DELAY = 1.2
EVIDENCE = os.path.join(REPO, "run", "FIX-406", "evidence", "cancel_matrix.json")
# 临时/半成品文件命名（ASTROCS_DESIGN §10 原子发布；aio/p3_output 实测命名族）
TEMP_PAT = re.compile(r"\.tmp|\.partial|\.tmppool|\.part$|\.incomplete$")


def _write_json(path, doc):
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(doc, fh)
    return path


def _run_cmd(args, timeout=900, env=None):
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout,
                          cwd=run_cwd(), env=env)


# ────────────────────────── 夹具（最小真实链路）──────────────────────────
def _build_fixtures(root):
    """编译夹具生成器 + 生成数据 + 跑通 normalize×2 → mosaic → export 输入。

    与 eng/tests/cli/test_phase123_pipeline.py::setUpClass 同源（同一批夹具生成器），
    只额外为 export 输入副本补 BUNIT（见 _annotate_p3_input）。
    """
    os.makedirs(root, exist_ok=True)
    objs = _cfitsio_objs(root)
    incs = _common_incs()
    p1 = os.path.join(root, "p1fx")
    r = _run_cmd(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                  os.path.join(REPO, "tests", "backend", "phase1_fixture_main.cpp"),
                  os.path.join(REPO, "lib", "infrastructure", "aio", "src", "aio_fits.cpp"),
                  os.path.join(REPO, "lib", "infrastructure", "aio", "src", "aio_api.cpp"),
                  os.path.join(REPO, "lib", "infrastructure", "aio", "src", "aio_log.cpp"),
                  os.path.join(REPO, "lib", "infrastructure", "aio", "src", "aio_compressor.cpp"),
                  *objs, "-lz", "-lzstd", "-llz4", "-o", p1])
    assert r.returncode == 0, r.stderr[-800:]
    p2 = os.path.join(root, "p2fx")
    r = _run_cmd(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                  os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp"),
                  *_aio_srcs(), *objs, "-lz", "-lzstd", "-llz4", "-o", p2])
    assert r.returncode == 0, r.stderr[-800:]
    p1data = os.path.join(root, "p1data")
    os.makedirs(p1data, exist_ok=True)
    r = _run_cmd([p1, "--make-noisy", p1data])
    assert "FIXTURES_OK" in r.stdout, r.stderr
    # 逐帧 normalize 持久化产品（mosaic 输入）
    outs = []
    for idx in (1, 2):
        out = os.path.join(root, "p1_%d" % idx)
        os.makedirs(out, exist_ok=True)
        cfg = _write_json(os.path.join(root, "p1_%d.json" % idx), {
            "schema_version": "1",
            "input_lights": [os.path.join(p1data, "light_%d.fits" % idx)],
            "master_bias": os.path.join(p1data, "bias.fits"),
            "master_dark": os.path.join(p1data, "dark.fits"),
            "master_flat": os.path.join(p1data, "flat.fits"),
            "dark_optimization": True, "output_dir": out,
            "wcs": dict(WCS_EXPLICIT), "drizzle": dict(DRIZZLE)})
        r = _run_cmd([EXE, "normalize", "--json", cfg, "--events-jsonl", "-y"])
        assert r.returncode == 0, r.stderr[-600:]
        outs.append(os.path.join(out, "light_%d" % idx))
    p2out = os.path.join(root, "p2out")
    os.makedirs(p2out, exist_ok=True)
    cfg2 = _write_json(os.path.join(root, "p2.json"), {
        "schema_version": "1", "hips_paths": outs, "output_dir": p2out})
    r = _run_cmd([EXE, "mosaic", "--json", cfg2, "--events-jsonl", "-y"])
    assert r.returncode == 0, r.stderr[-600:]
    p3in = _annotate_p3_input(p2out, os.path.join(root, "p3in"))
    return {"p1data": p1data, "p1a": outs[0], "p1b": outs[1], "p2out": p2out,
            "p3in": p3in}


_EXPORT_PROBE = {
    "schema_version": "1", "center": {"ra_deg": 210.0, "dec_deg": 34.0},
    "scale_deg_per_px": 0.5, "width_px": 20, "height_px": 20, "sampler": "bilinear",
    "projection": "TAN", "coverage_output": "mask", "output_mode": "surface_brightness"}


def _export_rc(hips_dir, scratch):
    out = os.path.join(scratch, "out")
    os.makedirs(out, exist_ok=True)
    doc = dict(_EXPORT_PROBE, source={"hips_dir": os.path.abspath(hips_dir)},
               output_dir=out)
    cfg = _write_json(os.path.join(scratch, "probe.json"), doc)
    r = _run_cmd([EXE, "export", "--json", cfg, "--events-jsonl", "-y"], timeout=600)
    return r.returncode, (r.stderr or "")


def _set_bunit(hips_dir, value):
    props = os.path.join(hips_dir, "signal", "properties")
    with open(props, encoding="utf-8") as fh:
        lines = [l for l in fh.read().splitlines()
                 if l.split("=", 1)[0].strip() != "BUNIT"]
    lines.append("BUNIT=" + value)
    with open(props, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")


def _p3_input_ok(hips_dir):
    """缓存夹具自检: 当前生产门是否接受该 export 输入（实跑一次 export 探针）。"""
    if not os.path.isdir(hips_dir):
        return False
    with tempfile.TemporaryDirectory(prefix="fix406_p3chk_") as scratch:
        rc, _err = _export_rc(hips_dir, scratch)
    return rc == 0


def _annotate_p3_input(src, dst):
    """复制 mosaic 产物 → export 输入，并补当前生产门接受的面亮度 BUNIT 词形。

    生产侧守卫（P3-INPUT-BUNIT-MISSING / NOT-SURFACE-BRIGHTNESS）要求输入
    signal/properties 逐字声明面亮度单位；mosaic 写侧尚未写 BUNIT（PHASE3_PROJ_IMPL
    §16 C5 登记缺口，归属其它任务）⇒ 本测试夹具自行补齐，词形按生产诊断自报
    （"expected '<串>'"）自适应，仍以生产门为准（不改产品、不放宽断言）。
    """
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    candidates = ["ADU/px^2", "ADU/px^-2", "ADU"]
    with tempfile.TemporaryDirectory(prefix="fix406_bunit_") as scratch:
        _set_bunit(dst, candidates[0])
        rc, err = _export_rc(dst, scratch)
        m = re.search(r"expected '([^']+)'", err)
        if rc != 0 and m and m.group(1) not in candidates:
            candidates.insert(0, m.group(1))
        for cand in candidates:
            _set_bunit(dst, cand)
            rc, err = _export_rc(dst, scratch)
            if rc == 0:
                return dst
    raise AssertionError("export 输入夹具无被生产门接受的 BUNIT 词形; last rc=%d %s"
                         % (rc, err[-300:]))


class _FixtureCache:
    """夹具只建一次（同类/跨方法复用；ASTROCS_FIX406_FIXTURES 可指向已建缓存）。"""

    data = None
    root = None
    tmp = None

    @classmethod
    def get(cls):
        if cls.data is not None:
            return cls.data
        env = os.environ.get("ASTROCS_FIX406_FIXTURES")
        if env:
            env = os.path.abspath(env)   # 子进程 cwd = run/test_cli_cwd ⇒ 夹具须绝对路径
        if env and os.path.isfile(os.path.join(env, "READY")):
            cls.root = env
            p2out = os.path.join(env, "p2out")
            p3in = os.path.join(env, "p3in")
            # 缓存夹具的 BUNIT 词形可能随生产门漂移 ⇒ 复用前先实跑一次 export 探针，
            # 不通就按当前生产诊断重新自适应标注（判据仍以生产门为准，不放宽）。
            if not _p3_input_ok(p3in):
                p3in = _annotate_p3_input(p2out, p3in)
            cls.data = {
                "p1data": os.path.join(env, "p1data"),
                "p1a": os.path.join(env, "p1a", "light_1"),
                "p1b": os.path.join(env, "p1b", "light_2"),
                "p2out": p2out,
                "p3in": p3in}
            return cls.data
        cls.tmp = tempfile.mkdtemp(prefix="fix406_fx_")
        cls.root = cls.tmp
        cls.data = _build_fixtures(cls.tmp)
        open(os.path.join(cls.tmp, "READY"), "w").write("ok\n")
        return cls.data


class TestFix406SigtermCancel(unittest.TestCase):
    """三阶段 × 三取消点 × {SIGTERM, SIGINT 抽样} 取消矩阵。"""

    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"
        cls.fx = _FixtureCache.get()
        cls.tmp = tempfile.mkdtemp(prefix="fix406_run_")
        cls.results = []

    @classmethod
    def tearDownClass(cls):
        # 证据落位（run/ 为 gitignore 面）
        try:
            os.makedirs(os.path.dirname(EVIDENCE), exist_ok=True)
            with open(EVIDENCE, "w", encoding="utf-8") as fh:
                json.dump({"matrix": cls.results,
                           "exe": EXE,
                           "note": "FIX-406 SIGTERM/SIGINT 取消矩阵实测"},
                          fh, indent=1)
        except OSError:
            pass
        if _FixtureCache.tmp:
            shutil.rmtree(_FixtureCache.tmp, ignore_errors=True)
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ---- 配置构造（绝对路径；子进程 cwd = run/test_cli_cwd）----
    def _cfg(self, phase, out, name):
        if phase == "normalize":
            doc = {"schema_version": "1",
                   "input_lights": [os.path.join(self.fx["p1data"], "light_1.fits")],
                   "master_bias": os.path.join(self.fx["p1data"], "bias.fits"),
                   "master_dark": os.path.join(self.fx["p1data"], "dark.fits"),
                   "master_flat": os.path.join(self.fx["p1data"], "flat.fits"),
                   "dark_optimization": True, "output_dir": out,
                   "wcs": dict(WCS_EXPLICIT), "drizzle": dict(DRIZZLE)}
        elif phase == "mosaic":
            doc = {"schema_version": "1",
                   "hips_paths": [self.fx["p1a"], self.fx["p1b"]], "output_dir": out}
        else:
            doc = dict(_EXPORT_PROBE, source={"hips_dir": self.fx["p3in"]},
                       output_dir=out)
        return _write_json(os.path.join(self.tmp, name), doc)

    # ---- 单次取消注入 ----
    def _cancel_run(self, phase, point, env_var, signum):
        out = os.path.join(self.tmp, "out_%s_%s_%d" % (phase, point, signum))
        os.makedirs(out, exist_ok=True)
        cfg = self._cfg(phase, out, "cfg_%s_%s_%d.json" % (phase, point, signum))
        env = dict(os.environ, **{env_var: SLEEP_MS})
        t0 = time.time()
        p = subprocess.Popen([EXE, phase, "--json", cfg, "--events-jsonl", "-y"],
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                             cwd=run_cwd(), env=env)
        time.sleep(SIGNAL_DELAY)
        p.send_signal(signum)
        try:
            so, se = p.communicate(timeout=180)
        except subprocess.TimeoutExpired:
            p.kill()
            so, se = p.communicate()
            self.fail("%s/%s: 取消后进程未退出（信号未达协作取消路径）" % (phase, point))
        dt = time.time() - t0
        events = [json.loads(l) for l in so.splitlines() if l.strip()]
        manifests = []
        for f in sorted(os.listdir(out)):
            if f.startswith("astrocs_run_") and f.endswith(".json"):
                try:
                    with open(os.path.join(out, f), encoding="utf-8") as fh:
                        manifests.append(json.load(fh))
                except ValueError:
                    self.fail("%s/%s: manifest 非法 JSON: %s" % (phase, point, f))
        temps = []
        for root, _dirs, files in os.walk(out):
            for f in files:
                if TEMP_PAT.search(f):
                    temps.append(os.path.join(root, f))
        row = {"phase": phase, "cancel_point": point, "hook": env_var,
               "signal": int(signum), "rc": p.returncode, "seconds": round(dt, 2),
               "events": len(events),
               "final_status": (events[-1].get("status") if events else None),
               "manifests": [{"status": m.get("status"),
                              "artifacts": len(m.get("artifacts", []))}
                             for m in manifests],
               "temp_files": temps}
        self.results.append(row)
        return p.returncode, events, manifests, temps, se, row

    def _assert_cancelled(self, phase, point, env_var, signum=signal.SIGTERM):
        rc, events, manifests, temps, se, row = self._cancel_run(phase, point, env_var, signum)
        tag = "%s/%s/%s" % (phase, point, signum)
        self.assertEqual(rc, 9, "%s: 必须 exit 9（04 §2 / DESIGN §7.2）; stderr=%s"
                         % (tag, se[-300:]))
        self.assertTrue(events, "%s: 取消必须发事件流（final 可机读）" % tag)
        self.assertEqual(events[-1]["kind"], "final", "%s: 末事件必须是 final" % tag)
        self.assertEqual(events[-1].get("status"), "cancelled", "%s: final.status" % tag)
        self.assertEqual(events[-1].get("exit_code"), 9, "%s: final.exit_code" % tag)
        for m in manifests:
            self.assertNotEqual(m.get("status"), "complete",
                                "%s: 取消不得留 complete manifest" % tag)
        self.assertEqual(temps, [], "%s: 取消不得留临时/半成品文件" % tag)
        # 取消点可区分（非退化判据）
        if point == "startup":
            self.assertEqual(manifests, [],
                             "%s: 启动期取消在 output_dir/run_context 建立前 ⇒ 不写 manifest" % tag)
            self.assertIsNone(events[-1].get("run_manifest"),
                              "%s: 未写 manifest 时 final.run_manifest 必须 null（不伪造）" % tag)
        else:
            self.assertEqual(len(manifests), 1,
                             "%s: 计算/写盘期取消必须恰一份 incomplete manifest" % tag)
            man = manifests[0]
            self.assertEqual(man.get("status"), "incomplete", "%s: manifest.status" % tag)
            self.assertEqual(man.get("phases"), [{"normalize": 1, "mosaic": 2,
                                                  "export": 3}[phase]],
                             "%s: manifest.phases" % tag)
            self.assertIn("cancel", json.dumps(man, ensure_ascii=False).lower(),
                          "%s: incomplete manifest 必须自带取消原因" % tag)
            self.assertEqual(row["final_status"], "cancelled", tag)
        if point == "write":
            self.assertGreaterEqual(len(manifests[0].get("artifacts", [])), 1,
                                    "%s: 写盘期取消时产物已落盘 ⇒ artifacts 非空（判据非退化）" % tag)
        return row

    # ---- 验收门 ①: SIGTERM × 三阶段 × 三取消点 ----
    def test_01_sigterm_matrix_all_phases_all_points(self):
        for phase in PHASES:
            for point, env_var in CANCEL_POINTS:
                with self.subTest(phase=phase, cancel_point=point, sig="SIGTERM"):
                    self._assert_cancelled(phase, point, env_var, signal.SIGTERM)

    # ---- 验收门 ②: SIGINT 等价（写盘点全阶段 + 计算点抽样）----
    def test_02_sigint_equivalent_paths(self):
        for phase in PHASES:
            with self.subTest(phase=phase, cancel_point="write", sig="SIGINT"):
                self._assert_cancelled(phase, "write", "ASTROCS_TEST_WRITE_SLEEP_MS",
                                       signal.SIGINT)
        with self.subTest(phase="normalize", cancel_point="compute", sig="SIGINT"):
            self._assert_cancelled(phase="normalize", point="compute",
                                   env_var="ASTROCS_TEST_PIPELINE_SLEEP_MS",
                                   signum=signal.SIGINT)

    # ---- 判据非退化: 三取消点证据必须可区分 ----
    def test_03_cancel_points_are_distinguishable(self):
        rows = {(r["phase"], r["cancel_point"]): r for r in self.results}
        for phase in PHASES:
            start = rows.get((phase, "startup"))
            comp = rows.get((phase, "compute"))
            wr = rows.get((phase, "write"))
            if not (start and comp and wr):
                self.skipTest("矩阵未跑（test_01 失败时本判据无输入）")
            self.assertEqual(len(start["manifests"]), 0, "%s: startup 无 manifest" % phase)
            self.assertEqual(len(comp["manifests"]), 1, "%s: compute 有 manifest" % phase)
            self.assertEqual(len(wr["manifests"]), 1, "%s: write 有 manifest" % phase)
            self.assertGreater(wr["manifests"][0]["artifacts"],
                               comp["manifests"][0]["artifacts"],
                               "%s: 写盘点产物登记必须多于计算点（非退化）" % phase)


class TestCancelHandlerRegistration(unittest.TestCase):
    """平台取消路径的**结构化登记门**（Linux 可跑；功能面在 Windows 类）。"""

    TOKEN = os.path.join(REPO, "lib", "infrastructure", "cli", "cancel_token.h")
    MAIN = os.path.join(REPO, "lib", "infrastructure", "cli", "main.cpp")

    def test_01_posix_and_windows_handlers_registered(self):
        with open(self.TOKEN, encoding="utf-8") as fh:
            src = fh.read()
        self.assertIn("std::signal(SIGINT, posix_signal_handler)", src)
        self.assertIn("std::signal(SIGTERM, posix_signal_handler)", src)
        # Windows 等价路径: SetConsoleCtrlHandler 安装 + 处理函数对全部控制类型置位
        self.assertIn("SetConsoleCtrlHandler(console_ctrl_handler, TRUE)", src)
        self.assertIn("BOOL WINAPI console_ctrl_handler(DWORD type)", src)
        self.assertIn("cancel_flag().store(true", src)
        self.assertIn("return TRUE;", src)
        with open(self.MAIN, encoding="utf-8") as fh:
            main = fh.read()
        self.assertIn("install_cancel_handlers()", main,
                      "main 必须安装取消处理器（否则信号不达 cancel_flag）")

    def test_02_single_atomic_flag_source(self):
        """取消标志唯一源: 不得在别处再造第二个取消标志（信号只置位 cancel_flag）。"""
        with open(self.TOKEN, encoding="utf-8") as fh:
            src = fh.read()
        self.assertIn("std::atomic<bool>& cancel_flag()", src)
        self.assertIn("bool is_cancelled()", src)
        self.assertEqual(src.count("static std::atomic<bool> flag"), 1,
                         "取消标志必须恰一处静态定义（单向置位语义）")


@unittest.skipUnless(os.name == "nt", "Windows 控制台控制事件（CTRL_C/CTRL_BREAK）")
class TestWindowsConsoleCtrl(unittest.TestCase):
    """Windows 等价路径功能面（CTRL_C_EVENT / CTRL_BREAK_EVENT → 协作取消 → exit 9）。

    平台限制（显式登记，非 waiver）:
      * CTRL_CLOSE_EVENT 无法由进程/父进程程序化投递（无对应 Win32 API；由关闭控制台
        窗口/注销/关机触发），且系统在 handler 返回后强制终止进程（handler 只置位、
        不等收尾）⇒ **不保证** exit 9。生产语义按 DESIGN §7.2 的 Ctrl-C 路径
        （CTRL_C_EVENT/CTRL_BREAK_EVENT）验收；CTRL_CLOSE 的处置（是否在 handler 内
        同步等待收尾）需负责人裁决，本任务只登记，不改语义。
      * 本仓 Windows 功能面由 hosted CI（.github/workflows/ci-windows.yml）承载；
        本类在本节点自动 skip（无 Windows 运行时），skip 不等于通过。
    """

    def test_01_ctrl_c_and_ctrl_break_exit9(self):
        fx = _FixtureCache.get()
        tmp = tempfile.mkdtemp(prefix="fix406_win_")
        try:
            for ctrl, name in ((signal.CTRL_C_EVENT, "CTRL_C"),
                               (signal.CTRL_BREAK_EVENT, "CTRL_BREAK")):
                out = os.path.join(tmp, "out_" + name)
                os.makedirs(out, exist_ok=True)
                cfg = _write_json(os.path.join(tmp, "cfg_%s.json" % name), {
                    "schema_version": "1",
                    "input_lights": [os.path.join(fx["p1data"], "light_1.fits")],
                    "master_bias": os.path.join(fx["p1data"], "bias.fits"),
                    "master_dark": os.path.join(fx["p1data"], "dark.fits"),
                    "master_flat": os.path.join(fx["p1data"], "flat.fits"),
                    "dark_optimization": True, "output_dir": out,
                    "wcs": dict(WCS_EXPLICIT), "drizzle": dict(DRIZZLE)})
                env = dict(os.environ, ASTROCS_TEST_SLEEP_MS=SLEEP_MS)
                p = subprocess.Popen([EXE, "normalize", "--json", cfg, "--events-jsonl", "-y"],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                     text=True, cwd=run_cwd(), env=env,
                                     creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
                time.sleep(SIGNAL_DELAY)
                p.send_signal(ctrl)
                so, se = p.communicate(timeout=180)
                self.assertEqual(p.returncode, 9, "%s → exit 9; stderr=%s" % (name, se[-300:]))
                events = [json.loads(l) for l in so.splitlines() if l.strip()]
                self.assertEqual(events[-1]["kind"], "final")
                self.assertEqual(events[-1]["status"], "cancelled")
                for f in os.listdir(out):
                    if f.startswith("astrocs_run_"):
                        with open(os.path.join(out, f), encoding="utf-8") as fh:
                            self.assertNotEqual(json.load(fh)["status"], "complete")
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
