#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/validate_workflow_binding.py — workflow 与 ci/checks.json 联动核死校验器（CI-001B）。

用法:
    python3 ci/validate_workflow_binding.py [--repo-root DIR] [--manifest P]
                                            [--registry P] [--lock P] [--json] [--out P]

核死内容（任一违规 exit 1；全部通过 exit 0）:

  B1  绑定声明可解析、schema_version==1、step_id 唯一；
  B2  声明与 workflow **双向逐 step 对应**：workflow 里多一步 = unbound_step，
      声明里多一条 = stale_binding，顺序漂移 = step_order_drift，job 缺失 =
      job_missing，YAML 不可解析 = workflow_unreadable；
  B3  role 必须在封闭集合内（unknown_role）；
  B4  pinned-action：必须有 uses、无 run、uses 锁到 ci/actions.lock.json 的完整 SHA；
  B5  artifact-upload：uses 必须是 upload-artifact 且锁 SHA、path 非空、
      if-no-files-found 已声明、if 已显式声明（always()/success()）；
  B6  ci-entrypoint：run 体与声明 exec_body 逐字（空白正常化）相等
      （entrypoint_body_drift）；非声明入口的仓库脚本/构建命令出现在体里即
      business_command_in_workflow；
  B7  host-provisioning：体（去注释行后）不得出现仓库路径 token 或构建工具
      （cmake/ctest/ninja/msbuild/gcc/g++/clang++/make）：business_command_in_workflow；
      且必须至少声明一个 provisioning 动词；
  B8  registry-runner：体必须逐字等于 <python|python3> ci/run.py --profile <profile_expr>；
      字面 profile 必须在注册表内；表达式 profile 必须绑定到声明里 provides_profile 的
      select_profile 步；触发面（workflow_dispatch choices）必须全部是注册表 profile；
  B9  registry-bound-step / infra-step：run 体必须逐字等于
      <python|python3> ci/wf_step.py --step <STEP_ID>；exec 非空、exec[0] 合法、
      exec 引用的 ci/ 脚本必须存在（exec_target_missing / exec_not_under_ci）；
  B10 注册表交叉核死：binds_check / serves_checks 必须在注册表内且 waivable=false
      （bound_check_not_registered / bound_check_waivable / serves_check_not_registered /
      serves_check_waivable）；require_outputs 必须是 binds_check.outputs 的子集
      （output_contract_drift）；
  B11 fail-closed：绑定不可豁免检查的步必须 if: always()（nonwaivable_step_skippable），
      且体/条件里不得出现 ::warning:: 跳过路径（warning_skip_branch）；
      任何 run 体出现 ::warning:: 且同时引用注册表 outputs 路径 = warning_skip_branch_on_registered_output；
  B12 反复制：注册表检查命令引用的仓库脚本 token、以及声明里 exec 的脚本 token，
      不得再出现在任何 workflow run 体内（registry_command_copied_into_workflow）；
  B13 actions 锁：全部 uses 必须锁到锁文件内完整 SHA（sha_not_locked），
      锁文件结构非法 = lock_invalid；
  B14 挂起/取代保护面：concurrency.group 必须含 github.ref 与 github.event_name
      （concurrency_group_weak）、cancel-in-progress 必须显式布尔、job 必须声明
      timeout-minutes（job_timeout_missing）；
  B15 挂账可见：声明里的 pending_registration 逐条进 notices（不计错，但绝不静默）；
  B16 范围外 workflow 文件（如 fatduck.yml）逐条进 notices，但仍参与 B12 反复制扫描。

输出: stdout 一份 JSON 摘要（--json 时单行）。
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
DEFAULT_MANIFEST = Path(__file__).resolve().parent / "workflow_binding.json"
DEFAULT_REGISTRY = Path(__file__).resolve().parent / "checks.json"
DEFAULT_LOCK = Path(__file__).resolve().parent / "actions.lock.json"
DEFAULT_WORKFLOW_DIR = ".github/workflows"

ALLOWED_ROLES = (
    "pinned-action",
    "ci-entrypoint",
    "host-provisioning",
    "registry-runner",
    "registry-bound-step",
    "infra-step",
    "artifact-upload",
    "check-body",
)
EXECUTABLE_ROLES = ("registry-bound-step", "infra-step", "check-body")
CHECK_BODY_ROLE = "check-body"
DISPATCH_RECORD = "run/ci/wf_step/"
DISPATCHER = "ci/wf_step.py"
RUNNER = "ci/run.py"
CI_ENTRYPOINTS = ("ci/bootstrap.py", "ci/select_profile.py")
RUNNERS = ("python", "python3")
DOLLAR_BRACE = "$" + "{{"
PROFILE_EXPR_RE = re.compile(
    "^" + re.escape(DOLLAR_BRACE) + r"\s*steps\.([A-Za-z0-9_-]+)\.outputs\.profile\s*"
    + re.escape("}}") + "$")
