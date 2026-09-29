#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CLI-CPU-PROFILE-SOURCE：cpu_profile 的来源口径与"缺画像/坏画像"的分别处置（命令面门）。

规范依据（唯一权威链，逐句可核）：
  * docs/ASTROCS_DESIGN.md §7.1（唯一命令树/命令面）：cpu_profile 的**唯一来源** =
    `benchmark` 实测写下的那一份；**没跑过 `benchmark`（安装目录没有那份 profile）⇒ 最小
    可用运行**（全部 x64 平台都具备的基线指令集 + 保守并行，照常成功退出）；**画像存在但
    校验不过**（取值非法 / schema 不匹配 / 与本机失配）⇒ 运行以校验器给出的非零退出码失败。
  * docs/ASTROCS_DESIGN.md §9「画像的来源与缺省行为」；docs/api/CLI_PROTOCOL_V1.md §1；
    docs/contracts/CONFIG_CONTRACT.md「cpu_profile 的来源与缺省口径」；
    docs/architecture/CPU_BACKEND_ARCH.md §6（失败与回退）。
  * 退出码唯一源 = lib/infrastructure/cli/exit_codes.h（表见 docs/ASTROCS_DESIGN.md §7.2）：
    ARGS=2 / INPUT=3 / BACKEND=5。

用例矩阵（正例与负例**同数据、同命令、同 cwd**，唯一变量 = 安装目录里那一份画像文件）：
  正例 1（无画像）：安装目录与用户级降级落点都没有 profile ⇒ rc=0，stderr 的
         `cpu_profile source=none` 且 `backend=astrocs.cpu.baseline`（基线最小可用运行）。
  正例 2（有画像）：安装目录放 benchmark 实测产出、并按画像内容声明 avx2 与内存比例的那一份
         ⇒ rc=0，`source=install-dir`，且运行按画像内容取值（`percent=<画像值>`）；当安装树
         带可用 avx2 backend 时 provider=avx2（按画像选核），否则按预检回退 baseline 并如实
         断言该回退（判据不因环境静默放宽）。
  负例 1（取值非法：kernels.*.workers=0）⇒ rc=BACKEND(5)。
  负例 2（schema 不匹配：schema=astrocs.cpu-profile/v1）⇒ rc=BACKEND(5)。
  负例 3（结构损坏：非 JSON 文本）⇒ rc=INPUT(3)。
  负例 4（手工指定入口）：normalize/mosaic/export 带 `--cpu-profile <path>` ⇒ rc=ARGS(2)
         且 stderr 报 unknown flag（对外命令面不存在画像手工指定选项）。
  负例 5（坏画像 ≠ 无画像）：同一命令、同一数据，删掉画像 rc=0、把画像改坏 rc≠0 —— 两者
         必须落到不同判词，不得混为一类。

画像夹具的来源纪律：**唯一来源 = 产品自己的 `benchmark`**（真跑一次并缓存；缓存按二进制
sha256 定位，并以"本机仍能校验通过"的便宜探针自检，失效即重跑）。测试不手写、不复制任何
机器绑定字段（quota_signature / logical_available / source_commit / self_test_sha256 全部由
产品产出），故换机器、换提交后夹具自动重建。

