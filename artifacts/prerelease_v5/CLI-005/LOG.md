# CLI-005 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS CLI-005 行(同 CLI-004 接入 Phase2;验收=同上+实际 production route 被 test 覆盖); PHASE2_API_V1(API-P2-001 冻结: 阶段流水/所有权图/预算绑定/错误码映射 §1-4); 04 协议。

## 动作
1. lib/phase2_session/p2_session.{h,cpp}: API-P2-001 冻结生产路由——**coverage(p2_coverage_build 两次调用合同)→ sampler(p2_sample_controls 两次调用, 预算绑定 §3: sampler=1 串行=确定性 reference)→ UPM build(p2_upm_build, blocks=预算 cpu_workers 注入; target_order 随 coverage 实测)→ persist(p2_upm_save 可选)**; 所有权合同 §1 全落实(cov_guard/p2_upm_close); 取消点=阶段边界(UPM 整模型不写半成品); 错误映射 §4(rc=1→PARAM/rc=2→STATE/error_kind=input→CLI 3)。
2. CLI `phase2 run` 真接线: 进程内直调(无子进程); cancel 桥接/budget=affinity/monitor 事件与 phase1 同构; manifest complete+artifacts→verify 可用; 测试钩子同语义。
3. CMake: lib/phase2 全 10 源+acr(kernel_registry/device_executor)+cuda_bridge_stub(Linux)+aio_hips_reader/writer+aio_upm+healpix_core 编入 CLI(-w 第三方豁免; 无 -march)。
4. tests/backend/phase2_fixture_main.cpp: 合成 HiPS fixture(aio_hips writer 生产函数直写: nside=512 order0 12 tile×2 帧, signal/support, F1=1.0/F2=1.25 常量域)。
5. tests/cli/test_phase2_inprocess.py 5 集成测试: 生产路由 complete(事件单调+manifest+verify)/路由数值(obs=1536, overlap_controls=768, n_inputs=2)**/无子进程硬证(/proc children 空)**/错误映射(缺失输入→3, 坏 JSON→2, 缺键→2)/SIGINT 取消→9。
6. golden test_07 取消语义改由 run 管线证明(phase2 已真接线需会话配置)。

## 验证
- 端到端实测: coverage 12 cells→sampler 1536 obs→UPM build ok→exit 0; 事件/artifact/final 单调。
- 全量回归 unittest **164/164 OK**(新增 5)。
- 过程修复: target_order=-1 被 production build 拒(须显式层级)→取 coverage.target_order; 输入缺失映射 3(error_kind)。

## 限制与遗留
- 本会话路由覆盖 API-P2 §1 的 coverage/sampler/UPM 段; calibrate_block/rejection/integration 像素栈段(需 candidate stack 收集+行带切分)随 CODE 域生产管线扩展——会话结构已按所有权合同预留。
- sampler 并行第一遍(P2_ENABLE_OPENMP)保持 OFF(确定性 reference 冻结), 与 §3 预算绑定一致。

## 产物
lib/phase2_session/p2_session.{h,cpp}; cli/main.cpp+cli/CMakeLists.txt; tests/backend/phase2_fixture_main.cpp; tests/cli/test_phase2_inprocess.py; 本日志。

## PASS 判定
Phase2 进程内调用(无子进程硬证); 实际生产函数路由(coverage/sampler/UPM 全真实现)被集成测试覆盖; cancel/budget/monitor 注入且实测; 事件完整+错误映射正确。CLI-005 = PASS。
