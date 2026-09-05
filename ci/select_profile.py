#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/select_profile.py —— V8-CI-007 事件->profile 选择规则。

规则（与 workflow ci-linux.yml 的 select_profile 步骤配套）：
- push             -> linux-main
- schedule         -> linux-deep
- workflow_dispatch -> --requested 指定的 choice（linux-main|linux-deep）；
  requested 为空或不在白名单 -> exit 2（受控失败）。

输出（GITHUB_OUTPUT 兼容）：
- stdout 正常路径写 ``profile=<id>``（单行，可 ``>> $GITHUB_OUTPUT``）；
- ``--json`` 额外（或独立）输出 ``{event, requested, profile, reason}``。

Exit code：0=选定；2=事件非法 / dispatch requested 无效。
本脚本不做网络请求、不写文件。
"""
from __future__ import annotations

import argparse
import json
import sys

SCHEMA_VERSION = 1
TASK_ID = "V8-CI-007"

EVENT_PUSH = "push"
EVENT_SCHEDULE = "schedule"
EVENT_DISPATCH = "workflow_dispatch"
KNOWN_EVENTS = (EVENT_PUSH, EVENT_SCHEDULE, EVENT_DISPATCH)

PROFILE_LINUX_MAIN = "linux-main"
PROFILE_LINUX_DEEP = "linux-deep"
DISPATCH_CHOICES = (PROFILE_LINUX_MAIN, PROFILE_LINUX_DEEP)

PUSH_PROFILE = PROFILE_LINUX_MAIN
SCHEDULE_PROFILE = PROFILE_LINUX_DEEP

_EXIT_OK = 0
_EXIT_INVALID = 2


def resolve(event: str, requested: str | None) -> tuple[str | None, str]:
    """核心规则：返回 (profile, reason)；profile=None 表示受控拒绝。"""
    event = (event or "").strip()
    requested = (requested or "").strip()

    if event not in KNOWN_EVENTS:
        return None, (f"未知事件 {event!r}（支持 {'|'.join(KNOWN_EVENTS)}）")

    if event == EVENT_PUSH:
        return PUSH_PROFILE, f"push 事件固定 {PUSH_PROFILE}"

    if event == EVENT_SCHEDULE:
        return SCHEDULE_PROFILE, f"schedule 事件固定 {SCHEDULE_PROFILE}"

    # workflow_dispatch：必须显式给出合法 choice
    if not requested:
        return None, ("workflow_dispatch 需要显式 --requested "
                      f"({'|'.join(DISPATCH_CHOICES)}之一)")
    if requested not in DISPATCH_CHOICES:
        return None, (f"requested {requested!r} 不在 dispatch 白名单 "
                      f"({'|'.join(DISPATCH_CHOICES)})")
    return requested, f"workflow_dispatch 采用 requested={requested}"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="ci/select_profile.py",
        description="V8-CI-007：按触发事件选择 CI profile（GITHUB_OUTPUT 兼容）。")
    parser.add_argument("--event", required=True,
                        help="push|schedule|workflow_dispatch")
    parser.add_argument("--requested", default="",
                        help="workflow_dispatch 的 profile choice（其余事件忽略）")
    parser.add_argument("--json", action="store_true",
                        help="stdout 输出 {event, requested, profile, reason} JSON")
    args = parser.parse_args(argv)

    profile, reason = resolve(args.event, args.requested)
    if args.json:
        # --json：仅输出机读 JSON（不追加 profile= 行，保证可 json.loads）
        print(json.dumps({
            "schema_version": SCHEMA_VERSION,
            "task_id": TASK_ID,
            "event": args.event.strip(),
            "requested": args.requested.strip(),
            "profile": profile,
            "reason": reason,
        }, ensure_ascii=False))
        return _EXIT_OK if profile is not None else _EXIT_INVALID

    if profile is None:
        # 受控失败：单行结构化信息到 stderr，退出码 2，无 traceback
        print(f"select_profile: INVALID: {reason}", file=sys.stderr)
        return _EXIT_INVALID

    # GITHUB_OUTPUT 兼容格式（单行 key=value，可直接 >> "$GITHUB_OUTPUT"）
    print(f"profile={profile}")
    return _EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
