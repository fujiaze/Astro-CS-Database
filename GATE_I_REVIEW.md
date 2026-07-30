# Gate I 审核包 — 进度 + 问题 + 方案

> 生成时间：2026-07-30
> 范围：engineering_authoritative Gate A-I 体系（用户确认"以最新包为准"）
> 目的：执行 I-001~I-003 前的状态审计与方案确认

---

## 1. 进度总览

| Gate | 任务 | 状态 | 证据 |
|------|------|------|------|
| A | A-001~A-004 数据整理 | DONE | GATE_A_REPORT.md ✓ |
| B | B-001~B-003 Stage1代表帧 | DONE | GATE_B_REPORT.md ✓ |
| C | C-001~C-004 HISS格式 | DONE | GATE_C_REPORT.md ✓, HISS_FORMAT_V2.md 已 FROZEN |
| D | D-001 多帧球面重合 | DONE | GATE_D_REPORT.md ✓ |
| E | E-001~E-004 全局加性曲面 | DONE | GATE_E_REPORT.md ✓ |
| F | F-001~F-002 银心最小三片 | DONE | GATE_F_REPORT.md ✓ |
| G | G-001~G-005 32帧叠加+量化 | DONE | GATE_G_REPORT.md（3片旧版，未更新30帧）；G-004/G-005 TASK_REPORT ✓ |
| H | H-001~H-004 资源调度 | DONE（有限制） | **缺 GATE_H_REPORT.md**；H-004 压力测试为模拟非真实执行 |
| I | I-001~I-003 最终回归+发布 | TODO | — |

### 真实产物确认（Glob 验证，非 LS，因 output/ 被 .gitignore 排除）
- `output/G-004/T4_RED_GalaxyCenter_30frame_fused.hcsd` — 1.3MB 主文件 ✓
- `output/G-004/*.hiss` — 30个单帧 HISS ✓
- `output/G-004/*.hcsd.snr` + `*.debug.npz` — 附属文件 ✓
- G-005 量化报告 + 5张可视化 ✓

---

## 2. 发现的问题

### 问题1：GATE_G_REPORT.md 未更新为30帧版本
- GATE_G_REPORT.md 内容是 F-002 的3片验证（"本批3片"）
- G-004/G-005 的 TASK_REPORT 是30帧版本，但 Gate 报告未同步更新
- **影响**：Gate G 验收报告与实际完成内容不符

### 问题2：GATE_H_REPORT.md 缺失
- H-001~H-004 的 TASK_REPORT 都存在，但无汇总 GATE_H_REPORT.md
- **影响**：Gate H 验收无正式汇总报告

### 问题3：H-004 压力测试为模拟执行
- H-004 TASK_REPORT 明确写："模拟非真实执行，压力测试使用成本模型预测值模拟，非真实 DLL 调用"
- 4个内存预算场景全部 PASS，0 OOM，但基于模拟值
- **影响**：真实资源调度能力未在真实管线上验证

### 问题4：PROJECT_STATE.yaml 滞后
- 显示 current_gate=G, current_task=G-004
- 实际 G-005 已完成，应进入 Gate I

---

## 3. I-001~I-003 执行方案

### I-001：冻结格式、CLI和算法契约（文档工作，无争议）

| 契约 | 现状 | 动作 |
|------|------|------|
| HISS_FORMAT_V2.md | C-001 已 FROZEN | 无需改动，确认冻结 |
| CLI 契约 | 已实现 stage1/stage2/inspect/capabilities；缺 validate/browser/resume/benchmark | 冻结已实现4个为正式契约；未实现的5个标记为 planned（README §14.1 要求） |
| 算法契约 | Stage2 算法链 E-001~E-004 + G-001~G-003 已实现 | 汇总冻结为 ALGORITHM_CONTRACT.md |
| Stage2 job schema | stage2_job.schema.json 已存在 | 确认冻结 |
| 资源 profile schema | resource_profile.schema.json 已存在 | 确认冻结 |

**交付**：`engineering_authoritative/contracts/CLI_CONTRACT.md` + `ALGORITHM_CONTRACT.md` + 更新 PROJECT_STATE

### I-002：710帧最终回归与失败分类

**关键约束**：
- 80s/帧 × 710 ≈ 15 小时（用户此前明确反对"在前面批量跑"）
- H-004 资源调度为模拟，真实管线未压力验证
- Stage1 性能 80s/帧 用户认为太慢

**方案建议（分阶段）**：
1. **阶段A（快速验证）**：跑 T2/T3/T4 各5帧代表帧（共15帧，~20分钟），确认冻结契约后无回归
2. **阶段B（全量回归）**：若阶段A通过，启动710帧后台运行，生成失败分类报告
3. **失败分类**：按 PLATESOLVE/DRIZZLE/CALIBRATE/PHOTOMETRIC 等阶段分类

### I-003：发布包、网站素材索引、最终审计ZIP

**发布包内容建议**：
- `dist/AstroCS-CLI-v1/`：orchestrator.exe + 所有 DLL + 浏览器 exe + 依赖 DLL
- 示例 HCSD：G-004 的30帧银心叠加结果
- 工程文档：README + Gate A-I 报告 + 证据索引
- SHA256SUMS.txt + verify.bat
- 网站素材索引：流程图、截图、性能数据清单

---

## 4. 待确认决策点

1. **GATE_G_REPORT.md 更新**：是否更新为30帧版本？（建议：是）
2. **GATE_H_REPORT.md 补齐**：是否基于 H-001~H-004 现有证据补齐汇总报告？（建议：是，但注明 H-004 为模拟执行限制）
3. **I-002 710帧策略**：分阶段（先15帧验证再全量）vs 直接全量 vs 跳过？
4. **I-003 发布包**：内容是否如上建议？
