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
# M8-F-003: UT-CLI test_cli_single_install 前置产物是根 build/ 构建树的
# build/{astrocs,libastrocs_runtime.so} 双件; 旧步只 cp astrocs, 缺 .so ⇒
# 该门 setUpClass 恒 SkipTest(CI 执行数为 0)。此处补建 runtime 平台库并落根树。
# 注: 该门的 install 面走 build/linux-control(cm/install_layout.cmake 白名单),
# 干净树必须把 install 载荷目标一并构建, 否则 cmake --install 会在
# libastrocs_io.so / modules/*.so 处缺失失败(旧步只建 astrocs)。
timeout 2400 cmake --build build/linux-control -j 2 --target   astrocs_runtime astrocs_io astrocs_noop astrocs_cpu_baseline   astrocs_catalog_gaia astrocs_p1_drizzle astrocs_p1_calibration   astrocs_p1_cosmetic astrocs_p1_hips_writer
mkdir -p build && cp -f build/linux-control/astrocs build/astrocs
cp -f build/linux-control/libastrocs_runtime.so build/libastrocs_runtime.so
# tests/backend oracle fixture(如 test_phase3_reproject_oracle)用
# -IREPO/build 取 version_generated.h; 根 build/ 仅被 cp 二进制,
# 需补生成头(R19 34204130361 UT-BACKEND setUpClass 实证)。
cp -f build/linux-control/version_generated.h build/version_generated.h

# V3 B3-A3/B3-A4: 独立 oracle 门 (UT-API) 与 lib/phase2 gtest 目标的链接输入。
#   - lib/astro_image_io/astro_image_io.dll 是 gitignore 的共享库构建产物
#     (干净检出无); tests/api 的 seam/UPM/reject 独立 oracle 门以 g++ 直接
#     链接它。UT-API 的接缝门自本批起 fail-closed (缺库即红, 不再静默
#     skip), 故此处必须真实构建。
#   - build/linux-openmp-on/libphase2.a (P2_ENABLE_OPENMP=ON 归档) 同为
#     上述 oracle 门的链接输入。
#   - lib/astro_image_io Makefile 已补 -fPIC (共享对象必需; gcc14 对 TLS
#     local-exec 重定位报 R_X86_64_TPOFF32, 否则无法链接)。
mkdir -p build/linux-openmp-on
if [ ! -f lib/astro_image_io/astro_image_io.dll ]; then
  timeout 1800 make -C lib/astro_image_io -j 2 all
fi
if [ ! -f build/linux-openmp-on/libphase2.a ]; then
  timeout 900 cmake -S lib/phase2 -B build/linux-openmp-on \
    -DP2_ENABLE_OPENMP=ON -DCMAKE_BUILD_TYPE=Release
  timeout 900 cmake --build build/linux-openmp-on --target phase2 -j 2
fi

./build/astrocs version --json && ./build/cli/astrocs version --json