跑法：python3 -m unittest discover -s eng/tests/cli -t eng/tests/cli
"""
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cli_test_hygiene import run_cwd  # noqa: E402

CACHE = os.path.join(REPO, "run", "temp", "cpu_profile_source_policy")

# ARCH-001 迁移: 新布局优先, 旧路径回退（与 eng/tests/cli/test_phase1_inprocess.py 同源）。
AIO = next((p for p in (os.path.join(REPO, "lib", "infrastructure", "aio"),
                        os.path.join(REPO, "lib", "astro_image_io")) if os.path.isdir(p)),
           os.path.join(REPO, "lib", "infrastructure", "aio"))
SKIP_FITS = (r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
             r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
             r"imcopy|imarith|tabcompile|sortcol|tabselect")


def cli_binary():
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    for rel in (("build", "acsd"), ("build", "cli", "acsd")):
        cand = os.path.join(REPO, *rel)
        if os.path.isfile(cand):
            return cand
    return os.path.join(REPO, "build", "acsd")


EXE = cli_binary()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _sig_of(paths):
    """源/二进制签名：mtime_ns + size（缓存失效判据；不读内容以省时间）。"""
    h = hashlib.sha256()
    for p in sorted(paths):
        st = os.stat(p)
        h.update(("%s|%d|%d\n" % (p, st.st_mtime_ns, st.st_size)).encode("utf-8"))
    return h.hexdigest()[:16]


def _fixture_sources():
    cdir = os.path.join(AIO, "third_party", "cfitsio")
    cf = [os.path.join(cdir, f) for f in sorted(os.listdir(cdir))
          if f.endswith(".c") and not re.search(SKIP_FITS, f)]
    own = [os.path.join(REPO, "eng", "tests", "backend", "phase1_fixture_main.cpp")] + \
          [os.path.join(AIO, "src", n) for n in ("aio_fits.cpp", "aio_api.cpp", "aio_log.cpp",
                                                 "aio_compressor.cpp")]
    return cf, own


def build_fixture(dst):
    """构建 phase1 数据夹具（cfitsio + aio 最小闭包），与 test_phase1_inprocess 同配方。"""
    cf, own = _fixture_sources()
    cdir = os.path.join(AIO, "third_party", "cfitsio")
    objs = []
    for c in cf:
        o = os.path.join(dst, os.path.basename(c)[:-2] + ".o")
        subprocess.run(["gcc", "-O2", "-w", "-I" + cdir, "-c", c, "-o", o],
                       check=True, capture_output=True, timeout=300)
        objs.append(o)
    exe = os.path.join(dst, "fixture")
    r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS",
                        "-I" + os.path.join(REPO, "lib", "include"),
                        "-I" + os.path.join(AIO, "include"),
                        "-I" + os.path.join(AIO, "src"),
                        "-I" + cdir, *own, *objs, "-lz", "-lzstd", "-llz4", "-o", exe],
                       capture_output=True, text=True, timeout=900)
    if r.returncode != 0:
        raise AssertionError("fixture 构建失败：" + r.stderr[-800:])
    return exe


def _install_tree(dst, with_providers=True):
    """造一个"安装目录"：二进制 + 随行 providers/（发行布局的落点口径）。"""
    os.makedirs(dst, exist_ok=True)
    exe = os.path.join(dst, "acsd")
    shutil.copy2(EXE, exe)
    src_pv = os.path.join(os.path.dirname(EXE), "providers")
    if with_providers and os.path.isdir(src_pv):
        dst_pv = os.path.join(dst, "providers")
        if not os.path.isdir(dst_pv):
            shutil.copytree(src_pv, dst_pv)
    return exe


def _env_for(install_dir):
    """用户级降级落点随安装目录隔离（XDG_DATA_HOME 须为绝对路径才被采纳）。"""
    env = os.environ.copy()
    xdg = os.path.join(install_dir, "xdg")
    os.makedirs(xdg, exist_ok=True)
    env["XDG_DATA_HOME"] = xdg
    return env


def _run_cli(install_dir, args, env=None, timeout=900):
    return subprocess.run([os.path.join(install_dir, "acsd"), *args], capture_output=True,
                          text=True, timeout=timeout, cwd=run_cwd(),
                          env=env if env is not None else _env_for(install_dir))


def _benchmark_profile():
    """画像夹具 = 产品 `benchmark` 的实测产物（缓存 + 自检；唯一来源纪律见模块 docstring）。"""
    cached = os.path.join(CACHE, "profile-" + sha256_file(EXE)[:16], "cpu_profile.json")
    if os.path.isfile(cached) and _profile_still_valid(cached):
        return cached
    tmp = tempfile.mkdtemp(prefix="acsd_bench_")
    _install_tree(tmp)
    r = subprocess.run([os.path.join(tmp, "acsd"), "benchmark"], capture_output=True, text=True,
                       timeout=1800, cwd=run_cwd(), env=_env_for(tmp))
    toks = r.stdout.split()
    # 判据只认"画像是否真被产出且本机可用"：verdict 面属 benchmark 自身的科学判定，
    # 在负载机上来回波动（PASS/FAIL），而本模块的夹具需求是那份可校验的画像本体。
    if not toks or not os.path.isfile(toks[0]):
        raise AssertionError("benchmark 未产出画像：rc=%d stdout=%r stderr=%r"
                             % (r.returncode, r.stdout[-300:], r.stderr[-500:]))
    src = toks[0]                                   # 产品自报落点（首 token）
    verdict = toks[1] if len(toks) > 1 else "?"
    os.makedirs(os.path.dirname(cached), exist_ok=True)
    shutil.copy2(src, cached)
    if not _profile_still_valid(cached):
        raise AssertionError("benchmark 产出的画像未通过运行期校验（verdict=%s rc=%d）"
                             % (verdict, r.returncode))
    return cached


def _profile_still_valid(profile_path):
    """便宜探针：合法画像 ⇒ 会话越过画像门（在配置面失败 rc=2）；画像被判不可用 ⇒ rc=5。"""
    tmp = tempfile.mkdtemp(prefix="acsd_probe_")
    _install_tree(tmp)
    shutil.copy2(profile_path, os.path.join(tmp, "cpu_profile.json"))
    cfg = os.path.join(tmp, "min.json")
    with open(cfg, "w", encoding="utf-8") as fh:
        json.dump({"inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
                   "output_dir": tmp}, fh)
    r = _run_cli(tmp, ["normalize", "--json", cfg, "-force", "-y"], timeout=300)
    return r.returncode != 5


def _variant_profile(base_path, tmp):
    """按画像内容声明 avx2 + 非默认内存比例（用于按画像选核/取值的可判别正例）。"""
    with open(base_path, encoding="utf-8") as fh:
        doc = json.load(fh)
    for _kid, row in doc["kernels"].items():
        row["provider"] = "avx2"
        row["workers"] = 5
    doc["host"]["memory_budget_percent"] = 42
    out = os.path.join(tmp, "profile_variant.json")
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, indent=1)
    return out


def _broken_profile(base_path, tmp, kind):
    with open(base_path, encoding="utf-8") as fh:
        raw = fh.read()
    out = os.path.join(tmp, "profile_broken_%s.json" % kind)
    if kind == "text":
        with open(out, "w", encoding="utf-8") as fh:
            fh.write("{ this is not json")
        return out
    doc = json.loads(raw)
    if kind == "workers0":
        kid = sorted(doc["kernels"])[0]
        doc["kernels"][kid]["workers"] = 0
    elif kind == "schema":
        doc["schema"] = "astrocs.cpu-profile/v1"
    else:
        raise AssertionError("未知坏画像类别：" + kind)
    with open(out, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, indent=1)
    return out


def _avx2_backend_usable(install_dir):
    """环境能力（产品自证，不手写平台表）：安装树的 avx2 backend 预检是否 pass。"""
    manifest = os.path.join(install_dir, "providers", "backends.manifest.json")
    if not os.path.isfile(manifest):
        return False
    r = _run_cli(install_dir, ["doctor", "--json"], timeout=300)
    try:
        doc = json.loads(r.stdout)
    except Exception:
        return False
    for ck in doc.get("checks", []):
        if ck.get("name") in ("backend_preflight:avx2", "backend_preflight:astrocs.cpu.avx2"):
            return ck.get("status") == "pass"
    return False


def _cached_fixture():
    cf, own = _fixture_sources()
    sig = _sig_of(cf + own)
    d = os.path.join(CACHE, "fixture-" + sig)
    exe = os.path.join(d, "fixture")
    if os.path.isfile(exe):
        return exe
    os.makedirs(d, exist_ok=True)
    return build_fixture(d)


class TestCpuProfileSourcePolicy(unittest.TestCase):
    """画像来源唯一（benchmark）+ 缺画像基线最小可用 + 坏画像判红 + 命令面无手工指定入口。"""

    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build acsd）"
        cls.work = tempfile.mkdtemp(prefix="cp_policy_")
        cls.fixture = _cached_fixture()
        cls.data = os.path.join(cls.work, "data")
        os.makedirs(cls.data)
        r = subprocess.run([cls.fixture, "--make", cls.data], capture_output=True, text=True,
                           timeout=300, cwd=run_cwd())
        assert "FIXTURES_OK" in r.stdout, r.stderr
        cls.cfg = os.path.join(cls.work, "cfg.json")
        with open(cls.cfg, "w", encoding="utf-8") as fh:
            json.dump({
                "input_lights": [os.path.join(cls.data, "light_1.fits"),
                                 os.path.join(cls.data, "light_2.fits")],
                "master_bias": os.path.join(cls.data, "bias.fits"),
                "master_dark": os.path.join(cls.data, "dark.fits"),
                "master_flat": os.path.join(cls.data, "flat.fits"),
                "dark_optimization": True,
                "wcs": {"crpix1": 32.5, "crpix2": 32.5, "crval1": 210.0, "crval2": 34.0,
                        "cd11": -2.7777777777777776e-4, "cd12": 0.0,
                        "cd21": 0.0, "cd22": 2.7777777777777776e-4},
                "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 1},
                "output_dir": os.path.join(cls.work, "out"),
            }, fh)
        cls.base_profile = _benchmark_profile()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.work, ignore_errors=True)

    def _case_install(self, name, profile_path=None):
        d = os.path.join(self.work, "install_" + name)
        _install_tree(d)
        if profile_path is not None:
            shutil.copy2(profile_path, os.path.join(d, "cpu_profile.json"))
        return d

    def _cfg_for(self, install_dir):
        with open(self.cfg, encoding="utf-8") as fh:
            doc = json.load(fh)
        out = os.path.join(install_dir, "out")
        os.makedirs(out, exist_ok=True)
        doc["output_dir"] = out
        p = os.path.join(install_dir, "cfg_case.json")
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return p

    def _normalize(self, install_dir):
        return _run_cli(install_dir, ["normalize", "--json", self._cfg_for(install_dir), "-y"])

    # ── 正例 1：没跑过 benchmark ⇒ 基线最小可用运行 ────────────────────────────
    def test_01_no_profile_runs_baseline(self):
        inst = self._case_install("no_profile")
        self.assertFalse(os.path.isfile(os.path.join(inst, "cpu_profile.json")),
                         "本用例前提：安装目录没有画像")
        r = self._normalize(inst)
        self.assertEqual(r.returncode, 0, r.stderr[-600:])
        self.assertIn("cpu_profile source=none", r.stderr, "来源标记须明示「无 benchmark 画像」")
        self.assertIn("acsd: no cpu_profile at", r.stderr)
        self.assertIn("backend=astrocs.cpu.baseline", r.stderr, "缺画像 ⇒ 基线最小可用运行")
        self.assertIn("conservative baseline route", r.stderr,
                      "基线口径须给出逐 kernel 回退事实行（缺画像 = 保守路由，不是坏输入）")

    # ── 正例 2：有画像 ⇒ 按画像运行/选核 ──────────────────────────────────────
    def test_02_profile_drives_runtime_choices(self):
        prof = _variant_profile(self.base_profile, self.work)
        inst = self._case_install("with_profile", prof)
        r = self._normalize(inst)
        self.assertEqual(r.returncode, 0, r.stderr[-600:])
        self.assertIn("cpu_profile source=install-dir", r.stderr)
        self.assertIn("percent=42", r.stderr, "运行须采纳画像声明的内存比例（按内容而非按存在）")
        if _avx2_backend_usable(inst):
            self.assertIn("provider=avx2", r.stderr, "画像声明 avx2 ⇒ 按画像选核")
            self.assertRegex(r.stderr, r"route table: 12/12 kernels on variant provider")
        else:
            self.assertIn("backend=astrocs.cpu.baseline", r.stderr,
                          "安装树无可用 avx2 backend ⇒ 预检回退 baseline（回退如实可见）")
            self.assertRegex(r.stderr, r"route table: 0/12 kernels on variant provider")

    # ── 负例 1/2/3：画像存在但校验不过 ⇒ 判红（与「无画像」不同类）─────────────
    def test_03_bad_profile_values_is_red(self):
        inst = self._case_install("bad_workers",
                                  _broken_profile(self.base_profile, self.work, "workers0"))
        r = self._normalize(inst)
        self.assertEqual(r.returncode, 5, "取值非法的画像 ⇒ BACKEND(5)：%s" % r.stderr[-400:])
        self.assertIn("exists but is unusable", r.stderr)
        self.assertNotIn("source=none", r.stderr, "坏输入不得被当成「没跑过 benchmark」")

    def test_04_bad_profile_schema_is_red(self):
        inst = self._case_install("bad_schema",
                                  _broken_profile(self.base_profile, self.work, "schema"))
        r = self._normalize(inst)
        self.assertEqual(r.returncode, 5, "schema 不匹配 ⇒ BACKEND(5)：%s" % r.stderr[-400:])
        self.assertIn("exists but is unusable", r.stderr)

    def test_05_corrupt_profile_is_red(self):
        inst = self._case_install("bad_text",
                                  _broken_profile(self.base_profile, self.work, "text"))
        r = self._normalize(inst)
        self.assertEqual(r.returncode, 3, "结构损坏(json) ⇒ INPUT(3)：%s" % r.stderr[-400:])
        self.assertIn("exists but is unusable", r.stderr)

    # ── 负例 4：对外命令面不存在画像手工指定入口 ───────────────────────────────
    def test_06_no_manual_profile_option_on_public_surface(self):
        inst = self._case_install("flag")
        for cmd in ("normalize", "mosaic", "export"):
            r = _run_cli(inst, [cmd, "--json", self.cfg, "--cpu-profile",
                                "/tmp/whatever.json", "-y"], timeout=120)
            self.assertEqual(r.returncode, 2,
                             "%s 接受了 --cpu-profile：%s" % (cmd, r.stdout[-200:]))
            self.assertIn("unknown flag '--cpu-profile'", r.stderr)
        h = _run_cli(inst, ["--help"], timeout=120)
        self.assertNotIn("cpu-profile", h.stdout, "--help 不得列出画像路径选项")

    # ── 负例 5：坏画像 ≠ 无画像（同数据同命令，判词必须不同）──────────────────
    def test_07_bad_profile_is_not_the_same_class_as_missing(self):
        miss = self._case_install("class_missing")
        r_miss = self._normalize(miss)
        bad = self._case_install("class_bad",
                                 _broken_profile(self.base_profile, self.work, "schema"))
        r_bad = self._normalize(bad)
        self.assertEqual(r_miss.returncode, 0)
        self.assertNotEqual(r_bad.returncode, r_miss.returncode,
                            "坏画像与缺画像必须分别处置（一绿一红），不得混为一类")
        self.assertEqual(r_bad.returncode, 5)


if __name__ == "__main__":
    unittest.main(verbosity=2)
