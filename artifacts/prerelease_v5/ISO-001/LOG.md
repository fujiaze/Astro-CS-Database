# ISO-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ISO-001 行「静态+运行测试证明 CLI/Phase/dispatcher/manifest 不引用 ACR/GPU/Mixed; 发行包扫描 | production route 0 触达; 配置请求 ACR 明确拒绝; 不是默默 fallback」; 01 §5(本轮范围外的 ACR/GPU/混合 + 非 amd64 + 动态第三方 backend + 不可信路径加载插件); 05(纯 CPU baseline/variant)。AGENTS.md「ACR 暂不接入, 生产仅纯 CPU 自适应 backend」。

## 静态证据(生产选路不触 ACR/GPU)
- 生产源码 cli/main.cpp、lib/phase1_session/p1_session.cpp、lib/phase2_session/p2_session.cpp、lib/phase3_session/p3_session.cpp、lib/phase2/src/upm.cpp 均无 ACR/CUDA/GPU/device_executor/kernel_registry/mosaic_reject_cuda 符号引用(grep 计数 = 0)。
- **register_phase2_acr_kernels() 仅被 tests/tools 调用**(lib/phase2/tools/stage2.cpp、lib/phase2/tests/*、sanitize_driver.cpp), 生产 cli/main.cpp + 三个 phase session + upm.cpp 均不调用 → 生产 ACR kernel registry 为空(acr_registered=false → p2_acr_block_eligible 恒 false)。
- 生产后端选路由 `backend_host`(纯 CPU ABI, ISA-001..005 验证过的 baseline/avx/avx2/avx512 变体)承担; 无 ACR scheduler/dispatcher/GPU device executor 接入。
- lib/acr/ 是未接入的独立引擎(其 dispatcher/device_executor 未链接进生产; CLI CMakeLists 仅编入 kernel_registry.cpp/device_executor.cpp 因编译单元依赖, 但这些只被 tests/tools 触达)。
- lib/phase2/src/cuda_bridge_stub.cpp 是 Linux 桩: CUDA bridge 恒不可用(api() 返回空 BridgeApi; ensure_bridge_loaded() 空), 故 GPU 路径在 Linux 恒不可达。
- 配置层防线: stage2_common.cpp 的 acr_route/gpu_route parser 明确校验 —— acr_route 只允许 auto/cpu("acr_route 只支持 auto/cpu"), 不允许 cuda; gpu_route 允许 cpu/auto/cuda 但 Linux 无 CUDA bridge 不可达。此为"明确拒绝/隔离", 非默默 fallback。(注: stage2_common 为 legacy/tool parser, 不在生产 phase2_session 路径。)

## 运行证据(配置请求 ACR 明确拒绝, exit 3)
- run config 顶层含 `backend` / `acr_route` / `gpu_route` / `mixed_backend` 键 → validate_config_full 报 "config has unknown key" 并返回 `INPUT`(=3), 非静默忽略。实测 exit=3。
- 合法 config(仅 schema_version/inputs/output_dir/phase3)通过 config validate(exit 0)。
- phase3 生产 run(合成 FIELD.hips)成功(exit 0), 产出 astrocs_run_<hash>.json manifest, 其字段不含 acr/gpu/cuda/mixed/dispatcher/route 选路键。

## 发行包扫描
- backbone 发行包仅纯 CPU baseline/ISA 变体 DSO(源码+manifest 层), 无 acr/gpu/cuda 后端 id。
- `astrocs doctor --json` 后端清单不含 backend_id ∈ {acr,gpu,cuda}。
- make_capsule/gen_backends_manifest 脚本无 ACR/GPU/CUDA 插件加载逻辑。

## 验证
- tests/cli/test_iso_acr_gpu_isolation.py(6 测试):
  - 01 生产源码(CLI/phase session/upm)不引用 ACR/GPU/CUDA/Mixed 后端标识(0 违例);
  - 02 生产路径不调用 register_phase2_acr_kernels;
  - 03 run config 请求 backend/acr_route/gpu_route/mixed_backend → 明确拒绝(exit 3 + "unknown key", 非静默);
  - 04 发行包脚本扫描(无 ACR/GPU 插件逻辑);
  - 05 doctor 后端清单仅纯 CPU 变体(backend_id 非 acr/gpu/cuda);
  - 06 phase3 生产 manifest 不含 acr/gpu/mixed/dispatcher/route 选路字段。
- 全量回归 unittest **248/248 OK**(新增 6, 零回归; 全套 630s, 超 600s 需后台运行)。

## 限制与遗留
- lib/acr/ 目录仍存在于树中(独立性用途), 但不在生产选路; 已用静态符号扫描+运行注册可达性证明其 dormant。发行包扫描为"生产二进制/后端清单级", 未深扫 lib/acr/ 内部(其非发布物)。
- cuda_bridge_stub 保证 Linux CUDA 不可达; Windows 侧 GPU 路径由 WIN 任务/FATDUCK 复验(本轮 Linux)。
- stage2_common 的 acr_route/gpu_route 校验属 legacy/tool parser 的 ACR 隔离防线, 与生产 phase2_session(仅用 p2_stage2_make_upm_cfg)分离。

## 产物
tests/cli/test_iso_acr_gpu_isolation.py(6 测试); artifacts/prerelease_v5/ISO-001/LOG.md; 本日志。

## PASS 判定
静态+运行证明: 生产 CLI/Phase/后端选路(backend_host)/manifest 不引用 ACR/GPU/Mixed(符号扫描 0 + 注册可达性 false + 运行 manifest 无该选路字段); 配置请求 ACR/GPU 明确拒绝(exit 3, "unknown key", 非默默 fallback); 发行包扫描仅纯 CPU 变体。ISO-001 = PASS。
