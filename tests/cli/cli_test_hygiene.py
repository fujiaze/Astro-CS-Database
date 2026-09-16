#!/usr/bin/env python3
"""FIX-UTCLI-HYGIENE: UT-CLI 子进程工作目录统一落 run/（gitignore 面）。

背景（ci/checks.json::UT-CLI 以 mutates_workspace=false 执行）：检查前后对比
`git status --porcelain=v1 --untracked-files=all`，任何在仓库内新增/修改且未被
忽略的文件都是 dirty 违规。UT-CLI 的 CLI/夹具子进程若以仓库根为 cwd，则会把
按 cwd 相对解析的落点写到仓库里：

  * CLI `output_dir` 缺省 `"."`（cli/commands.cpp run 路径）：astrocs_run_*.json、
    run_context.json、resource_samples.csv / resource_summary.json /
    worker_balance.csv（cli/resource_recorder.h）、alloc_samples.csv /
    alloc_report.json（cli/memory_report.h）。
  * lib/infrastructure/aio 写侧日志路径硬编码为相对路径
    `lib/infrastructure/aio/logs/astro_image_io.log`（lib/infrastructure/aio/src/aio_log.cpp）。

  * CLI run 图渲染器路径取 `ASTROCS_REPO`（缺省 `"."`，即 cwd 相对）：
    `<repo>/tools/quality/gen_run_graphs.py`（cli/commands.cpp write_run_graphs）。
    cwd 重定向后该相对路径不可达 → DOT/SVG/L0 静默缺失。这里用产品**既有**环境变量
    开关显式声明仓库根（非改产品）。

本模块只提供**测试侧**的 cwd/env 落点：把子进程 cwd 指到 `run/test_cli_cwd/`（AGENTS.md
工作域目录，`run/*` 全部 gitignore），并用既有 `ASTROCS_REPO` 开关让 cwd 无关的
仓库根定位保持正确。不改产品行为、不放宽/删除任何断言、不使用 waiver/skip/dirty_ignore。
"""
import os

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# CLI 的 run 图渲染器按 ASTROCS_REPO/tools/quality/gen_run_graphs.py 定位（缺省
# "."，cwd 相对）。子进程 cwd 已重定向到 run/，必须显式声明真实仓库根，否则
# RT-009 的 DOT/SVG/L0 产物静默缺失（test_phase123_pipeline.test_08_run_graphs）。
# 该变量是 cli/commands.cpp:600 既有开关，非新增产品面。
os.environ.setdefault("ASTROCS_REPO", REPO)

__all__ = ["REPO", "run_cwd"]


def run_cwd():
    """返回并确保存在 UT-CLI 子进程统一 cwd（run/ 下，gitignore）。"""
    d = os.path.join(REPO, "run", "test_cli_cwd")
    os.makedirs(d, exist_ok=True)
    return d
