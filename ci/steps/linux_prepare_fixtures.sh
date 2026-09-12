#!/usr/bin/env bash
# ci/steps/linux_prepare_fixtures.sh - linux-main 验证侧 fixture 准备步（CI-001B 迁出 workflow）。
#  来源：.github/workflows/ci-linux.yml 原 step "Prepare linux-main run/temp fixtures" 的
#  run: 体逐行迁移（CI-001B 目标 1：workflow 的 run: 体不再复制业务命令，统一由 ci/ 声明体承担）。
#  绑定声明见 ci/workflow_binding.json（step_id=LINUX-PREPARE-FIXTURES）。
#  该步自身注册为独立检查项的挂账见任务 commit message（ci/checks.json 单写者冲突）。
set -euo pipefail
# V8.1-CI：UT-BACKEND p1004 联合门读 run/temp/mon001_cfg.json（V6.1
# 手工 MON-001 残留，从无入库/生成方 → hosted 恒缺失，test_02/03 必
# FAIL）；p1004 workload 顶层 run 走 phase3，需 HiPS fixture。
# ci/prepare_linux_fixtures.py 用仓库 vendored AIO/healpix 源自编译
# fixture 生成 FIELD.hips + V1 顶层 config（不联网拉业务数据）；
# f1f2/p2006 族 fixture 由 tests/backend/fixture_common.ensure_f1f2_hips
# 自举（无需此步）。
timeout 600 python3 ci/prepare_linux_fixtures.py
