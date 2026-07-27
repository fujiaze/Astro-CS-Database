#!/usr/bin/env python3
"""P06-003 更新控制文件: MASTER_TASK_REGISTER.csv / PROJECT_STATE.yaml / CURRENT_TASK.md"""
import os

CTRL = "engineering/control"

# ========== 1. MASTER_TASK_REGISTER.csv ==========
csv_path = f"{CTRL}/MASTER_TASK_REGISTER.csv"
with open(csv_path, "r", encoding="utf-8") as f:
    lines = f.readlines()

updated = []
for line in lines:
    if line.startswith("P06-003,"):
        # P06-003,P06,HCSD 输出与独立读取,P06-002;P04-003,G6,IN_PROGRESS,evidence/P06-003
        parts = line.strip().split(",")
        parts[6] = "DONE"
        line = ",".join(parts) + "\n"
    updated.append(line)

with open(csv_path, "w", encoding="utf-8") as f:
    f.writelines(updated)
print("[OK] MASTER_TASK_REGISTER.csv updated (P06-003 -> DONE)")

# ========== 2. PROJECT_STATE.yaml ==========
yaml_path = f"{CTRL}/PROJECT_STATE.yaml"
yaml_content = """project: AstroCS CLI Core
state_schema: 1
current_gate: G7
current_task: P07-001
status: TODO
last_completed_task: P06-003
product_target: CLI Real-Data Core v1
baseline_tag: astrocs-baseline-p00
g0_passed: true
g0_evidence:
  - P00-001 仓库与环境事实基线 DONE
  - P00-002 恢复并锁定全部模块源码 DONE
  - P00-003 旧 CLI 真实数据基线 DONE (stage1 45s/stage2 7s, sdet_detect_ex×2, G-001/G-002 确认)
g1_passed: true
g1_evidence:
  - P01-001 PipelineFrame 唯一归属决策 DONE (ADR-005)
  - P01-002 数据块注册表与 schema 校验器 DONE
  - P01-003 HISS/HCSD 格式版本与 round-trip DONE
g2_passed: true
g2_evidence:
  - P02-001 PlateSolve 全量 TestData 与旧路径基线 DONE
  - P02-002 共享 detections 候选路径与 star_det v1 DONE
  - P02-003 PlateSolve 全量 A/B 与路径决策 DONE
  - P02-004 实施选定 PlateSolve 单次检测路径 DONE
  - P02-005 Dynamic PSF float32 API DONE
  - P02-006 Gaia 查询边界与缓存 DONE
  - P02-007 PlateSolve 无退化与单次检测专项 Gate DONE
g3_passed: true
g3_evidence:
  - P03-001 真实校准输入接线 DONE
  - P03-002 配置参数端到端追踪 DONE
  - P03-003 严格失败与禁止静默跳过 DONE
  - P03-004 SNR 稀疏模型与 SIP 一致性 DONE
g4_passed: true
g4_evidence:
  - P04-001 CLI request 与 effective config DONE
  - P04-002 JSONL 事件与稳定错误码 DONE
  - P04-003 capabilities 与 inspect 命令 DONE
  - P04-004 取消、超时与 partial 输出 DONE
g5_passed: true
g5_evidence:
  - P05-001 真实参考数据集登记 DONE
  - P05-002 Stage1 真实数据端到端 DONE
  - P05-003 Stage1 负面与恢复测试 DONE
g6_passed: true
g6_evidence:
  - P06-001 Stage2 真实输入兼容检查 DONE
  - P06-002 球面梯度与稳健叠加证据 DONE
  - P06-003 HCSD 输出与独立读取 DONE
notes:
  - GUI is later and controls CLI through a machine protocol.
  - PlateSolve path changes are conditional on full TestData non-regression; fallback preserves internal detection and exports it to PSF.
  - Preserve existing git configuration and user changes.
  - v1.1 开发包已部署;v1.0 证据归档于 engineering_archive_v1.0/。
  - P00-P03 阶段全部完成,G0-G3 gate 通过。进入 P04 CLI 协议阶段。
  - P04-001 DONE (CLI request + effective config + SHA-256 hash + stdout/stderr 分离 + inspect/capabilities 子命令, 234/234 测试通过)。
  - P04-001 残留: 嵌套合并仅顶层覆盖 (留待 P04-002 评估); 真实 stage 成功路径未验证 (留待 P05-002); HISS/HCSD provenance 写入未实现 (留待 P05/P06)。
  - P04-002 DONE (JSONL 事件 schema 13 种 + 错误码注册表 22 条 + output_jsonl_event_ex + 错误码一致性保证 + 双事件并发向后兼容, 229/229 测试通过, VERDICT: PASS)。
  - P04-002 残留: TIMEOUT/CANCELLED 未实际触发 (留待 P04-004); 模块特定码 20-28 未实际使用 (留待 P05+); 嵌套合并保持 P04-001 实现 (GUI 暂无需求)。
  - P04-003 DONE (capabilities 扩展 modules/stages/schema_versions + inspect --hiss/--hcsd/--frame 三子命令 + HISS_INVALID/HCSD_INVALID/INPUT_INVALID 错误码实际触发 + P04-004 集成修复, 317/317 测试通过, VERDICT: PASS)。
  - P04-003 残留: DLL 版本号大多 unknown (留待 P05+); FITS 扩展头未支持 (留待 P05+); snr_format 字段类型小问题 (低优先级)。
  - P04-004 DONE (stage 级超时 + 取消信号处理 + partial 输出 + TIMEOUT/CANCELLED 错误码实际触发)。
  - P05-001 DONE (真实参考数据集登记)。
  - P05-002 DONE (Stage1 真实数据端到端, 6 个 HISS 文件生成)。
  - P05-003 DONE (Stage1 负面与恢复测试)。
  - P06-001 DONE (Stage2 真实输入兼容检查, 8/8 兼容性检查 PASS, SNR² 权重代码路径已证明触发, VERDICT: PASS)。证据: evidence/P06-001/。残留: G-002 既存缺口 (has_snr=0 退化为等权); gaia_client_create_ex 失败 (梯度校正回退)。
  - P06-002 DONE (球面梯度与稳健叠加证据, 7/7 验证 PASS, SNR² 权重真实生效 definitive proof, 梯度校正管线完整运行, 确定性保证, VERDICT: PASS)。证据: evidence/P06-002/。残留: G-002 真实数据 has_snr=0 仍退化为等权 (待 P03-004 修复后回归); HCSD has_snr 字段不传播 (不影响堆叠数学); 梯度校正 fit_rms=0.0 (C003 副本差异为 0, 待未来用不同帧测试非零差异)。
  - P06-003 DONE (HCSD 输出与独立读取, 7/7 验证 PASS, 17 个子测试全部 PASS, VERDICT: PASS)。证据: evidence/P06-003/。验证: 子叶索引 leaf_index 结构正确 (T1 78/49152 非空与 P00-003 baseline 一致, leaf_ipix 一致, sum(data_length)=n_pix); metadata 必填字段齐全 (nside/nested/n_pix/has_snr + caller 元数据); 输入追溯有效 (n_frames/n_pix/mean_pixel_count 与 stage2 日志一致); inspect --hcsd 独立读取成功 (DLL 全加载, JSONL 输出); 字节级结构符合契约 (Magic/JSON头/leaf_index/sorted_ipix 升序/文件大小); 按子叶读取 aio_hcsd_read_leaf 正确 (T1 79/79, T6 6/6 逐子叶 ipix+pixel 与全量读取一致); HCSD 字节级可重现 (T1 SHA-256 = P00-003 baseline SHA-256)。残留: 无 format_version 字段 (contract §9.1, v1.1+ 待修复); 无校验和 (§9.2); meta 无显式 input_hiss_files (§4.3 不强制); DLL 路径需用 lib/orchestrator/cpp/ (非本任务引入)。
last_updated: 2026-07-27
"""
with open(yaml_path, "w", encoding="utf-8") as f:
    f.write(yaml_content)
