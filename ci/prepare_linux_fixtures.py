#!/usr/bin/env python3
"""ci/prepare_linux_fixtures.py — linux-main CI 检查的宿主侧 fixture 准备。

背景（UT-BACKEND p1004 联合门依赖的两个非受控输入）:
1. tests/backend/test_p1004_joint_gate.py 读 run/temp/mon001_cfg.json——该文件
   从未入库（.gitignore run/* 全忽略），历史上是本地手动准备的 MON-001 实验残留，
   hosted runner 上恒缺失 → test_02/03 必 FAIL。
2. 跑通 phase3 run 需要 HiPS fixture（p3_session 严格校验 source.hips_dir）。
   本脚本用仓库内 vendored AIO 源自编译 fixture 生成 FIELD.hips（与
   tests/cli/test_monitor_events.py setUpClass 同配方，tests/backend/
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

REPO = Path(__file__).resolve().parents[1]
AIO = REPO / "lib" / "astro_image_io"
OUT_DIR = Path("/tmp/mon001_run_out")
CFG = REPO / "run" / "temp" / "mon001_cfg.json"
HIPS = REPO / "run" / "temp" / "FIELD.hips"


def _run(argv: list[str], **kw) -> subprocess.CompletedProcess:
    kw.setdefault("capture_output", True)
    kw.setdefault("text", True)
    return subprocess.run(argv, **kw)


def build_fixture(tmp: Path) -> None:
    """编译 AIO fixture 并生成 FIELD.hips（配方 = tests/cli/test_phase3_inprocess.py
    setUpClass 的 vendored cfitsio 静态编译 + tests/cli/test_monitor_events.py
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

    srcs = [
        REPO / "tests" / "backend" / "phase2_fixture_main.cpp",
        AIO / "src" / "hips" / "aio_hips_writer.cpp",
        AIO / "src" / "hips" / "aio_hips_reader.cpp",
        AIO / "src" / "aio_fits.cpp",
        AIO / "src" / "aio_api.cpp",
        AIO / "src" / "aio_log.cpp",
        AIO / "src" / "aio_compressor.cpp",
        REPO / "lib" / "common" / "healpix" / "healpix_core.cpp",
    ]
    incs = [
        f"-I{REPO / 'include'}",
        f"-I{AIO / 'include'}",
        f"-I{AIO / 'src'}",
        f"-I{cdir}",
        f"-I{REPO / 'lib' / 'common'}",
        f"-I{REPO / 'lib' / 'common' / 'healpix'}",
    ]
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
    tests/cli/test_monitor_events.py 的 40x30 nearest 小图合成门）。"""
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
            "max_tiles": 64,
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
