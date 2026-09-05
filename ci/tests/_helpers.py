# -*- coding: utf-8 -*-
"""V8-CI-002 单测共享 fixture 工具（owner=SA-CI-32）。

所有测试在 tempfile 临时目录中构建独立 git 仓库并注入小型 checks fixture，
绝不写主工作区 tracked 文件；所有外部命令均带 timeout。
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]   # 主仓库根（只读引用）
RUNNER = REPO / "ci" / "run.py"


def sh(argv: list[str], cwd: Path, timeout: float = 60) -> subprocess.CompletedProcess:
    """运行外部命令（统一 timeout，纪律要求）。"""
    return subprocess.run(argv, cwd=str(cwd), capture_output=True, text=True, timeout=timeout)


def make_repo(root: Path) -> Path:
    """在临时目录构建最小 git 仓库（main 分支，两个 tracked 文件，一次初始提交）。"""
    root.mkdir(parents=True, exist_ok=True)
    sh(["git", "init", "-q", "-b", "main"], cwd=root)
    sh(["git", "config", "user.email", "ci-test@astrocs.invalid"], cwd=root)
    sh(["git", "config", "user.name", "CI Fixture"], cwd=root)
    (root / "A.txt").write_text("A\n", encoding="utf-8")
    (root / "B.txt").write_text("B\n", encoding="utf-8")
    sh(["git", "add", "-A"], cwd=root)
    sh(["git", "commit", "-q", "-m", "init"], cwd=root)
    return root


def write_registry(repo: Path, checks: list[dict], *, with_schema: bool = False) -> Path:
    """向 fixture 仓库写入 ci/checks.json（可选复制主仓库 registry schema 副本）。"""
    reg_dir = repo / "ci"
    reg_dir.mkdir(parents=True, exist_ok=True)
    path = reg_dir / "checks.json"
    path.write_text(
        json.dumps({"schema_version": 1, "checks": checks}, ensure_ascii=False, indent=1),
        encoding="utf-8",
    )
    if with_schema:
        src = REPO / "ci" / "checks.schema.json"
        if src.is_file():
            (reg_dir / "checks.schema.json").write_text(src.read_text(encoding="utf-8"),
                                                        encoding="utf-8")
    return path


def check(**overrides) -> dict:
    """构造一个合法的最小检查条目（字段集与控制包 checks.schema.json 对齐）。"""
    base = {
        "id": "T-OK",
        "profiles": ["fast"],
        "platform": "any",
        "command": ["python3", "-c", "print('ok')"],
        "timeout_seconds": 30,
        "heavy": False,
        "mutates_workspace": False,
        "outputs": [],
        "waivable": False,
        "changed_paths": [],
        "requires_monitor": False,
    }
    base.update(overrides)
    return base


def run_runner(args: list[str], repo: Path, timeout: float = 150) -> subprocess.CompletedProcess:
    """调用 ci/run.py（--repo-root 指向 fixture 仓库）。"""
    return sh([sys.executable, str(RUNNER), "--repo-root", str(repo), *args],
              cwd=repo, timeout=timeout)


def write_ci_result_schema(repo: Path) -> Path:
    """向 fixture 仓库复制主仓库 ci/ci_result.schema.json。

    runner 在 write_outputs 前从 <repo>/ci/ci_result.schema.json 加载结果 schema
    自校验（run.py:1049），fixture 仓库必须带副本；执行模式测试统一调用。
    （V8-CI-002 attempt3 最小扩展：新增函数，不改任何既有签名。）
    """
    reg_dir = repo / "ci"
    reg_dir.mkdir(parents=True, exist_ok=True)
    dst = reg_dir / "ci_result.schema.json"
    dst.write_text((REPO / "ci" / "ci_result.schema.json").read_text(encoding="utf-8"),
                   encoding="utf-8")
    return dst


def load_json(path: Path) -> dict:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def load_check_result(out_root: Path, check_id: str) -> dict:
    return load_json(Path(out_root) / "checks" / f"{check_id}.json")


def load_ci_result(out_root: Path) -> dict:
    return load_json(Path(out_root) / "CI_RESULT.json")
