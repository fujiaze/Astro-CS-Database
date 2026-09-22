#!/usr/bin/env python3
"""eng/ci/prepare_linux_fixtures.py — linux-main CI 检查的宿主侧 fixture 准备。

背景（UT-BACKEND p1004 联合门依赖的两个非受控输入）:
1. eng/tests/backend/test_p1004_joint_gate.py 读 run/temp/mon001_cfg.json——该文件
   从未入库（.gitignore run/* 全忽略），历史上是本地手动准备的 MON-001 实验残留，
   hosted runner 上恒缺失 → test_02/03 必 FAIL。
2. 跑通 phase3 run 需要 HiPS fixture（p3_session 严格校验 source.hips_dir）。
   本脚本用仓库内 vendored AIO 源自编译 fixture 生成 FIELD.hips（与
   eng/tests/cli/test_monitor_events.py setUpClass 同配方，eng/tests/backend/
   phase2_fixture_main.cpp），不联网拉任何业务数据。

产物:
  run/temp/FIELD.hips       合成 HiPS field fixture
  run/temp/mon001_cfg.json  V1 顶层 config（schema_version/inputs/output_dir/
                            phase3 子对象），output_dir=/tmp/mon001_run_out
  /tmp/mon001_run_out/      p1004 断言的 resource_summary 输出目录

红线: 只写 run/temp 与 /tmp/mon001_run_out，不触碰 docs/lib/tests 语义面。
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
AIO = REPO / "lib" / "infrastructure" / "aio"
SHARED = REPO / "lib" / "algorithms" / "shared"
OUT_DIR = Path("/tmp/mon001_run_out")
CFG = REPO / "run" / "temp" / "mon001_cfg.json"
HIPS = REPO / "run" / "temp" / "FIELD.hips"

# ── 输入解析：不写死目录 ─────────────────────────────────────────────────────
# 本脚本是 wf_step 步 LINUX-PREPARE-FIXTURES 的实现（eng/ci/workflow_binding.json
# 的 step_id -> check_id 绑定）。它消费的测试面输入按「步 → 检查项 → 唯一检查注册表
# changed_paths」逐级解析，不另抄一份路径：ENGINEERING_SPEC §10 规定
# eng/ci/checks.json 是唯一检查注册表，其 changed_paths 的路径域由 CHK-IMPACT-MAP
# 机器保证「锚存活 + 无退役引用」。根 tests/ 已随根目录整合退役（测试落
# eng/tests/，ENGINEERING_SPEC §7「其他固定目录」），写死旧根只会在编译期炸成
# cc1plus「没有那个文件或目录」。
STEP_ID = "LINUX-PREPARE-FIXTURES"
BINDING_REL = Path("eng/ci/workflow_binding.json")
REGISTRY_REL = Path("eng/ci/checks.json")
FIXTURE_BASENAME = "phase2_fixture_main.cpp"


def _registry_input_dirs() -> list[Path]:
    """本步在唯一检查注册表里声明的输入目录（去通配后实存者）。"""
    try:
        binding = json.loads((REPO / BINDING_REL).read_text(encoding="utf-8"))
        check_id = next(s["check_id"] for s in binding["steps"]
                        if s.get("step_id") == STEP_ID)
        registry = json.loads((REPO / REGISTRY_REL).read_text(encoding="utf-8"))
        entry = next(c for c in registry["checks"] if c.get("id") == check_id)
    except (OSError, json.JSONDecodeError, KeyError, StopIteration) as e:
        raise SystemExit(f"ANCHOR_STALE: {STEP_ID} 的 {BINDING_REL}/{REGISTRY_REL} "
                         f"绑定不可解析: {e}")
    dirs = []
    for rel in entry.get("changed_paths") or []:
        cand = REPO / (rel[:-3] if rel.endswith("/**") else rel)
        if cand.is_dir():
            dirs.append(cand)
    if not dirs:
        raise SystemExit(f"ANCHOR_STALE: 检查项 {check_id} 的 changed_paths 无实存目录")
    return dirs


def fixture_src() -> Path:
    """fixture 源 = 注册输入域内的唯一同名文件（0 处/多 处 ⇒ 点名失败）。"""
    hits = [d / FIXTURE_BASENAME for d in _registry_input_dirs()
            if (d / FIXTURE_BASENAME).is_file()]
    if len(hits) != 1:
        raise SystemExit("ANCHOR_STALE: %s 在注册输入域命中 %d 处（应恰 1 处）: %s"
                         % (FIXTURE_BASENAME, len(hits),
                            [str(h.relative_to(REPO)) for h in hits]))
    return hits[0]


ROOT_CMAKE = REPO / "CMakeLists.txt"
COMMON_TARGET = "astrocs_common"


def _strip_cmake_comments(text: str) -> str:
    return "\n".join(ln for ln in text.splitlines()
                     if not ln.lstrip().startswith("#"))


def shared_lib_sources() -> list[Path]:
    """共享算法基础库（根构建图 target astrocs_common）的源清单。

    fixture 的 AIO 链需要 sha256（aio_file_io.h 的 aio_file::sha256_hex）与
    healpix_core 两个 TU。旧实现手抄了 healpix_core 而漏了同期进 target 的
    sha256.cpp ⇒ 链接期 undefined reference（ENGINEERING_SPEC §10「锚存活」的
    兄弟条款：登记面与构建图不得各写一份）。改为一律从根 CMakeLists 的
    add_library(astrocs_common ...) 逐字解析，target 增删源文件时本脚本自动跟随。
    """
    try:
        text = _strip_cmake_comments(ROOT_CMAKE.read_text(encoding="utf-8"))
    except OSError as e:
        raise SystemExit(f"ANCHOR_STALE: {ROOT_CMAKE.name} 不可读: {e}")
    m = re.search(r"add_library\(\s*" + COMMON_TARGET + r"\s+\w+\s+(.*?)\)", text, re.S)
    if not m:
        raise SystemExit(f"ANCHOR_STALE: 根构建图无 add_library({COMMON_TARGET} ...) 源清单")
    srcs = [REPO / tok for tok in
            re.findall(r"[A-Za-z0-9_./\-]+\.(?:cpp|cc|c)", m.group(1))]
    if not srcs:
        raise SystemExit(f"ANCHOR_STALE: {COMMON_TARGET} 源清单解析为空（抽取退化）")
    return srcs


def require_paths(named: dict) -> None:
    """锚存活（ENGINEERING_SPEC §10）：本脚本引用的每个路径必须存在，失效时在
    编译前点名失败（ANCHOR_STALE），不让 g++/cc1plus 报「没有那个文件或目录」。"""
    missing = [(k, p) for k, p in named.items() if not p.exists()]
    if missing:
        detail = "; ".join(
            f"{k} {p.relative_to(REPO) if str(p).startswith(str(REPO)) else p}"
            for k, p in missing)
        raise SystemExit(f"ANCHOR_STALE: {detail}")


def _run(argv: list[str], **kw) -> subprocess.CompletedProcess:
    kw.setdefault("capture_output", True)
    kw.setdefault("text", True)
    return subprocess.run(argv, **kw)


def build_fixture(tmp: Path) -> None:
    """编译 AIO fixture 并生成 FIELD.hips（配方 = eng/tests/cli/test_phase3_inprocess.py
    setUpClass 的 vendored cfitsio 静态编译 + eng/tests/cli/test_monitor_events.py
    的 --make-field 入口）。"""
    # 1) vendored cfitsio 全量 .c → .o（排除测试同款非库文件）
    cdir = AIO / "third_party" / "cfitsio"
    objs: list[str] = []
    for f in sorted(os.listdir(cdir)):
        if not f.endswith(".c"):
            continue
        if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                     r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                     r"imcopy|imarith|tabcompile|sortcol|tabselect", f):
            continue
        o = tmp / (f[:-2] + ".o")
        r0 = _run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", str(cdir / f),
                   "-o", str(o)], timeout=300)
        if r0.returncode != 0:
            sys.stderr.write(r0.stderr[-400:])
            raise SystemExit(f"prepare_linux_fixtures: cfitsio {f} 编译失败")
        objs.append(str(o))

    srcs = [fixture_src(),
            AIO / "src" / "hips" / "aio_hips_writer.cpp",
            AIO / "src" / "hips" / "aio_hips_reader.cpp",
            AIO / "src" / "aio_fits.cpp",
            AIO / "src" / "aio_api.cpp",
            AIO / "src" / "aio_log.cpp",
            AIO / "src" / "aio_compressor.cpp",
            *shared_lib_sources()]   # sha256 + healpix_core（构建图 astrocs_common）
    # 公共头在 lib/include（根 include/ 已退役）；与 eng/tests/backend/fixture_common.py
    # 的 fixture 编译配方同口径。
    inc_dirs = [
        REPO / "lib" / "include",
        AIO / "include",
        AIO / "src",
        cdir,
        SHARED,
        SHARED / "healpix",
    ]
    require_paths({"cfitsio_dir": cdir,
                   **{f"include:{d.relative_to(REPO)}": d for d in inc_dirs},
                   **{f"src:{s.relative_to(REPO)}": s for s in srcs}})
    incs = [f"-I{d}" for d in inc_dirs]
    exe = tmp / "fixture"
    r = _run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
              *[str(s) for s in srcs], *objs, "-lz", "-lzstd", "-llz4",
              "-o", str(exe)], timeout=900)
    if r.returncode != 0:
        sys.stderr.write(r.stderr[-800:])
        raise SystemExit("prepare_linux_fixtures: fixture 编译失败")
    data = tmp / "data"
    data.mkdir(parents=True, exist_ok=True)
    r2 = _run([str(exe), "--make-field", str(data)], timeout=300)
    if "HIPS_FIXTURES_OK" not in r2.stdout:
        sys.stderr.write(r2.stderr[-400:] + r2.stdout[-400:])
        raise SystemExit("prepare_linux_fixtures: FIELD.hips 生成失败")
    src = data / "FIELD.hips"
    if HIPS.exists():
        shutil.rmtree(HIPS)
    HIPS.parent.mkdir(parents=True, exist_ok=True)
    shutil.move(str(src), str(HIPS))


def write_config() -> None:
    """生成 mon001_cfg.json（V1 顶层合同 + phase3 子对象，参数对齐
    eng/tests/cli/test_monitor_events.py 的 40x30 nearest 小图合成门）。"""
    CFG.parent.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    cfg = {
        "schema_version": "1",
        "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
        "output_dir": str(OUT_DIR),
        "phase3": {
            "source": {"hips_dir": str(HIPS)},
            "center": {"ra_deg": 210.0, "dec_deg": 34.0},
            "scale_deg_per_px": 0.1,
            "width_px": 40,
            "height_px": 30,
            "projection": "TAN",
            "sampler": "nearest",
            "coverage_output": "mask",
            # P3-006/DOC-003 内存守卫: 40x30 视场默认 max_tiles=min(1024,
            # ceil(W·H/512²)+16)=17; 显式 64 是残方, phase3 run 触发
            # ACS_ERR_BUDGET(5) → UT-BACKEND p1004 test_02/03 CI 恒败。
            # 与 eng/tests/backend/test_p1004_joint_gate.py::_ensure_mon001_fixture
            # 语义对齐: 不设该键取默认。
        },
    }
    CFG.write_text(json.dumps(cfg, ensure_ascii=False, indent=1) + "\n",
                   encoding="utf-8")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="cifix_") as td:
        build_fixture(Path(td))
    write_config()
    print("prepare_linux_fixtures OK:",
          json.dumps({"hips": str(HIPS), "config": str(CFG),
                      "output_dir": str(OUT_DIR)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
