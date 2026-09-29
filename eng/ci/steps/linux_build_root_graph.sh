#!/usr/bin/env bash
# eng/ci/steps/linux_build_root_graph.sh - UT-BACKEND/UT-CLI 构建树门准备步（CI-001B 迁出 workflow）。
#  来源：.github/workflows/ci-linux.yml 原 step "Build root graph for UT-BACKEND/UT-CLI
#  build-tree gates" 的 run: 体逐行迁移（CI-001B 目标 1）。
#  绑定声明见 eng/ci/workflow_binding.json（step_id=LINUX-BUILD-ROOT-GRAPH）。
#  该步自身注册为独立检查项的挂账见任务 commit message。
set -euo pipefail
# V8.1-CI：UT-CLI test_cli_single_install（BLD-002 后）扫描根图构建树
# build/{acsd,libacsd_runtime.so}；UT-BACKEND p1004/p2006/p3006
# 调 build/acsd 顶层入口；cli 子图 build/cli/astrocs 供 phaseN run
# in-process 测试。linux-control preset（Unix Makefiles, 测试 ON）。
# 这是验证侧构建树，非发布产物面（BLD-002 install 白名单不受影响）。
timeout 2400 cmake --preset linux-control
timeout 2400 cmake --build build/linux-control -j 2 --target acsd
# UT-BACKEND p2006 族 `acsd test synthetic` 依赖 unit 门二进制；
# cmd_test_synthetic 默认找 build/root-cmake/tests/unit（V6.1 布局），
# linux-control 树在 build/linux-control/tests/unit → job env
# ASTROCS_TEST_BIN_DIR 重定向（仅 cmd_test_synthetic 读取，无副作用面）。
timeout 1200 cmake --build build/linux-control -j 2 \
  --target p1_ir_facade_test p2_upm_synthetic_test
# ROOT-008 收口：旧 `cli/` 子图（`build/cli/astrocs`）已退役 —— 唯一产品二进制是
# 根图的 `build/acsd`（eng/tests/cli/test_phase123_pipeline.py:24 逐字声明）。
# 旧 `cli/CMakeLists.txt` 为布局重构前的遗留副本，其引用路径（`../lib/astro_image_io`、
# `../lib/backend_host`、`../lib/acr`）在 ROOT-008 搬迁后已全部不存在，配置必然失败。
# 本步曾因此恒红；此处删除该退役子构建，phaseN in-process 测试统一用根图 `build/acsd`。
# M8-F-003: UT-CLI test_cli_single_install 前置产物是根 build/ 构建树的
# build/{acsd,libacsd_runtime.so} 双件; 旧步只 cp acsd, 缺 .so ⇒
# 该门 setUpClass 恒 SkipTest(CI 执行数为 0)。此处补建 runtime 平台库并落根树。
# 注: 该门的 install 面走 build/linux-control(cm/install_layout.cmake 白名单),
# 干净树必须把 install 载荷目标一并构建, 否则 cmake --install 会在
# libacsd_io.so / modules/*.so 处缺失失败(旧步只建 acsd)。
# R-52/B 组：目标清单必须覆盖安装脚本引用的**全部**载荷目标（15 个）——先建齐、再重链。
#   旧清单只列 9 个（缺 acsd 及 3 个 cpu 变体 + 3 个 cpuprov），
#   于是 preinstall 重链会在未构建的目标上失败（"没有规则可制作目标 …capability_detect.c.o"）。
timeout 2400 cmake --build build/linux-control -j 2 --target \
  acsd acsd_runtime acsd_io astrocs_noop astrocs_cpu_baseline \
  astrocs_cpu_avx2 astrocs_cpu_avx512 \
  astrocs_cpuprov_baseline astrocs_cpuprov_avx2 astrocs_cpuprov_avx512 \
  astrocs_catalog_gaia astrocs_p1_drizzle astrocs_p1_calibration \
  astrocs_p1_cosmetic astrocs_p1_hips_writer \
  astrocs_backends_manifest astrocs_providers_manifest
