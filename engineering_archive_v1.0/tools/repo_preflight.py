#!/usr/bin/env python3
"""Generate a read-only AstroCS repository preflight report.

All external commands have explicit timeouts. The tool does not modify the repo.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from typing import Sequence

COMMAND_TIMEOUT_SECONDS = 30


def run(cmd: Sequence[str], cwd: Path) -> dict:
    try:
        cp = subprocess.run(
            list(cmd), cwd=cwd, text=True, encoding='utf-8', errors='replace',
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=COMMAND_TIMEOUT_SECONDS, check=False,
        )
        return {'cmd': list(cmd), 'returncode': cp.returncode,
                'stdout': cp.stdout.strip(), 'stderr': cp.stderr.strip(), 'timeout': False}
    except subprocess.TimeoutExpired as exc:
        return {'cmd': list(cmd), 'returncode': None,
                'stdout': (exc.stdout or '').strip() if isinstance(exc.stdout, str) else '',
                'stderr': (exc.stderr or '').strip() if isinstance(exc.stderr, str) else '',
                'timeout': True}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--repo', default='.')
    ap.add_argument('--output', required=True)
    args = ap.parse_args()
    repo = Path(args.repo).resolve()
    out = Path(args.output).resolve()
    out.mkdir(parents=True, exist_ok=True)

    commands = {
        'status': run(['git','status','--short','--branch'], repo),
        'head': run(['git','show','-s','--format=%H%n%cI%n%s','HEAD'], repo),
        'count': run(['git','rev-list','--count','HEAD'], repo),
        'tags': run(['git','tag','-l'], repo),
        'remotes': run(['git','remote','-v'], repo),
        'submodules': run(['git','submodule','status'], repo),
        'tracked': run(['git','ls-files'], repo),
    }

    expected = [
        'lib/astro_image_io','lib/calibration','lib/data_pipeline','lib/dynamic_psf',
        'lib/gaia_xpsd_client','lib/healpix_db','lib/orchestrator',
        'lib/photometric_calib','lib/plate_solve','lib/snr_estimator','lib/star_detector',
        'lib/healpix_db/healpix_drizzle','lib/healpix_db/healpix_stack',
    ]
    paths = {p: (repo/p).exists() for p in expected}
    build_files = sorted(str(p.relative_to(repo)) for p in repo.rglob('*') if p.is_file() and (
        p.name in {'Makefile','CMakeLists.txt','pyproject.toml'} or p.suffix.lower() in {'.ps1','.bat'}))
    test_files = sorted(str(p.relative_to(repo)) for p in repo.rglob('*') if p.is_file() and (
        '/test/' in p.as_posix() or '/tests/' in p.as_posix() or p.name.startswith('test_')))
    ci_files = sorted(str(p.relative_to(repo)) for p in repo.rglob('*') if p.is_file() and (
        '/.github/workflows/' in p.as_posix() or p.name in {'.gitlab-ci.yml','azure-pipelines.yml'}))

    report = {
        'repo': str(repo),
        'command_timeout_seconds': COMMAND_TIMEOUT_SECONDS,
        'commands': commands,
        'expected_paths': paths,
        'build_files': build_files,
        'test_files': test_files,
        'ci_files': ci_files,
        'root_cmake': (repo/'CMakeLists.txt').exists(),
        'root_requirements': list(map(str, repo.glob('requirements*.txt'))),
    }
    (out/'preflight.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')

    lines = ['# AstroCS Repository Preflight','',f'- Repo: `{repo}`',
             f'- Command timeout: {COMMAND_TIMEOUT_SECONDS}s','', '## Expected paths']
    for p, ok in paths.items():
        lines.append(f"- [{'x' if ok else ' '}] `{p}`")
    lines += ['', '## Git', '```text', commands['head']['stdout'], commands['status']['stdout'], '```',
              '', f"- Tracked files: {len(commands['tracked']['stdout'].splitlines()) if commands['tracked']['returncode']==0 else 'ERROR'}",
              f"- Tags: {len(commands['tags']['stdout'].splitlines()) if commands['tags']['stdout'] else 0}",
              f"- Build files: {len(build_files)}", f"- Test files: {len(test_files)}",
              f"- CI files: {len(ci_files)}", f"- Root CMake: {report['root_cmake']}",
              '', '## Blocking findings']
    blocking = [p for p in ['lib/healpix_db/healpix_drizzle','lib/healpix_db/healpix_stack'] if not paths[p]]
    if blocking:
        lines += [f'- Missing required source: `{p}`' for p in blocking]
    else:
        lines.append('- None detected by this preflight.')
    (out/'preflight.md').write_text('\n'.join(lines)+'\n', encoding='utf-8')

    hashes = []
    for p in [out/'preflight.json', out/'preflight.md']:
        hashes.append(f'{sha256(p)}  {p.name}')
    (out/'artifacts.sha256').write_text('\n'.join(hashes)+'\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