SHA_RE = re.compile(r"^[0-9a-f]{40}$")
BUILD_TOOL_RE = re.compile(
    r"(?<![\w.-])(cmake|ctest|ninja|msbuild|gcc|g\+\+|clang\+\+|make)(?![\w.-])")
REPO_PATH_TOKENS = (
    "ci/", "tools/", "tests/", "lib/", "cli/", "cmake/", "scripts/", "packaging/",
    "contracts/", "providers/", "runtime/", "modules/", "include/", "docs/",
    "evidence/", "工程控制/",
)
PROVISIONING_VERBS = ("apt-get", "vcpkg", "pip", "choco", "winget", "Out-File")
WARNING_MARK = "::warning::"
SCRIPT_TOKEN_RE = re.compile(r"[A-Za-z0-9_./-]+\.(?:py|sh)")


def normalize(text: str) -> str:
    text = str(text).replace("\r\n", "\n").replace("\r", "\n")
    return re.sub(r"\s+", " ", text).strip()


def code_lines(body: str) -> str:
    """去注释行后的命令体（注释里的说明文字不参与业务命令扫描）。"""
    keep = []
    for line in str(body).replace("\r\n", "\n").splitlines():
        if line.strip().startswith("#"):
            continue
        keep.append(line)
    return normalize("\n".join(keep))


def load_yaml(path: Path) -> tuple[dict | None, str | None]:
    try:
        return yaml.safe_load(path.read_text(encoding="utf-8")), None
    except Exception as exc:  # noqa: BLE001 - YAML 解析失败即 FAIL 证据
        return None, repr(exc)


def job_of(doc: dict) -> tuple[str, dict] | tuple[None, None]:
    jobs = doc.get("jobs") or {}
    if len(jobs) != 1:
        return None, None
    name = list(jobs)[0]
    return name, jobs[name]


def on_key(doc: dict):
    return doc[True] if True in doc else doc.get("on")


def registry_fingerprints(registry: dict, manifest: dict) -> set[str]:
    """反复制指纹：注册表命令引用的仓库脚本 + 声明 exec 的脚本 token。"""
    found: set[str] = set()
    for check in registry.get("checks", []):
        for tok in check.get("command") or []:
            if isinstance(tok, str) and tok.endswith((".py", ".sh")) and "/" in tok:
                found.add(tok)
    for step in manifest.get("steps", []):
        for tok in step.get("exec") or []:
            if isinstance(tok, str) and tok.endswith((".py", ".sh")) and "/" in tok:
                found.add(tok)
    for extra in manifest.get("forbid_exec_tokens") or []:
        found.add(extra)
    # 派发器与注册表执行器是本设计的白名单调用面，不属于「被复制的业务命令」。
    found.discard(DISPATCHER)
    found.discard(RUNNER)
    return found


def registry_output_paths(registry: dict) -> set[str]:
    out: set[str] = set()
    for check in registry.get("checks", []):
        for rel in check.get("outputs") or []:
            out.add(rel)
    return out