print("[OK] PROJECT_STATE.yaml updated (G6 passed, current_task -> P07-001, last_completed -> P06-003)")

# ========== 3. CURRENT_TASK.md ==========
md_path = f"{CTRL}/CURRENT_TASK.md"
md_content = """# 当前任务：P07-001 性能与峰值内存基线

读取 `tasks/P07-001.md` 并执行。记录新路径性能，确认去重收益和无异常回退。

## 上一任务完成情况

- P06-003 HCSD 输出与独立读取: DONE (VERDICT: PASS)
  - 证据: evidence/P06-003/
  - 7/7 验证 PASS (17 个子测试全部 PASS)
  - 子叶索引 leaf_index 结构正确 (T1 78/49152 非空与 P00-003 baseline 一致, leaf_ipix 一致, sum(data_length)=n_pix)
  - metadata 必填字段齐全 (nside/nested/n_pix/has_snr + caller 元数据 filter/n_frames/sigma_clip/stack_stats)
  - 输入追溯有效 (n_frames=2=输入 HISS 数, n_pix=15522966=stage2 日志, mean_pixel_count=1.9850 一致)
  - inspect --hcsd 独立读取成功 (DLL 全加载 9/9, JSONL result+completed 输出)
  - 字节级结构符合契约 (Magic/JSON头/leaf_index/sorted_ipix 升序/文件大小 全部验证)
  - 按子叶读取 aio_hcsd_read_leaf 正确 (T1 79/79, T6 6/6 逐子叶 ipix+pixel 与全量读取完全一致)
  - HCSD 字节级可重现 (T1 SHA-256 = P00-003 baseline SHA-256)
  - 残留: 无 format_version (§9.1); 无校验和 (§9.2); meta 无 input_hiss_files (§4.3 不强制); DLL 路径需用 lib/orchestrator/cpp/ (非本任务引入)

## P07-001 依赖

- P05-002 (DONE, Stage1 真实数据端到端)
- P06-003 (DONE, HCSD 输出与独立读取)

## 执行步骤

1. 按 `docs/10_STAGE2_REAL_DATA_VALIDATION_SPEC.md` 执行
2. 固定硬件/线程/配置后比较
3. 记录峰值内存、耗时、泄漏与取消后状态
4. 性能异常必须定位，不能只提高门限
5. 独立复核以 VERDICT: PASS 结束

完成独立复核后, 更新状态并进入依赖满足的下一任务。
"""
with open(md_path, "w", encoding="utf-8") as f:
    f.write(md_content)
print("[OK] CURRENT_TASK.md updated (-> P07-001)")
print("\nAll control files updated.")
