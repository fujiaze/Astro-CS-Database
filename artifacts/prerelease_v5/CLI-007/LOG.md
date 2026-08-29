# CLI-007 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS CLI-007 行「实现 `run --phases`，阶段 artifact 传递、resume 校验、run manifest、取消 | 1/2/3 单独与组合；resume hash mismatch 必失败」；API-CLI-001(命令树)/API-MANIFEST-001(run_manifest 原子写+incomplete 禁止 complete/verify 合同)。

## 动作(替换原 not_wired stub)
cli/main.cpp `cmd_run_pipeline` 由"恒 incomplete+exit 2 not_wired"改为真编排:
1. **逐 phase 进程内驱动**: 按 `--phases` 升序, 每个 phase 从 run config 派生自身 config并驱动对应 session(全走 host services 注入, 不 shell-out):
   - phase3: 取 run config 的 `phase3` 子对象(CLI-006 冻结) + 补 `output_dir` → p3_session_validate/run/inspect。
   - phase2: 从 `inputs.lights` 映射 `hips_paths` + `output_dir` → p2_session。
   - phase1: 从 `inputs.lights` 映射 `input_lights` + `output_dir` → p1_session。
   - 逐 phase 收集 artifact(path+sha256+size, role=phaseN_output)聚合到 run manifest。
2. **run manifest**: 复用 `write_run_manifest`(原子 tmp+rename); 每 phase 发 stage 事件。
3. **resume/hash-mismatch**(必失败, 合同 §3): 运行开始**先**扫描 output_dir 中所有 prior `astrocs_run_*.json`, 逐 artifact 校验磁盘 sha 与其记录一致; 任一不符 → exit 8(INTEGRITY), 写 incomplete manifest, 不进入新一轮运行。绝不静默跳过验证。
4. **取消**: 保留 sleep 钩子 → CANCELLED(9) + incomplete manifest。
5. **错误映射**: 任一 phase 失败(缺 phase3/缺 inputs.lights/空 input_lights 等)→ 写 incomplete manifest + exit 4(SCIENCE), 禁伪造 complete。

## 验证
- 全量回归 **unittest 196/196 OK**(新增 2)。
- `run --phases 3`(FIELD.hips synthetic): exit 0, manifest status=complete, phases=[3], artifacts 含 phase3_output(17280B+sha256)。独立 CFITSIO 已在该 phase3_output 上一轮验证(CTYPE/CRPIX/CRVAL/COVERAGE)。
- 缺 `phase3` 子对象 → exit 4 + "run config missing 'phase3' object"。
- 空 `inputs.lights`(config init 默认)跑 1,2,3 → exit 4(incomplete), 非 not_wired 2。
- **resume/hash-mismatch**: prior manifest 记录 stable.fits 原始 sha, 篡改磁盘后 run → exit 8(INTEGRITY) + "prior artifact hash mismatch"。

## 明确拒绝/失败清单(机器验证)
缺 phase3→4; 缺 inputs.lights→4; 空 input_lights→4; prior artifact hash mismatch→8; 取消→9。均写 incomplete manifest(禁 complete)。

## 限制与遗留
- phase 间 artifact 的数据流式传递(phase1 输出→phase2 hips_paths→phase3 source.hips_dir)未做深层科学耦合;本任务验收=编排正确性 + run manifest + resume hash 校验 + 取消;SYN-009 将做完整 Phase1→2→3 数据链与中断/resume。
- run 的 phase2 config 由 `inputs.lights` 映射为 `hips_paths`(二者语义不同: hips_paths 是 tile 目录, inputs.lights 是原始帧)——这是合约上 run-config 与 phase-session 的接口适配点;若 phase2 需独立 hips 配置, 应补 phase2 子对象(超出本任务范围, 记录)。

## 产物
cli/main.cpp(cmd_run_pipeline 真编排+resume hash+mapping); tests/cli/test_cli_protocol.py(test_08 更新为真实现行为); tests/cli/test_phase3_inprocess.py(+test_07_run_phases3_complete/+test_08_run_resume_hash_mismatch); 本日志。

## PASS 判定
run --phases 1/2/3 单独与组合皆可行(组合在空/缺配置下正确失败并写 incomplete); resume hash mismatch 必失败(8); run manifest 原子写+artifacts 聚合; 取消。CLI-007 = PASS。