def validate(repo: Path, manifest_path: Path, registry_path: Path, lock_path: Path,
             workflow_dir: str) -> tuple[list[dict], list[dict], dict]:
    errors: list[dict] = []
    notices: list[dict] = []
    report: dict = {"schema_version": 1, "task": "CI-001B", "repo": str(repo),
                    "manifest": str(manifest_path), "registry": str(registry_path),
                    "workflows": {}, "steps": []}

    def err(code: str, where: str, detail: str = "") -> None:
        errors.append({"code": code, "where": where, "detail": detail})

    def notice(code: str, where: str, detail: str = "") -> None:
        notices.append({"code": code, "where": where, "detail": detail})

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        err("manifest_unreadable", str(manifest_path), repr(exc))
        return errors, notices, report
    try:
        registry = json.loads(registry_path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001
        err("registry_unreadable", str(registry_path), repr(exc))
        return errors, notices, report

    if manifest.get("schema_version") != 1:
        err("manifest_shape", "schema_version", repr(manifest.get("schema_version")))
    declared = manifest.get("steps")
    if not isinstance(declared, list) or not declared:
        err("manifest_shape", "steps", "must be non-empty array")
        return errors, notices, report

    checks = {c.get("id"): c for c in registry.get("checks", [])}
    profiles: dict[str, int] = {}
    for c in registry.get("checks", []):
        for p in c.get("profiles") or []:
            profiles[p] = profiles.get(p, 0) + 1
    report["registry"] = {"checks": len(checks), "profiles": profiles}

    # actions.lock
    lock_shas: set[str] = set()
    lock_entries = 0
    try:
        lock = json.loads(lock_path.read_text(encoding="utf-8"))
        entries = lock.get("entries") or []
        lock_entries = len(entries)
        for e in entries:
            sha = e.get("commit_sha")
            if not isinstance(sha, str) or not SHA_RE.match(sha):
                err("lock_invalid", str(lock_path), "bad commit_sha " + repr(sha))
                continue
            lock_shas.add(sha)
    except Exception as exc:  # noqa: BLE001
        err("lock_invalid", str(lock_path), repr(exc))
    report["actions_lock"] = {"entries": lock_entries, "shas": sorted(lock_shas)}

    seen_ids: set[str] = set()
    for step in declared:
        sid = step.get("step_id")
        if not isinstance(sid, str) or not sid:
            err("manifest_shape", "step_id", repr(sid))
            continue
        if sid in seen_ids:
            err("duplicate_step_id", sid, "")
        seen_ids.add(sid)

    wf_dir = repo / workflow_dir
    check_bodies = [s for s in declared if s.get("role") == CHECK_BODY_ROLE]
    step_decls = [s for s in declared if s.get("role") != CHECK_BODY_ROLE]
    for body in check_bodies:
        sid = body.get("step_id")
        where = "check-body:" + str(sid)
        cid = body.get("check_id")
        if not cid:
            err("check_body_unbound", where, "缺 check_id")
            continue
        cres = checks.get(cid)
        if cres is None:
            err("check_body_not_registered", where, str(cid))
            continue
        report["steps"].append({
            "step_id": sid, "workflow": None, "name": None, "role": CHECK_BODY_ROLE,
            "check_id": cid, "binds_check": body.get("binds_check"),
            "serves_checks": body.get("serves_checks") or [],
        })
        if cres.get("waivable") is not False:
            err("check_body_waivable", where, str(cid))
        want = DISPATCHER + " --step " + str(sid)
        got = normalize(" ".join(cres.get("command") or [])).replace(chr(34), "")
        if not any(got == normalize(r + " " + want) for r in RUNNERS):
            err("check_command_drift", where,
                str(cid) + ".command 必须逐字等于 <python|python3> " + want + " 实际: " + got)
        record = DISPATCH_RECORD + str(sid) + ".json"
        if record not in (cres.get("outputs") or []):
            err("check_outputs_missing_dispatch_record", where,
                str(cid) + ".outputs 缺 " + record)
        if body.get("fail_closed") is not True:
            err("check_body_not_fail_closed", where, "注册表侧门必须 fail_closed=true")
        argv = body.get("exec") or []
        if not isinstance(argv, list) or not argv:
            err("exec_missing", where, "")
        else:
            if argv[0] not in ("python", "python3", "bash"):
                err("exec_runner_invalid", where, str(argv[0]))
            target = next((t for t in argv[1:]
                           if not str(t).startswith("-")
                           and ("/" in str(t) or str(t).endswith((".py", ".sh")))), None)
            if target is None:
                err("exec_target_missing", where, "exec 未引用 ci/ 体文件")
            else:
                if not str(target).startswith("ci/"):
                    err("exec_not_under_ci", where, str(target))
                if not (repo / str(target)).is_file():
                    err("exec_target_missing", where, str(target))
        bound = body.get("binds_check")
        if bound:
            bres = checks.get(bound)
            if bres is None:
                err("bound_check_not_registered", where, str(bound))
            else:
                if bres.get("waivable") is not False:
                    err("bound_check_waivable", where, str(bound))
                declared_outputs = list(bres.get("outputs") or [])
                for rel in body.get("require_outputs") or []:
                    if rel not in declared_outputs:
                        err("output_contract_drift", where,
                            str(rel) + " 不在 " + str(bound) + ".outputs")
        for sid2 in body.get("serves_checks") or []:
            sres = checks.get(sid2)
            if sres is None:
                err("serves_check_not_registered", where, str(sid2))
                continue
            if sres.get("waivable") is not False:
                err("serves_check_waivable", where, str(sid2))
            if not set(sres.get("profiles") or []) & set(cres.get("profiles") or []):
                err("serves_check_profile_mismatch", where,
                    str(sid2) + " 与 " + str(cid) + " 无共同 profile")

    scope_files = sorted({s.get("workflow") for s in step_decls if s.get("workflow")})
    present = sorted(p.name for p in wf_dir.glob("*.yml")) if wf_dir.is_dir() else []
    for name in present:
        rel = workflow_dir + "/" + name
        if rel not in scope_files:
            notice("out_of_scope_workflow", rel,
                   "本任务写域/绑定范围外（未声明）；仍参与 B12 反复制扫描")

    fingerprints = registry_fingerprints(registry, manifest)
    registered_outputs = registry_output_paths(registry)
    report["binding"] = {"forbidden_tokens": sorted(fingerprints)}

    for wf_rel in scope_files:
        wf_path = repo / wf_rel
        if not wf_path.is_file():
            err("workflow_missing", wf_rel, "")
            continue
        doc, yerr = load_yaml(wf_path)
        if doc is None:
            err("workflow_unreadable", wf_rel, str(yerr))
            continue
        job_name, job = job_of(doc)
        if job is None:
            err("job_missing", wf_rel, "workflow 必须恰好一个 job")
            continue
        steps = job.get("steps") or []
        declared_here = [s for s in step_decls if s.get("workflow") == wf_rel]
        runners = [s for s in declared_here if s.get("role") == "registry-runner"]
        if not runners:
            err("registry_runner_missing", wf_rel,
                "workflow 必须恰有一条 ci/run.py --profile 注册表执行步")
        elif len(runners) > 1:
            err("registry_runner_duplicate", wf_rel, str(len(runners)))
        for s in declared_here:
            if s.get("job") != job_name:
                err("job_missing", wf_rel, "声明 job=" + str(s.get("job")) + " 实际=" + job_name)

        wf_names = [s.get("name") for s in steps]
        decl_names = [s.get("name") for s in declared_here]
        for name in wf_names:
            if name not in decl_names:
                err("unbound_step", wf_rel, "workflow 步未在绑定声明内: " + str(name))
        for name in decl_names:
            if name not in wf_names:
                err("stale_binding", wf_rel, "声明步在 workflow 内不存在: " + str(name))
        if wf_names == decl_names and len(steps) != len(declared_here):
            err("step_order_drift", wf_rel, "步数不一致")
        elif [n for n in wf_names if n in decl_names] != [n for n in decl_names if n in wf_names]:
            err("step_order_drift", wf_rel, "共同步顺序漂移")

        concurrency = doc.get("concurrency") or {}
        group = str(concurrency.get("group") or "")
        if "github.ref" not in group or "github.event_name" not in group:
            err("concurrency_group_weak", wf_rel,
                "concurrency.group 必须同时含 github.ref 与 github.event_name: " + repr(group))
        if not isinstance(concurrency.get("cancel-in-progress"), bool):
            err("concurrency_group_weak", wf_rel, "cancel-in-progress 必须显式布尔")
        if not isinstance(job.get("timeout-minutes"), int):
            err("job_timeout_missing", wf_rel, "job 必须声明 timeout-minutes")
        triggers = on_key(doc) or {}
        choices = []
        dispatch = triggers.get("workflow_dispatch") or {}
        if isinstance(dispatch, dict):
            inputs = dispatch.get("inputs") or {}
            for spec in inputs.values():
                if isinstance(spec, dict) and isinstance(spec.get("options"), list):
                    choices.extend(spec["options"])
        for choice in choices:
            if choice not in profiles:
                err("unknown_profile", wf_rel, "dispatch choice 不在注册表: " + str(choice))

        report["workflows"][wf_rel] = {
            "job": job_name,
            "steps": len(steps),
            "concurrency_group": group,
            "cancel_in_progress": concurrency.get("cancel-in-progress"),
            "job_timeout_minutes": job.get("timeout-minutes"),
            "dispatch_profile_choices": choices,
        }

        for idx, raw in enumerate(steps):
            name = raw.get("name")
            entry = next((s for s in declared_here if s.get("name") == name), None)
            body = raw.get("run")
            uses = raw.get("uses")
            where = wf_rel + "#" + str(name)
            if entry is None:
                continue
            role = entry.get("role")
            if role not in ALLOWED_ROLES:
                err("unknown_role", where, repr(role))
                continue
            report["steps"].append({
                "step_id": entry.get("step_id"), "workflow": wf_rel, "name": name,
                "role": role, "binds_check": entry.get("binds_check"),
                "serves_checks": entry.get("serves_checks") or [],
                "pending_registration": entry.get("pending_registration"),
            })
            if entry.get("pending_registration"):
                notice("pending_registration", where,
                       str(entry.get("pending_registration")) + "（挂账：需在 ci/checks.json 注册为检查项）")

            if uses:
                sha = str(uses).split("@", 1)[1].split()[0] if "@" in str(uses) else ""
                if not SHA_RE.match(sha):
                    err("sha_not_locked", where, str(uses))
                elif lock_shas and sha not in lock_shas:
                    err("sha_not_locked", where, str(uses) + " 不在 actions.lock")

            if role == "pinned-action":
                if not uses:
                    err("uses_missing", where, "")
                if body:
                    err("run_forbidden", where, "pinned-action 步不得有 run")
            elif role == "artifact-upload":
                if not str(uses or "").startswith("actions/upload-artifact"):
                    err("uses_not_upload_artifact", where, str(uses))
                with_ = raw.get("with") or {}
                if not str(with_.get("path") or "").strip():
                    err("upload_path_missing", where, "")
                if not str(with_.get("if-no-files-found") or "").strip():
                    err("upload_if_no_files_found_missing", where, "")
                if not str(raw.get("if") or "").strip():
                    err("upload_if_missing", where, "")
            elif role == "ci-entrypoint":
                want = normalize(entry.get("exec_body") or "")
                got = code_lines(body or "")
                if not body:
                    err("entrypoint_body_missing", where, "")
                elif got != want:
                    err("entrypoint_body_drift", where, "体与声明 exec_body 不一致")
                for tok in SCRIPT_TOKEN_RE.findall(code_lines(body or "")):
                    if tok in CI_ENTRYPOINTS:
                        continue
                    err("business_command_in_workflow", where, "非声明入口脚本: " + tok)
            elif role == "host-provisioning":
                code = code_lines(body or "")
                if not body:
                    err("run_body_missing", where, "")
                for tok in REPO_PATH_TOKENS:
                    # 只认仓库相对路径 token：绝对路径（/usr/include/、/usr/lib/）不算
                    if re.search(r"(?<![\w/.-])" + re.escape(tok), code):
                        err("business_command_in_workflow", where, "仓库路径 token: " + tok)
                m = BUILD_TOOL_RE.search(code)
                if m:
                    err("business_command_in_workflow", where, "构建工具: " + m.group(1))
                for tok in SCRIPT_TOKEN_RE.findall(code):
                    err("business_command_in_workflow", where, "脚本调用: " + tok)
                if not any(verb in code for verb in PROVISIONING_VERBS):
                    err("provisioning_verb_missing", where, "未声明 provisioning 动词")
            elif role == "registry-runner":
                want = RUNNER + " --profile " + str(entry.get("profile_expr"))
                got = code_lines(body or "").replace(chr(34), "")
                ok_body = False
                for runner in RUNNERS:
                    if got == normalize(runner + " " + want).replace(chr(34), ""):
                        ok_body = True
                if not body:
                    err("run_body_missing", where, "")
                elif not ok_body:
                    err("runner_not_canonical", where,
                        "体必须逐字等于 <python|python3> " + want + " 实际: " + got)
                expr = str(entry.get("profile_expr") or "").strip()
                m = PROFILE_EXPR_RE.match(expr)
                if not expr:
                    err("unknown_profile", where, "profile_expr 缺失")
                elif m:
                    step_ref = m.group(1)
                    home = [s for s in declared_here if s.get("step_ref") == step_ref]
                    if not home:
                        err("profile_expr_unbound", where, "profile 表达式未绑定声明步: " + step_ref)
                    elif not any(s.get("provides_profile") for s in home):
                        err("profile_expr_unbound", where, "声明步未标注 provides_profile: " + step_ref)
                elif expr not in profiles:
                    err("unknown_profile", where, "字面 profile 不在注册表: " + expr)
            elif role in EXECUTABLE_ROLES:
                # 派发步的体只允许调用派发器本体：任何其他仓库脚本 token 都算复制业务命令
                for tok in SCRIPT_TOKEN_RE.findall(code_lines(body or "")):
                    if tok != DISPATCHER:
                        err("business_command_in_workflow", where, "非派发器脚本: " + tok)
                want = DISPATCHER + " --step " + str(entry.get("step_id"))
                got = code_lines(body or "")
                ok_body = any(got == normalize(r + " " + want) for r in RUNNERS)
                if not body:
                    err("run_body_missing", where, "")
                elif not ok_body:
                    err("dispatch_body_mismatch", where,
                        "体必须逐字等于 <python|python3> " + want + " 实际: " + got)
                argv = entry.get("exec") or []
                if not isinstance(argv, list) or not argv:
                    err("exec_missing", where, "")
                else:
                    if argv[0] not in ("python", "python3", "bash"):
                        err("exec_runner_invalid", where, str(argv[0]))
                    target = next((t for t in argv[1:]
                                   if not str(t).startswith("-") and ("/" in str(t) or str(t).endswith((".py", ".sh")))), None)
                    if target is None:
                        err("exec_target_missing", where, "exec 未引用 ci/ 体文件")
                    else:
                        if not str(target).startswith("ci/"):
                            err("exec_not_under_ci", where, str(target))
                        if not (repo / str(target)).is_file():
                            err("exec_target_missing", where, str(target))
                bound = entry.get("binds_check")
                if bound:
                    cres = checks.get(bound)
                    if cres is None:
                        err("bound_check_not_registered", where, str(bound))
                    else:
                        if cres.get("waivable") is not False:
                            err("bound_check_waivable", where, str(bound))
                        declared_outputs = list(cres.get("outputs") or [])
                        for rel in entry.get("require_outputs") or []:
                            if rel not in declared_outputs:
                                err("output_contract_drift", where,
                                    str(rel) + " 不在 " + str(bound) + ".outputs")
                for sid in entry.get("serves_checks") or []:
                    cres = checks.get(sid)
                    if cres is None:
                        err("serves_check_not_registered", where, str(sid))
                        continue
                    if cres.get("waivable") is not False:
                        err("serves_check_waivable", where, str(sid))
                    if entry.get("when_profile") and entry["when_profile"] not in (cres.get("profiles") or []):
                        err("serves_check_profile_mismatch", where,
                            str(sid) + " 不含 profile " + str(entry.get("when_profile")))
                bound_nonwaivable = bool(bound) and (checks.get(bound) or {}).get("waivable") is False
                cond = str(raw.get("if") or "")
                if bound_nonwaivable and normalize(cond) != "always()":
                    err("nonwaivable_step_skippable", where,
                        "绑定不可豁免检查的步必须 if: always()（当前: " + repr(cond) + "）")
                if bound_nonwaivable and WARNING_MARK in normalize(body or ""):
                    err("warning_skip_branch", where, "不可豁免门的体出现 ::warning:: 跳过路径")
                if entry.get("fail_closed") and not bound_nonwaivable and not entry.get("serves_checks"):
                    err("fail_closed_unbound", where, "fail_closed 步既无 binds_check 也无 serves_checks")

            if body:
                code = code_lines(body)
                if WARNING_MARK in code:
                    for rel in registered_outputs:
                        if rel and rel in code:
                            err("warning_skip_branch_on_registered_output", where, rel)
                            break
                for tok in fingerprints:
                    if tok in code:
                        err("registry_command_copied_into_workflow", where, tok)

    report["verdict"] = "PASS" if not errors else "FAIL"
    report["error_count"] = len(errors)
    report["notice_count"] = len(notices)
    return errors, notices, report


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="ci/validate_workflow_binding.py",
        description="workflow 与 ci/checks.json 联动核死校验器")
    ap.add_argument("--repo-root", default=None)
    ap.add_argument("--manifest", default=None)
    ap.add_argument("--registry", default=None)
    ap.add_argument("--lock", default=None)
    ap.add_argument("--workflow-dir", default=DEFAULT_WORKFLOW_DIR)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--out", default=None)
    args = ap.parse_args(argv)

    repo = Path(args.repo_root).resolve() if args.repo_root else REPO
    manifest = Path(args.manifest) if args.manifest else repo / "ci" / DEFAULT_MANIFEST.name
    registry = Path(args.registry) if args.registry else repo / "ci" / DEFAULT_REGISTRY.name
    lock = Path(args.lock) if args.lock else repo / "ci" / DEFAULT_LOCK.name

    errors, notices, report = validate(repo, manifest, registry, lock, args.workflow_dir)
    payload = dict(report)
    payload["errors"] = errors
    payload["notices"] = notices
    text = json.dumps(payload, ensure_ascii=False, sort_keys=True)
    if args.out:
        out = Path(args.out)
        if not out.is_absolute():
            out = repo / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True),
                       encoding="utf-8")
    print(text if args.json else json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
