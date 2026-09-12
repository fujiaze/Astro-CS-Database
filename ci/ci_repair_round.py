#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/ci_repair_round.py — CI 修复常驻线轮报器（CI-REPAIR-001）。

职责（控制包 06 §2「CI 修复线（常驻）」+ §6.1「上一轮 GitHub Checks 全量回拉结果
（含失败 job 日志摘录）与修复映射」）：

  1. 回拉指定 SHA 的 GitHub Checks API 全量结果（check-runs 分页）；
  2. 回拉 Actions runs / jobs（含每 step 结论）并下载**失败 job 的日志**摘录；
  3. 收集 run/ci/monitor/*.json（heavy 检查的资源监控证据）并登记 sha256；
  4. 生成 <round>/FINDINGS.json：失败 job/检查 + 日志摘录 + 归因域 + 承接节点 +
     白名单内可修判定 + 最小修复映射（域外红灯登记 finding，不顺手修）。

凭据纪律：token 仅从环境 GITHUB_TOKEN/GH_TOKEN 或 ~/.git-credentials 读取，只进
Authorization 头，绝不落盘/回显；所有对外请求显式 timeout。

fail-closed（宁缺勿假）：
  * 未登记归因域的红灯 → domain=UNKNOWN + needs_mapping=true（--strict 下 exit 3）；
  * 失败 job 日志取证失败 → log_status=unavailable（--strict 下 exit 3），绝不静默丢弃；
  * 存在未完成（in_progress/queued）run → exit 4，绝不判 GREEN；
  * API 4xx/5xx 或 SHA 不可读 → exit 2，且**不写任何 FINDINGS.json**（杜绝伪造绿灯面）。

exit: 0 全绿; 1 有红灯; 2 输入/网络/API 错误; 3 --strict 下报告取证不完整; 4 有未完成 run。
优先级（显式）：2 > 4 > 3 > 1 > 0。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import sys
import urllib.error
import urllib.parse
import urllib.request

SCHEMA_VERSION = 1
SCHEMA_ID = "cprun/ci-repair-findings-v1"
DEFAULT_API = "https://api.github.com"
RED_CONCLUSIONS = ("failure", "timed_out", "cancelled", "action_required",
                   "startup_failure", "stale")
OK_CONCLUSIONS = ("success", "neutral", "skipped")
WHITELIST_PREFIXES = ("ci/", "tools/quality/", "run/ci_repair/")
EXCERPT_MARKERS = ("##[error]", "Error", "error:", "FAIL", "AssertionError",
                   "失败", "rc=1", "Traceback", "CMake Error", "ninja: error")
EXCERPT_MAX_LINES = 40
EXCERPT_MAX_BYTES = 4000
LOG_MAX_BYTES = 262144

# ---------------------------------------------------------------- 归因表 ----
# 红灯 → 归因域 / 承接节点 / 根因 / 白名单内可修判定。
# root_cause_path 是**根因所在文件**（不是检查器所在文件）：CI-REPAIR-001 的
# 写入白名单只有 run/ci_repair、ci/、tools/quality，凡根因落在这三域之外的
# 红灯一律 in_whitelist=false → 登记 finding 交域主，禁止就地放宽检查器。
ATTRIBUTION = {
    "AGENTS-GOV": {
        "domain": "GOV",
        "owner_node": "GOV-001|GOV-002",
        "root_cause": "AGENTS.md 瘦身（V5 治理要素迁入冻结宪章）后未被 tools/check_agents_gov.py 的 10 项关键词断言同步",
        "root_cause_path": "AGENTS.md",
        "in_whitelist": False,
        "minimal_patch": "在 AGENTS.md 恢复/等价重述 10 项治理要素（main-only/amd64/节点/cpu-only/单入口/资源门禁/无硬编码/alpha-发布/状态机/不停工），或由负责人裁决将该断言改为引用冻结宪章条目",
    },
    "DATA-ARTIFACTS": {
        "domain": "DATA",
        "owner_node": "DATA-001",
        "root_cause": "docs/contracts/DATA_SEMANTICS.md 声明的 DATA-HIPS-001/DATA-TILE-001 未登记进 DATA_ARTIFACTS.md 的 schema 清单",
        "root_cause_path": "docs/contracts/DATA_ARTIFACTS.md",
        "in_whitelist": False,
        "minimal_patch": "在 docs/contracts/DATA_ARTIFACTS.md 首表补 DATA-HIPS-001/DATA-TILE-001 两行（scalar/shape/unit/coordinate/invalid/ownership/serialization 八列齐备）",
    },
    "THREAD-BUDGET": {
        "domain": "ARCH",
        "owner_node": "RT-001A|RT-001B",
        "root_cause": "lib/{cosmetic,drizzle,calibration}/src/module_entry.cpp 的 *_OMP_SET 宏经 omp_set_num_threads 注入，未登记进 tools/arch/check_thread_budget.py REGISTERED",
        "root_cause_path": "lib/cosmetic/src/module_entry.cpp",
        "in_whitelist": False,
        "minimal_patch": "三处宏是 host budget 注入形态（非硬编码字面量）：登记进 tools/arch/check_thread_budget.py REGISTERED（键=module_entry.cpp 不可，须逐文件路径键），或按 ARCH-004 改为经 host callback 注入",
    },
    "CON-COMMENTS": {
        "domain": "CON",
        "owner_node": "（待派：lib 测试面注释锚）",
        "root_cause": "lib/{astro_image_io/tests/p1hips,phase1_session/tests/p1sess,calibration/tests/p1cal} 4 文件注释提及不变量但无 SCI-/ALG- ID（checker 规则本身合规，禁放宽）",
        "root_cause_path": "lib/astro_image_io/tests/p1hips/p1hips_tests_properties.cpp",
        "in_whitelist": False,
        "minimal_patch": "在 4 个文件的对应不变量注释补 SCI/ALG ID 引用（如 HIPS_WRITER §9 不变量行补 SCI-HIPS-xxx），不得改 tools/quality/contracts/check_comments.py",
    },
    "CON-FULL-INTEGRATION": {
        "domain": "CON",
        "owner_node": "（随 CON-COMMENTS 收敛）",
        "root_cause": "check_full_integration 聚合到 check_comments rc=1 → INTEG-P1-FAIL（纯级联，无独立根因）",
        "root_cause_path": "tools/quality/contracts/check_full_integration.py",
        "in_whitelist": False,
        "minimal_patch": "CON-COMMENTS 转绿后自动转绿；禁以跳过聚合项方式转绿",
    },
    "UT-ARCH": {
        "domain": "ARCH",
        "owner_node": "ARCH-AUDIT-P1",
        "root_cause": "tests/arch 3 项失败：thread budget 2 项（同 THREAD-BUDGET 根因）+ test_inventory 幂等（docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv 落后于 tools/arch/build_production_execution_inventory.py 生成器）",
        "root_cause_path": "docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv",
        "in_whitelist": False,
        "minimal_patch": "用 tools/arch/build_production_execution_inventory.py 重新生成并提交 CSV（含 p1hips_tests 等新 target 行）",
    },
    "UT-BACKEND": {
        "domain": "SCI",
        "owner_node": "BASE-UTIL-001|SCI-F2-001|SCI-F3-001",
        "root_cause": "tests/backend 多项后端面失败：calibration oracle 10x 偏差（CAL0/CAL1、MASTER_FLAT[0]=0.0）、phase2 integrate weight_mode=2 缺 per-frame ivar（DATA-UNC-001 §30.1 拒绝路径）、p2003 UPM persist 缺失、p2006/p2007/p3006 resource gate utilization_p75_low 与 resource 事件缺失",
        "root_cause_path": "lib/calibration/",
        "in_whitelist": False,
        "minimal_patch": "按冻结宪章 §10.5 四类归因逐项落到域主任务（BASE-UTIL-001 已在诊断 utilization_p75_low；calibration oracle 10x 偏差与 ivar 缺失须独立域任务）",
    },
    "UT-CLI": {
        "domain": "CLI",
        "owner_node": "UT-CLI-MAINT",
        "root_cause": "mutates_workspace=false 的检查改写了工作区：astrocs graph 落 graph/*.json、memory-report 落 alloc_report.json/alloc_samples.csv，目标目录解析为 CWD(=仓库根)",
        "root_cause_path": "cli/commands.cpp",
        "in_whitelist": False,
        "minimal_patch": "cli 侧把运行产物默认落 run/resource/（AGENTS.md 目录规范），或 tests/cli 显式传 --out-dir run/...；禁以 dirty_ignore 登记掩盖「运行产物落根目录」违规",
    },
    "WIN-BUILD-RELEASE": {
        "domain": "BUILD",
        "owner_node": "CI-002|WIN-000",
        "root_cause": "lib/gaia_xpsd_client/CMakeLists.txt:68 find_package(ZLIB REQUIRED) 只消费 ACS_ZLIB_ROOT，未消费 runner 注入的 ACS_ZLIB_LIB=zs.lib → ZLIB_LIBRARY 未命中（found version 1.3.2, missing ZLIB_LIBRARY）",
        "root_cause_path": "lib/gaia_xpsd_client/CMakeLists.txt",
        "in_whitelist": False,
        "minimal_patch": "按根 CMakeLists astrocs_cfitsio 段 HOSTFIX-23④ 同款，把 $ENV{ACS_ZLIB_LIB} 直连进 ZLIB_LIBRARY（set(ZLIB_LIBRARY \"$ENV{ACS_ZLIB_ROOT}/lib/$ENV{ACS_ZLIB_LIB}\" CACHE FILEPATH ...)）；Linux env 空时行为零变化",
    },
    "WIN-PACKAGE-CANDIDATE": {
        "domain": "BUILD",
        "owner_node": "CI-002|WIN-000",
        "root_cause": "WIN-BUILD-RELEASE 级联：configure 失败 → build 树不存在 → install 阶段立即 exit（0.2s）",
        "root_cause_path": "lib/gaia_xpsd_client/CMakeLists.txt",
        "in_whitelist": False,
        "minimal_patch": "随 WIN-BUILD-RELEASE 根因修复自动转绿",
    },
    # job 级（workflow job 名）——job 红灯是 profile 内检查项红灯的聚合
    "linux": {
        "domain": "CI-PLATFORM",
        "owner_node": "CI-REPAIR-001（本线：聚合与映射）",
        "root_cause": "job 级红灯 = linux-main profile 内检查项红灯聚合（无独立根因）",
        "root_cause_path": "ci/run.py",
        "in_whitelist": False,
        "minimal_patch": "随 profile 内红灯逐项收敛；本线只负责轮报与映射（聚合项本身不可单独修复）",
    },
    "windows": {
        "domain": "CI-PLATFORM",
        "owner_node": "CI-REPAIR-001（本线：聚合与映射）",
        "root_cause": "job 级红灯 = windows-main profile 内检查项红灯聚合（无独立根因）",
        "root_cause_path": "ci/run.py",
        "in_whitelist": False,
        "minimal_patch": "随 profile 内红灯逐项收敛；本线只负责轮报与映射（聚合项本身不可单独修复）",
    },
    "Select Fatduck candidate (GitHub-hosted)": {
        "domain": "CI-PLATFORM",
        "owner_node": "CI-001B|WIN-000",
        "root_cause": "计划任务选取 Fatduck 候选失败（self-hosted 节点不可用/无合格候选 run）",
        "root_cause_path": ".github/workflows/fatduck.yml",
        "in_whitelist": False,
        "minimal_patch": "按 CI-001B 收敛 workflow 与注册表联动（域外：.github/）",
    },
}


class RepairError(RuntimeError):
    """输入/网络/API 级错误 → exit 2（fail-closed，不产出 FINDINGS）。"""


# ------------------------------------------------------------------ 工具 -----
def utc_now() -> str:
    from datetime import datetime, timezone
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def read_token(explicit_env: str = "GITHUB_TOKEN") -> str:
    """token 只从环境或 ~/.git-credentials 读取；绝不落盘、绝不回显。"""
    for name in (explicit_env, "GH_TOKEN"):
        val = (os.environ.get(name) or "").strip()
        if val:
            return val
    cred = pathlib.Path.home() / ".git-credentials"
    if cred.is_file():
        for line in cred.read_text(encoding="utf-8", errors="ignore").splitlines():
            m = re.match(r"https://([^:]*):([^@]*)@github\.com", line.strip())
            if m:
                return m.group(2)
    return ""


def scrub(text: str, token: str) -> str:
    if token and token in text:
        text = text.replace(token, "***")
    return text


class _CrossHostStripAuth(urllib.request.HTTPRedirectHandler):
    """重定向到其它主机（如 job 日志 → Azure Blob 签名 URL）时剥离 Authorization。

    否则签名 URL 收到 Bearer token 会以 401 InvalidAuthenticationInfo 拒绝
    （实证：/actions/jobs/<id>/logs 302 → *.blob.core.windows.net 401）。
    """

    def redirect_request(self, req, fp, code, msg, headers, newurl):  # noqa: D102
        new = super().redirect_request(req, fp, code, msg, headers, newurl)
        if new is None:
            return None
        try:
            if urllib.parse.urlparse(newurl).hostname != urllib.parse.urlparse(req.full_url).hostname:
                return urllib.request.Request(
                    newurl, headers={k: v for k, v in req.headers.items()
                                     if k.lower() != "authorization"},
                    method="GET")
        except Exception:                                # noqa: BLE001
            return new
        return new


_OPENER = urllib.request.build_opener(_CrossHostStripAuth)


def api_get(url: str, token: str, timeout: float) -> tuple[int, bytes, dict]:
    req = urllib.request.Request(url, headers={
        "Accept": "application/vnd.github+json",
        "User-Agent": "astrocs-ci-repair-round",
        "X-GitHub-Api-Version": "2022-11-28",
    })
    if token:
        req.add_header("Authorization", "Bearer " + token)
    try:
        with _OPENER.open(req, timeout=timeout) as resp:
            return resp.status, resp.read(LOG_MAX_BYTES), dict(resp.headers)
    except urllib.error.HTTPError as exc:            # 4xx/5xx：交由调用方判定
        body = b""
        try:
            body = exc.read(4096)
        except Exception:                            # noqa: BLE001
            pass
        return exc.code, body, dict(exc.headers or {})
    except Exception as exc:                         # noqa: BLE001
        raise RepairError("网络/超时错误: %s: %s" % (url, exc)) from exc


def api_json(url: str, token: str, timeout: float) -> object:
    status, body, _ = api_get(url, token, timeout)
    if status != 200:
        snippet = scrub(body[:200].decode("utf-8", "replace"), token)
        raise RepairError("API %s → HTTP %d %s" % (url, status, snippet))
    try:
        return json.loads(body.decode("utf-8"))
    except Exception as exc:                         # noqa: BLE001
        raise RepairError("API %s → 非 JSON 响应: %s" % (url, exc)) from exc


def paginate(base: str, key: str, token: str, timeout: float, max_pages: int = 10) -> list:
    """逐页拉取 {base}?per_page=100&page=N，直到不足一页或达上限。"""
    items: list = []
    for page in range(1, max_pages + 1):
        sep = "&" if "?" in base else "?"
        doc = api_json("%s%sper_page=100&page=%d" % (base, sep, page), token, timeout)
        chunk = doc.get(key) or []
        items.extend(chunk)
        if len(chunk) < 100:
            break
    return items


def is_red(conclusion) -> bool:
    return str(conclusion or "").lower() in RED_CONCLUSIONS


def excerpt(text: str, token: str) -> list:
    """失败日志摘录：优先错误标记行及其上下文，封顶行数/字节。"""
    lines = scrub(text, token).splitlines()
    picked: list[int] = []
    for idx, line in enumerate(lines):
        if any(m in line for marker in EXCERPT_MARKERS for m in (marker,)):
            picked.extend(range(max(0, idx - 1), min(len(lines), idx + 2)))
    if not picked:
        picked = list(range(max(0, len(lines) - EXCERPT_MAX_LINES), len(lines)))
    out, seen, size = [], set(), 0
    for idx in picked:
        if idx in seen:
            continue
        seen.add(idx)
        line = lines[idx]
        size += len(line.encode("utf-8")) + 1
        if size > EXCERPT_MAX_BYTES or len(out) >= EXCERPT_MAX_LINES:
            out.append("…（摘录封顶，完整日志见 raw/）")
            break
        out.append(line)
    return out


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def write_json(path: pathlib.Path, doc: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(doc, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
                    encoding="utf-8")


def attribute(name: str) -> dict:
    entry = ATTRIBUTION.get(name)
    if entry is None:
        return {
            "domain": "UNKNOWN",
            "owner_node": None,
            "root_cause": "未登记归因域（红灯必须显式映射后才可派发）",
            "root_cause_path": None,
            "in_whitelist": False,
            "minimal_patch": "在 ci/ci_repair_round.py ATTRIBUTION 登记域名/承接节点/根因路径",
            "needs_mapping": True,
        }
    out = dict(entry)
    out["needs_mapping"] = False
    return out


def monitor_evidence(monitor_dir: pathlib.Path, out_dir: pathlib.Path) -> list:
    rows = []
    if not monitor_dir.is_dir():
        return rows
    dest = out_dir / "monitor"
    dest.mkdir(parents=True, exist_ok=True)
    for src in sorted(monitor_dir.glob("*.json")):
        target = dest / src.name
        shutil.copyfile(src, target)
        doc = {}
        try:
            doc = json.loads(src.read_text(encoding="utf-8", errors="replace"))
        except Exception:                            # noqa: BLE001
            doc = {}
        rows.append({
            "name": src.name,
            "source": str(src).replace(os.sep, "/"),
            "path": str(target).replace(os.sep, "/"),
            "sha256": sha256_file(src),
            "cpu_samples": len(doc.get("cpu_samples") or []),
            "verdict": (doc.get("frozen_gate") or {}).get("verdict"),
        })
    return rows


def _read_ci_result(spec: str) -> tuple[dict, dict]:
    """读取 CI_RESULT.json（支持 .json 或 CI 上传的 artifact .zip）。

    返回 (doc, logs)，logs 为 {"<CHECK-ID>": 文本}。
    """
    path = pathlib.Path(spec)
    if not path.is_file():
        raise RepairError("--ci-result 不存在: %s" % spec)
    if path.suffix.lower() == ".zip":
        import zipfile
        with zipfile.ZipFile(path) as z:
            names = z.namelist()
            target = sorted(n for n in names if n.endswith("CI_RESULT.json"))
            if not target:
                raise RepairError("%s 内无 CI_RESULT.json" % spec)
            doc = json.loads(z.read(target[0]).decode("utf-8", "replace"))
            base = target[0].rsplit("/", 1)[0]
            logs = {}
            for name in names:
                if name.startswith(base + "/logs/") and name.endswith(".log"):
                    logs[name.rsplit("/", 1)[1][:-4]] = z.read(name).decode("utf-8", "replace")
        return doc, logs
    doc = json.loads(path.read_text(encoding="utf-8", errors="replace"))
    logs = {}
    logdir = path.parent / "logs"
    if logdir.is_dir():
        for item in sorted(logdir.glob("*.log")):
            logs[item.stem] = item.read_text(encoding="utf-8", errors="replace")
    return doc, logs


def ingest_ci_result(spec: str, sha: str, token: str) -> tuple[list, dict]:
    """把一份 CI_RESULT.json 内的**检查项**红灯并入 findings（fail-closed）。

    * source_sha 与目标 SHA 不一致 → RepairError（防跨轮证据污染 / 防伪造绿灯）；
    * 逐项 verdict != PASS 一律登记，绝不静默丢弃；
    * 有同名日志则附摘录（CI 侧 logs/<ID>.log）。
    """
    doc, logs = _read_ci_result(spec)
    got = str(doc.get("source_sha") or "")
    if got != sha:
        raise RepairError("--ci-result %s 的 source_sha=%s 与目标 SHA=%s 不一致（拒绝跨轮证据）"
                          % (spec, got[:12] or "<空>", sha[:12]))
    reds, ok = [], 0
    for chk in doc.get("checks") or []:
        verdict = str(chk.get("verdict"))
        if verdict == "PASS":
            ok += 1
            continue
        cid = str(chk.get("id"))
        entry = {
            "kind": "check",
            "name": cid,
            "workflow": doc.get("profile"),
            "profile": doc.get("profile"),
            "run_id": doc.get("run_id"),
            "job_id": None,
            "conclusion": verdict,
            "url": None,
            "steps_failed": [],
            "exit_code": chk.get("exit_code"),
            "reason": chk.get("reason"),
            "dirty_violations": chk.get("dirty_violations") or [],
            "evidence": spec,
            "log_status": "unavailable",
            "log_excerpt": None,
            "log_path": None,
        }
        entry.update(attribute(cid))
        text = logs.get(cid)
        if text:
            entry["log_status"] = "excerpt"
            entry["log_excerpt"] = excerpt(text, token)
        else:
            entry["log_error"] = "artifact 内无 logs/%s.log" % cid
        reds.append(entry)
    meta = {"evidence": spec, "profile": doc.get("profile"), "run_id": doc.get("run_id"),
            "verdict": doc.get("verdict"), "passed_checks": ok, "red_checks": len(reds)}
    return reds, meta


# ------------------------------------------------------------------ 主流程 ---
def collect(args, token: str) -> dict:
    repo, sha = args.repo, args.sha
    out = pathlib.Path(args.round_dir)
    raw = out / "raw"
    raw.mkdir(parents=True, exist_ok=True)
    enc = urllib.parse.quote(repo, safe="/")
    api = args.api_base.rstrip("/")

    check_runs = paginate("%s/repos/%s/commits/%s/check-runs" % (api, enc, sha),
                          "check_runs", token, args.timeout)
    write_json(raw / "check-runs.json", {"total_count": len(check_runs), "check_runs": check_runs})

    runs = paginate("%s/repos/%s/actions/runs?head_sha=%s" % (api, enc, sha),
                    "workflow_runs", token, args.timeout)
    write_json(raw / "runs.json", {"total_count": len(runs), "workflow_runs": runs})

    jobs_by_run, reds, gaps = {}, [], 0
    incomplete = [r for r in runs if str(r.get("status")) != "completed"]
    for run in sorted(runs, key=lambda r: r.get("id") or 0):
        rid = run.get("id")
        jobs = paginate("%s/repos/%s/actions/runs/%s/jobs" % (api, enc, rid),
                        "jobs", token, args.timeout)
        write_json(raw / ("jobs-%s.json" % rid), {"total_count": len(jobs), "jobs": jobs})
        jobs_by_run[str(rid)] = jobs
        for job in sorted(jobs, key=lambda j: j.get("id") or 0):
            name = str(job.get("name") or "")
            concl = job.get("conclusion")
            if str(job.get("status")) != "completed" or not is_red(concl):
                continue
            jid = job.get("id")
            entry = {
                "kind": "job",
                "name": name,
                "workflow": run.get("name"),
                "run_id": rid,
                "run_event": run.get("event"),
                "job_id": jid,
                "conclusion": concl,
                "url": job.get("html_url"),
                "steps_failed": [s.get("name") for s in (job.get("steps") or [])
                                 if is_red(s.get("conclusion"))],
                "log_status": "unavailable",
                "log_excerpt": None,
                "log_path": None,
            }
            entry.update(attribute(name))
            status, body, _ = api_get("%s/repos/%s/actions/jobs/%s/logs" % (api, enc, jid),
                                      token, args.timeout)
            if status == 200 and body:
                log_path = raw / ("log-%s.txt" % jid)
                log_path.write_bytes(body[:LOG_MAX_BYTES])
                entry["log_status"] = "excerpt"
                entry["log_path"] = str(log_path).replace(os.sep, "/")
                entry["log_excerpt"] = excerpt(body.decode("utf-8", "replace"), token)
            else:
                entry["log_status"] = "unavailable"
                entry["log_error"] = "HTTP %d" % status
                gaps += 1
            if entry["needs_mapping"]:
                gaps += 1
            reds.append(entry)

    for cr in sorted(check_runs, key=lambda c: c.get("id") or 0):
        if str(cr.get("status")) != "completed" or not is_red(cr.get("conclusion")):
            continue
        entry = {
            "kind": "check_run",
            "name": str(cr.get("name") or ""),
            "workflow": ((cr.get("check_suite") or {}).get("app") or {}).get("name"),
            "run_id": None,
            "job_id": cr.get("id"),
            "conclusion": cr.get("conclusion"),
            "url": cr.get("details_url") or cr.get("html_url"),
            "steps_failed": [],
            "log_status": "not_available_for_check_run",
            "log_excerpt": None,
            "log_path": None,
        }
        entry.update(attribute(entry["name"]))
        if entry["needs_mapping"]:
            gaps += 1
        reds.append(entry)

    ci_results = []
    for spec in (args.ci_result or []):
        extra_reds, meta = ingest_ci_result(spec, sha, token)
        ci_results.append(meta)
        reds.extend(extra_reds)

    # job 级红灯 = 同 profile 内检查项红灯的聚合：显式建立链接（不重复计数）
    for red in reds:
        if red["kind"] != "job":
            continue
        prof = {"linux": "linux-main", "windows": "windows-main"}.get(red["name"])
        red["aggregates"] = sorted(r["name"] for r in reds
                                   if r["kind"] == "check" and r.get("profile") == prof)

    reds.sort(key=lambda r: (r["kind"], r["name"], str(r.get("job_id"))))

    for red in reds:
        if red["kind"] == "check" and red["log_status"] != "excerpt":
            gaps += 1

    monitor = monitor_evidence(pathlib.Path(args.monitor_dir), out)
    if not monitor:
        gaps += 1

    in_whitelist = [r for r in reds if r.get("in_whitelist")]
    findings = {
        "schema_version": SCHEMA_VERSION,
        "schema": SCHEMA_ID,
        "task": "CI-REPAIR-001",
        "generated_utc": args.now or utc_now(),
        "repo": repo,
        "sha": sha,
        "api_base": api,
        "verdict": "RED" if reds else ("INCOMPLETE" if incomplete else "GREEN"),
        "counters": {
            "check_runs": len(check_runs),
            "runs": len(runs),
            "jobs": sum(len(v) for v in jobs_by_run.values()),
            "reds": len(reds),
            "red_checks": len([r for r in reds if r["kind"] == "check_run"]),
            "red_jobs": len([r for r in reds if r["kind"] == "job"]),
            "red_checks_internal": len([r for r in reds if r["kind"] == "check"]),
            "ci_results_ingested": len(ci_results),
            "incomplete_runs": len(incomplete),
            "gaps": gaps,
        },
        "scope": {
            "whitelist_prefixes": list(WHITELIST_PREFIXES),
            "repairable_in_whitelist": len(in_whitelist),
            "out_of_whitelist": len(reds) - len(in_whitelist),
            "in_whitelist_reds": sorted(r["name"] for r in in_whitelist),
        },
        "domains": _domain_rollup(reds),
        "reds": reds,
        "incomplete_runs": sorted([{"id": r.get("id"), "name": r.get("name"),
                                    "status": r.get("status"), "url": r.get("html_url")}
                                   for r in incomplete], key=lambda d: str(d["id"])),
        "monitor": monitor,
        "ci_results": ci_results,
        "notes": [
            "归因域为「根因所在域」，与检查器所在目录无必然关系；in_whitelist 判定按 CI-REPAIR-001 写入白名单（run/ci_repair、ci/、tools/quality）。",
            "域外红灯只登记 finding（控制包 CI-REPAIR-001 禁令：不顺手修复域外问题）。",
            "禁以放宽检查器/登记 dirty_ignore 掩盖红灯；转绿必须由根因域任务按 minimal_patch 落地。",
        ],
    }
    write_json(out / "PULL.json", {
        "schema_version": SCHEMA_VERSION,
        "generated_utc": findings["generated_utc"],
        "repo": repo, "sha": sha, "api_base": api,
        "check_runs": [{"id": c.get("id"), "name": c.get("name"), "status": c.get("status"),
                        "conclusion": c.get("conclusion")} for c in sorted(check_runs, key=lambda c: c.get("id") or 0)],
        "runs": [{"id": r.get("id"), "name": r.get("name"), "event": r.get("event"),
                  "status": r.get("status"), "conclusion": r.get("conclusion")}
                 for r in sorted(runs, key=lambda r: r.get("id") or 0)],
        "jobs": {k: [{"id": j.get("id"), "name": j.get("name"), "status": j.get("status"),
                      "conclusion": j.get("conclusion")} for j in sorted(v, key=lambda j: j.get("id") or 0)]
                 for k, v in sorted(jobs_by_run.items())},
        "monitor": monitor,
    })
    write_json(out / "FINDINGS.json", findings)
    return findings


def _domain_rollup(reds: list) -> list:
    buckets: dict[str, dict] = {}
    for red in reds:
        key = red.get("domain") or "UNKNOWN"
        b = buckets.setdefault(key, {"domain": key, "reds": 0, "names": [], "owner_nodes": []})
        b["reds"] += 1
        b["names"].append(red["name"])
        node = red.get("owner_node")
        if node and node not in b["owner_nodes"]:
            b["owner_nodes"].append(node)
    out = []
    for key in sorted(buckets):
        b = buckets[key]
        b["names"] = sorted(b["names"])
        b["owner_nodes"] = sorted(b["owner_nodes"])
        out.append(b)
    return out


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(description="CI 修复常驻线轮报器（CI-REPAIR-001）")
    ap.add_argument("--repo", required=True, help="owner/name")
    ap.add_argument("--sha", required=True, help="40 hex source SHA")
    ap.add_argument("--round-dir", required=True)
    ap.add_argument("--monitor-dir", default="run/ci/monitor")
    ap.add_argument("--api-base", default=DEFAULT_API)
    ap.add_argument("--timeout", type=float, default=30.0)
    ap.add_argument("--token-env", default="GITHUB_TOKEN")
    ap.add_argument("--now", default=None, help="固定生成时间（确定性复现/回归）")
    ap.add_argument("--ci-result", action="append", default=[], metavar="PATH",
                    help="CI_RESULT.json 或 CI artifact .zip（可多次；source_sha 必须与 --sha 一致）")
    ap.add_argument("--strict", action="store_true",
                    help="取证缺口（未登记归因域/日志不可得/无监控证据）→ exit 3")
    return ap


def main(argv: list | None = None) -> int:
    args = build_parser().parse_args(argv)
    if not re.fullmatch(r"[0-9a-f]{7,40}", args.sha or ""):
        print("ci_repair_round: --sha 非法（需 7-40 位十六进制）", file=sys.stderr)
        return 2
    host = urllib.parse.urlparse(args.api_base).hostname or ""
    if urllib.parse.urlparse(args.api_base).scheme != "https" and host not in ("127.0.0.1", "localhost", "::1"):
        print("ci_repair_round: --api-base 仅允许 https（或本地回环 fixture）", file=sys.stderr)
        return 2
    token = read_token(args.token_env)
    try:
        findings = collect(args, token)
    except RepairError as exc:
        print("ci_repair_round: %s" % scrub(str(exc), token), file=sys.stderr)
        return 2
    c = findings["counters"]
    print("verdict=%s sha=%s reds=%d (checks=%d jobs=%d) incomplete_runs=%d gaps=%d"
          % (findings["verdict"], args.sha[:12], c["reds"], c["red_checks"], c["red_jobs"],
             c["incomplete_runs"], c["gaps"]))
    print("report: %s" % (pathlib.Path(args.round_dir) / "FINDINGS.json"))
    if c["incomplete_runs"]:
        return 4
    if args.strict and c["gaps"]:
        return 3
    return 1 if c["reds"] else 0


if __name__ == "__main__":
    sys.exit(main())