# R-52/B 组：Makefiles 树（linux-control preset）的安装载荷由**显式 preinstall 重链**供给
#   —— CMake 为 Unix Makefiles 生成器把安装源指向 CMakeFiles/CMakeRelink.dir/<tgt>，
#   而 cmake --build --target <tgt> 与 make <tgt> 都**不触发** <tgt>/preinstall（只有显式目标才触发）。
#   verification 树**不得依赖环境自动完成该步**（R-52 第 1 条）⇒ 此处显式执行，逐个载荷失败即红。
for _t in acsd acsd_runtime acsd_io astrocs_noop astrocs_cpu_baseline   astrocs_cpu_avx2 astrocs_cpu_avx512   astrocs_cpuprov_baseline astrocs_cpuprov_avx2 astrocs_cpuprov_avx512   astrocs_catalog_gaia astrocs_p1_drizzle astrocs_p1_calibration   astrocs_p1_cosmetic astrocs_p1_hips_writer; do
  timeout 600 make -C build/linux-control "$_t/preinstall"
done
# R-52/B 组：载荷清单与安装脚本**单源比对**，缺件即红（fail-closed）。
#   清单不再手抄 —— 直接读 cmake_install.cmake 的安装源路径，脚本改了就自动跟上。
python3 - <<'PYEOF'
import os, re, sys
src = open("build/linux-control/cmake_install.cmake", encoding="utf-8").read()
want = sorted({m.group(1) for m in re.finditer(r'FILES "([^"]+)"', src)})
want = [p for p in want if "${" not in p]          # 只比绝对规范路径
missing = [p for p in want if not os.path.isfile(p)]
if missing:
    print("INSTALL-PAYLOAD-INCOMPLETE: 安装脚本要求 %d 件，缺 %d 件：" % (len(want), len(missing)),
          file=sys.stderr)
    for p in missing: print("  missing: " + p, file=sys.stderr)
    print("  处置：补建对应目标（含其 preinstall 重链），不放松该判据。", file=sys.stderr)
    sys.exit(1)
print("INSTALL-PAYLOAD-COMPLETE: %d 件齐备" % len(want))
PYEOF
mkdir -p build && cp -f build/linux-control/acsd build/acsd
cp -f build/linux-control/libacsd_runtime.so build/libacsd_runtime.so
# eng/tests/backend oracle fixture(如 test_phase3_reproject_oracle)用
# -IREPO/build 取 version_generated.h; 根 build/ 仅被 cp 二进制,
# 需补生成头(R19 34204130361 UT-BACKEND setUpClass 实证)。
cp -f build/linux-control/version_generated.h build/version_generated.h

# V3 B3-A3/B3-A4: 独立 oracle 门 (UT-API) 与 lib/phase2 gtest 目标的链接输入。
#   - lib/infrastructure/aio/astro_image_io.dll 是 gitignore 的共享库构建产物
#     (干净检出无); eng/tests/api 的 seam/UPM/reject 独立 oracle 门以 g++ 直接
#     链接它。UT-API 的接缝门自本批起 fail-closed (缺库即红, 不再静默
#     skip), 故此处必须真实构建。
#   - build/linux-openmp-on/libphase2.a (P2_ENABLE_OPENMP=ON 归档) 同为
#     上述 oracle 门的链接输入。
#   - lib/infrastructure/aio Makefile 已补 -fPIC (共享对象必需; gcc14 对 TLS
#     local-exec 重定位报 R_X86_64_TPOFF32, 否则无法链接)。
mkdir -p build/linux-openmp-on
# ROOT-008 收口：AIO 已迁至 lib/infrastructure/aio/（旧 lib/astro_image_io 退役）。
# 旧路径引用使本步在干净树恒红（make: *** lib/astro_image_io: 没有那个文件或目录）。
if [ ! -f lib/infrastructure/aio/astro_image_io.dll ]; then
  timeout 1800 make -C lib/infrastructure/aio -j 2 all
fi
if [ ! -f build/linux-openmp-on/libphase2.a ]; then
  # ARCH-001 目录等价迁移：lib/phase2 已迁至 lib/algorithms/coverage（target 名 phase2
  # 与产物名 libphase2.a 均未变，见该目录 CMakeLists 的 add_library(phase2 STATIC ...)）。
  # 同本脚本 :82-83 的 ROOT-008 收口同型 —— 旧路径引用使本步在干净树恒红。
  timeout 900 cmake -S lib/algorithms/coverage -B build/linux-openmp-on \
    -DP2_ENABLE_OPENMP=ON -DCMAKE_BUILD_TYPE=Release
  timeout 900 cmake --build build/linux-openmp-on --target phase2 -j 2
fi

# ROOT-008 收口：唯一产品二进制是根图 build/acsd（build/cli/astrocs 已退役）；
# 且 CLI 的版本入口是 `--version`（`version` 是未知子命令，旧写法使本步恒红）。
./build/acsd --version --json
