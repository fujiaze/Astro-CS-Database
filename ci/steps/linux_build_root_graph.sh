#!/usr/bin/env bash
# ci/steps/linux_build_root_graph.sh - UT-BACKEND/UT-CLI 构建树门准备步（CI-001B 迁出 workflow）。
#  来源：.github/workflows/ci-linux.yml 原 step "Build root graph for UT-BACKEND/UT-CLI
#  build-tree gates" 的 run: 体逐行迁移（CI-001B 目标 1）。
#  绑定声明见 ci/workflow_binding.json（step_id=LINUX-BUILD-ROOT-GRAPH）。
#  该步自身注册为独立检查项的挂账见任务 commit message。
set -euo pipefail
# V8.1-CI：UT-CLI test_cli_single_install（BLD-002 后）扫描根图构建树
# build/{astrocs,libastrocs_runtime.so}；UT-BACKEND p1004/p2006/p3006
# 调 build/astrocs 顶层入口；cli 子图 build/cli/astrocs 供 phaseN run
# in-process 测试。linux-control preset（Unix Makefiles, 测试 ON）。
# 这是验证侧构建树，非发布产物面（BLD-002 install 白名单不受影响）。
timeout 2400 cmake --preset linux-control
timeout 2400 cmake --build build/linux-control -j 2 --target astrocs
# UT-BACKEND p2006 族 `astrocs test synthetic` 依赖 unit 门二进制；
# cmd_test_synthetic 默认找 build/root-cmake/tests/unit（V6.1 布局），
# linux-control 树在 build/linux-control/tests/unit → job env
# ASTROCS_TEST_BIN_DIR 重定向（仅 cmd_test_synthetic 读取，无副作用面）。
timeout 1200 cmake --build build/linux-control -j 2 \
  --target p1_ir_facade_test p2_upm_synthetic_test
# cli 子图 build/cli/astrocs 供 phaseN in-process 测试（UT-CLI 面 EXE）
mkdir -p build/cli && cmake -S cli -B build/cli -DCMAKE_BUILD_TYPE=Release \
  -DASTROCS_ENABLE_ACR=OFF >/dev/null
timeout 2400 cmake --build build/cli -j 2 --target astrocs
mkdir -p build && cp -f build/linux-control/astrocs build/astrocs
# tests/backend oracle fixture(如 test_phase3_reproject_oracle)用
# -IREPO/build 取 version_generated.h; 根 build/ 仅被 cp 二进制,
# 需补生成头(R19 34204130361 UT-BACKEND setUpClass 实证)。
cp -f build/linux-control/version_generated.h build/version_generated.h
./build/astrocs version --json && ./build/cli/astrocs version --json
