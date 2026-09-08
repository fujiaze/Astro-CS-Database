# CPU-007 执行计划（SA-CPU7, SubAgent）

任务：profile 存储/失效/原子更新（V7.1 04_CPU_RESOURCE_TASKS.md CPU-007；V8.1 03_P0_REMEDIATION_TASKS.md §V8-CPU-002）。

## 规格要点（两处控制包合并）

1. 默认路径：Windows `%LOCALAPPDATA%/AstroCS/cpu_profile.json`；Linux XDG data（XDG_DATA_HOME → ~/.local/share）/AstroCS/cpu_profile.json。
2. 原子更新：写临时 → 校验 → 原子 rename；目标文件任何时刻要么旧完整要么新完整。
3. 绑定面：schema（astrocs.cpu-profile/v2）/ product build（source_commit、benchmark_binary_sha256）/ CPU（vendor/family/model/stepping/xcr0/features）/ OS（os_abi）/ provider hash。
4. 验收：损坏、旧版本、机器变化、半写文件不被使用；无 profile 运行清晰 warning；profile 不进入源码/审核包原始数据。
5. V8-CPU-002：没有/失配 profile → generic ISA + 动态多线程（不退单线程）；不支持路径 NOT_APPLICABLE（路由域，不在本层）。

## 层次合同（复用既有唯一实现，不复制校验链）

- 文本级：`verify_profile_v2`（CPU-003）
- 身份级：`check_profile_identity_v1`（CPU-005）
- 消费级：`profile_kernel_benchmark_valid`（CPU-005）
- 与 CPU-006 `astrocs.benchmark-report/v1` 结构性隔离：本层只接受 cpu-profile/v2，report 文本被 verify 拒绝（双向负例验证）。

## 交付物

- `lib/backend_host/profile_store.h/.cpp`：`default_profile_path_v1` / `save_profile_atomic_v1`（写临时→fsync→复读比对→verify→rename；源码树防线）/ `load_profile_checked_v1`（三段校验链 + 失效隔离 `.rejected-<utc>` 只改名不删除）/ `classify_profile_rejection_v1`（corrupted/old_schema/stale_machine/stale_build 词表）。
- `tests/unit/cpu007_profile_store_test.cpp`：14 组（正例 4 + 负向样例 8 + 词表/回归 2）。
- CMake 注册：根 CMakeLists（astrocs_cpu 源列表）、tests/unit/CMakeLists.txt。
- 真实链路证据：CLI `benchmark cpu --quick` 实跑 profile → save/load 全链 + 5 负例（含异二进制拒用）。

## 验证记录（load 全程 <2，详见 logs/）

c01 构建 astrocs_cpu+测试 rc=0；c02 单测 ALL PASS；c03 CLI 全量链接 rc=0；c04 ctest cpu/profile/bench 18/18；c05 bench quick 实跑 PASS；c06 真实链路驱动 ALL PASS；c07 全量 ctest 77/77（avx512 selftest 机器不支持 Skipped，与基线一致）。

## 边界遵守

仅改 lib/backend_host/（新增 profile_store）+ tests/unit/ 新测试 + 2 处 CMake 注册 + evidence/；未触碰 docs/tools/ci/.github/tests/backend/tests/cli/cli/lib/gaia_xpsd_client/lib/calibration/lib/phase2/lib/phase3_*/lib/photometric_calib；未执行 git add/commit/push。
